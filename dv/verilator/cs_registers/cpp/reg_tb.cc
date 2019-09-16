#include "verilated_toplevel.h"
#include "verilator_sim_ctrl.h"

#include "register_driver.h"
#include "register_model.h"

#include <functional>
#include <iostream>
#include <signal.h>

ibex_cs_registers *top;
VerilatorSimCtrl *simctrl;
RegisterModel *reg_model;
RegisterDriver *reg_driver;
int num_transactions;

// dummy definition since this DPI call doesn't exist
extern "C" {
void simutil_verilator_memload(const char *file) {}
}

static void SignalHandler(int sig) {
  if (!simctrl) {
    return;
  }

  switch (sig) {
  case SIGINT:
    simctrl->RequestStop();
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

static void SetupSignalHandler() {
  struct sigaction sigIntHandler;

  sigIntHandler.sa_handler = SignalHandler;
  sigemptyset(&sigIntHandler.sa_mask);
  sigIntHandler.sa_flags = 0;

  sigaction(SIGINT, &sigIntHandler, NULL);
  sigaction(SIGUSR1, &sigIntHandler, NULL);
}

// Function called once every clock cycle from SimCtrl
int OnClock(int time) {
  int retval = 0;
  // Increment number of transactions
  if (top->csr_access_i)
    ++num_transactions;
  // Call register model checker
  reg_model->OnClock();
  // Call register driver for sync signals
  reg_driver->OnClock();
  if (num_transactions >= 10000) {
    retval = 1;
  }
  return retval;
}

int main(int argc, char **argv) {
  int retcode;
  int seed = 0;
  simctrl = new VerilatorSimCtrl(top, top->clk_i, top->rst_ni,
                                 VerilatorSimCtrlFlags::ResetPolarityNegative);
  // init top verilog instance
  top = new ibex_cs_registers;
  // Create TB model
  reg_model = new RegisterModel(top, simctrl);
  // Create TB driver
  reg_driver = new RegisterDriver(top, simctrl);
  num_transactions = 0;


  SetupSignalHandler();

  if (!simctrl->ParseCommandArgs(argc, argv, retcode)) {
    goto free_return;
  }

  std::cout << "Simulation of Ibex" << std::endl
            << "==================" << std::endl
            << std::endl;

  if (simctrl->TracingPossible()) {
    std::cout << "Tracing can be toggled by sending SIGUSR1 to this process:"
              << std::endl
              << "$ kill -USR1 " << getpid() << std::endl;
  }

  // Add SimCtrl callback
  simctrl->AddCallback(&OnClock);

  simctrl->Run();

  std::cout << "Drove " << std::dec << num_transactions
            << " register transactions" << std::endl;
  simctrl->PrintStatistics();

  if (simctrl->TracingEverEnabled()) {
    std::cout << std::endl
              << "You can view the simulation traces by calling" << std::endl
              << "$ gtkwave " << simctrl->GetSimulationFileName() << std::endl;
  }

free_return:
  delete top;
  delete simctrl;
  delete reg_model;
  delete reg_driver;
  return retcode;
}
