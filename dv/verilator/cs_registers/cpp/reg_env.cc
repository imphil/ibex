#include "reg_env.h"

EnvDriver::EnvDriver(EnvInterface *env_signals, int seed)
    : generator_(seed), env_signals_(env_signals), cycle_count_(0),
      reset_delay_(0),
      delay_dist_(std::uniform_int_distribution<int>(100, 1000)) {}

void EnvDriver::OnClock() {
  if (--reset_delay_ == 0) {
    reset_delay_ = delay_dist_(generator_);
    cycle_count_ = 0;
  }
  if (++cycle_count_ < 3) {
    env_signals_->rst_n = 0;
  } else {
    env_signals_->rst_n = 1;
  }
}
