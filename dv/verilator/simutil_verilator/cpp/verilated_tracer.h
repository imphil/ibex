#ifndef VERILATED_TRACER_H_
#define VERILATED_TRACER_H_


// VM_TRACE_FMT_FST must be set by the user when calling Verilator with
// --trace-fst. VM_TRACE is set by Verilator itself.
#if VM_TRACE == 1
#  ifdef VM_TRACE_FMT_FST
#    include "verilated_fst_c.h"
#    define VM_TRACE_CLASS_NAME VerilatedFstC
#  else
#    include "verilated_vcd_c.h"
#    define VM_TRACE_CLASS_NAME VerilatedVcdC
#  endif
#endif

#if VM_TRACE == 1
/**
 * "Base" for all tracers in Verilator with common functionality
 *
 * This class is (like the VerilatedToplevel class) a workaround for the
 * insufficient class hierarchy in Verilator-generated C++ code.
 *
 * Once Verilator is improved to support this functionality natively this class
 * should go away.
 */
class VerilatedTracer {
 public:
  VerilatedTracer() : impl_(nullptr) {
    impl_ = new VM_TRACE_CLASS_NAME();
  };

  ~VerilatedTracer() {
    delete impl_;
  }


  bool isOpen() const {
    return impl_->isOpen();
  };

  void open(const char* filename) {
    impl_->open(filename);
  };

  void close() {
    impl_->close();
  };

  void dump(vluint64_t timeui) {
    impl_->dump(timeui);
  }

  operator VM_TRACE_CLASS_NAME*() const { assert(impl_); return impl_; }

 private:
  VM_TRACE_CLASS_NAME* impl_;
};
#else
/**
 * No-op tracer interface
 */
class VerilatedTracer {
 public:
  VerilatedTracer() {};
  ~VerilatedTracer() {}
  bool isOpen() const { return false; };
  void open(const char* filename) {};
  void close() {};
  void dump(vluint64_t timeui) {}
};
#endif // VM_TRACE == 1



#if VM_TRACE == 1
#define VERILATED_TOPLEVEL_TRACE_CALL(topname) \
  topname::trace(static_cast<VM_TRACE_CLASS_NAME*>(tfp), levels, options);
#else
#define VERILATED_TOPLEVEL_TRACE_CALL(topname) \
  assert(0 && "Tracing not enabled.");
#endif

#endif // VERILATED_TRACER_
