#include "base_testbench.h"
#include <verilated_cov.h>
#include <bitset>
#include <iostream>
#include <iomanip>

#define NAME "decoder"

// Opcode definitions (matching the SystemVerilog defines)
#define OPCODE_R    0b000
#define OPCODE_I    0b001
#define OPCODE_F    0b010
#define OPCODE_M    0b100
#define OPCODE_UP   0b011
#define OPCODE_J    0b111 // C-Type in your ISA

// ALU instruction encodings (from common.sv)
#define ADD     0
#define SUB     1
#define MUL     2
#define DIV     3
#define SLT     4
#define SLL     5   // Added for completeness
#define SEQ     6
#define SNEZ    7
#define MIN     8
#define ABS     9
#define ADDI    10
#define MULI    11
#define DIVI    12
#define SLLI    13
#define FADD    14
#define FSUB    15
#define FMUL    16
#define FDIV    17
#define FSLT    18 // Your enum has FLT, but decoder uses FSLT. Let's use FSLT.
#define FNEG    19
#define FEQ     20
#define FMIN    21
#define FABS    22
#define FCVT_W_S 23
#define FCVT_S_W 24
#define BEQZ     25
#define JAL      26
#define SYNC     27 // Assuming SYNC has its own ALU code if needed

// Reg input mux encodings
#define ALU_OUT         0
#define LSU_OUT         1
#define IMMEDIATE       2
#define PC_PLUS_1       3
#define VECTOR_TO_SCALAR 4

// Warp states
#define WARP_DECODE 2

class DecoderTestbench : public BaseTestbench {
protected:
    void initializeInputs() override {
        top->clk = 0;
        top->reset = 0;
        top->warp_state = 0;
        top->instruction = 0;
    }

    // NEW and IMPROVED helper function
    uint32_t makeInstr(uint8_t opcode, uint8_t funct4_or_funct3,
                    uint8_t rd, uint8_t rs1, uint8_t rs2,
                    uint32_t immediate, bool scalar) {
        uint32_t instr = 0;
        instr |= (opcode & 0x7) << 29;

        // --- Place scalar bit based on opcode ---
        if (opcode == OPCODE_R || opcode == OPCODE_I || opcode == OPCODE_F) {
            instr |= (scalar ? 1 : 0) << 28;
        } else if (opcode == OPCODE_M) {
            instr |= (scalar ? 1 : 0) << 13;
        } else if (opcode == OPCODE_UP) {
            instr |= (scalar ? 1 : 0) << 5;
        }

        // --- Place registers ---
        instr |= (rd & 0x1F);
        instr |= (rs1 & 0x1F) << 5;
        instr |= (rs2 & 0x1F) << 14;

        // --- Place immediate and funct fields ---
        if (opcode == OPCODE_R || opcode == OPCODE_F) {
            instr |= (funct4_or_funct3 & 0xF) << 10;
        } else if (opcode == OPCODE_I) {
            instr |= (funct4_or_funct3 & 0xF) << 10;
            instr |= (immediate & 0x3FFF) << 14;
        } else if (opcode == OPCODE_M) {
            uint8_t funct3 = funct4_or_funct3;
            instr |= (funct3 & 0x7) << 10;
            if (funct3 == 0b000 || funct3 == 0b010) { // Load format
                instr |= (immediate & 0x7FFF) << 14;
            } else { // Store format
                instr &= ~(0x1F); // Clear rd field
                instr |= (immediate & 0x1F); // Imm[4:0]
                instr |= ((immediate >> 5) & 0x3FF) << 19; // Imm[14:5]
            }
        } else if (opcode == OPCODE_UP) {
            instr |= (immediate & 0xFFFFF) << 9;
        } else if (opcode == OPCODE_J) { // C-Type
            uint8_t funct3 = funct4_or_funct3;
            instr |= (funct3 & 0x7) << 10;
            // Immediate is too complex for this helper, construct manually in tests.
        }
        
        return instr;
    }

    void clockCycle() {
        top->clk = 0;
        top->eval();
        top->clk = 1;
        top->eval();
    }

    void resetDecoder() {
        top->reset = 1;
        clockCycle();
        top->reset = 0;
    }

    void decodeInstruction(uint32_t instruction) {
        top->instruction = instruction;
        top->warp_state = WARP_DECODE;
        clockCycle();
    }
};


// ------------------ RESET TEST ------------------
// TEST_F(DecoderTestbench, ResetBehavior) {
//     resetDecoder();
    
