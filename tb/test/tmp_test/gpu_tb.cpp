#include "base_testbench.h"
#include <verilated_cov.h>
#include <bitset>
#include <iostream>
#include <iomanip>
#include <vector>
#include <queue>

#define NAME "gpu"

// GPU Parameters (matching the module defaults)
#define DATA_MEM_NUM_CHANNELS 8
#define INSTRUCTION_MEM_NUM_CHANNELS 8
#define NUM_CORES 1
#define WARPS_PER_CORE 1
#define THREADS_PER_WARP 16

// Memory simulation helpers
struct MemoryRequest {
    uint32_t address;
    uint32_t data;
    bool is_write;
    int channel;
};

class GpuTestbench : public BaseTestbench {
protected:
    // Memory simulation queues
    std::queue<MemoryRequest> data_mem_requests;
    std::queue<MemoryRequest> instruction_mem_requests;
    
    // Simple memory arrays for simulation
    std::vector<uint32_t> data_memory;
    std::vector<uint32_t> instruction_memory;
    
    void initializeInputs() override {
        // Initialize clock and reset
        top->clk = 0;
        top->reset = 1;
        top->execution_start = 0;
        
        // Initialize kernel configuration - FIX: Pack into single wide vector
        // The kernel_config might be a packed struct that needs proper bit packing
        // Check if kernel_config is a single wide signal or array of 32-bit words
        
        // Method 1: If kernel_config is a single 128-bit packed signal
        // Pack all config into a single 128-bit value
        uint64_t config_low = 0;
        uint64_t config_high = 0;
        
        // Pack: [31:0] = base_instructions_address, [63:32] = base_data_address
        config_low = (uint64_t(0x2000) << 32) | uint64_t(0x1000);
        // Pack: [95:64] = num_blocks, [127:96] = num_warps_per_block  
        config_high = (uint64_t(1) << 32) | uint64_t(1);
        
        // If kernel_config is treated as array of 32-bit words:
        top->base_instr = 0x1000;      // base_instructions_address
        top->base_data = 0x2000;      // base_data_address  
        top->num_blocks = 1;           // num_blocks
        top->warps_per_block = 1;           // num_warps_per_block
        
        // Initialize memory arrays
        data_memory.resize(0x10000, 0);
        instruction_memory.resize(0x10000, 0);
        
        // Initialize all memory interface signals
        initializeMemorySignals();
    }
    
    void initializeMemorySignals() {
        // Data memory interface - fix: these are single signals, not arrays
        top->data_mem_read_ready = 1;
        top->data_mem_write_ready = 1;
        
        // Initialize data arrays properly
        for (int i = 0; i < DATA_MEM_NUM_CHANNELS; i++) {
            top->data_mem_read_data[i] = 0;
        }
        
        // Instruction memory interface - fix: single ready signal
        top->instruction_mem_read_ready = 1;
        
        // Initialize instruction data arrays properly
        for (int i = 0; i < INSTRUCTION_MEM_NUM_CHANNELS; i++) {
            top->instruction_mem_read_data[i] = 0;
        }
    }
    
    void clockCycle() {
        top->clk = 0;
        top->eval();
        top->clk = 1;
        top->eval();
        handleMemoryRequests();
    }
    
