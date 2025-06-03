#include "base_testbench.h"
#include <cstdlib>
#include <iomanip>

#define NAME "data_mem"

class DataMemTestbench : public BaseTestbench {
protected:
    void initializeInputs() override {
        top->clk = 0;
        top->WDME = 0;
        top->A = 0;
        top->WD = 0;
    }

    void toggleClock() {
        top->clk = 0;
        top->eval();
        top->clk = 1;
        top->eval();
    }

    void waitCycles(int n) {
        for (int i = 0; i < n; ++i) toggleClock();
    }
};

TEST_F(DataMemTestbench, ReadPreloadedData) {
    // Try to read from first 16 words (addresses 0x00 to 0x3C)
    for (int i = 0; i < 16; ++i) {
        uint32_t addr = i * 4;

        top->A = addr;
        toggleClock();  // Synchronous read from BRAM
        uint32_t val = top->RD;

        std::cout << "0x" << std::hex << std::setw(4) << std::setfill('0') << addr
                  << ": 0x" << std::setw(8) << val << std::endl;
    }
}


TEST_F(DataMemTestbench, WriteAndReadBack) {
    const int test_count = 5;
    const uint32_t base_addr = 0x10;

    // Write data to memory
    for (int i = 0; i < test_count; ++i) {
        uint32_t addr = base_addr + i * 4;
        uint32_t data = 0xA0B0C000 | i;

        top->A = addr;
        top->WD = data;
        top->WDME = 1;
        toggleClock();  // perform write
    }

    top->WDME = 0; // disable write mode
    toggleClock();

    // Read and print back the data
    for (int i = 0; i < test_count; ++i) {
        uint32_t addr = base_addr + i * 4;

        top->A = addr;
        toggleClock();  // wait for data to be output (BRAM latency)

        uint32_t rd = top->RD;

        std::cout << "Address 0x" << std::setw(4) << std::setfill('0') << std::hex << addr
                  << ": 0x" << std::setw(8) << std::setfill('0') << rd << std::endl;

        EXPECT_EQ(rd, 0xA0B0C000 | i)
            << "Read mismatch at address 0x" << std::hex << addr;
    }
}

int main(int argc, char **argv)
{
    Verilated::commandArgs(argc, argv);
    testing::InitGoogleTest(&argc, argv);
    Verilated::mkdir("logs");

    int result = RUN_ALL_TESTS();

    VerilatedCov::write(("logs/coverage_" + std::string(NAME) + ".dat").c_str());

    return result;
}