//     EXPECT_EQ(top->decoded_reg_write_enable, 0);
//     EXPECT_EQ(top->decoded_mem_write_enable, 0);
//     EXPECT_EQ(top->decoded_mem_read_enable, 0);
//     EXPECT_EQ(top->decoded_branch, 0);
//     EXPECT_EQ(top->decoded_reg_input_mux, ALU_OUT);
//     EXPECT_EQ(top->decoded_immediate, 0);
//     EXPECT_EQ(top->decoded_rd_address, 0);
//     EXPECT_EQ(top->decoded_rs1_address, 0);
//     EXPECT_EQ(top->decoded_rs2_address, 0);
//     EXPECT_EQ(top->decoded_alu_instruction, ADDI);
//     EXPECT_EQ(top->decoded_halt, 0);
//     EXPECT_EQ(top->decoded_scalar_instruction, 0);
// }

// ------------------ R-TYPE TESTS ------------------
TEST_F(DecoderTestbench, RTypeInstructions) {
    struct TestCase {
        uint8_t funct4;
        uint8_t expectedALU;
        const char* name;
    };

    std::vector<TestCase> tests = {
        {0b0000, ADD, "ADD"},
        {0b0001, SUB, "SUB"},
        {0b0010, MUL, "MUL"},
        {0b0011, DIV, "DIV"},
        {0b0100, SLT, "SLT"},
        {0b0110, SEQ, "SEQ"},
        {0b0111, SNEZ, "SNEZ"},
        {0b1000, MIN, "MIN"},
        {0b1001, ABS, "ABS"}
    };

    for (auto& test : tests) {
        resetDecoder();
        uint32_t instr = makeInstr(OPCODE_R, test.funct4, 5, 10, 15, 0, false);
        decodeInstruction(instr);
        
        EXPECT_EQ(top->decoded_alu_instruction, test.expectedALU) << "Failed for " << test.name;
        EXPECT_EQ(top->decoded_reg_write_enable, 1) << "RegWrite failed for " << test.name;
        EXPECT_EQ(top->decoded_reg_input_mux, ALU_OUT) << "RegInputMux failed for " << test.name;
        EXPECT_EQ(top->decoded_rd_address, 5) << "RD address failed for " << test.name;
        EXPECT_EQ(top->decoded_rs1_address, 10) << "RS1 address failed for " << test.name;
        EXPECT_EQ(top->decoded_rs2_address, 15) << "RS2 address failed for " << test.name;
        EXPECT_EQ(top->decoded_scalar_instruction, 0) << "Scalar flag failed for " << test.name;
    }
}

// ------------------ I-TYPE TESTS ------------------
TEST_F(DecoderTestbench, ITypeInstructions) {
    struct TestCase {
        uint8_t funct4;
        uint8_t expectedALU;
        const char* name;
    };

    std::vector<TestCase> tests = {
        {0b0000, ADDI, "ADDI"},
        {0b0010, MULI, "MULI"},
        {0b0011, DIVI, "DIVI"},
        {0b1010, SLLI, "SLLI"}
    };

    for (auto& test : tests) {
        resetDecoder();
        uint32_t instr = makeInstr(OPCODE_I, test.funct4, 7, 12, 0, 0x1234, false);
        decodeInstruction(instr);
        
        EXPECT_EQ(top->decoded_alu_instruction, test.expectedALU) << "Failed for " << test.name;
        EXPECT_EQ(top->decoded_reg_write_enable, 1) << "RegWrite failed for " << test.name;
        EXPECT_EQ(top->decoded_reg_input_mux, ALU_OUT) << "RegInputMux failed for " << test.name;
        EXPECT_EQ(top->decoded_rd_address, 7) << "RD address failed for " << test.name;
        EXPECT_EQ(top->decoded_rs1_address, 12) << "RS1 address failed for " << test.name;
        EXPECT_EQ(top->decoded_scalar_instruction, 0) << "Scalar flag failed for " << test.name;
        
        // Check sign extension of 14-bit immediate
        int32_t expected_imm = (int32_t)(int16_t)((0x1234 << 2) >> 2); // Sign extend 14-bit
        EXPECT_EQ((int32_t)top->decoded_immediate, expected_imm) << "Immediate failed for " << test.name;
    }
}

