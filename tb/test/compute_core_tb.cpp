#include "base_testbench.h"
#include <verilated_cov.h>
#include <gtest/gtest.h>
#include <cstdint>
#include <map>
#include <functional>
#include <iostream>
#include <fstream>
#include <sstream>
using namespace std;

#define NAME "compute_core"
// NOTE: These parameters MUST match what you compile Verilog with.
#define WARPS_PER_CORE 4
#define THREADS_PER_WARP 16
#define NUM_LSUS (THREADS_PER_WARP + 1)

// Opcode definitions
#define OPCODE_R    0b000
#define OPCODE_I    0b001
#define OPCODE_F    0b010
#define OPCODE_UP   0b011
#define OPCODE_M    0b100
#define OPCODE_C    0b111

class ComputeCoreTestbench : public BaseTestbench {
protected:
    std::map<uint32_t, uint32_t> instr_mem;
    std::map<uint32_t, uint32_t> data_mem;

    void loadProgramFromHex(const std::string& hex_filepath) {
        instr_mem.clear(); // Clear any previous program
        std::ifstream hex_file(hex_filepath);
        ASSERT_TRUE(hex_file.is_open()) << "Could not open hex file: " << hex_filepath;

        std::string line;
        uint32_t current_address = 0;
        while (std::getline(hex_file, line)) {
            // Skip empty lines or comments
            if (line.empty() || line[0] == '/' || line[0] == '#') {
                continue;
            }
            
            // Convert hex string to uint32_t
            uint32_t instruction;
            std::stringstream ss;
            ss << std::hex << line;
            ss >> instruction;

            std::cout << "  PC[" << current_address << "]: 0x" << std::hex << instruction << std::dec << std::endl; // ADD THIS LINE

            // Store in our simulated instruction memory
            instr_mem[current_address] = instruction;

            // Increment address for the next instruction (assuming word-addressable)
            // Your ISA is byte-addressable but instructions are 4 bytes long.
            current_address++; // This assumes your assembler outputs instructions for PC 0, 1, 2...
        }
        
        std::cout << "Loaded " << instr_mem.size() << " instructions from " << hex_filepath << std::endl;
    }

    void loadDataFromHex(const std::string& hex_filepath, uint32_t base_address = 0) {
        // We do NOT clear data_mem here, to allow multiple loads if needed.
        // It will be cleared explicitly in tests.
        std::ifstream hex_file(hex_filepath);
        ASSERT_TRUE(hex_file.is_open()) << "Could not open data hex file: " << hex_filepath;

        std::string line;
        uint32_t current_offset = 0;
        while (std::getline(hex_file, line)) {
            if (line.empty() || line[0] == '/' || line[0] == '#') continue;

            uint32_t data_word;
            std::stringstream ss;
            ss << std::hex << line;
            ss >> data_word;

            uint32_t address = base_address + current_offset;
            data_mem[address] = data_word;
            std::cout << "  DATA[0x" << std::hex << address << "]: 0x" << data_word << std::dec << std::endl;
            
            // Increment address by 4 for the next word (byte-addressable memory)
            current_offset += 4;
        }
        std::cout << "Loaded " << data_mem.size() << " data words from " << hex_filepath << std::endl;
    }

    void tick() {
        top->clk = 0;
        top->eval();
        top->clk = 1;
        top->eval();
    }

    void runSimulation(int cycles) {
        for (int i = 0; i < cycles; ++i) {
            // --- Before the clock edge ---
            // Combinational Memory Read Logic (DUT requests, TB provides)
            for (int w = 0; w < WARPS_PER_CORE; ++w) {
                if (top->instruction_mem_read_valid & (1ULL << w)) {
                    // Verilator flattens arrays: module.port[idx] -> module.port_idx
                    uint32_t addr = top->instruction_mem_read_address[w];
                    top->instruction_mem_read_data[w] = instr_mem.count(addr) ? instr_mem[addr] : 0;
                }
            }
            for (int t = 0; t < NUM_LSUS; ++t) {
                if (top->data_mem_read_valid & (1ULL << t)) {
                    uint32_t addr = top->data_mem_read_address[t];
                    top->data_mem_read_data[t] = data_mem.count(addr) ? data_mem[addr] : 0xDEADBEEF;
                }
            }

            // --- Clock Tick ---
            top->clk = 0;
            top->eval();
            top->clk = 1;
            top->eval();

            // --- After the clock edge ---
            for (int t = 0; t < NUM_LSUS; ++t) {
                if (top->data_mem_write_valid & (1ULL << t)) {
                    data_mem[top->data_mem_write_address[t]] = top->data_mem_write_data[t];
                }
            }
        }
    }

