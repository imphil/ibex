// Copyright lowRISC contributors.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0

#include "register_driver.h"
#include "verilated_toplevel.h"

void RegisterDriver::OnInitial() {
  delay_ = 1;
  reg_access_ = 0;
  generator_ = std::default_random_engine(simctrl_->seed);
  delay_dist_ = std::uniform_int_distribution<int>(1, 20);
  addr_dist_ = std::uniform_int_distribution<int>(0x3A0, 0x3BF);
  wdata_dist_ = std::uniform_int_distribution<int>(0, 0xFFFFFFFF);
  write_dist_ = std::uniform_int_distribution<int>(0, 1);
}

void RegisterDriver::Randomize() {

  // generate addr
  reg_addr_ = addr_dist_(generator_);

  // read/write
  reg_op_ = write_dist_(generator_);

  // wdata
  if (reg_op_) {
    reg_wdata_ = wdata_dist_(generator_);
  }
  // delay
  delay_ = delay_dist_(generator_);

  reg_access_ = 1;
}

void RegisterDriver::DriveSignals() {
  ibex_cs_registers* dut_cs_registers = dynamic_cast<ibex_cs_registers*>(dut_);
  dut_cs_registers->csr_access_i = reg_access_;
  dut_cs_registers->instr_new_id_i = reg_access_;
  dut_cs_registers->csr_addr_i = reg_addr_;
  dut_cs_registers->csr_wdata_i = reg_wdata_;
  dut_cs_registers->csr_op_i = reg_op_;
}

void RegisterDriver::OnClock() {
  if (--delay_ == 0) {
    Randomize();
  } else {
    reg_access_ = 0;
  }
  DriveSignals();
}
