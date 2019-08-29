// Copyright lowRISC contributors.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0

#ifndef REGISTER_MODEL_H_
#define REGISTER_MODEL_H_

#include <vector>
#include <stdint.h>
#include "sequential_block.h"

class BaseRegister {
public:
  BaseRegister(uint32_t addr, std::vector<BaseRegister *> *map_pointer);
  virtual ~BaseRegister() = default;
  virtual void RegisterReset();
  virtual uint32_t RegisterWrite(uint32_t newval);
  virtual uint32_t RegisterSet(uint32_t newval);
  virtual uint32_t RegisterClear(uint32_t newval);
  virtual bool MatchAddr(uint32_t addr);
  virtual uint32_t RegisterRead();
  virtual uint32_t IsLocked();

protected:
  uint32_t register_value_;
  uint32_t register_address_;
  std::vector<BaseRegister *> *map_pointer_;
};

class PmpCfgRegister : public BaseRegister {
public:
  PmpCfgRegister(uint32_t addr, std::vector<BaseRegister *> *map_pointer)
      : BaseRegister(addr, map_pointer) {}
  uint32_t IsLocked();
  uint32_t RegisterWrite(uint32_t newval);
  uint32_t RegisterSet(uint32_t newval);
  uint32_t RegisterClear(uint32_t newval);

private:
  const uint32_t raz_mask_ = 0x9F9F9F9F;
};

class PmpAddrRegister : public BaseRegister {
public:
  PmpAddrRegister(uint32_t addr, std::vector<BaseRegister *> *map_pointer)
      : BaseRegister(addr, map_pointer) {}
  uint32_t IsLocked();
};

class NonImpRegister : public BaseRegister {
public:
  NonImpRegister(uint32_t addr, std::vector<BaseRegister *> *map_pointer)
      : BaseRegister(addr, map_pointer) {}
  uint32_t RegisterRead();
  uint32_t RegisterWrite(uint32_t newval);
  uint32_t RegisterSet(uint32_t newval);
  uint32_t RegisterClear(uint32_t newval);
};

class RegisterModel : public SequentialBlock {
public:
  using SequentialBlock::SequentialBlock;
  ~RegisterModel();
  void OnInitial();
  void OnClock();

private:
  void RegisterReset();
  std::vector<BaseRegister *> register_map_;
};

#endif // REGISTER_MODEL_H_