    void initializeInputs() override {
        top->clk = 1; top->reset = 1; top->start = 0; top->block_id = 0;
        top->kernel_config[3] = 0; // base instruction address
        top->kernel_config[2] = 0; // base data memory address
        top->kernel_config[0] = 2;  // num_warps_per_block
        top->kernel_config[1] = 1; // num_blocks

        top->instruction_mem_read_ready = -1; // All ready
        top->data_mem_read_ready = -1;
        top->data_mem_write_ready = -1;
        
        runSimulation(2);
        top->reset = 0;
    }

    void loadAndRun(const std::map<uint32_t, uint32_t>& program) {
        instr_mem = program;
        //data_mem.clear();
        top->start = 1;
        tick();
        top->start = 0;
        
        int max_cycles = 500;
        for (int i = 0; i < max_cycles; ++i) {
            if (top->done) {
                std::cout << "Core finished in " << i << " cycles." << std::endl;
                return;
            }
            runSimulation(1);
        }
        FAIL() << "Core did not finish within the " << max_cycles << " cycle timeout.";
    }

    bool waitForCondition(std::function<bool()> condition, int maxCycles = 100) {
        for (int i = 0; i < maxCycles; ++i) {
            if (condition()) return true;
            runSimulation(1);
        }
        return false;
    }
    
    static uint32_t float_to_bits(float f) { return *reinterpret_cast<uint32_t*>(&f); }
    static float bits_to_float(uint32_t bits) { return *reinterpret_cast<float*>(&bits); }
};

// TEST_F(ComputeCoreTestbench, ResetBehavior) {
//     runSimulation(1);
//     EXPECT_EQ(top->done, 0);
//     EXPECT_EQ(top->instruction_mem_read_valid, 0);
//     EXPECT_EQ(top->data_mem_read_valid, 0);
//     EXPECT_EQ(top->data_mem_write_valid, 0);
// }

TEST_F(ComputeCoreTestbench, SimplestExit) {
    // Program:
    // 0: exit
    std::map<uint32_t, uint32_t> program;

    // Construct the 'exit' instruction manually.
    // According to the ISA: opcode=111, funct3=111. Other bits are don't-care.
    uint32_t exit_instr = (OPCODE_C << 29) | (0b111 << 10);
    program[0] = exit_instr;
    
    // Configured in Initialise inputs
    
    loadAndRun(program);

    // The `loadAndRun` function will fail if it times out.
    // If we reach this point, it means `top->done` was asserted.
    SUCCEED() << "Core successfully fetched, decoded, and executed an EXIT instruction.";
}

TEST_F(ComputeCoreTestbench, RScalarTest) {
    // 1. Load the program from the hex file
    loadProgramFromHex("test/tmp_test/hex/rscalar.hex"); // Assumes the file is in the build/run directory

    // 2. Configure the core for the test
    // Core configured in Initialise Input helper function

    // 3. Run the simulation
    loadAndRun(instr_mem); // Pass the populated map to the existing runner

    // 4. Check the result
    EXPECT_EQ(data_mem[42], 10) << "Scalar ALU/Store data path failed when loaded from hex.";
    EXPECT_EQ(data_mem[43], 20) << "Scalar ALU/Store data path failed when loaded from hex.";
    EXPECT_EQ(data_mem[44], 0) << "Scalar ALU/Store data path failed when loaded from hex.";
    EXPECT_EQ(data_mem[45], 100) << "Scalar ALU/Store data path failed when loaded from hex.";
    EXPECT_EQ(data_mem[46], 1) << "Scalar ALU/Store data path failed when loaded from hex.";
    EXPECT_EQ(data_mem[47], 0) << "Scalar ALU/Store data path failed when loaded from hex.";
    EXPECT_EQ(data_mem[48], 1) << "Scalar ALU/Store data path failed when loaded from hex.";
    EXPECT_EQ(data_mem[49], 1) << "Scalar ALU/Store data path failed when loaded from hex.";
    EXPECT_EQ(data_mem[50], 5) << "Scalar ALU/Store data path failed when loaded from hex.";
}


