#include "base_testbench.h"
#include <gtest/gtest.h>
#include <vector>
#include <map>
#include <functional>
#include <cstdint>

// These parameters MUST match the Verilog module
#define THREADS_PER_WARP 16
#define SCALAR_LSU_IDX THREADS_PER_WARP

// Helper structure to define a memory request for the test
struct MemoryRequest {
    int thread_idx;
    bool is_write;
    uint32_t address; // This is a WORD address
    uint32_t data; // Only used for writes
};

// Testbench class for the Memory Coalescing Unit (MCU)
class MCUTestbench : public BaseTestbench {
protected:
    // Simulated AXI BRAM (our main memory)
    // Map from BYTE address to 32-bit data word
    std::map<uint32_t, uint32_t> axi_memory;

    // --- Testbench Helper Methods ---

    // This method is called once per test by the GTest framework
    void initializeInputs() override {
        // Set all DUT inputs to a known, idle state
        top->start_mcu_transaction = 0;

        for (int i = 0; i <= THREADS_PER_WARP; ++i) {
            top->consumer_read_valid[i] = 0;
            top->consumer_read_address[i] = 0;
            top->consumer_write_valid[i] = 0;
            top->consumer_write_address[i] = 0;
            top->consumer_write_data[i] = 0;
        }

        top->m_axi_awready = 0;
        top->m_axi_wready = 0;
        top->m_axi_bvalid = 0;
        top->m_axi_bresp = 0;
        top->m_axi_bid = 0;
        top->m_axi_arready = 0;
        top->m_axi_rvalid = 0;
        top->m_axi_rlast = 0;
        top->m_axi_rdata = 0;
        top->m_axi_rid = 0;
        top->m_axi_rresp = 0;

        // Reset the DUT
        top->clk = 0;
        top->reset = 1;
        top->eval();
        top->clk = 1;
        top->eval();
        top->reset = 0;
        top->clk = 0;
        top->eval();
    }

    // Single clock tick
    void tick() {
        // --- Actual Clock Tick (Part 1: Posedge) ---
        top->clk = 1;
        top->eval();

        // --- Simulate AXI Memory behavior (combinatorially after posedge) ---
        top->m_axi_awready = 1;
        top->m_axi_arready = 1;
        top->m_axi_wready = 1;
        top->m_axi_bready = 1;
        top->m_axi_rready = 1;

        // --- Write Address Channel ---
        if (top->m_axi_awvalid && top->m_axi_awready) {
            printf("[TB] AXI Slave: Saw AWVALID. Latching AWADDR=0x%x, AWLEN=%d\n", top->m_axi_awaddr, top->m_axi_awlen);
            write_burst_addr = top->m_axi_awaddr;
            write_burst_len = top->m_axi_awlen;
            write_burst_count = 0;
        }

        // --- Write Data Channel ---
        if (top->m_axi_wvalid && top->m_axi_wready) {
            uint32_t current_write_addr = write_burst_addr + (write_burst_count * 4);
            printf("[TB] AXI Slave: Saw WVALID. Writing 0x%x to addr 0x%x. WLAST=%d\n", top->m_axi_wdata, current_write_addr, top->m_axi_wlast);
            axi_memory[current_write_addr] = top->m_axi_wdata;
            write_burst_count++;
            if (top->m_axi_wlast) {
                bvalid_next_cycle = true;
            }
        }

        // --- Write Response Channel ---
        if (bvalid_next_cycle) {
             printf("[TB] AXI Slave: Asserting BVALID\n");
        }
        top->m_axi_bvalid = bvalid_next_cycle;
        bvalid_next_cycle = false;

        // --- Read Address Channel ---
        if (top->m_axi_arvalid && top->m_axi_arready) {
            printf("[TB] AXI Slave: Saw ARVALID. Latching ARADDR=0x%x, ARLEN=%d\n", top->m_axi_araddr, top->m_axi_arlen);
            read_burst_addr = top->m_axi_araddr;
            read_burst_len = top->m_axi_arlen;
            read_burst_count = 0;
            read_burst_active = true;
        }

        // --- Read Data Channel ---
        if (read_burst_active && top->m_axi_rready) {
            top->m_axi_rvalid = 1;
            uint32_t current_read_addr = read_burst_addr + (read_burst_count * 4);
            top->m_axi_rdata = axi_memory.count(current_read_addr) ? axi_memory[current_read_addr] : 0xDEADBEEF;
            top->m_axi_rlast = (read_burst_count == read_burst_len);
            printf("[TB] AXI Slave: Asserting RVALID. Reading 0x%x from addr 0x%x. RLAST=%d\n", top->m_axi_rdata, current_read_addr, top->m_axi_rlast);

            if (top->m_axi_rlast) {
                read_burst_active = false;
            }
            read_burst_count++;
        } else {
            top->m_axi_rvalid = 0;
            top->m_axi_rlast = 0;
        }

        // --- Actual Clock Tick (Part 2: Negedge) ---
        top->clk = 0;
        top->eval();
    }

