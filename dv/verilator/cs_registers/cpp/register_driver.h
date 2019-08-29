// Copyright lowRISC contributors.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0

#ifndef REGISTER_DRIVER_H_
#define REGISTER_DRIVER_H_

#include <random>
#include "sequential_block.h"

class RegisterDriver : public SequentialBlock {

  using SequentialBlock::SequentialBlock;
public:
  void OnInitial();
  void OnClock();

private:
  void Randomize();
  void DriveSignals();

  std::default_random_engine generator_;
  int delay_;
  bool reg_access_;
  uint32_t reg_op_;
  std::uniform_int_distribution<int> delay_dist_;
  std::uniform_int_distribution<int> addr_dist_;
  std::uniform_int_distribution<int> wdata_dist_;
  std::uniform_int_distribution<int> write_dist_;
  uint32_t reg_addr_;
  uint32_t reg_wdata_;
};

#endif // REGISTER_DRIVER_H_