// TEST_F(ComputeCoreTestbench, IScalarTest) {
//     // Assembly Intent:
//     // s.li sp, 42
//     // s.li s1, 10
//     // s.sw s1, 0(sp)     // mem[42] = 10
//     // s.lw s2, 0(sp)     // s2 = 10
//     // s.addi s3, s2, 10  // s3 = 20
//     // s.muli s3, s2, 10  // s3 = 100
//     // s.divi s3, s2, 10  // s3 = 1
//     // s.slli s3, s2, 2   // s3 = 40
//     // exit
//     // We assume the program is written to store the final result of s3 (40)
//     // back into memory at address 42 for verification.

//     loadProgramFromHex("test/tmp_test/iscalar.hex");
//     loadAndRun(instr_mem);
//     loadProgramFromHex("test/tmp_test/iscalar.hex");
//     loadAndRun(instr_mem);

//     // Check that the final result (40) was stored at the stack address.
//     EXPECT_EQ(data_mem[44], 20) << "Scalar ADDI failed";
//     EXPECT_EQ(data_mem[45], 100) << "Scalar MULI failed";
//     EXPECT_EQ(data_mem[46], 1) << "Scalar DIVI failed";
//     EXPECT_EQ(data_mem[47], 40) << "Scalar SLLI failed"; // failing
// }


// TEST_F(ComputeCoreTestbench, JumpTest) {
//     // Assembly Intent:
//     // s.li s1, 4         // i = 4
//     // s.slt s4, s2, s3   // s2=i=4, s3=3 -> s4 = (4 < 3) = 0
//     // j for_end1         // Unconditionally jump
//     // for_end1:
//     // s.li s5, 5
//     // exit
//     // The program should execute the jump and then load 5 into s5.
//     // We assume the program stores the final value of s5 (5) at address 42.

//     loadProgramFromHex("test/tmp_test/jump.hex");
//     loadAndRun(instr_mem);
//     loadProgramFromHex("test/tmp_test/jump.hex");
//     loadAndRun(instr_mem);

//     // The program should store 5 at the end.
//     EXPECT_EQ(data_mem[46], 0) << "Jump test failed to store the correct final value.";
//     EXPECT_EQ(data_mem[42], 5) << "Jump test failed to store the correct final value.";
// }

// TEST_F(ComputeCoreTestbench, CScalarTest) {
//     // Assembly Intent:
//     // s.li s1, 4         // i = 4
//     // ...
//     // s.slt s4, s2, s3   // s2=i=4, s3=3 -> s4 = (4 < 3) = 0
//     // s.beqz s4, for_end1// Branch if s4 is zero. It is, so branch is taken.
//     // for_end1:
//     // s.li s5, 5
//     // exit
//     // The program should take the branch and load 5 into s5.
//     // We assume the program stores the final value of s5 (5) at address 42.

//     loadProgramFromHex("test/tmp_test/cscalar.hex");
//     loadAndRun(instr_mem);
//     loadProgramFromHex("test/tmp_test/cscalar.hex");
//     loadAndRun(instr_mem);

//     // The program should branch and store 5 at the end.
//     EXPECT_EQ(data_mem[43], 0) << "CScalar conditional branch test failed.";
//     EXPECT_EQ(data_mem[44], 5) << "CScalar conditional branch test failed.";
// }

// TEST_F(ComputeCoreTestbench, FScalarTest) {
//     // 1. Clear data memory from any previous tests
//     data_mem.clear();

