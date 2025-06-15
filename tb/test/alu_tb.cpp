#include "base_testbench.h"
#include <verilated_cov.h>
#include <gtest/gtest.h>
#include <bit>

#define ADD         0
#define SUB         1
#define MUL         2
#define DIV         3
#define SLT         4
#define SLL         5
#define SEQ         6
#define SNEZ        7
#define MIN         8
#define ABS         9
#define ADDI        10
#define MULI        11
#define DIVI        12
#define SLLI        13
#define SEQI        27
#define BEQZ        25
#define JAL        26
#define BEQ0        28

// Base class for testing the sequential ALU
class ALUTestbench : public BaseTestbench {
protected:
    void clockCycle() {
        // Falling edge
        top->eval();
        // Rising edge
        top->eval();
    }

    // This method is called by the test fixture setup
    void initializeInputs() override {
        top->instruction = 0;
        top->ALUop1 = 0;
        top->ALUop2 = 0;
        top->IMM = 0;
    }

    // Helper to reset the DUT
    void resetDUT() {
        clockCycle(); // Hold reset for one cycle
        top->eval();    // Let reset go low
    }

    // A clean way to run a single ALU operation
    void run_operation(uint32_t instruction, int32_t op1, int32_t op2, int32_t imm = 0, uint32_t pc = 0) {
        top->instruction = instruction;
        top->pc = pc; // <-- ADD THIS LINE
        top->ALUop1 = op1;
        top->ALUop2 = op2;
        top->IMM = imm;
        clockCycle();
    }
};

TEST_F(ALUTestbench, AddTest) {
    resetDUT();
    int32_t op1 = 5;
    int32_t op2 = 10;
    run_operation(ADD, op1, op2);
    EXPECT_EQ(top->Result, op1 + op2);
}

TEST_F(ALUTestbench, SubtractionTest) {
    resetDUT();
    int32_t op1 = 20;
    int32_t op2 = 5;
    run_operation(SUB, op1, op2);
    EXPECT_EQ(top->Result, op1 - op2);
}

TEST_F(ALUTestbench, MulTest) {
    resetDUT();
    int32_t op1 = 5;
    int32_t op2 = 6;
    run_operation(MUL, op1, op2);
    EXPECT_EQ(top->Result, op1 * op2);
}

TEST_F(ALUTestbench, DivTest) {
    resetDUT();
    int32_t op1 = 20;
    int32_t op2 = 4;
    run_operation(DIV, op1, op2);
    EXPECT_EQ(top->Result, op1 / op2);
}

TEST_F(ALUTestbench, LessThanTestTrue) {
    resetDUT();
    run_operation(SLT, 3, 5);
    EXPECT_EQ(top->Result, 1);
}

TEST_F(ALUTestbench, LessThanTestFalse) {
    resetDUT();
    run_operation(SLT, 5, 3);
    EXPECT_EQ(top->Result, 0);
}


TEST_F(ALUTestbench, SLLTest) {
    resetDUT();
    run_operation(SLL, 1, 2);
    EXPECT_EQ(top->Result, 4);
}

TEST_F(ALUTestbench, EqualToTestTrue) {
    resetDUT();
    run_operation(SEQ, 5, 5);
    EXPECT_EQ(top->Result, 1);
}

TEST_F(ALUTestbench, EqualToTestFalse) {
    resetDUT();
    run_operation(SEQ, 8, 5);
    EXPECT_EQ(top->Result, 0);
}

TEST_F(ALUTestbench, MinTestOp1) {
    resetDUT();
    run_operation(MIN, 3, 5);
    EXPECT_EQ(top->Result, 3);
}

TEST_F(ALUTestbench, AbsTest) {
    resetDUT();
    // For -8 (0xFFFFFFF8), it will result in 0x7FFFFFF8.
    // Let's test based on the actual Verilog implementation.
    run_operation(ABS, 0xFFFFFFF8, 0); // op1 = -8
    EXPECT_EQ(top->Result, 8);
}

TEST_F(ALUTestbench, AddImmediateTest) {
    resetDUT();
    int32_t op1 = 100;
    int32_t imm = -20; // Test with a negative immediate
    run_operation(ADDI, op1, 0, imm); // op2 is ignored for I-type
    EXPECT_EQ(top->Result, op1 + imm);
}

TEST_F(ALUTestbench, MultiplyImmediateTest) {
    resetDUT();
    int32_t op1 = 15;
    int32_t imm = 3;
    run_operation(MULI, op1, 0, imm);
    EXPECT_EQ(top->Result, op1 * imm);
}

TEST_F(ALUTestbench, SLLI_Test) {
    resetDUT();
    int32_t op1 = 0x0000000F; // Value is 15
    int32_t imm = 4;          // Shift by 4
    run_operation(SLLI, op1, 0, imm);
    // Expect 15 << 4 = 240 (0x000000F0)
    EXPECT_EQ(top->Result, 0x000000F0);
}

TEST_F(ALUTestbench, SEQI_Test_True) {
    resetDUT();
    int32_t op1 = 4;
    int32_t imm = 4;

    // For an I-type instruction, op2 is ignored.
    // Pass the immediate value in the 'imm' parameter.
    run_operation(SEQI, op1, 0, imm); // Correct usage

    EXPECT_EQ(top->Result, 1);
}

TEST_F(ALUTestbench, BEQZ_ConditionTrue) {
    resetDUT();
    int32_t op1_is_zero = 0;
    // For BEQZ, op1 is rs1, op2 is not used.
    // The ALU checks if op1 is zero and sets the EQ flag.
    run_operation(BEQZ, op1_is_zero, 0);

    EXPECT_EQ(top->Result, 1) << "Result should be 1 when condition is true";
}

TEST_F(ALUTestbench, BEQZ_ConditionFalse) {
    resetDUT();
    int32_t op1_not_zero = 123;
    run_operation(BEQZ, op1_not_zero, 0);

    EXPECT_EQ(top->Result, 0) << "Result should be 0 when condition is false";
}

TEST_F(ALUTestbench, JumpTest) {
    resetDUT();
    uint32_t current_pc = 0x100;
    int32_t offset = 0x40; // Jump forward 16 instructions (16*4 = 64 bytes)
    
    // The ALU calculates the target address: PC + immediate offset
    run_operation(JAL, 0, 0, offset, current_pc);
    
    EXPECT_EQ(top->Result, current_pc + offset);
}

TEST_F(ALUTestbench, BEQ0_ConditionTrue) {
    resetDUT();
    int32_t op1_is_zero = 1;
    // For BEQZ, op1 is rs1, op2 is not used.
    // The ALU checks if op1 is zero and sets the EQ flag.
    run_operation(BEQ0, op1_is_zero, 1);

    EXPECT_EQ(top->Result, 1) << "Result should be 1 when condition is true";
}

TEST_F(ALUTestbench, BEQ0_ConditionFalse) {
    resetDUT();
    int32_t op1_not_zero = 0;
    run_operation(BEQ0, op1_not_zero, 1);

    EXPECT_EQ(top->Result, 0) << "Result should be 0 when condition is false";
}
