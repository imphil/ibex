// Copyright lowRISC contributors.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0

#include "verilator_sim_ctrl.h"

#include <fcntl.h>
#include <gelf.h>
#include <getopt.h>
#include <libelf.h>
#include <map>
#include <signal.h>
#include <sys/stat.h>
#include <sysexits.h>
#include <utility>
#include <unistd.h>
#include <vltstd/svdpi.h>

#include <iostream>

// This is defined by Verilator and passed through the command line
#ifndef VM_TRACE
#define VM_TRACE 0
#endif

// Static pointer to a single simctrl instance
// used by SignalHandler
static VerilatorSimCtrl *simctrl;

static void SignalHandler(int sig) {
  if (!simctrl) {
    return;
  }

  switch (sig) {
    case SIGINT:
      simctrl->RequestStop(true);
      break;
    case SIGUSR1:
      if (simctrl->TracingEnabled()) {
        simctrl->TraceOff();
      } else {
        simctrl->TraceOn();
      }
      break;
  }
}

/**
 * Get the current simulation time
 *
 * Called by $time in Verilog, converts to double, to match what SystemC does
 */
double sc_time_stamp() {
  if (simctrl) {
    return simctrl->GetTime();
  } else {
    return 0;
  }
}

// DPI Exports
extern "C" {
extern void simutil_verilator_memload(const char *file);
extern void simutil_verilator_set_mem(int index, const svLogicVecVal *val);
}

VerilatorSimCtrl::VerilatorSimCtrl(VerilatedToplevel *top, CData &sig_clk,
                                   CData &sig_rst, VerilatorSimCtrlFlags flags)
    : top_(top),
      sig_clk_(sig_clk),
      sig_rst_(sig_rst),
      flags_(flags),
      time_(0),
      tracing_enabled_(false),
      tracing_enabled_changed_(false),
      tracing_ever_enabled_(false),
      tracing_possible_(VM_TRACE),
      initial_reset_delay_cycles_(2),
      reset_duration_cycles_(2),
      request_stop_(false),
      simulation_success_(true),
      tracer_(VerilatedTracer()),
      term_after_cycles_(0),
      callback_(nullptr) {}

int VerilatorSimCtrl::SetupSimulation(int argc, char **argv) {
  int retval;
  // Setup the signal handler for this instance
  RegisterSignalHandler();
  // Parse the command line argumanets
  if (!ParseCommandArgs(argc,argv,retval)) {
    return retval;
  }
  return 0;
}

void VerilatorSimCtrl::RunSimulation() {
  // Print helper message for tracing
  if (TracingPossible()) {
    std::cout << "Tracing can be toggled by sending SIGUSR1 to this process:"
              << std::endl
              << "$ kill -USR1 " << getpid() << std::endl;

  }
  // Run the simulation
  Run();
  // Print simulation speed info
  PrintStatistics();
  // Print helper message for tracing
  if (TracingEverEnabled()) {
    std::cout << std::endl
              << "You can view the simulation traces by calling" << std::endl
              << "$ gtkwave " << GetSimulationFileName() << std::endl;
  }
}

void VerilatorSimCtrl::RegisterSignalHandler() {
  struct sigaction sigIntHandler;

  // Point the static simctrl pointer at this object
  simctrl = this;

  sigIntHandler.sa_handler = SignalHandler;
  sigemptyset(&sigIntHandler.sa_mask);
  sigIntHandler.sa_flags = 0;

  sigaction(SIGINT, &sigIntHandler, NULL);
  sigaction(SIGUSR1, &sigIntHandler, NULL);
}

void VerilatorSimCtrl::RequestStop(bool simulation_success) {
  request_stop_ = true;
  simulation_success_ &= simulation_success;
}

bool VerilatorSimCtrl::TraceOn() {
  bool old_tracing_enabled = tracing_enabled_;

  tracing_enabled_ = tracing_possible_;
  tracing_ever_enabled_ = tracing_enabled_;

  if (old_tracing_enabled != tracing_enabled_) {
    tracing_enabled_changed_ = true;
  }
  return tracing_enabled_;
}