    void handleMemoryRequests() {
        // Handle data memory read requests
        if (top->data_mem_read_valid && top->data_mem_read_ready) {
            for (int i = 0; i < DATA_MEM_NUM_CHANNELS; i++) {
                uint32_t addr = top->data_mem_read_address[i];
                // Convert word address to array index
                uint32_t word_addr = addr / 4;
                if (word_addr < data_memory.size()) {
                    top->data_mem_read_data[i] = data_memory[word_addr];
                    std::cout << "Data read: channel " << i << ", addr 0x" << std::hex << addr 
                              << ", data 0x" << data_memory[word_addr] << std::dec << std::endl;
                }
            }
        }
        
        // Handle data memory write requests
        if (top->data_mem_write_valid && top->data_mem_write_ready) {
            for (int i = 0; i < DATA_MEM_NUM_CHANNELS; i++) {
                uint32_t addr = top->data_mem_write_address[i];
                uint32_t data = top->data_mem_write_data[i];
                // Convert word address to array index
                uint32_t word_addr = addr / 4;
                if (word_addr < data_memory.size()) {
                    data_memory[word_addr] = data;
                    std::cout << "Data write: channel " << i << ", addr 0x" << std::hex << addr 
                              << ", data 0x" << data << std::dec << std::endl;
                }
            }
        }
        
        // Handle instruction memory read requests
        if (top->instruction_mem_read_valid && top->instruction_mem_read_ready) {
            for (int i = 0; i < INSTRUCTION_MEM_NUM_CHANNELS; i++) {
                uint32_t addr = top->instruction_mem_read_address[i];
                // Convert word address to array index
                uint32_t word_addr = addr / 4;
                if (word_addr < instruction_memory.size()) {
                    top->instruction_mem_read_data[i] = instruction_memory[word_addr];
                    // std::cout << "Instruction read: channel " << i << ", addr 0x" << std::hex << addr 
                    //           << ", instr 0x" << instruction_memory[word_addr] << std::dec << std::endl;
                }
            }
        }
    }
    
    void reset() {
        top->reset = 1;
        clockCycle();
        clockCycle();
        top->reset = 0;
        clockCycle();
    }
    
    void startExecution() {
        top->execution_start = 1;
        clockCycle();
        top->execution_start = 0;
    }
    
    void waitForCompletion(int max_cycles = 10000) {
        int cycles = 0;
        while (!top->execution_done && cycles < max_cycles) {
            clockCycle();
            cycles++;
            
            // Print periodic status
            if (cycles % 100 == 0) {
                //std::cout << "Cycle " << cycles << ", execution_done = " << (int)top->execution_done << std::endl;
            }
        }
        
        if (top->execution_done) {
            std::cout << "GPU execution completed in " << cycles << " cycles" << std::endl;
        } else {
            std::cout << "GPU execution timed out after " << cycles << " cycles" << std::endl;
        }
        
        EXPECT_TRUE(top->execution_done) << "GPU execution did not complete within " << max_cycles << " cycles";
    }
    
    void loadInstructions(const std::vector<uint32_t>& instructions, uint32_t base_addr = 0x1000) {
        // Convert byte address to word address
        uint32_t word_base = base_addr / 4;
        
        std::cout << "Loading " << instructions.size() << " instructions at word address 0x" 
                  << std::hex << word_base << std::dec << std::endl;
        
        for (size_t i = 0; i < instructions.size(); i++) {
            if ((word_base + i) < instruction_memory.size()) {
                instruction_memory[word_base + i] = instructions[i];
                std::cout << "  [" << (word_base + i) << "] = 0x" << std::hex << instructions[i] << std::dec << std::endl;
            }
        }
    }
    
    void loadData(const std::vector<uint32_t>& data, uint32_t base_addr = 0x2000) {
        // Convert byte address to word address  
        uint32_t word_base = base_addr / 4;
        
        std::cout << "Loading " << data.size() << " data words at word address 0x" 
                  << std::hex << word_base << std::dec << std::endl;
        
        for (size_t i = 0; i < data.size(); i++) {
            if ((word_base + i) < data_memory.size()) {
                data_memory[word_base + i] = data[i];
                std::cout << "  [" << (word_base + i) << "] = 0x" << std::hex << data[i] << std::dec << std::endl;
            }
        }
    }
    
    // FIX: Add debug function to check kernel config interpretation
    void debugKernelConfig() {
        std::cout << "=== Kernel Config Debug ===" << std::endl;
        std::cout << "Base instr: " << top->base_instr << std::endl;
        std::cout << "Base data: " << top->base_data << std::endl;
        std::cout << "Num Blocks: " << top->num_blocks << std::endl;
        std::cout << "Warps per block: " << top->warps_per_block << std::endl;
    }
    
