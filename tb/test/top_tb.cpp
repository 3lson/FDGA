#include "sync_testbench.h"
#include <verilated_cov.h>

#define NAME "top"

class TopTestbench : public SyncTestbench {
protected:
    bool resetReleased = false;

    void initializeInputs() override {
        top->clk = 1;
        top->rst = 0;
    }

};

TEST_F(TopTestbench, ExecuteSimpleProgram) {
    runSimulation(20000);  // Run enough cycles for instructions to complete

    EXPECT_EQ(top->a0, 5);
}

int main(int argc, char **argv) {
    Verilated::commandArgs(argc, argv);
    testing::InitGoogleTest(&argc, argv);
    Verilated::mkdir("logs");

    int result = RUN_ALL_TESTS();
    VerilatedCov::write(("logs/coverage_" + std::string(NAME) + ".dat").c_str());
    return result;
}