bool VerilatorSimCtrl::TraceOff() {
  if (tracing_enabled_) {
    tracing_enabled_changed_ = true;
  }
  tracing_enabled_ = false;
  return tracing_enabled_;
}

void VerilatorSimCtrl::PrintHelp() const {
  std::cout << "Execute a simulation model for " << top_->name()
            << "\n"
               "\n";
  if (tracing_possible_) {
    std::cout << "-t|--trace                    Write a trace file from the"
                 " start\n";
  }
  std::cout << "-m|--meminit=name,file[,type] Initialize memory NAME with FILE"
               " [of TYPE]\n"
               "                              TYPE is either 'elf' or 'vmem'\n"
               "                              Use \"list\" for NAME without "
               "FILE or TYPE to print registered memory regions\n"
               "-c|--term-after-cycles=N  Terminate simulation after N cycles\n"
               "-h|--help                     Show help\n"
               "\n"
               "All further arguments are passed to the design and can be used "
               "in the \n"
               "design, e.g. by DPI modules.\n";
}

bool VerilatorSimCtrl::RegisterMemoryArea(const std::string name,
                                          const std::string location) {
  MemArea mem = {.name = name, .location = location};

  auto ret = mem_register_.emplace(name, mem);
  if (ret.second == false) {
      std::cerr << "ERROR: Can not register \"" << name << "\" at: \""
                << location << "\" (Previously defined at: \""
                << ret.first->second.location << "\")" << std::endl;
      return false;
  }
  return true;
}

MemInitType VerilatorSimCtrl::GetMemInitType(std::string name) {
  if (name.compare("elf") == 0) {
    return kElf;
  } else if (name.compare("vmem") == 0) {
    return kVmem;
  }
  return kUnknown;
}

MemInitType VerilatorSimCtrl::ExtractTypeFromName(std::string filename) {
  size_t ext_pos = filename.find_last_of(".");
  std::string ext = filename.substr(ext_pos + 1);

  if (ext_pos == std::string::npos) {
    return kEmpty;
  }
  return GetMemInitType(ext);
}

void VerilatorSimCtrl::PrintMemRegions() {
  std::cout << "Registerd memory regions:" << std::endl;
  for (const auto &m : mem_register_) {
    std::cout << "\t'" << m.second.name << "' at location: '"
              << m.second.location << "'" << std::endl;
  }
}

/// Initialize a specific memory.
///
/// Use RegisterMemoryArea() before calling InitMem() in order to have the
/// memory descriptions available in mem_register_.
bool VerilatorSimCtrl::InitMem(std::string mem_argument, int &retcode) {
  MemArea m;
  if (!ParseMemArg(mem_argument, &m, retcode)) {
    return false;
  }
  // Search for corresponding registered memory based on the name
  auto registerd = mem_register_.find(m.name);
  if (registerd != mem_register_.end()) {
    m.location = registerd->second.location;
  } else {
    std::cerr << "Memory location not set for: '" << m.name.data() << "'"
              << std::endl;
    PrintMemRegions();
    retcode = EX_DATAERR;
    return false;
  }
  return MemWrite(m, retcode);
}

// Parse argument section specific to memory initialization.
// Must be in the form of: name,file[,type]
bool VerilatorSimCtrl::ParseMemArg(std::string mem_argument, MemArea *m,
                                   int &retcode) {
  std::array<std::string, 3> args;
  size_t pos = 0;
  size_t end_pos = 0;
  size_t i;
  for (i = 0; i < 3; ++i) {
    end_pos = mem_argument.find(",", pos);
    // Check for possible exit conditions
    if (pos == end_pos) {
      std::cerr << "ERROR: empty filed in: " << mem_argument << std::endl;
      retcode = EX_USAGE;
      return false;
    } else if (end_pos == std::string::npos) {
      args[i] = mem_argument.substr(pos);
      break;
    }
    args[i] = mem_argument.substr(pos, end_pos - pos);
    pos = end_pos + 1;
  }
  // mem_argument is not empty as getopt requires an argument,
  // but not a valid argument for memory initialization
  if (i == 0) {
    // Special keyword for printing memory regions
    if (mem_argument.compare("list") == 0) {
      retcode = EX_OK;
      PrintMemRegions();
    } else {
      std::cerr << "ERROR: meminit must be in \"name,file[,type]\""
                << " got: " << mem_argument << std::endl;
      retcode = EX_USAGE;
    }
    return false;
  } else if (i == 1) {
    // Type not set explicitly
    m->type = ExtractTypeFromName(args[1]);
  } else {
    m->type = GetMemInitType(args[2]);
  }
  m->name = args[0];
  m->path = args[1];
  if (!IsFileReadable(m->path)) {
    std::cerr << "ERROR: Memory initialization file "
              << "'" << m->path << "'"
              << " is not readable." << std::endl;
    retcode = EX_NOINPUT;
    return false;
  }
  retcode = EX_OK;
  return true;
}

