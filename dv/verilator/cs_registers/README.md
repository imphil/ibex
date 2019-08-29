Ibex simulation CS Register Testbench
=====================================

This directory contains a basic sample testbench in C++ for testing correctness
of CS registers implemented in Ibex.

It is a work in progress and only tests a handful of registers, and is missing many features.

How to build and run the example
--------------------------------

From the Ibex top level:

   ```sh
   fusesoc --cores-root=. run --setup --build lowrisc:dv_verilator:cs_registers
   ./build/lowrisc_dv_verilator_cs_registers_0/default-verilator/Vibex_cs_registers
   ```
Details of the testbench
------------------------

cpp/reg\_tb.cc - contains main function and instantiation of SimCtrl

cpp/reg\_driver.cc - contains functions to generate random register transactions

cpp/reg\_model.cc - contains a model of the register state and checking against RTL

cpp/reg\_env.cc - contains a driver for environment signals (just reset at the moment)