    void setKernelConfig(uint32_t base_instr, uint32_t base_data, uint32_t num_blocks, uint32_t warps_per_block) {
        std::cout << "=== KERNEL CONFIG STRUCT ASSIGNMENT ===" << std::endl;
        std::cout << "Setting: instr_base=0x" << std::hex << base_instr 
                << ", data_base=0x" << base_data 
                << ", blocks=" << std::dec << num_blocks 
                << ", warps_per_block=" << warps_per_block << std::endl;

        // Direct struct-style assignment
        top->base_instr = base_instr;
        top->base_data         = base_data;
        top->num_blocks               = num_blocks;
        top->warps_per_block       = warps_per_block;

        // Debug output
        std::cout << "=== Kernel Config (Verilator struct fields) ===" << std::endl;
        std::cout << "  base_instructions_address = 0x" << std::hex << top->base_instr << std::endl;
        std::cout << "  base_data_address         = 0x" << std::hex << top->base_data << std::endl;
        std::cout << "  num_blocks                = "   << std::dec << top->num_blocks << std::endl;
        std::cout << "  num_warps_per_block       = "   << std::dec << top->warps_per_block << std::endl;
        std::cout << "===============================================" << std::endl;
    }

        
    // Helper to print current GPU state
    void printGpuState() {
        std::cout << "=== GPU State ===" << std::endl;
        std::cout << "execution_done: " << (int)top->execution_done << std::endl;
        std::cout << "data_mem_read_valid: " << (int)top->data_mem_read_valid << std::endl;
        std::cout << "data_mem_write_valid: " << (int)top->data_mem_write_valid << std::endl;
        std::cout << "instruction_mem_read_valid: " << (int)top->instruction_mem_read_valid << std::endl;
        std::cout << "=================" << std::endl;
    }
    
    // Helper functions to create Elson-V ISA instructions
    uint32_t createIType(uint8_t funct4, uint8_t rs1, uint8_t rd, uint16_t imm, bool pred = false) {
        // I-type: opcode=001
        uint32_t instr = 0;
        instr |= (0x1 << 29);           // opcode = 001
        instr |= (pred ? 1 : 0) << 28;  // predicate
        instr |= (imm & 0x3FFF) << 14;  // 14-bit immediate
        instr |= (funct4 & 0xF) << 10;  // funct4
        instr |= (rs1 & 0x1F) << 5;     // rs1
        instr |= (rd & 0x1F);           // rd
        std::cout << "I-type" << instr << std::endl;
        return instr;
    }
    
    uint32_t createRType(uint8_t funct4, uint8_t rs1, uint8_t rs2, uint8_t rd, bool pred = false) {
        // R-type: opcode=000
        uint32_t instr = 0;
        instr |= (0x0 << 29);           // opcode = 000
        instr |= (pred ? 1 : 0) << 28;  // predicate
        // bits [27:19] are don't care (9 bits)
        instr |= (rs2 & 0x1F) << 14;    // rs2
        instr |= (funct4 & 0xF) << 10;  // funct4
        instr |= (rs1 & 0x1F) << 5;     // rs1
        instr |= (rd & 0x1F);           // rd
        return instr;
    }
    
    uint32_t createExitInstruction() {
        // C-type exit: opcode=111, funct3=111
        uint32_t instr = 0;
        instr |= (0x7 << 29);           // opcode = 111
        // bits [28:13] are don't care (16 bits)
        instr |= (0x7 << 10);           // funct3 = 111
        // bits [9:0] are don't care (10 bits)
        return instr;
    }

