// Copyright lowRISC contributors.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0

#ifndef VERILATOR_SIM_CTRL_H_
#define VERILATOR_SIM_CTRL_H_

#include <verilated.h>

#include <chrono>
#include <string>
#include <functional>
#include <map>

#include "verilated_toplevel.h"

enum VerilatorSimCtrlFlags {
  Defaults = 0,
  ResetPolarityNegative = 1,
};

// Callback function to be called every clock cycle
// The parameter is simulation time
typedef std::function<void(int /* sim time */)> SimCtrlCallBack;

enum MemInitType {
  kUnknown = 0,
  kEmpty,
  kElf,
  kVmem,
};

struct MemArea {
  std::string name;      // Unique identifier
  std::string location;  // Design scope location
  std::string path;      // System file path
  MemInitType type;      // Type of init file
};

struct BufferDesc {
  uint8_t *data;
  size_t length;
};

class VerilatorSimCtrl {
 public:
  VerilatorSimCtrl(VerilatedToplevel *top, CData &clk, CData &rst_n,
                   VerilatorSimCtrlFlags flags = Defaults);

  /**
   * A helper function to execute some standard setup commands.
   *
   * This function performs the followind tasks:
   * 1. Sets up a signal handler to enable tracing to be turned on/off during
   *    a run by sending SIGUSR1 to the process
   * 2. Parses a C-style set of command line arguments (see ParseCommandArgs)
   *
   * @return return code (0 = success)
   */
  int SetupSimulation(int argc, char **argv);

  /**
   * A helper function to execute a standard set of run commands.
   *
   * This function performs the followind tasks:
   * 1. Prints some tracer-related helper messages
   * 2. Runs the simulation
   * 3. Prints some further helper messages and statistics once the simulation
   *    has run to completion
   */
  void RunSimulation();

  /**
   * Register the signal handler
   */
  void RegisterSignalHandler();

  /**
   * Print help how to use this tool
   */
  void PrintHelp() const;

  /**
   * Parse command line arguments
   *
   * This removes all recognized command-line arguments from argc/argv.
   *
   * The return value of this method indicates if the program should exit with
   * retcode: if this method returns true, do *not* exit; if it returns *false*,
   * do exit.
   */
  bool ParseCommandArgs(int argc, char **argv, int &retcode);

  /**
   * Run the main loop of the simulation
   *
   * This function blocks until the simulation finishes.
   */
  void Run();

  /**
   * Register a memory as instantiated by generic ram
   *
   * The |name| must be a unique identifier. The function will return false
   * if |name| is already used. |location| is the path to the scope of the
   * instantiated memory, which needs to support the DPI-C interfaces
   * 'simutil_verilator_memload' and 'simutil_verilator_set_mem' used for
   * 'vmem' and 'elf' files, respectively.
   *
   * Memories must be registered before command arguments are parsed by
   * ParseCommandArgs() in order for them to be known.
   */
  bool RegisterMemoryArea(const std::string name, const std::string location);

  /**
   * Get the current time in ticks
   */
  unsigned long GetTime() { return time_; }

  /**
   * Get the simulation result
   */
  bool WasSimulationSuccessful() { return simulation_success_; }

  /**
   * Set the number of clock cycles (periods) before the reset signal is
   * activated
   */
  void SetInitialResetDelay(unsigned int cycles);

  /**
   * Set the number of clock cycles (periods) the reset signal is activated
   */
  void SetResetDuration(unsigned int cycles);

  /**
   * Request the simulation to stop
   */
  void RequestStop(bool simulation_success);

  /**
   * Enable tracing (if possible)
   *
   * Enabling tracing can fail if no tracing support has been compiled into the
   * simulation.
   *
   * @return Is tracing enabled?
   */
  bool TraceOn();

  /**
   * Disable tracing
   *
   * @return Is tracing enabled?
   */
  bool TraceOff();

  /**
   * Is tracing currently enabled?
   */
  bool TracingEnabled() { return tracing_enabled_; }

  /**
   * Has tracing been ever enabled during the run?
   *
   * Tracing can be enabled and disabled at runtime.
   */
  bool TracingEverEnabled() { return tracing_ever_enabled_; }

  /**
   * Is tracing support compiled into the simulation?
   */
  bool TracingPossible() { return tracing_possible_; }

  /**
   * Print statistics about the simulation run
   */
  void PrintStatistics();

  const char *GetSimulationFileName() const;

  /**
   * Set a callback function to run every cycle
   */
  void SetOnClockCallback(SimCtrlCallBack callback);

 private:
  VerilatedToplevel *top_;
  CData &sig_clk_;
  CData &sig_rst_;
  VerilatorSimCtrlFlags flags_;
  unsigned long time_;
  bool tracing_enabled_;
  bool tracing_enabled_changed_;
  bool tracing_ever_enabled_;
  bool tracing_possible_;
  unsigned int initial_reset_delay_cycles_;
  unsigned int reset_duration_cycles_;
  volatile unsigned int request_stop_;
  volatile bool simulation_success_;
  std::chrono::steady_clock::time_point time_begin_;
  std::chrono::steady_clock::time_point time_end_;
  VerilatedTracer tracer_;
  std::map<std::string, MemArea> mem_register_;
  int term_after_cycles_;
  SimCtrlCallBack callback_;

  unsigned int GetExecutionTimeMs();
  void SetReset();
  void UnsetReset();
  bool IsFileReadable(std::string filepath);
  bool FileSize(std::string filepath, int &size_byte);
  void Trace();
  bool ElfFileToBinary(std::string file_name, void **data, size_t &len_bytes);
  bool InitMem(std::string mem_argument, int &retcode);
  MemInitType ExtractTypeFromName(std::string filename);
  MemInitType GetMemInitType(std::string name);
  bool ParseMemArg(std::string mem_argument, MemArea *m, int &retcode);
  bool MemWrite(MemArea &m, int &retcode);
  bool MemWriteElf(const std::string path, int &retcode);
  void MemWriteVmem(const std::string path);
  void PrintMemRegions();
};

#endif  // VERILATOR_SIM_CTRL_H_
