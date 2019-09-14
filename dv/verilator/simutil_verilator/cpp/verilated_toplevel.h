// Copyright lowRISC contributors.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0

#ifndef VERILATED_TOPLEVEL_H_
#define VERILATED_TOPLEVEL_H_

#include <verilated.h>
#include "verilated_tracer.h"

#define STR(s) #s
#define STR2(s) STR(s)

#include STR2(topname.h)




/**
 * Pure abstract class (interface) for verilated toplevel modules
 *
 * Verilator-produced toplevel modules do not have a common base class defining
 * the methods such as eval(); instead, they are only inheriting from the
 * generic VerilatedModule class, which doesn't have toplevel-specific
 * functionality. This makes it impossible to write code which accepts any
 * toplevel module as input by specifying the common "toplevel base class".
 *
 * This class, VerilatedToplevel, fills this gap by defining an abstract base
 * class for verilated toplevel modules. This class should be used together with
 * the VERILATED_TOPLEVEL macro.
 *
 * Note that this function is a workaround until Verilator gains this
 * functionality natively.
 *
 * To support the different tracing implementations (VCD, FST or no tracing),
 * the trace() function is modified to take a VerilatedTracer argument instead
 * of the tracer-specific class.
 */




class VerilatedToplevel: public topname {
  public:
  VerilatedToplevel(const char* name="TOP") {};
  const char* name() const { return STR(topname); }
  void trace(VerilatedTracer& tfp, int levels, int options=0) {
    VERILATED_TOPLEVEL_TRACE_CALL(topname)
  }
};

#endif // VERILATED_TOPLEVEL_H_