//     // 2. Load the program and data files
//     loadProgramFromHex("test/tmp_test/fscalar.hex");
//     // We need to create a `data_fscalar.hex` file with the constants.
//     // Example: Let's say it contains 1065353216 (1.0f) and 1073741824 (2.0f)
//     loadDataFromHex("test/tmp_test/data_fscalar.hex"); // This will load data starting at 0x1000
//     // 2. Load the program and data files
//     loadProgramFromHex("test/tmp_test/fscalar.hex");
//     // We need to create a `data_fscalar.hex` file with the constants.
//     // Example: Let's say it contains 1065353216 (1.0f) and 1073741824 (2.0f)
//     loadDataFromHex("test/tmp_test/data_fscalar.hex"); // This will load data starting at 0x1000

//     // 3. Run the simulation
//     loadAndRun(instr_mem);
//     // 3. Run the simulation
//     loadAndRun(instr_mem);

//     // 4. Verify results using floating point comparisons
//     // fs1 and fs2 are loaded with constants from data memory. Let's assume they are 2.0f and 1.0f.
//     // Assembly: FADD fs3, fs1, fs2 => fs3 = 2.0 + 1.0 = 3.0
//     // 4. Verify results using floating point comparisons
//     // fs1 and fs2 are loaded with constants from data memory. Let's assume they are 2.0f and 1.0f.
//     // Assembly: FADD fs3, fs1, fs2 => fs3 = 2.0 + 1.0 = 3.0
    
//     // Check values stored back to memory. The addresses (e.g., 44, 45) will
//     // depend on your s.fsw instructions in fscalar.s
//     // Using EXPECT_FLOAT_EQ for safer floating point comparison.
//     EXPECT_FLOAT_EQ(bits_to_float(data_mem[45]), 3.0f) << "Scalar FADD failed";
//     EXPECT_FLOAT_EQ(bits_to_float(data_mem[46]), 1.0f) << "Scalar FSUB failed";
//     EXPECT_FLOAT_EQ(bits_to_float(data_mem[47]), 2.0f) << "Scalar FMUL failed";
//     EXPECT_FLOAT_EQ(bits_to_float(data_mem[48]), 2.0f) << "Scalar FDIV failed";
//     EXPECT_EQ(data_mem[49], 0) << "Scalar FLT failed (2.0 < 1.0 is false)";
//     EXPECT_FLOAT_EQ(bits_to_float(data_mem[50]), -1.0f) << "Scalar FNEG failed";
//     EXPECT_EQ(data_mem[51], 0) << "Scalar FEQ failed (2.0 == 1.0 is false)";
//     EXPECT_FLOAT_EQ(bits_to_float(data_mem[52]), 1.0f) << "Scalar FABS failed";
//     EXPECT_EQ(data_mem[53], 1) << "Scalar FCVT.W.S (float to int) failed";
// }

TEST_F(ComputeCoreTestbench, MScalarTest) {
    // Assembly Intent: Load a float, store it back to a different location.
    // lui s1, %hi(.LC0)
    // s.flw fs1, %lo(.LC0)(s1)  # Load 1.0f into fs1
    // s.fsw fs1, -108(s0)       # Store fs1 somewhere
    data_mem.clear();

    // The store address is `-108(s0)`. Let's assume s0 is set to a base address
    // like 200 to make the final address positive and verifiable (200 - 108 = 92).
    loadProgramFromHex("test/tmp_test/hex/mscalar.hex");
    loadDataFromHex("test/tmp_test/hex/data_mscalar.hex");

    loadAndRun(instr_mem);

    // Verify that the value 1.0f was loaded from 0x1000 and stored at address 92.
    EXPECT_EQ(bits_to_float(data_mem[42]), 1.0f) << "Scalar Store Failed";
    EXPECT_EQ(data_mem[43], 32) << "Scalar Load Failed";
    EXPECT_EQ(data_mem[44], 32) << "Scalar Load Failed";
}

// TEST_F(ComputeCoreTestbench, RVectorTest) {
//     // 1. Load the program from the hex file
//     loadProgramFromHex("test/tmp_test/rvector.hex"); // Assumes the file is in the build/run directory

//     // 2. Configure the core for the test
//     // Core configured in Initialise Input helper function

//     // 3. Run the simulation
//     loadAndRun(instr_mem); // Pass the populated map to the existing runner