    // Helper to apply a set of requests from the LSUs
    void applyRequests(const std::vector<MemoryRequest>& requests) {
        for (const auto& req : requests) {
            if (req.is_write) {
                top->consumer_write_valid[req.thread_idx] = 1;
                top->consumer_write_address[req.thread_idx] = req.address;
                top->consumer_write_data[req.thread_idx] = req.data;
            } else {
                top->consumer_read_valid[req.thread_idx] = 1;
                top->consumer_read_address[req.thread_idx] = req.address;
            }
        }
    }

    // A clean way to run a multi-cycle MCU transaction
    void runTransaction(const std::vector<MemoryRequest>& requests, int timeout_cycles = 100) {
        // 1. Apply the consumer requests to the DUT's inputs
        applyRequests(requests);

        // 2. Pulse the start signal for one cycle
        top->start_mcu_transaction = 1;
        tick();
        top->start_mcu_transaction = 0;
        
        // 3. De-assert consumer valid signals (the DUT should have latched them)
        for (int i = 0; i <= THREADS_PER_WARP; ++i) {
            top->consumer_read_valid[i] = 0;
            top->consumer_write_valid[i] = 0;
        }

        // 4. Run the simulation until the MCU is no longer busy
        for (int i = 0; i < timeout_cycles; ++i) {
            if (!top->mcu_is_busy) {
                tick(); // one final tick to propagate final outputs
                SUCCEED() << "MCU finished transaction in " << i + 1 << " cycles.";
                return;
            }
            tick();
        }      
        FAIL() << "MCU timed out (did not become idle).";
    }

private:
    // Internal state for simulating AXI behavior
    bool bvalid_next_cycle = false;
    uint32_t write_burst_addr = 0;
    uint32_t write_burst_len = 0;
    int write_burst_count = 0;

    bool read_burst_active = false;
    uint32_t read_burst_addr = 0;
    uint32_t read_burst_len = 0;
    int read_burst_count = 0;
};


// --- TEST CASES ---

// TEST_F(MCUTestbench, IdleState) {
//     initializeInputs();
//     EXPECT_FALSE(top->mcu_is_busy);
// }

TEST_F(MCUTestbench, ScalarRead) {
    initializeInputs();
    axi_memory[0x1000 * 4] = 0xABCD1234;
    std::vector<MemoryRequest> requests = {
        { .thread_idx = SCALAR_LSU_IDX, .is_write = false, .address = 0x1000, .data = 0 }
    };
    runTransaction(requests);
    EXPECT_TRUE(top->consumer_read_ready[SCALAR_LSU_IDX]);
    EXPECT_EQ(top->consumer_read_data[SCALAR_LSU_IDX], 0xABCD1234);
}

// TEST_F(MCUTestbench, ScalarWrite) {
//     initializeInputs();
//     std::vector<MemoryRequest> requests = {
//         { .thread_idx = SCALAR_LSU_IDX, .is_write = true, .address = 0x2000, .data = 0xCAFEBABE }
//     };
//     runTransaction(requests);
//     ASSERT_TRUE(axi_memory.count(0x2000 * 4));
//     EXPECT_EQ(axi_memory[0x2000 * 4], 0xCAFEBABE);
// }

// TEST_F(MCUTestbench, SimpleCoalescedWrite) {
//     initializeInputs();
//     std::vector<MemoryRequest> requests = {
//         { .thread_idx = 0, .is_write = true, .address = 0x100, .data = 0xAAAAAAAA },
//         { .thread_idx = 1, .is_write = true, .address = 0x101, .data = 0xBBBBBBBB },
//         { .thread_idx = 2, .is_write = true, .address = 0x102, .data = 0xCCCCCCCC },
//         { .thread_idx = 3, .is_write = true, .address = 0x103, .data = 0xDDDDDDDD }
//     };
//     runTransaction(requests);
//     EXPECT_EQ(axi_memory[0x100 * 4], 0xAAAAAAAA);
//     EXPECT_EQ(axi_memory[0x101 * 4], 0xBBBBBBBB);
//     EXPECT_EQ(axi_memory[0x102 * 4], 0xCCCCCCCC);
//     EXPECT_EQ(axi_memory[0x103 * 4], 0xDDDDDDDD);
// }

// TEST_F(MCUTestbench, UncoalescedDivergentRead) {
//     initializeInputs();
//     axi_memory[0x100 * 4] = 100;
//     axi_memory[0x250 * 4] = 250;
//     axi_memory[0x375 * 4] = 375;
//     std::vector<MemoryRequest> requests = {
//         { .thread_idx = 2, .is_write = false, .address = 0x100 },
//         { .thread_idx = 7, .is_write = false, .address = 0x250 },
//         { .thread_idx = 11, .is_write = false, .address = 0x375 }
//     };
//     runTransaction(requests);
//     EXPECT_TRUE(top->consumer_read_ready[2]);
//     EXPECT_EQ(top->consumer_read_data[2], 100);
//     EXPECT_TRUE(top->consumer_read_ready[7]);
//     EXPECT_EQ(top->consumer_read_data[7], 250);
//     EXPECT_TRUE(top->consumer_read_ready[11]);
//     EXPECT_EQ(top->consumer_read_data[11], 375);
// }