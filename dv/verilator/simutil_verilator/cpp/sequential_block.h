// Copyright lowRISC contributors.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0

#ifndef SEQUENTIAL_BLOCK_H_
#define SEQUENTIAL_BLOCK_H_

#include "verilator_sim_ctrl.h"

class SequentialBlock {
 public:
  SequentialBlock(VerilatedToplevel *dut, VerilatorSimCtrl *sc);
 protected:
  VerilatedToplevel *dut_;
  VerilatorSimCtrl *simctrl_;

  void OnInitial();
  void OnClock();
  void OnFinish();
};

#endif
