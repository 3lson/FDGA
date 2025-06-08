#include "sync_testbench.h"
#include <verilated_cov.h>
#include <vector>
#include <deque>

// Include your custom ISA header
#include "elsonv_instructions.hpp" // Make sure this is the correct filename

// Use the namespaces for cleaner code
using namespace sim;
using namespace sim::instructions;

#define NAME "gpu" // Used for coverage file naming

// Global memory models for simplicity. They are accessible by all tests.
std::vector<IData> instruction_memory;
std::vector<IData> data_memory;


class GpuTestbench : public SyncTestbench {
protected:
    // This is called automatically by GTest before each test
    void initializeInputs() override {
        // Initialize all DUT inputs to a known, default state
        top->clk = 1;
        top->reset = 1; // Start in reset
        top->execution_start = 0;
        
        // --- MODIFICATION for separate ports ---
        top->p_base_instructions_address = 0;
        top->p_base_data_address = 0;
        top->p_num_blocks = 0;
        top->p_num_warps_per_block = 0;
        
        // Tie all memory 'ready' signals low by default
        top->instruction_mem_read_ready = 0;
        top->data_mem_read_ready = 0;
        top->data_mem_write_ready = 0;
    }
    
    // Custom helper to load a program into our software memory model
    void loadProgram(const std::vector<InstructionBits>& program) {
        instruction_memory.clear();
        for (const auto& instr : program) {
            instruction_memory.push_back(instr);
        }
    }

    // Custom helper to load initial data into our software memory model
    void loadData(const std::vector<IData>& data) {
        data_memory.clear();
        data_memory.insert(data_memory.end(), data.begin(), data.end());
        // Ensure memory is large enough for potential writes
        data_memory.resize(1024, 0); 
    }

    // This method will be called every clock cycle from within our test logic
    // to simulate the behavior of the external memory system.
    void handleMemory() {
        // --- Instruction Memory Simulation ---
        top->instruction_mem_read_ready = 0; // Default to not ready
        for (int i = 0; i < 8; ++i) { // Using literal based on your RTL
            if ((top->instruction_mem_read_valid >> i) & 1) {
                top->instruction_mem_read_ready |= (1 << i); // Signal ready on this channel
                uint32_t addr = top->instruction_mem_read_address[i];
                if (addr < instruction_memory.size()) {
                    top->instruction_mem_read_data[i] = instruction_memory[addr];
                } else {
                    top->instruction_mem_read_data[i] = 0; // Out-of-bounds read returns 0
                }
            }
        }
        
        // --- Data Memory Simulation ---
        top->data_mem_read_ready = 0;
        top->data_mem_write_ready = 0;
        for (int i = 0; i < 8; ++i) { // Using literal based on your RTL
            // Handle writes from the DUT
            if ((top->data_mem_write_valid >> i) & 1) {
                top->data_mem_write_ready |= (1 << i);
                uint32_t addr_bytes = top->data_mem_write_address[i];
                uint32_t addr_words = addr_bytes / 4; // Convert byte address to word index
                if (addr_words < data_memory.size()) {
                    data_memory[addr_words] = top->data_mem_write_data[i];
                }
            }
            // Handle reads from the DUT
            if ((top->data_mem_read_valid >> i) & 1) {
                top->data_mem_read_ready |= (1 << i);
                uint32_t addr_bytes = top->data_mem_read_address[i];
                uint32_t addr_words = addr_bytes / 4;
                if (addr_words < data_memory.size()) {
                    top->data_mem_read_data[i] = data_memory[addr_words];
                } else {
                    top->data_mem_read_data[i] = 0;
                }
            }
        }
    }
};

// =================================================================
// ==                      TEST CASES                             ==
// =================================================================

TEST_F(GpuTestbench, AddiAndStore) {
    // 1. Setup program and initial data
    loadProgram({
        addi(5_x, 0_x, 42),
        sw(5_x, 0_x, 0),
        exit()
    });
    loadData({});

    // 2. Reset Sequence
    runSimulation(2); // Hold reset
    top->reset = 0;
    top->eval();

    // 3. Drive Inputs and Start Pulse
    top->p_base_instructions_address = 0;
    top->p_base_data_address = 0;
    top->p_num_blocks = 1;
    top->p_num_warps_per_block = 1;
    top->execution_start = 1;
    
    top->eval();        // Let inputs propagate
    runSimulation(1);   // Clock tick to latch inputs
    top->execution_start = 0; // De-assert start pulse

    // 4. Run simulation until DUT signals completion
    int timeout = 1000;
    while(timeout-- > 0 && !top->execution_done) {
        handleMemory();   // Respond to any memory requests
        runSimulation(1); // Advance to the next clock cycle
    }

    // 5. Check results
    EXPECT_GT(timeout, 0) << "Simulation timed out!";
    EXPECT_EQ(data_memory[0], 42);
}

TEST_F(GpuTestbench, LoadAddStore) {
    // 1. Setup
    loadProgram({
        lw(6_x, 0_x, 0),    // lw x6, 0(x0)
        lw(7_x, 0_x, 4),    // lw x7, 4(x0)
        add(8_x, 6_x, 7_x), // add x8, x6, x7
        sw(8_x, 0_x, 8),    // sw x8, 8(x0)
        exit()
    });
    loadData({10, 20}); // mem[0]=10 (at byte addr 0), mem[1]=20 (at byte addr 4)

    // 2. Run
    runSimulation(2);
    top->reset = 0;
    top->eval();

    top->p_base_instructions_address = 0;
    top->p_base_data_address = 0;
    top->p_num_blocks = 1;
    top->p_num_warps_per_block = 1;
    top->execution_start = 1;
    top->eval();
    runSimulation(1);
    top->execution_start = 0;

    int timeout = 1000;
    while(timeout-- > 0 && !top->execution_done) {
        handleMemory();
        runSimulation(1);
    }
    
    // 3. Check
    EXPECT_GT(timeout, 0) << "Simulation timed out!";
    EXPECT_EQ(data_memory[0], 10);
    EXPECT_EQ(data_memory[1], 20);
    EXPECT_EQ(data_memory[2], 30); // 10 + 20 should be stored at byte address 8 (word 2)
}


// Main function boilerplate for Google Test
int main(int argc, char **argv) {
    Verilated::commandArgs(argc, argv);
    testing::InitGoogleTest(&argc, argv);
    Verilated::mkdir("logs");
    auto res = RUN_ALL_TESTS();
    VerilatedCov::write(("logs/coverage_" + std::string(NAME) + ".dat").c_str());
    return res;
}