bool VerilatorSimCtrl::MemWrite(MemArea &m, int &retcode) {
  svScope scope;

  scope = svGetScopeFromName(m.location.data());
  if (!scope) {
    std::cerr << "ERROR: No Memory found at " << m.location.data() << std::endl;
    retcode = EX_UNAVAILABLE;
    return false;
  }
  svSetScope(scope);

  switch (m.type) {
    case kEmpty:  // elf file might have no extension at all
    case kElf:
      return MemWriteElf(m.path, retcode);
    case kVmem:
      MemWriteVmem(m.path);
      break;
    case kUnknown:
    default:
      std::cerr << "ERROR: Unknown file type for " << m.location.data()
                << std::endl;
      retcode = EX_DATAERR;
      return false;
  }
  retcode = EX_OK;
  return true;
}

bool VerilatorSimCtrl::MemWriteElf(const std::string path, int &retcode) {
  uint8_t *buf;
  size_t len_bytes;

  if (!ElfFileToBinary(path.data(), (void **)&buf, len_bytes)) {
    retcode = EX_SOFTWARE;
    return false;
  }
  for (int i = 0; i < len_bytes / 4; ++i) {
    simutil_verilator_set_mem(i, (svLogicVecVal *)&buf[4 * i]);
  }
  free(buf);
  retcode = EX_OK;
  return true;
}

void VerilatorSimCtrl::MemWriteVmem(const std::string path) {
  simutil_verilator_memload(path.data());
}

bool VerilatorSimCtrl::ParseCommandArgs(int argc, char **argv, int &retcode) {
  const struct option long_options[] = {
      {"meminit", required_argument, nullptr, 'm'},
      {"term-after-cycles", required_argument, nullptr, 'c'},
      {"trace", no_argument, nullptr, 't'},
      {"help", no_argument, nullptr, 'h'},
      {nullptr, no_argument, nullptr, 0}};

  retcode = EX_OK;
  while (1) {
    int c = getopt_long(argc, argv, ":m:c:th", long_options, nullptr);
    if (c == -1) {
      break;
    }

    // Disable error reporting by getopt
    opterr = 0;

    switch (c) {
      case 0:
        break;
      case 'm':
        if (!InitMem(optarg, retcode)) {
          return false;
        }
        break;
      case 't':
        if (!tracing_possible_) {
          std::cerr << "ERROR: Tracing has not been enabled at compile time."
                    << std::endl;
          retcode = EX_USAGE;
          return false;
        }
        TraceOn();
        break;
      case 'c':
        term_after_cycles_ = atoi(optarg);
        break;
      case 'h':
        PrintHelp();
        return false;
      case ':':  // missing argument
        std::cerr << "ERROR: Missing argument." << std::endl;
        PrintHelp();
        retcode = EX_USAGE;
        return false;
      case '?':
      default:;
        // Ignore unrecognized options since they might be consumed by
        // Verilator's built-in parsing below.
    }
  }

  Verilated::commandArgs(argc, argv);
  return true;
}