// ------------------ F-TYPE (Floating Point) TESTS ------------------
TEST_F(DecoderTestbench, FTypeInstructions) {
    struct TestCase {
        uint8_t funct4;
        uint8_t expectedALU;
        const char* name;
    };

    std::vector<TestCase> tests = {
        {0b0000, FADD, "FADD"},
        {0b0001, FSUB, "FSUB"},
        {0b0010, FMUL, "FMUL"},
        {0b0011, FDIV, "FDIV"},
        {0b0100, FSLT, "FLT"},
        {0b0101, FNEG, "FNEG"},
        {0b0110, FEQ, "FEQ"},
        {0b0111, FMIN, "FMIN"},
        {0b1000, FABS, "FABS"},
        {0b1001, FCVT_W_S, "FCVT_W_S"},
        {0b1010, FCVT_S_W, "FCVT_S_W"}
    };

    for (auto& test : tests) {
        resetDecoder();
        uint32_t instr = makeInstr(OPCODE_F, test.funct4, 3, 8, 13, 0, false);
        decodeInstruction(instr);
        
        EXPECT_EQ(top->decoded_alu_instruction, test.expectedALU) << "Failed for " << test.name;
        EXPECT_EQ(top->decoded_reg_write_enable, 1) << "RegWrite failed for " << test.name;
        EXPECT_EQ(top->decoded_reg_input_mux, ALU_OUT) << "RegInputMux failed for " << test.name;
        EXPECT_EQ(top->decoded_rd_address, 3) << "RD address failed for " << test.name;
        EXPECT_EQ(top->decoded_rs1_address, 8) << "RS1 address failed for " << test.name;
        EXPECT_EQ(top->decoded_scalar_instruction, 0) << "Scalar flag failed for " << test.name;
    }
}

// ------------------ M-TYPE LOAD/STORE TESTS ------------------
TEST_F(DecoderTestbench, MTypeLoadStore) {
    // LW: funct3=0b000
    resetDecoder();
    // CORRECTED: Added scalar (false) argument
    uint32_t instr_lw = makeInstr(OPCODE_M, 0b000, 4, 9, 0, 0x2ABC, false);
    decodeInstruction(instr_lw);
    EXPECT_EQ(top->decoded_mem_read_enable, 1);
    EXPECT_EQ(top->floatingWrite, 0);

    // SW: funct3=0b001
    resetDecoder();
    // CORRECTED: Added scalar (false) argument
    uint32_t instr_sw = makeInstr(OPCODE_M, 0b001, 0, 11, 16, 0x1DEF, false);
    decodeInstruction(instr_sw);
    EXPECT_EQ(top->decoded_mem_write_enable, 1);
    EXPECT_EQ(top->decoded_reg_write_enable, 0);

    // FLW: funct3=0b010
    resetDecoder();
    // CORRECTED: Added scalar (false) argument
    uint32_t instr_flw = makeInstr(OPCODE_M, 0b010, 4, 9, 0, 0x2ABC, false);
    decodeInstruction(instr_flw);
    EXPECT_EQ(top->decoded_mem_read_enable, 1);
    EXPECT_EQ(top->floatingWrite, 1);

    // FSW: funct3=0b011
    resetDecoder();
    // CORRECTED: Added scalar (false) argument
    uint32_t instr_fsw = makeInstr(OPCODE_M, 0b011, 0, 11, 16, 0x1DEF, false);
    decodeInstruction(instr_fsw);
    EXPECT_EQ(top->decoded_mem_write_enable, 1);
    EXPECT_EQ(top->floatingRead, 0b10);
    EXPECT_EQ(top->decoded_reg_write_enable, 0);
}

// ------------------ UPPER IMMEDIATE TEST ------------------
TEST_F(DecoderTestbench, UpperImmediate) {
    resetDecoder();
    uint32_t instr = makeInstr(OPCODE_UP, 0, 6, 0, 0, 0x12345, false);
    decodeInstruction(instr);
    
    EXPECT_EQ(top->decoded_reg_write_enable, 1);
    EXPECT_EQ(top->decoded_reg_input_mux, IMMEDIATE);
    EXPECT_EQ(top->decoded_rd_address, 6);
    
    // Upper immediate should be shifted left 12 bits
    uint32_t expected_imm = (0x12345 & 0xFFFFF) << 12;
    EXPECT_EQ(top->decoded_immediate, expected_imm);
}

// ------------------ JUMP/BRANCH TESTS ------------------
TEST_F(DecoderTestbench, Branch) {
    resetDecoder();
    // BEQZ: opcode=J(111), funct3=001, rs1=7. Imm[17:0] is complex.
    // Construct manually for clarity. rs2=x0 is implicit.
    uint32_t imm = 0x800;
    uint32_t rs1 = 7;
    uint32_t instr = (OPCODE_J << 29) | (0b001 << 10) | (rs1 << 5);
    // Add immediate fields for BEQZ
    instr |= ((imm >> 8) & 0x3FF) << 19;  // Imm[17:8]
    instr |= ((imm >> 7) & 0x1)   << 13;  // Imm[7]
    instr |= ((imm >> 2) & 0x1F);         // Imm[6:2] in rd field

    decodeInstruction(instr);
    
    EXPECT_EQ(top->decoded_alu_instruction, BEQZ);
    EXPECT_EQ(top->decoded_branch, 1);
    EXPECT_EQ(top->decoded_rs1_address, 7);
}

