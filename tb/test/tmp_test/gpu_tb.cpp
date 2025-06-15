#include "base_testbench.h"
#include <gtest/gtest.h>
#include <vector>
#include <map>
#include <functional>
#include <cstdint>
#include <fstream>
#include <sstream>
#include <iomanip>

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
        
        // AXI inputs from the testbench (slave) to the DUT (master)
        // must be initialized.
        top->m_axi_awready = 0;
        top->m_axi_wready = 0;
        top->m_axi_bvalid = 0;
        top->m_axi_bresp = 0;
        top->m_axi_bid = 0;
        top->m_axi_arready = 0;
        top->m_axi_rvalid = 0;
        top->m_axi_rdata = 0;
        top->m_axi_rresp = 0;
        top->m_axi_rlast = 0;
        top->m_axi_rid = 0;
        
        // This is still needed for the instruction memory interface
        top->instruction_mem_read_ready = 0;

        tick(); // Tick once to apply reset
        top->reset = 0;
    }

    void printMemoryRange(uint32_t start_addr = 0, uint32_t end_addr = 50) {
        std::cout << "Memory contents from address " << start_addr << " to " << end_addr << ":\n";
        std::cout << "Address\t\tValue\n";
        std::cout << "-------\t\t-----\n";
        
        for (uint32_t addr = start_addr; addr <= end_addr; addr++) {
            auto it = data_memory.find(addr);
            uint32_t value = (it != data_memory.end()) ? it->second : 0xDEADBEEF;
            
            std::cout << "0x" << std::hex << std::setw(8) << std::setfill('0') << addr 
                    << "\t0x" << std::hex << std::setw(8) << std::setfill('0') << value << std::endl;
        }
    }

    // This tick function now simulates an AXI Slave memory.
    void tick() {
        // --- Combinational Logic (Before the clock edge) ---

        // Instruction Memory
        // 'valid' and 'ready' are single ports (packed arrays in SV)
        top->instruction_mem_read_ready = top->instruction_mem_read_valid;

        if (top->instruction_mem_read_valid) {
            // --- FIX: Add [0] index for address and data (unpacked arrays in SV) ---
            uint32_t addr = top->instruction_mem_read_address[0];
            if (instruction_memory.count(addr)) {
                top->instruction_mem_read_data[0] = instruction_memory[addr];
            } else {
                top->instruction_mem_read_data[0] = 0; // Return NOP
            }
        }
        
        // --- AXI Data Memory Slave Simulation ---
        // (This part is correct and remains unchanged)
        top->m_axi_awready = 0;
        top->m_axi_wready = 0;
        top->m_axi_bvalid = 0;
        top->m_axi_arready = 0;
        top->m_axi_rvalid = 0;
        top->m_axi_rlast = 0;

        // Handle Write Transaction
        if (top->m_axi_awvalid) {
            top->m_axi_awready = 1;
        }
        if (top->m_axi_wvalid) {
            top->m_axi_wready = 1;
            uint32_t byte_addr = top->m_axi_awaddr;
            data_memory[byte_addr] = top->m_axi_wdata;
            printf("[TB] AXI: Acknowledging WRITE of 0x%x to BYTE addr 0x%x\n", top->m_axi_wdata, byte_addr);
            top->m_axi_bvalid = 1;
        }

        // Handle Read Transaction
        if (top->m_axi_arvalid) {
            top->m_axi_arready = 1;
            uint32_t byte_addr = top->m_axi_araddr;
            printf("[TB] AXI: Responding to READ from BYTE addr 0x%x\n", byte_addr);
            if (data_memory.count(byte_addr)) {
                top->m_axi_rdata = data_memory[byte_addr];
            } else {
                top->m_axi_rdata = 0xDEADBEEF;
            }
            top->m_axi_rvalid = 1;
            top->m_axi_rlast = 1;
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

    void runAndComplete(int timeout_cycles = 100) {
        // --- Set kernel config ---
        // REMOVED: top->data_mem_read_ready = 1;
        // REMOVED: top->data_mem_write_ready = 1;
        // The instruction mem ready is still needed as it's a separate, non-AXI interface.
        // Let's set it to always be ready in the tick function for simplicity.

        top->base_instr = 0;
        top->base_data = 0;
        top->num_blocks = 1;
        top->warps_per_block = 1;
        
        // --- Start execution ---
        top->execution_start = 1;
        tick();
        top->execution_start = 0;

        // --- Run until done ---
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
    loadProgramFromHex("test/tmp_test/mcu.hex");
    data_memory.clear();
    runAndComplete();

    uint32_t expected_byte_address = 42 * 4; // 168
    uint32_t expected_data = 32;

    ASSERT_TRUE(data_memory.count(expected_byte_address))
        << "The program did not write to the expected memory BYTE address 0x"
        << std::hex << expected_byte_address;
    
    EXPECT_EQ(data_memory[expected_byte_address], expected_data)
        << "The data written to memory was incorrect.";
}

TEST_F(GpuTestbench, MCU_Vivado_IScalar) {
    loadProgramFromHex("test/tmp_test/vivado_iscalar.hex");
    data_memory.clear();
    runAndComplete();

    uint32_t expected_byte_address = 42 * 4; // 168
    uint32_t expected_data = 30;
    
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