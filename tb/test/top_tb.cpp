#include "Vdut.h"
#include "verilated.h"
#include <gtest/gtest.h>
#include <unordered_map>
#include <iomanip>

#define INST_CHANNELS 8
#define DATA_CHANNELS 8
#define MAX_CYCLES 1000

vluint64_t main_time = 0;
double sc_time_stamp() { return main_time; }

using IData = uint32_t;

template <uint32_t num_channels>
struct InstructionMemory {
    Vdut* top;
    std::unordered_map<IData, IData> memory;

    void connect(Vdut* dut) { top = dut; }

    void process() {
        for (uint32_t i = 0; i < num_channels; ++i) {
            if (top->instruction_mem_read_valid & (1 << i)) {
                IData addr = top->instruction_mem_read_address[i];
                top->instruction_mem_read_data[i] = memory[addr];
                top->instruction_mem_read_ready |= (1 << i);
            } else {
                top->instruction_mem_read_ready &= ~(1 << i);
            }
        }
    }

    void load(IData addr, IData val) { memory[addr] = val; }
};

template <uint32_t num_channels>
struct DataMemory {
    Vdut* top;
    std::unordered_map<IData, IData> memory;

    void connect(Vdut* dut) { top = dut; }

    void process() {
        for (uint32_t i = 0; i < num_channels; ++i) {
            if (top->data_mem_write_valid & (1 << i)) {
                memory[top->data_mem_write_address[i]] = top->data_mem_write_data[i];
                top->data_mem_write_ready |= (1 << i);
            } else {
                top->data_mem_write_ready &= ~(1 << i);
            }

            if (top->data_mem_read_valid & (1 << i)) {
                top->data_mem_read_data[i] = memory[top->data_mem_read_address[i]];
                top->data_mem_read_ready |= (1 << i);
            } else {
                top->data_mem_read_ready &= ~(1 << i);
            }
        }
    }

    IData read(IData addr) const {
        auto it = memory.find(addr);
        return (it != memory.end()) ? it->second : 0;
    }

    void dump() const {
        std::cout << "Memory Dump:\n";
        for (const auto& [addr, val] : memory) {
            std::cout << std::hex << "  [" << addr << "] = 0x" << val << "\n";
        }
    }
};

class GpuTest : public ::testing::Test {
protected:
    Vdut* top;
    InstructionMemory<INST_CHANNELS> instr_mem;
    DataMemory<DATA_CHANNELS> data_mem;

    void SetUp() override {
        Verilated::traceEverOn(true);
        Verilated::mkdir("logs");

        top = new Vdut;
        instr_mem.connect(top);
        data_mem.connect(top);

        // Initial reset
        top->clk = 0;
        top->reset = 1;
        for (int i = 0; i < 4; ++i) {
            tick();
        }
        top->reset = 0;
    }

    void TearDown() override {
        delete top;
    }

    void tick() {
        top->clk = 0; top->eval(); instr_mem.process(); data_mem.process(); ++main_time;
        top->clk = 1; top->eval(); instr_mem.process(); data_mem.process(); ++main_time;
    }
};

TEST_F(GpuTest, AddStoreWorks) {
    // Addi x5, x0, 10
    instr_mem.load(0, 0b00100000000001010100000000000000);
    // Addi x6, x0, 20
    instr_mem.load(1, 0b00100000000001100110000000000000);
    // add x7, x5, x6
    instr_mem.load(2, 0b00000000110101110111000000000000);
    // sw x7, 0(x0)
    instr_mem.load(3, 0b10000000000000111000000000000000);
    // exit
    instr_mem.load(4, 0b11100000000000000000000000000111);

    data_mem.memory.clear();

    // Kernel config inputs
    top->base_instructions_address = 0;
    top->base_data_address = 0;
    top->num_blocks = 1;
    top->num_warps_per_block = 1;
    top->execution_start = 1;

    std::cout << ">> num_blocks: " << top->num_blocks << std::endl;

    for (int i = 0; i < MAX_CYCLES; ++i) {
        tick();
        if (top->execution_done) {
            break;
        }
    }

    IData result = data_mem.read(0);
    EXPECT_EQ(result, 30u) << "Expected x7 (10+20) to be stored at mem[0]";
}