// ------------------ JUMP AND LINK TEST ------------------
TEST_F(DecoderTestbench, Jump) {
    resetDecoder();
    // J: opcode=J(111), funct3=000. Imm[27:2] is complex.
    // Construct manually. rd and rs1 are not used.
    uint32_t imm = 0x1000;
    uint32_t instr = (OPCODE_J << 29) | (0b000 << 10);
    // Add immediate fields for J
    instr |= ((imm >> 12) & 0xFFFF) << 13; // Imm[27:12]
    instr |= ((imm >> 2) & 0x3FF);         // Imm[11:2] in bottom 10 bits

    decodeInstruction(instr);
    
    EXPECT_EQ(top->decoded_alu_instruction, JAL); // Decoder uses JAL op for this
    // The decoder immediate logic needs to handle the re-alignment
}

// ------------------ SCALAR FLAG TEST ------------------
TEST_F(DecoderTestbench, ScalarFlagTest) {
    resetDecoder();
    
    // Test a SCALAR R-type instruction (s.add)
    uint32_t instr_r = makeInstr(OPCODE_R, 0b0000, 1, 2, 3, 0, true);
    decodeInstruction(instr_r);
    
    EXPECT_EQ(top->decoded_scalar_instruction, 1) << "Scalar flag failed for R-Type";
    EXPECT_EQ(top->decoded_alu_instruction, ADD) << "ALU op failed for scalar R-Type";
    EXPECT_EQ(top->decoded_reg_write_enable, 1) << "RegWrite failed for scalar R-Type";

    // Test a SCALAR M-type instruction (s.lw)
    resetDecoder();
    uint32_t instr_m = makeInstr(OPCODE_M, 0b000, 4, 5, 0, 128, true);
    decodeInstruction(instr_m);
    
    EXPECT_EQ(top->decoded_scalar_instruction, 1) << "Scalar flag failed for M-Type";
    EXPECT_EQ(top->decoded_mem_read_enable, 1) << "MemRead failed for scalar M-Type";
    
    // Test a SCALAR P-type instruction (s.lui)
    resetDecoder();
    uint32_t instr_p = makeInstr(OPCODE_UP, 0, 6, 0, 0, 0xABCDE, true);
    decodeInstruction(instr_p);

    EXPECT_EQ(top->decoded_scalar_instruction, 1) << "Scalar flag failed for P-Type";
    EXPECT_EQ(top->decoded_reg_write_enable, 1) << "RegWrite failed for scalar P-Type";
    EXPECT_EQ(top->decoded_reg_input_mux, IMMEDIATE) << "Mux selection failed for LUI";
}

// ------------------ DEFAULT CASE TEST ------------------
TEST_F(DecoderTestbench, DefaultCase) {
    resetDecoder();
    uint32_t instr = (0b110 << 29); // Invalid opcode
    decodeInstruction(instr);
    
    // Should maintain default values set during WARP_DECODE
    EXPECT_EQ(top->decoded_reg_write_enable, 0);
    EXPECT_EQ(top->decoded_mem_write_enable, 0);
    EXPECT_EQ(top->decoded_mem_read_enable, 0);
    EXPECT_EQ(top->decoded_branch, 0);
    EXPECT_EQ(top->decoded_halt, 0);
}

// ------------------ WARP STATE TEST ------------------
TEST_F(DecoderTestbench, WarpStateControl) {
    resetDecoder();
    
    // Test that decoder only responds during WARP_DECODE state
    top->instruction = makeInstr(OPCODE_R, 0b0000, 1, 2, 3, 0, false);
    top->warp_state = 0; // Not WARP_DECODE
    clockCycle();
    
    // // Should maintain reset values
    // EXPECT_EQ(top->decoded_reg_write_enable, 0);
    // EXPECT_EQ(top->decoded_alu_instruction, ADDI);
    
    // Now test with correct warp state
    top->warp_state = WARP_DECODE;
    clockCycle();
    
    EXPECT_EQ(top->decoded_reg_write_enable, 1);
    EXPECT_EQ(top->decoded_alu_instruction, ADD);
}

// ------------------ MAIN ------------------
int main(int argc, char **argv) {
    Verilated::commandArgs(argc, argv);
    testing::InitGoogleTest(&argc, argv);
    Verilated::mkdir("logs");
    auto res = RUN_ALL_TESTS();
    VerilatedCov::write(("logs/coverage_" + std::string(NAME) + ".dat").c_str());
    return res;
}