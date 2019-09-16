// Copyright lowRISC contributors.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0

#include "register_model.h"
#include "reg_env.h"

#include <iostream>

BaseRegister::BaseRegister(uint32_t addr,
                           std::vector<BaseRegister *> *map_pointer)
    : register_value_(0), register_address_(addr), map_pointer_(map_pointer) {}

uint32_t BaseRegister::RegisterWrite(uint32_t newval) {
  uint32_t lock_mask = IsLocked();
  uint32_t read_value = register_value_;
  register_value_ &= lock_mask;
  register_value_ |= (newval & ~lock_mask);
  return read_value;
}

uint32_t BaseRegister::RegisterSet(uint32_t newval) {
  uint32_t lock_mask = IsLocked();
  uint32_t read_value = register_value_;
  register_value_ |= (newval & ~lock_mask);
  return read_value;
}

uint32_t BaseRegister::RegisterClear(uint32_t newval) {
  uint32_t lock_mask = IsLocked();
  uint32_t read_value = register_value_;
  register_value_ &= (~newval | lock_mask);
  return read_value;
}

bool BaseRegister::MatchAddr(uint32_t addr) {
  return (addr == register_address_);
}

void BaseRegister::RegisterReset() { register_value_ = 0; }

uint32_t BaseRegister::RegisterRead() { return register_value_; }

uint32_t BaseRegister::IsLocked() { return 0; }

uint32_t PmpCfgRegister::IsLocked() {
  uint32_t lock_mask = 0;
  if (register_value_ & 0x80)
    lock_mask |= 0xFF;
  if (register_value_ & 0x8000)
    lock_mask |= 0xFF00;
  if (register_value_ & 0x800000)
    lock_mask |= 0xFF0000;
  if (register_value_ & 0x80000000)
    lock_mask |= 0xFF000000;
  return lock_mask;
}

uint32_t PmpCfgRegister::RegisterWrite(uint32_t newval) {
  uint32_t lock_mask = IsLocked();
  uint32_t read_value = register_value_;
  register_value_ &= lock_mask;
  register_value_ |= (newval & ~lock_mask);
  register_value_ &= raz_mask_;
  for (int i = 0; i < 4; i++) {
    // Reserved check, W = 1, R = 0
    if (((register_value_ >> (8 * i)) & 0x3) == 0x2) {
      register_value_ &= ~(0x3 << (8 * i));
    }
  }
  return read_value;
}

uint32_t PmpCfgRegister::RegisterSet(uint32_t newval) {
  uint32_t lock_mask = IsLocked();
  uint32_t read_value = register_value_;
  register_value_ |= (newval & ~lock_mask);
  register_value_ &= raz_mask_;
  for (int i = 0; i < 4; i++) {
    // Reserved check, W = 1, R = 0
    if (((register_value_ >> (8 * i)) & 0x3) == 0x2) {
      register_value_ &= ~(0x3 << (8 * i));
    }
  }
  return read_value;
}

uint32_t PmpCfgRegister::RegisterClear(uint32_t newval) {
  uint32_t lock_mask = IsLocked();
  uint32_t read_value = register_value_;
  register_value_ &= (~newval | lock_mask);
  register_value_ &= raz_mask_;
  for (int i = 0; i < 4; i++) {
    // Reserved check, W = 1, R = 0
    if (((register_value_ >> (8 * i)) & 0x3) == 0x2) {
      register_value_ &= ~(0x3 << (8 * i));
    }
  }
  return read_value;
}

