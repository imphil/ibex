// Copyright lowRISC contributors.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0

#include <signal.h>

#include <iostream>

#include "verilated_toplevel.h"
#include "verilator_sim_ctrl.h"

ibex_riscv_compliance *top;
VerilatorSimCtrl *simctrl;

int main(int argc, char **argv) {
  int retcode;
  top = new ibex_riscv_compliance;
  simctrl = new VerilatorSimCtrl(top, top->IO_CLK, top->IO_RST_N,
                                 VerilatorSimCtrlFlags::ResetPolarityNegative);

  // Setup simctrl
  simctrl->RegisterMemoryArea("ram", "TOP.ibex_riscv_compliance.u_ram");
  retcode = simctrl->SetupSimulation(argc, argv);
  if (retcode == kMemList) {
    // Error signal for listing memories, abort normal operation as successful
    retcode = 0;
    goto free_return;
  } else if (retcode != 0) {
    goto free_return;
  }

  // Run the simulation
  simctrl->RunSimulation();

free_return:
  delete top;
  delete simctrl;
  return retcode;
}
