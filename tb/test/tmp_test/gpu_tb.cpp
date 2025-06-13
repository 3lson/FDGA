#include "base_testbench.h"
#include <gtest/gtest.h>
#include <vector>
#include <map>
#include <functional>
#include <cstdint>
#include <fstream>
#include <sstream>

#define NAME "gpu"
#define NUM_CORES 1
#define WARPS_PER_CORE 1
#define THREADS_PER_WARP 16

class GpuTestbench : public BaseTestbench {
protected:
    // --- Testbench Memory Simulation ---
    std::map<uint32_t, uint32_t> instruction_memory;
    std::map<uint32_t, uint32_t> data_memory;

    void initializeInputs() override {
        top->clk = 0;
        top->reset = 1;
        top->execution_start = 0;
        
        top->base_instr = 0;
        top->base_data = 0;
        top->num_blocks = 0;
        top->warps_per_block = 0;

        // In a real system, ready signals are driven by the memory.
        // For the testbench, we will control them in tick().
        top->data_mem_read_ready = 0;
        top->data_mem_write_ready = 0;
        top->instruction_mem_read_ready = 0;
        
        tick(); // Tick once to apply reset
        top->reset = 0;
    }

    // FIX: A more realistic, pipelined tick function
    void tick() {
        // --- Combinational Memory Logic (Before the clock edge) ---
        // The memory system sees the requests from the previous cycle and prepares responses.

        // Instruction Memory: 1-cycle latency
        // If the GPU requested an instruction last cycle, provide it this cycle.
        top->instruction_mem_read_ready = top->instruction_mem_read_valid;
        if (top->instruction_mem_read_valid) {
            uint32_t addr = top->instruction_mem_read_address[0]; // word address
            if (instruction_memory.count(addr)) {
                top->instruction_mem_read_data[0] = instruction_memory[addr];
            } else {
                top->instruction_mem_read_data[0] = 0; // Return NOP
            }
        }
        
        // Data Memory: 1-cycle latency
        // The MCU drives the top-level ports. Our TB acts as the memory slave.
        top->data_mem_read_ready = top->data_mem_read_valid;
        if (top->data_mem_read_valid) {
            uint32_t byte_addr = top->data_mem_read_address[0];
            if (data_memory.count(byte_addr)) {
                top->data_mem_read_data[0] = data_memory[byte_addr];
            } else {
                top->data_mem_read_data[0] = 0xDEADBEEF;
            }
             printf("[TB] Memory: Responding to READ from BYTE addr 0x%x\n", byte_addr);
        }

        top->data_mem_write_ready = top->data_mem_write_valid;
        if (top->data_mem_write_valid) {
            uint32_t byte_addr = top->data_mem_write_address[0];
            data_memory[byte_addr] = top->data_mem_write_data[0];
            printf("[TB] Memory: Acknowledging WRITE of 0x%x to BYTE addr 0x%x\n",
                   top->data_mem_write_data[0], byte_addr);
        }

        // --- Clock Edge ---
        top->clk = 0;
        top->eval();
        top->clk = 1;
        top->eval();
    }

    void loadProgramFromHex(const std::string& hex_filepath) {
        instruction_memory.clear();
        std::ifstream hex_file(hex_filepath);
        ASSERT_TRUE(hex_file.is_open()) << "Could not open hex file: " << hex_filepath;

        std::string line;
        uint32_t current_address = 0;
        while (std::getline(hex_file, line)) {
            if (line.empty() || line[0] == '#') continue;
            std::stringstream ss;
            ss << std::hex << line;
            ss >> instruction_memory[current_address++];
        }
        std::cout << "Loaded " << instruction_memory.size() << " instructions." << std::endl;
    }

    void runAndComplete(int timeout_cycles = 500) {
        // Set kernel config
        top->base_instr = 0;
        top->base_data = 0;
        top->num_blocks = 1;
        top->warps_per_block = 1;
        
        // Start execution
        top->execution_start = 1;
        tick();
        top->execution_start = 0;

        // Run until done
        for (int i = 0; i < timeout_cycles; ++i) {
            if (top->execution_done) {
                // Run a few extra cycles for final writes to complete
                tick();
                tick();
                SUCCEED() << "GPU finished in " << i << " cycles.";
                return;
            }
            tick();
        }
        FAIL() << "GPU timed out and did not finish.";
    }
};


TEST_F(GpuTestbench, MCU_ScalarWriteIntegration) {
    // 1. Load the assembled program.
    // Assembly: s.li s1, 32; s.li s2, 42; s.sw s1, 0(s2); exit
    loadProgramFromHex("test/tmp_test/scalar_write_test.hex");

    // 2. Clear data memory
    data_memory.clear();

    // 3. Run the simulation
    runAndComplete();

    // 4. Verify the result.
    // The MCU converts word address 42 to byte address 168.
    uint32_t expected_byte_address = 42 * 4; // 168
    uint32_t expected_data = 32;

    ASSERT_TRUE(data_memory.count(expected_byte_address))
        << "The program did not write to the expected memory BYTE address 0x"
        << std::hex << expected_byte_address;
    
    EXPECT_EQ(data_memory[expected_byte_address], expected_data)
        << "The data written to memory was incorrect.";
}


// --- MAIN FUNCTION ---
int main(int argc, char **argv) {
    Verilated::commandArgs(argc, argv);
    testing::InitGoogleTest(&argc, argv);
    Verilated::mkdir("logs");
    auto result = RUN_ALL_TESTS();
    VerilatedCov::write(("logs/coverage_" + std::string(NAME) + ".dat").c_str());
    return result;
}