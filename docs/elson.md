# Vivado Integration notes -> For Report mainly


Question for Gemini: My GPU is word addressable as you can see with gpu.sv, would this be an issue with the current mcu.sv and Vivado AXI is byte addressable.

## 1. Refactor GPU pipeline for BRAM Access

Objective: To adapt the existing multi-channel, generic memory design into a single-channel, memory-mapped system suitable for implementation with on-chip BRAMs on an FPGA.

1. Created `gpu_defines.svh`:
A new header file was created to hold localparam constants defining the memory map (e.g., GLOBAL_MEM_BASE_ADDR, THREAD_LOCAL_MEM_BASE_ADDR).
Reason: This establishes a "single source of truth" for the memory layout. It prevents bugs by ensuring that the hardware (RTL) and software (C driver) are built from the exact same definitions. It also makes the memory map easy to read and modify in the future.

2. Simplified Address Widths in common.sv:
The DATA_MEMORY_ADDRESS_WIDTH and INSTRUCTION_MEMORY_ADDRESS_WIDTH were reduced from 32 to 16.
Reason: A 32-bit address space (4 Gigawords) is massive overkill for on-chip BRAMs. A 16-bit space (64K words) is still very generous for an FPGA and saves significant logic resources (LUTs, FFs) in the address paths, making the design smaller and easier to route.

3. Refactored gpu.sv Module:
- The DATA_MEM_NUM_CHANNELS and INSTRUCTION_MEM_NUM_CHANNELS parameters were changed from 8 to 1.
Reason: In an FPGA system, we will connect our GPU to single, dual-port BRAM blocks, not a multi-channel memory system like DDR. This change aligns the module's interface with its target hardware.

- Replaced Generic mem_controller with a Custom Arbiter in gpu.sv:
Change: The instantiations of data_memory_controller and program_memory_controller were completely removed. New logic was added that includes a round-robin arbiter for vector LSUs, priority logic for the scalar LSU, and an address translation block.
Reason: The generic mem_controller is not aware of our specific memory map. The new custom arbiter is the "brain" that enforces the memory map. It correctly identifies whether a memory request is for global shared memory (from the scalar LSU) or thread-local memory (from a vector LSU) and calculates the correct physical BRAM address for thread-local accesses.

4. Simplified Core Instantiation in gpu.sv:
The g_cores and g_lsu_connect generate blocks were removed, and a single compute_core is now instantiated directly.
Reason: Since NUM_CORES is 1, the generate block and extra wiring layer were unnecessary. This simplification makes the code cleaner and easier to read, directly connecting the core to the new memory arbiter.

