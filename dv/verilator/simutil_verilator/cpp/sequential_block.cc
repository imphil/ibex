// Copyright lowRISC contributors.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0

#include "sequential_block.h"

SequentialBlock::SequentialBlock(VerilatedToplevel *dut, VerilatorSimCtrl *sc)
    : dut_(dut),
      simctrl_(sc) {}

void SequentialBlock::OnInitial() {
}

void SequentialBlock::OnClock() {
}

void SequentialBlock::OnFinish() {
}