void VerilatorSimCtrl::Trace() {
  // We cannot output a message when calling TraceOn()/TraceOff() as these
  // functions can be called from a signal handler. Instead we print the message
  // here from the main loop.
  if (tracing_enabled_changed_) {
    if (TracingEnabled()) {
      std::cout << "Tracing enabled." << std::endl;
    } else {
      std::cout << "Tracing disabled." << std::endl;
    }
    tracing_enabled_changed_ = false;
  }

  if (!TracingEnabled()) {
    return;
  }

  if (!tracer_.isOpen()) {
    tracer_.open(GetSimulationFileName());
    std::cout << "Writing simulation traces to " << GetSimulationFileName()
              << std::endl;
  }

  tracer_.dump(GetTime());
}

const char *VerilatorSimCtrl::GetSimulationFileName() const {
#ifdef VM_TRACE_FMT_FST
  return "sim.fst";
#else
  return "sim.vcd";
#endif
}

void VerilatorSimCtrl::SetOnClockCallback(SimCtrlCallBack callback) {
  callback_ = callback;
}

void VerilatorSimCtrl::Run() {
  // We always need to enable this as tracing can be enabled at runtime
  if (tracing_possible_) {
    Verilated::traceEverOn(true);
    top_->trace(tracer_, 99, 0);
  }

  // Evaluate all initial blocks, including the DPI setup routines
  top_->eval();

  std::cout << std::endl
            << "Simulation running, end by pressing CTRL-c." << std::endl;

  time_begin_ = std::chrono::steady_clock::now();
  UnsetReset();
  Trace();
  while (1) {
    if (time_ >= initial_reset_delay_cycles_ * 2) {
      SetReset();
    }
    if (time_ >= reset_duration_cycles_ * 2 + initial_reset_delay_cycles_ * 2) {
      UnsetReset();
    }

    sig_clk_ = !sig_clk_;

    if (sig_clk_ && (callback_ != nullptr)) {
      callback_(time_);
    }

    top_->eval();
    time_++;

    Trace();

    if (request_stop_) {
      std::cout << "Received stop request, shutting down simulation."
                << std::endl;
      break;
    }
    if (Verilated::gotFinish()) {
      std::cout << "Received $finish() from Verilog, shutting down simulation."
                << std::endl;
      break;
    }
    if (term_after_cycles_ && time_ > term_after_cycles_) {
      std::cout << "Simulation timeout of " << term_after_cycles_
                << " cycles reached, shutting down simulation." << std::endl;
      break;
    }
  }

  top_->final();
  time_end_ = std::chrono::steady_clock::now();

  if (TracingEverEnabled()) {
    tracer_.close();
  }
}

void VerilatorSimCtrl::SetReset() {
  if (flags_ & ResetPolarityNegative) {
    sig_rst_ = 0;
  } else {
    sig_rst_ = 1;
  }
}

void VerilatorSimCtrl::UnsetReset() {
  if (flags_ & ResetPolarityNegative) {
    sig_rst_ = 1;
  } else {
    sig_rst_ = 0;
  }
}

void VerilatorSimCtrl::SetInitialResetDelay(unsigned int cycles) {
  initial_reset_delay_cycles_ = cycles;
}

void VerilatorSimCtrl::SetResetDuration(unsigned int cycles) {
  reset_duration_cycles_ = cycles;
}

bool VerilatorSimCtrl::IsFileReadable(std::string filepath) {
  struct stat statbuf;
  return stat(filepath.data(), &statbuf) == 0;
}

bool VerilatorSimCtrl::FileSize(std::string filepath, int &size_byte) {
  struct stat statbuf;
  if (stat(filepath.data(), &statbuf) != 0) {
    size_byte = 0;
    return false;
  }

  size_byte = statbuf.st_size;
  return true;
}

unsigned int VerilatorSimCtrl::GetExecutionTimeMs() {
  return std::chrono::duration_cast<std::chrono::milliseconds>(time_end_ -
                                                               time_begin_)
      .count();
}

