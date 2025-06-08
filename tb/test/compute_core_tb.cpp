#include "sync_testbench.h"
#include <verilated_cov.h>
#include <bitset>
#include <cstdint>

#define NAME "compute_core"

// Reduced parameters for memory-constrained compilation
#define WARPS_PER_CORE 2  // Reduced from 4
#define NUM_LSUS       4  // Reduced from 17

// Define warp states for clarity in debugging
#define WARP_IDLE      0
#define WARP_FETCH     1
#define WARP_DECODE    2
#define WARP_REQUEST   3
#define WARP_WAIT      4
#define WARP_EXECUTE   5
#define WARP_UPDATE    6
#define WARP_DONE      7

class ComputeCoreTestbench : public SyncTestbench {
protected:
    void initializeInputs() override {
        // Initialize all inputs to known state
        top->clk = 1;
        top->reset = 1;
        top->start = 0;
        top->block_id = 0;

        // Pack kernel_config_t struct (128-bit bus as 4x32-bit words)
        uint32_t base_instructions_address = 0;
        uint32_t base_data_address         = 0x1000;
        uint32_t num_blocks                = 1;
        uint32_t num_warps_per_block       = 1;

        top->kernel_config[0] = base_instructions_address;
        top->kernel_config[1] = base_data_address;
        top->kernel_config[2] = num_blocks;
        top->kernel_config[3] = num_warps_per_block;

        // Initialize memory interface signals
        top->instruction_mem_read_ready = 0;
        top->data_mem_read_ready = 0;
        top->data_mem_write_ready = 0;
        
        // Initialize arrays with reduced size
        for (int i = 0; i < WARPS_PER_CORE; ++i) {
            top->instruction_mem_read_data[i] = 0;
        }

        for (int i = 0; i < NUM_LSUS; ++i) {
            top->data_mem_read_data[i] = 0;
        }
    }

    // Helper function to wait for a condition with timeout
    bool waitForCondition(std::function<bool()> condition, int maxCycles = 20) {
        for (int i = 0; i < maxCycles; ++i) {
            if (condition()) {
                return true;
            }
            runSimulation(1);
        }
        return false;
    }
};

// Test 1: Basic reset behavior
TEST_F(ComputeCoreTestbench, ResetBehavior) {
    runSimulation(3); // Hold reset for multiple cycles

    // Verify reset state
    EXPECT_EQ(top->done, 0) << "Core should not be done after reset";
    EXPECT_EQ(top->instruction_mem_read_valid, 0) << "No instruction reads should be active after reset";
    EXPECT_EQ(top->data_mem_read_valid, 0) << "No data reads should be active after reset";
    EXPECT_EQ(top->data_mem_write_valid, 0) << "No data writes should be active after reset";
}

// Test 2: Start sequence and first fetch
TEST_F(ComputeCoreTestbench, StartSequence) {
    // Release reset
    top->reset = 0;
    runSimulation(1);

    // Pulse start signal
    top->start = 1;
    runSimulation(1);
    top->start = 0;

    // Wait for instruction fetch with timeout
    bool fetchStarted = waitForCondition([this]() {
        return (top->instruction_mem_read_valid & 1) != 0;
    });

    EXPECT_TRUE(fetchStarted) << "Core should start instruction fetch after start pulse";
    
    if (fetchStarted) {
        EXPECT_EQ(top->instruction_mem_read_address[0], 0) 
            << "First instruction should be fetched from address 0";
    }

    EXPECT_EQ(top->done, 0) << "Core should not be done immediately after start";
}

// Test 3: Instruction memory handshake
TEST_F(ComputeCoreTestbench, InstructionMemoryHandshake) {
    // Start the core
    top->reset = 0;
    runSimulation(1);
    top->start = 1;
    runSimulation(1);
    top->start = 0;

    // Wait for instruction request
    bool fetchRequested = waitForCondition([this]() {
        return (top->instruction_mem_read_valid & 1) != 0;
    });

    ASSERT_TRUE(fetchRequested) << "Core must request instruction fetch";

    // Simulate memory response
    top->instruction_mem_read_data[0] = 0x12345678; // Sample instruction
    top->instruction_mem_read_ready |= 1; // Assert ready for warp 0
    
    runSimulation(1);
    
    // Ready should cause the valid to deassert (handshake complete)
    EXPECT_EQ((top->instruction_mem_read_valid & 1), 0) 
        << "Valid should deassert after ready is acknowledged";
}

// Test 4: Multiple cycle operation
TEST_F(ComputeCoreTestbench, ExtendedOperation) {
    // Start the core
    top->reset = 0;
    runSimulation(1);
    top->start = 1;
    runSimulation(1);
    top->start = 0;

    // Run for multiple cycles to see state progression
    for (int cycle = 0; cycle < 50; ++cycle) {
        runSimulation(1);
        
        // Log some key signals periodically
        if (cycle % 10 == 0) {
            // std::cout << "Cycle " << cycle 
            //           << ": done=" << (int)top->done
            //           << ", inst_valid=0x" << std::hex << top->instruction_mem_read_valid
            //           << ", data_read_valid=0x" << top->data_mem_read_valid
            //           << std::dec << std::endl;
        }
        
        // Break early if done
        if (top->done) {
            std::cout << "Core completed at cycle " << cycle << std::endl;
            break;
        }
    }
    
    // This test mainly exercises the design without strict assertions
    // Useful for debugging and understanding behavior
}

int main(int argc, char **argv) {
    Verilated::commandArgs(argc, argv);
    testing::InitGoogleTest(&argc, argv);
    Verilated::mkdir("logs");
    
    std::cout << "Starting " << NAME << " testbench..." << std::endl;
    
    auto result = RUN_ALL_TESTS();
    
    VerilatedCov::write(("logs/coverage_" + std::string(NAME) + ".dat").c_str());
    
    return result;
}