//     // 4. Check the result
//     EXPECT_EQ(data_mem[42], 10) << "Scalar ALU/Store data path failed when loaded from hex.";
//     EXPECT_EQ(data_mem[43], 20) << "Scalar ALU/Store data path failed when loaded from hex.";
//     EXPECT_EQ(data_mem[44], 0) << "Scalar ALU/Store data path failed when loaded from hex.";
//     EXPECT_EQ(data_mem[45], 100) << "Scalar ALU/Store data path failed when loaded from hex.";
//     EXPECT_EQ(data_mem[46], 1) << "Scalar ALU/Store data path failed when loaded from hex.";
//     EXPECT_EQ(data_mem[47], 0) << "Scalar ALU/Store data path failed when loaded from hex.";
//     EXPECT_EQ(data_mem[48], 1) << "Scalar ALU/Store data path failed when loaded from hex.";
//     EXPECT_EQ(data_mem[49], 1) << "Scalar ALU/Store data path failed when loaded from hex.";
//     EXPECT_EQ(data_mem[50], 5) << "Scalar ALU/Store data path failed when loaded from hex.";
// }

// TEST_F(ComputeCoreTestbench, IVectorTest) {
//     // Assembly Intent:
//     // s.li sp, 42
//     // s.li s1, 10
//     // s.sw s1, 0(sp)     // mem[42] = 10
//     // s.lw s2, 0(sp)     // s2 = 10
//     // s.addi s3, s2, 10  // s3 = 20
//     // s.muli s3, s2, 10  // s3 = 100
//     // s.divi s3, s2, 10  // s3 = 1
//     // s.slli s3, s2, 2   // s3 = 40
//     // exit
//     // We assume the program is written to store the final result of s3 (40)
//     // back into memory at address 42 for verification.

//     loadProgramFromHex("test/tmp_test/ivector.hex");
//     loadAndRun(instr_mem);

//     // Check that the final result (40) was stored at the stack address.
//     EXPECT_EQ(data_mem[44], 20) << "Scalar ADDI failed";
//     EXPECT_EQ(data_mem[45], 100) << "Scalar MULI failed";
//     EXPECT_EQ(data_mem[46], 1) << "Scalar DIVI failed";
//     EXPECT_EQ(data_mem[47], 40) << "Scalar SLLI failed"; // failing
// }


// TEST_F(ComputeCoreTestbench, MVectorTest) {
//     // Assembly Intent: Load a float, store it back to a different location.
//     // lui s1, %hi(.LC0)
//     // s.flw fs1, %lo(.LC0)(s1)  # Load 1.0f into fs1
//     // s.fsw fs1, -108(s0)       # Store fs1 somewhere
//     data_mem.clear();

//     // The store address is `-108(s0)`. Let's assume s0 is set to a base address
//     // like 200 to make the final address positive and verifiable (200 - 108 = 92).
//     loadProgramFromHex("test/tmp_test/mvector.hex");
//     loadDataFromHex("test/tmp_test/data_mvector.hex");

//     loadAndRun(instr_mem);

//     // Verify that the value 1.0f was loaded from 0x1000 and stored at address 92.
//     EXPECT_EQ(bits_to_float(data_mem[42]), 1.0f) << "Scalar Store Failed";
//     EXPECT_EQ(data_mem[43], 32) << "Scalar Load Failed";
//     EXPECT_EQ(data_mem[44], 32) << "Scalar Load Failed";
// }

// TEST_F(ComputeCoreTestbench, FVectorTest) {
//     // 1. Clear data memory from any previous tests
//     data_mem.clear();

//     // 2. Load the program and data files
//     loadProgramFromHex("test/tmp_test/fvector.hex");
//     // We need to create a `data_fscalar.hex` file with the constants.
//     // Example: Let's say it contains 1065353216 (1.0f) and 1073741824 (2.0f)
//     loadDataFromHex("test/tmp_test/data_fvector.hex"); // This will load data starting at 0x1000

//     // 3. Run the simulation
//     loadAndRun(instr_mem);

//     // 4. Verify results using floating point comparisons
//     // fs1 and fs2 are loaded with constants from data memory. Let's assume they are 2.0f and 1.0f.
//     // Assembly: FADD fs3, fs1, fs2 => fs3 = 2.0 + 1.0 = 3.0
    
