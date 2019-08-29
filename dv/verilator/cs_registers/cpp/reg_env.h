#ifndef REG_ENV_H_
#define REG_ENV_H_

#include <random>
#include <stdint.h>

struct EnvInterface {
public:
  bool rst_n;
  bool machine_mode;
  bool PMP_ENABLE;
  int PMP_NUM_REGIONS;
  int PMP_GRANULARITY;
};

struct RegisterInterface {
public:
  bool reg_access;
  uint32_t reg_op;
  uint32_t reg_addr;
  uint32_t reg_wdata;
  uint32_t reg_rdata;
  bool reg_error;
};

class EnvDriver {
public:
  EnvDriver(EnvInterface *env_signals, int seed);
  void OnClock();

private:
  std::default_random_engine generator_;
  EnvInterface *env_signals_;
  int cycle_count_;
  int reset_delay_;
  std::uniform_int_distribution<int> delay_dist_;
};

#endif // REG_ENV_H_
