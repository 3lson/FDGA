#include "base_testbench.h"
#include <gtest/gtest.h>
#include <vector>
#include <map>
#include <functional>
#include <cstdint>
#include <fstream>
#include <sstream>

#define NAME "gpu"
// These must match gpu.sv
#define NUM_CORES 1
#define WARPS_PER_CORE 1
#define THREADS_PER_WARP 16

class GpuTestbench : public BaseTestbench {
protected:
    // --- Testbench Memory Simulation ---
    // Instruction memory, indexed by WORD address
    std::map<uint32_t, uint32_t> instruction_memory;
    // Data memory, indexed by BYTE address (to correctly simulate AXI)
    std::map<uint32_t, uint32_t> data_memory;

    // --- AXI Bus Simulation State ---
    // Write transaction state
    bool write_address_accepted = false;
    bool write_data_accepted = false;
    uint32_t pending_write_addr = 0;
    uint32_t pending_write_data = 0;
    
    // Read transaction state
    bool read_address_accepted = false;
    uint32_t pending_read_addr = 0;
    bool read_data_pending = false;

    void initializeInputs() override {
        top->clk = 0;
        top->reset = 1;
        top->execution_start = 0;
        
        // Initialize kernel configuration inputs
        top->base_instr = 0;
        top->base_data = 0;
        top->num_blocks = 0;
        top->warps_per_block = 0;

        // Initialize AXI slave inputs (all ready signals high for simplicity)
        top->data_mem_read_ready = 1;
        top->data_mem_write_ready = 1;
        top->instruction_mem_read_ready = 1;
        
        top->eval();
        top->clk = 1;
        top->eval();
        top->reset = 0;
    }

    void tick() {
        // --- Posedge Clock ---
        top->clk = 1;
        top->eval();

        // --- Simulate AXI Memory & Handshaking ---
        // Keep memory interface ready for simplicity
        top->data_mem_read_ready = 1;
        top->data_mem_write_ready = 1;
        top->instruction_mem_read_ready = 1;
        
        // --- Instruction Memory Read ---
        // Handle instruction fetch requests from cores
        if (top->instruction_mem_read_valid & 1) {
            uint32_t addr = top->instruction_mem_read_address[0];
            if (instruction_memory.count(addr)) {
                top->instruction_mem_read_data[0] = instruction_memory[addr];
            } else {
                top->instruction_mem_read_data[0] = 0; // Return NOP on invalid read
            }
            printf("[TB] Instruction Memory: Read from addr 0x%x, data=0x%x\n", 
                   addr, top->instruction_mem_read_data[0]);
        }

        // --- AXI Data Memory Write Handling ---
        // The MCU now drives the AXI interface, so we need to handle proper AXI protocol
        if (top->data_mem_write_valid && top->data_mem_write_ready) {
            uint32_t addr = top->data_mem_write_address[0];
            uint32_t data = top->data_mem_write_data[0];
            data_memory[addr] = data;
            printf("[TB] AXI Data Memory: Write 0x%x to addr 0x%x\n", data, addr);
        }

        // --- AXI Data Memory Read Handling ---
        if (top->data_mem_read_valid && top->data_mem_read_ready) {
            uint32_t addr = top->data_mem_read_address[0];
            if (data_memory.count(addr)) {
                top->data_mem_read_data[0] = data_memory[addr];
            } else {
                top->data_mem_read_data[0] = 0xDEADBEEF; // Default value for uninitialized memory
            }
            printf("[TB] AXI Data Memory: Read from addr 0x%x, data=0x%x\n", 
                   addr, top->data_mem_read_data[0]);
        }

        // --- Negedge Clock ---
        top->clk = 0;
        top->eval();
    }

    // Helper to load a program from a hex file into simulated instruction memory
    void loadProgramFromHex(const std::string& hex_filepath) {
        instruction_memory.clear();
        std::ifstream hex_file(hex_filepath);
        ASSERT_TRUE(hex_file.is_open()) << "Could not open hex file: " << hex_filepath;

        std::string line;
        uint32_t current_address = 0;
        while (std::getline(hex_file, line)) {
            if (line.empty() || line[0] == '#') continue;
            
            // Remove any whitespace and comments
            size_t comment_pos = line.find('#');
            if (comment_pos != std::string::npos) {
                line = line.substr(0, comment_pos);
            }
            
            // Skip empty lines after comment removal
            if (line.empty()) continue;
            
            std::stringstream ss;
            ss << std::hex << line;
            uint32_t instruction;
            if (ss >> instruction) {
                instruction_memory[current_address++] = instruction;
            }
        }
        std::cout << "Loaded " << instruction_memory.size() << " instructions." << std::endl;
        
        // Print first few instructions for debugging
        for (auto it = instruction_memory.begin(); it != instruction_memory.end() && it->first < 5; ++it) {
            printf("[TB] Instruction[%d] = 0x%08x\n", it->first, it->second);
        }
    }