void VerilatorSimCtrl::PrintStatistics() {
  double speed_hz = time_ / 2 / (GetExecutionTimeMs() / 1000.0);
  double speed_khz = speed_hz / 1000.0;

  std::cout << std::endl
            << "Simulation statistics" << std::endl
            << "=====================" << std::endl
            << "Executed cycles:  " << time_ / 2 << std::endl
            << "Wallclock time:   " << GetExecutionTimeMs() / 1000.0 << " s"
            << std::endl
            << "Simulation speed: " << speed_hz << " cycles/s "
            << "(" << speed_khz << " kHz)" << std::endl;

  int trace_size_byte;
  if (tracing_enabled_ && FileSize(GetSimulationFileName(), trace_size_byte)) {
    std::cout << "Trace file size:  " << trace_size_byte << " B" << std::endl;
  }
}

bool VerilatorSimCtrl::ElfFileToBinary(std::string file_name, void **data,
                                       size_t &len_bytes) {
  bool retval;
  std::list<BufferDesc> buffers;
  size_t offset = 0;
  (void)elf_errno();
  len_bytes = 0;

  if (elf_version(EV_CURRENT) == EV_NONE) {
    std::cerr << elf_errmsg(-1) << std::endl;
    return false;
  }

  int fd = open(file_name.c_str(), O_RDONLY, 0);
  if (fd < 0) {
    std::cerr << "Could not open file: " << file_name << std::endl;
    return false;
  }

  Elf *elf_desc;
  elf_desc = elf_begin(fd, ELF_C_READ, NULL);
  if (elf_desc == NULL) {
    std::cerr << elf_errmsg(-1) << " in: " << file_name << std::endl;
    retval = false;
    goto return_fd_end;
  }
  if (elf_kind(elf_desc) != ELF_K_ELF) {
    std::cerr << "Not a ELF file: " << file_name << std::endl;
    retval = false;
    goto return_elf_end;
  }
  // TODO: add support for ELFCLASS64
  if (gelf_getclass(elf_desc) != ELFCLASS32) {
    std::cerr << "Not a 32-bit ELF file: " << file_name << std::endl;
    retval = false;
    goto return_elf_end;
  }

  size_t phnum;
  if (elf_getphdrnum(elf_desc, &phnum) != 0) {
    std::cerr << elf_errmsg(-1) << " in: " << file_name << std::endl;
    retval = false;
    goto return_elf_end;
  }

  GElf_Phdr phdr;
  Elf_Data *elf_data;
  elf_data = NULL;
  for (size_t i = 0; i < phnum; i++) {
    if (gelf_getphdr(elf_desc, i, &phdr) == NULL) {
      std::cerr << elf_errmsg(-1) << " segment number: " << i
                << " in: " << file_name << std::endl;
      retval = false;
      goto return_elf_end;
    }
    if (phdr.p_type != PT_LOAD) {
      std::cout << "Program header number " << i << "is not of type PT_LOAD."
                << "Continue." << std::endl;
      continue;
    }
    elf_data = elf_getdata_rawchunk(elf_desc, phdr.p_offset, phdr.p_filesz,
                                    ELF_T_BYTE);

    if (elf_data == NULL) {
      retval = false;
      goto return_elf_end;
    }

    BufferDesc buf_data;
    buf_data.length = elf_data->d_size;
    len_bytes += buf_data.length;
    buf_data.data = (uint8_t *)malloc(elf_data->d_size);
    memcpy(buf_data.data, ((uint8_t *)elf_data->d_buf), buf_data.length);
    buffers.push_back(buf_data);

    size_t init_zeros = phdr.p_memsz - phdr.p_filesz;
    if (init_zeros > 0) {
      BufferDesc buf_zeros;
      buf_zeros.length = init_zeros;
      len_bytes += buf_zeros.length;
      buf_zeros.data = (uint8_t *)calloc(1, buf_zeros.length);
      buffers.push_back(buf_zeros);
    }
  }

  // Put the collected data into a continuous buffer
  // Memory is freed by the caller
  *data = (uint8_t *)malloc(len_bytes);
  for (std::list<BufferDesc>::iterator it = buffers.begin();
       it != buffers.end(); ++it) {
    memcpy(((uint8_t *)*data) + offset, it->data, it->length);
    offset += it->length;
    free(it->data);
  }
  buffers.clear();

  retval = true;

return_elf_end:
  elf_end(elf_desc);
return_fd_end:
  close(fd);
  return retval;
}