    void runGpuProgramWithMonitoring(const std::vector<uint32_t>& program) {
        reset();

        // Load sample data (can be modified or skipped)
        std::vector<uint32_t> test_data = {0xDEADBEEF, 0xCAFEBABE, 0x12345678, 0x87654321};
        loadData(test_data, 0x2000);

        std::cout << "Generated Elson-V instructions:" << std::endl;
        for (size_t i = 0; i < program.size(); i++) {
            std::cout << "  [" << i << "] = 0x" << std::hex << program[i] << std::dec << std::endl;
        }

        loadInstructions(program, 0x1000);
        setKernelConfig(0x1000, 0x2000, 1, 1);

        for (int i = 0; i < 6; i++) clockCycle();
        startExecution();

        std::cout << "Starting instruction execution monitoring..." << std::endl;
        int cycle_count = 0;
        bool saw_instruction_fetch = false;
        bool saw_data_access = false;

        while (!top->execution_done && cycle_count < 2000) {
            clockCycle();
            cycle_count++;

            if (top->instruction_mem_read_valid) {
                saw_instruction_fetch = true;
            }

            if (top->data_mem_read_valid || top->data_mem_write_valid) {
                saw_data_access = true;
                std::cout << "Data access detected at cycle " << cycle_count << std::endl;
            }

            if (cycle_count % 25 == 0) {
                printGpuState();
            }

            if (cycle_count < 10 && top->execution_done) {
                std::cout << "WARNING: Execution completed too quickly at cycle " << cycle_count << std::endl;
                break;
            }
        }

        std::cout << "Execution completed after " << cycle_count << " cycles" << std::endl;
        std::cout << "Saw instruction fetch: " << (saw_instruction_fetch ? "YES" : "NO") << std::endl;
        std::cout << "Saw data access: " << (saw_data_access ? "YES" : "NO") << std::endl;

        if (!saw_instruction_fetch) {
            std::cout << "ERROR: No instruction fetches - likely kernel configuration problem" << std::endl;
            std::cout << "Check if kernel_config bit packing matches Verilog module expectations" << std::endl;
        }

        EXPECT_TRUE(saw_instruction_fetch) << "No instruction fetches observed - kernel config issue";
    }

};

// // ------------------ BASIC INITIALIZATION TEST ------------------
// TEST_F(GpuTestbench, BasicInitialization) {
//     reset();
    
//     // Check initial state
//     EXPECT_EQ(top->execution_done, 0);
//     EXPECT_EQ(top->reset, 0);
    
//     // Verify memory interfaces are properly initialized
//     EXPECT_EQ(top->data_mem_read_valid, 0);
//     EXPECT_EQ(top->data_mem_write_valid, 0);
//     EXPECT_EQ(top->instruction_mem_read_valid, 0);
    
//     std::cout << "Basic initialization test passed" << std::endl;
// }

// // ------------------ KERNEL CONFIGURATION TEST ------------------
// TEST_F(GpuTestbench, KernelConfiguration) {
//     reset();
    
//     std::cout << "\n=== TESTING DIFFERENT KERNEL CONFIG METHODS ===" << std::endl;
    
//     // Test multiple kernel config methods to see which one works
//     std::vector<std::tuple<std::string, uint32_t, uint32_t, uint32_t, uint32_t>> test_configs = {
//         {"Simple", 0x1000, 0x2000, 1, 1},
//         {"Different", 0x4000, 0x8000, 2, 2},
//         {"Minimal", 0x100, 0x200, 1, 1}
//     };
    
//     for (auto& [name, base_instr, base_data, num_blocks, warps_per_block] : test_configs) {
//         std::cout << "\n--- Testing config: " << name << " ---" << std::endl;
        
//         // Reset and set configuration
//         reset();
//         setKernelConfig(base_instr, base_data, num_blocks, warps_per_block);
        
//         // Start execution and see what the GPU reports
//         startExecution();
        
//         // Give it a few cycles to process
//         for (int i = 0; i < 5; i++) {
//             clockCycle();
//         }
        
//         // Check if execution actually started (should not complete immediately for valid config)
//         if (top->execution_done) {
//             std::cout << "WARNING: Execution completed immediately - likely config issue" << std::endl;
//         } else {
//             std::cout << "Config seems to be accepted (execution started)" << std::endl;
//         }
//     }
    
//     std::cout << "\n=== END KERNEL CONFIG METHOD TESTING ===" << std::endl;
// }