//     // Check values stored back to memory. The addresses (e.g., 44, 45) will
//     // depend on your s.fsw instructions in fscalar.s
//     // Using EXPECT_FLOAT_EQ for safer floating point comparison.
//     EXPECT_FLOAT_EQ(bits_to_float(data_mem[45]), 3.0f) << "Vector FADD failed";
//     EXPECT_FLOAT_EQ(bits_to_float(data_mem[46]), 1.0f) << "Vector FSUB failed";
//     EXPECT_FLOAT_EQ(bits_to_float(data_mem[47]), 2.0f) << "Vector FMUL failed";
//     EXPECT_FLOAT_EQ(bits_to_float(data_mem[48]), 2.0f) << "Vector FDIV failed";
//     EXPECT_EQ(data_mem[49], 0) << "Vector FLT failed (2.0 < 1.0 is false)";
//     EXPECT_FLOAT_EQ(bits_to_float(data_mem[50]), -1.0f) << "Vector FNEG failed";
//     EXPECT_EQ(data_mem[51], 0) << "Vector FEQ failed (2.0 == 1.0 is false)";
//     EXPECT_FLOAT_EQ(bits_to_float(data_mem[52]), 1.0f) << "Vector FABS failed";
//     EXPECT_EQ(data_mem[53], 1) << "Vector FCVT.W.S (float to int) failed";
// }

// TEST_F(ComputeCoreTestbench, SyncInstructionTest) {
//     // This test verifies that the 'sync' instruction correctly stalls warps
//     // until all active warps in a block have reached the barrier.
    
//     // 1. Clear data memory from any previous tests
//     data_mem.clear();

//     // 2. Load the assembled program
//     loadProgramFromHex("test/tmp_test/sync_test.hex");

//     loadAndRun(instr_mem);

//     // 3. CRITICAL: Manually set up and start the simulation
//     // This sequence replaces the single call to loadAndRun().

//     // Step A: Set the specific configuration for this test.
//     // We are overriding the defaults set in initializeInputs().

//     // Fail the test if the core timed out.
//     // 5. Verify the results
//     // The verification logic remains the same.

//     // Check that Warp 0 (producer) did its job
//     ASSERT_FALSE(data_mem.count(42)) << "Producer (Warp 0) failed to write to its address.";
//     EXPECT_EQ(data_mem[42], 123) << "Producer (Warp 0) wrote the wrong value.";

//     // Check that Warp 1 (consumer) did its initial write
//     ASSERT_TRUE(data_mem.count(46)) << "Consumer (Warp 1) failed its initial write.";
//     EXPECT_EQ(data_mem[46], 999) << "Consumer (Warp 1) wrote the wrong initial value.";

//     // THE REAL TEST: Check that Warp 1 read the value produced by Warp 0 *after* the sync.
//     ASSERT_TRUE(data_mem.count(50)) << "Consumer (Warp 1) failed to write its verification value.";
//     EXPECT_EQ(data_mem[50], 123) << "Sync barrier failed: Consumer read from memory before the producer wrote to it.";
// }

// TEST_F(ComputeCoreTestbench, SXSLTTest) {
//     data_mem.clear();

//     loadProgramFromHex("test/tmp_test/sx_slt.hex");

//     loadDataFromHex("test/tmp_test/data_sx_slt.hex"); // This will load data starting at 0x1000

//     loadAndRun(instr_mem);

//     EXPECT_FLOAT_EQ(data_mem[42], 0xFFFF) << "SX.SLT failed";
// }

// TEST_F(ComputeCoreTestbench, SXSLTTest_0) {
//     data_mem.clear();

//     loadProgramFromHex("test/tmp_test/sx_slt_0.hex");

//     loadDataFromHex("test/tmp_test/data_sx_slt_0.hex"); // This will load data starting at 0x1000

//     loadAndRun(instr_mem);

//     EXPECT_FLOAT_EQ(data_mem[42], 0) << "SX.SLT.0 failed";
// }



int main(int argc, char **argv) {
    Verilated::commandArgs(argc, argv);
    testing::InitGoogleTest(&argc, argv);
    Verilated::mkdir("logs");
    auto result = RUN_ALL_TESTS();
    VerilatedCov::write(("logs/coverage_" + std::string(NAME) + ".dat").c_str());
    return result;
}