    // Initialize data memory with some test values if needed
    void initializeDataMemory() {
        // Clear any existing data
        data_memory.clear();
        
        // Optionally initialize some test data
        // data_memory[0x8000] = 0x12345678;  // Example initialization
    }

    // Main simulation runner
    void runAndComplete(int timeout_cycles = 1000) {
        // Set kernel configuration
        top->base_instr = 0;        // Program starts at PC=0
        top->base_data = 0x8000;    // Base address for data memory accesses
        top->num_blocks = 1;        // Single block for simple test
        top->warps_per_block = 1;   // Single warp per block
        
        printf("[TB] Starting GPU execution with config:\n");
        printf("     base_instr=0x%x, base_data=0x%x\n", 0, 0x8000);
        printf("     num_blocks=%d, warps_per_block=%d\n", 1, 1);
        
        // Start execution
        top->execution_start = 1;
        tick();
        top->execution_start = 0;
        
        printf("[TB] Execution started, waiting for completion...\n");

        // Run until done or timeout
        for (int i = 0; i < timeout_cycles; ++i) {
            tick();
            
            if (top->execution_done) {
                printf("[TB] GPU execution completed in %d cycles.\n", i);
                SUCCEED() << "GPU finished in " << i << " cycles.";
                return;
            }
            
            // Print periodic status
            if (i % 100 == 0 && i > 0) {
                printf("[TB] Cycle %d: Still running...\n", i);
            }
        }
        
        printf("[TB] GPU execution timed out after %d cycles.\n", timeout_cycles);
        FAIL() << "GPU timed out and did not finish within " << timeout_cycles << " cycles.";
    }

    // Helper function to print memory contents for debugging
    void printDataMemory() {
        printf("[TB] Data Memory Contents:\n");
        for (const auto& entry : data_memory) {
            printf("     [0x%08x] = 0x%08x\n", entry.first, entry.second);
        }
    }
};

// --- THE INTEGRATION TEST ---

TEST_F(GpuTestbench, MCU_ScalarWriteIntegration) {
    printf("[TB] Starting MCU Scalar Write Integration Test\n");
    
    // 1. Load the assembled program into our simulated instruction memory
    loadProgramFromHex("test/tmp_test/scalar_write_test.hex");

    // 2. Initialize data memory to ensure a clean slate
    initializeDataMemory();

    // 3. Run the simulation until completion
    runAndComplete();

    // 4. Print memory contents for debugging
    printDataMemory();

    // 5. Verify the expected results
    // The test program should write value 32 to address 42
    uint32_t expected_address = 42;
    uint32_t expected_data = 32;

    ASSERT_TRUE(data_memory.count(expected_address))
        << "The program did not write to the expected memory address 0x"
        << std::hex << expected_address;
    
    EXPECT_EQ(data_memory[expected_address], expected_data)
        << "Expected data 0x" << std::hex << expected_data 
        << " at address 0x" << expected_address
        << ", but found 0x" << data_memory[expected_address];
        
    printf("[TB] Test completed successfully!\n");
}

// Additional test for multiple memory operations
TEST_F(GpuTestbench, MCU_MultipleOperations) {
    printf("[TB] Starting MCU Multiple Operations Test\n");
    
    // This test could be used for more complex programs
    // that perform multiple reads and writes through the MCU
    
    // For now, just ensure the basic infrastructure works
    loadProgramFromHex("test/tmp_test/scalar_write_test.hex");
    initializeDataMemory();
    
    // Run with a longer timeout for more complex operations
    runAndComplete(2000);
    
    printDataMemory();
    
    // Basic verification that something happened
    EXPECT_FALSE(data_memory.empty()) << "No memory operations were performed";
}

// --- MAIN FUNCTION ---
int main(int argc, char **argv) {
    Verilated::commandArgs(argc, argv);
    testing::InitGoogleTest(&argc, argv);
    Verilated::mkdir("logs");
    
    printf("Starting GPU testbench with MCU integration...\n");
    
    auto result = RUN_ALL_TESTS();
    
    VerilatedCov::write(("logs/coverage_" + std::string(NAME) + ".dat").c_str());
    return result;
}