uint32_t PmpAddrRegister::IsLocked() {
  // Calculate which region this is
  uint32_t pmp_region = (register_address_ & 0xF);
  // Form the address of the corresponding CFG register
  uint32_t pmp_cfg_addr = 0x3A0 + (pmp_region / 4);
  // Form the address of the CFG registerfor the next region
  // For region 15, this will point to a non-existant register, which is fine
  uint32_t pmp_cfg_plus1_addr = 0x3A0 + ((pmp_region + 1) / 4);
  uint32_t cfg_value = 0;
  uint32_t cfg_plus1_value = 0;
  // Find and read the two CFG registers
  for (std::vector<BaseRegister *>::iterator it = map_pointer_->begin();
       it != map_pointer_->end(); ++it) {
    if ((*it)->MatchAddr(pmp_cfg_addr)) {
      cfg_value = (*it)->RegisterRead();
    }
    if ((*it)->MatchAddr(pmp_cfg_plus1_addr)) {
      cfg_plus1_value = (*it)->RegisterRead();
    }
  }
  // Shift to the relevant bits in the CFG registers
  cfg_value >>= ((pmp_region & 0x3) * 8);
  cfg_plus1_value >>= (((pmp_region + 1) & 0x3) * 8);
  // Locked if the lock bit is set, or the next region is TOR
  if ((cfg_value & 0x80) || ((cfg_plus1_value & 0x18) == 0x8)) {
    return 0xFFFFFFFF;
  } else {
    return 0;
  }
}

uint32_t NonImpRegister::RegisterRead() { return 0; }

uint32_t NonImpRegister::RegisterWrite(uint32_t newval) { return 0; }

uint32_t NonImpRegister::RegisterSet(uint32_t newval) { return 0; }

uint32_t NonImpRegister::RegisterClear(uint32_t newval) { return 0; }

void RegisterModel::OnInitial() {
  for (int i = 0; i < 4; i++) {
    uint32_t reg_addr = 0x3A0 + i;
    if (i < (4 / 4)) {
      register_map_.push_back(new PmpCfgRegister(reg_addr, &register_map_));
    } else {
      register_map_.push_back(new NonImpRegister(reg_addr, &register_map_));
    }
  }
  for (int i = 0; i < 16; i++) {
    uint32_t reg_addr = 0x3B0 + i;
    if (i < 4) {
      register_map_.push_back(new PmpAddrRegister(reg_addr, &register_map_));
    } else {
      register_map_.push_back(new NonImpRegister(reg_addr, &register_map_));
    }
  }
}

RegisterModel::~RegisterModel() {
  for (std::vector<BaseRegister *>::iterator it = register_map_.begin();
       it != register_map_.end(); ++it) {
    delete *it;
  }
  register_map_.clear();
}

void RegisterModel::RegisterReset() {
  for (std::vector<BaseRegister *>::iterator it = register_map_.begin();
       it != register_map_.end(); ++it) {
    (*it)->RegisterReset();
  }
}

void RegisterModel::OnClock() {
  ibex_cs_registers* dut_cs_registers = dynamic_cast<ibex_cs_registers*>(dut_);

  if (!dut_cs_registers->rst_ni) {
    RegisterReset();
    return;
  }
  // TODO add machine mode permissions to registers
  if (dut_cs_registers->csr_access_i && dut_cs_registers->instr_new_id_i) {
    uint32_t read_val;
    bool matched = 0;
    for (std::vector<BaseRegister *>::iterator it = register_map_.begin();
         it != register_map_.end(); ++it) {
      if ((*it)->MatchAddr(dut_cs_registers->csr_addr_i)) {
        matched = 1;
        switch (dut_cs_registers->csr_op_i) {
        case 0: {
          read_val = (*it)->RegisterRead();
          break;
        }
        case 1: {
          read_val = (*it)->RegisterWrite(dut_cs_registers->csr_wdata_i);
          break;
        }
        case 2: {
          read_val = (*it)->RegisterSet(dut_cs_registers->csr_wdata_i);
          break;
        }
        case 3: {
          read_val = (*it)->RegisterClear(dut_cs_registers->csr_wdata_i);
          break;
        }
        }
        if (read_val != dut_cs_registers->csr_rdata_o) {
          std::cout << "Error, reg_read addr: " << std::hex
                    << dut_cs_registers->csr_addr_i << std::endl;
          std::cout << "Expected: " << read_val
                    << " Got: " << dut_cs_registers->csr_rdata_o << std::endl;
          return;
        }
      }
    }
    if (!matched) {
      // Non existant register
      if (!dut_cs_registers->illegal_csr_insn_o) {
        std::cout << "Non-existant register: " << std::hex
                  << dut_cs_registers->csr_addr_i << std::endl;
        std::cout << " Should have signalled an error." << std::endl;
        return;
      }
      return;
    }
  }
  return;
}