// // ------------------ SIMPLE INSTRUCTION TEST (UPDATED) ------------------
TEST_F(GpuTestbench, SimpleInstructionExecution) {
    std::vector<uint32_t> program = {
        createIType(0x0, 0, 1, 1),      // addi x1, x0, 1    - Load 1 into x1
        createIType(0x0, 0, 2, 2),      // addi x2, x0, 2    - Load 2 into x2  
        createRType(0x0, 1, 2, 3),      // add x3, x1, x2    - x3 = x1 + x2 = 3
        createExitInstruction()         // exit
    };
    runGpuProgramWithMonitoring(program);
}

// // ------------------ EXECUTION START/STOP TEST ------------------
// // TEST_F(GpuTestbench, ExecutionStartStop) {
// //     reset();
    
// //     // Verify initial state
// //     EXPECT_EQ(top->execution_done, 0);
    
// //     // Load minimal program (just exit instruction)
// //     std::vector<uint32_t> exit_program = {
// //         createExitInstruction()
// //     };
// //     loadInstructions(exit_program, 0x1000);
    
// //     // Set kernel config for minimal execution
// //     setKernelConfig(0x1000, 0x2000, 1, 1);
    
// //     // Add delay for config settling
// //     for (int i = 0; i < 5; i++) {
// //         clockCycle();
// //     }
    
// //     startExecution();
    
// //     // Wait for completion (should be quick with exit instruction)
// //     waitForCompletion(1000);
// // }

// // ------------------ MEMORY ACCESS PATTERN TEST ------------------
// TEST_F(GpuTestbench, MemoryAccessPatterns) {
//     reset();
    
//     // Load test data pattern
//     std::vector<uint32_t> pattern_data;
//     for (int i = 0; i < 16; i++) {
//         pattern_data.push_back(0x1000 + i * 0x11);
//     }
//     loadData(pattern_data, 0x2000);
    
//     // Load simple memory test program
//     std::vector<uint32_t> memory_test_program = {
//         createIType(0x0, 0, 1, 0x800),  // addi x1, x0, 0x800 (data base >> 2)
//         createIType(0x2, 1, 2, 0),      // lw x2, 0(x1) - load from data base
//         createIType(0x2, 1, 3, 1),      // lw x3, 1(x1) - load from data base + 4
//         createRType(0x0, 2, 3, 4),      // add x4, x2, x3
//         createIType(0x3, 1, 4, 2),      // sw x4, 2(x1) - store to data base + 8
//         createExitInstruction()         // exit
//     };
//     loadInstructions(memory_test_program, 0x1000);
    
//     setKernelConfig(0x1000, 0x2000, 1, 1);
    
//     // Add config settling delay
//     for (int i = 0; i < 5; i++) {
//         clockCycle();
//     }
    
//     startExecution();
    
//     // Monitor execution with detailed logging
//     int instruction_fetches = 0;
//     int data_reads = 0;
//     int data_writes = 0;
    
//     for (int i = 0; i < 1000 && !top->execution_done; i++) {
//         clockCycle();
        
//         if (top->instruction_mem_read_valid) instruction_fetches++;
//         if (top->data_mem_read_valid) data_reads++;
//         if (top->data_mem_write_valid) data_writes++;
//     }
    
//     std::cout << "Memory access summary:" << std::endl;
//     std::cout << "  Instruction fetches: " << instruction_fetches << std::endl;
//     std::cout << "  Data reads: " << data_reads << std::endl;
//     std::cout << "  Data writes: " << data_writes << std::endl;
    
//     // We should see some activity
//     EXPECT_GT(instruction_fetches, 0) << "No instruction fetches observed";
// }

// // ------------------ RESET DURING EXECUTION TEST ------------------
// TEST_F(GpuTestbench, ResetDuringExecution) {
//     reset();
    
//     // Load a long-running program
//     std::vector<uint32_t> long_program;
//     for (int i = 0; i < 100; i++) {
//         long_program.push_back(createIType(0x0, 0, 0, 0)); // NOP (addi x0, x0, 0)
//     }
//     long_program.push_back(createExitInstruction());
//     loadInstructions(long_program, 0x1000);
    
//     setKernelConfig(0x1000, 0x2000, 1, 1);
    
//     // Config settling delay
//     for (int i = 0; i < 5; i++) {
//         clockCycle();
//     }
    
//     startExecution();
    
//     // Run for a while
//     for (int i = 0; i < 50; i++) {
//         clockCycle();
//     }
    
//     // Apply reset
//     std::cout << "Applying reset during execution..." << std::endl;
//     top->reset = 1;
//     clockCycle();
//     clockCycle();
//     top->reset = 0;
//     clockCycle();
    
//     // Verify reset state
//     EXPECT_EQ(top->execution_done, 0);
//     EXPECT_EQ(top->data_mem_read_valid, 0);
//     EXPECT_EQ(top->data_mem_write_valid, 0);
//     EXPECT_EQ(top->instruction_mem_read_valid, 0);
    
//     std::cout << "Reset during execution test completed" << std::endl;
// }

// // ------------------ PARAMETER VERIFICATION TEST ------------------
// TEST_F(GpuTestbench, ParameterVerification) {
//     // These are compile-time checks, but we can verify the testbench constants
//     EXPECT_EQ(DATA_MEM_NUM_CHANNELS, 8);
//     EXPECT_EQ(INSTRUCTION_MEM_NUM_CHANNELS, 8);
//     EXPECT_EQ(NUM_CORES, 1);
//     EXPECT_EQ(WARPS_PER_CORE, 1);
//     EXPECT_EQ(THREADS_PER_WARP, 16);
    
//     std::cout << "GPU Configuration:" << std::endl;
//     std::cout << "  Cores: " << NUM_CORES << std::endl;
//     std::cout << "  Warps per core: " << WARPS_PER_CORE << std::endl;
//     std::cout << "  Threads per warp: " << THREADS_PER_WARP << std::endl;
//     std::cout << "  Data memory channels: " << DATA_MEM_NUM_CHANNELS << std::endl;
//     std::cout << "  Instruction memory channels: " << INSTRUCTION_MEM_NUM_CHANNELS << std::endl;
// }

// ------------------ R-TYPE SUB AND MUL TEST ------------------

// TEST_F(GpuTestbench, RTypeMulExecution) {
//     std::vector<uint32_t> program = {
//         createIType(0x0, 0, 1, 10),      // addi x1, x0, 10
//         createIType(0x0, 0, 2, 3),       // addi x2, x0, 3
//         createRType(0x1, 1, 2, 3),       // sub x3, x1, x2 => 10 - 3 = 7
//         createRType(0x4, 1, 2, 4),       // mul x4, x1, x2 => 10 * 3 = 30
//         createExitInstruction()
//     };
//     runGpuProgramWithMonitoring(program);
// }


// // ------------------ BRANCH-LIKE PATTERN TEST ------------------
// TEST_F(GpuTestbench, LoopPatternSimulation) {
//     std::vector<uint32_t> program = {
//         createIType(0x0, 0, 1, 0),             // addi x1, x0, 0          ; x1 = 0 (loop counter)
//         createIType(0x0, 0, 2, 5),             // addi x2, x0, 5          ; x2 = 5 (limit)
//         createRType(0x0, 1, 0, 1),             // add x1, x1, x0          ; x1 += 0 (noop)
//         createRType(0x1, 2, 1, 3),             // sub x3, x2, x1          ; check if x1 < x2
//         createRType(0x0, 1, 0, 1),             // add x1, x1, x0 again    ; x1 += 0 again
//         createExitInstruction()               // exit
//     };
//     runGpuProgramWithMonitoring(program);
// }


// ------------------ MAIN ------------------
int main(int argc, char **argv) {
    Verilated::commandArgs(argc, argv);
    testing::InitGoogleTest(&argc, argv);
    Verilated::mkdir("logs");
    auto res = RUN_ALL_TESTS();
    VerilatedCov::write(("logs/coverage_" + std::string(NAME) + ".dat").c_str());
    return res;
}