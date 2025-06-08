#pragma once
#include "verilated.h"
#include <array>
#include <algorithm>
#include <bitset>
#include <cstdint>
#include <string_view>
#include <format>
#include <stdexcept>
#include <string>

// Simple assertion helper for compile-time and runtime errors
inline void assert_or_err(bool condition, const char* error_message){
    if(!condition){
        throw std::runtime_error(error_message);
    }
}

namespace sim {

using IData = uint32_t;

// ======= Opcodes ========

enum class Opcode : IData {
    R_TYPE = 0b000,
    I_TYPE = 0b001,
    F_TYPE = 0b010,
    P_TYPE = 0b011,
    M_TYPE = 0b100,
    X_TYPE = 0b101,
    C_TYPE = 0b111
    // Custom opcodes (also only scalar)
    //HALT     =  0b1111111,         // Used by HALT
    //SX_SLT   =  0b1111110,         // Used by SX_SLT
    //SX_SLTI  =  0b1111101          // Used by SX_SLTI
};

// This function checks if the scalar flag at bit [28] is set.
// It applies to a fully-formed 32-bit instr
constexpr bool is_scalar(IData instruction_bits) {
    return (instruction_bits >> 28u) & 1u;
}

//A convenience function, which is just the opposite of is_scalar
constexpr bool is_vector(IData instruction_bits) {
    return !is_scalar(instruction_bits);
}

// This function extracts the 3-bit opcode from bits [31:29]
constexpr Opcode get_opcode(IData instruction_bits) {
    return static_cast<Opcode>((instruction_bits >> 29u) & 0b111);
}

// ====== Funct4 ======

enum class Funct4 : IData {
// R-type
    ADD             = 0b0000,
    SUB             = 0b0001,
    MUL             = 0b0010,
    DIV             = 0b0011,
    SLT             = 0b0100,
    SGT             = 0b0101,
    SEQ             = 0b0110,
    SNEZ            = 0b0111,
    MIN             = 0b1000,
    ABS             = 0b1001,
// I-type
    ADDI             = 0b0000,
    MULI             = 0b0010,
    DIVI             = 0b0011,
    SLLI             = 0b1010,
// M-type
    M_LW              = 0b0000,
    M_SW              = 0b0001,
    M_FLW             = 0b1000,
    M_FSW             = 0b1001,
// F-type
    FADD_S            = 0b0000,
    FSUB_S            = 0b0001,
    FMUL_S            = 0b0010,
    FDIV_S            = 0b0011,
    FLT_S            = 0b0100,
    FNEG_S            = 0b0101,
    FEQ_S            = 0b0110,
    FMIN_S            = 0b0111,
    FABS_S            = 0b1000,
    FCVT_W_S            = 0b1001,
    FCVT_S_W           = 0b1010,

};


// ====== Funct3 ======

enum class Funct3 : IData {
    JUMP = 0b000,
    BEQZ = 0b0001,
    CALL = 0b010,
    RET = 0b011,
    SYNC = 0b110,
    EXIT = 0b111,
};

// ====== Mnemonic =====

enum class MnemonicName {
    ADD, SUB, MUL, DIV, SLT, SGT, SEQ, SNEZ, MIN, ABS,
    ADDI, MULI, DIVI, SLLI,
    LW, SW, FLW, FSW,
    LUI,
    J, BEQZ, CALL, RET, SYNC, EXIT,
    FADD_S, FSUB_S, FMUL_S, FDIV_S, FLT_S, FNEG_S, FEQ_S, FMIN_S, FABS_S, FCVT_W_S, FCVT_S_W
};


// ====== Register Definition =====
// Represents a physical register. In Elson-V, we distinguish between
// integer (x) and float (f) registers. The 'type' char helps with this.
struct Register {
    IData num;
    char type; // 'x' for integer, 'f' for float

    [[nodiscard]] constexpr auto bits() const -> IData { return num; }
};

// ====== Instruction Builder =====
// This class is the workhorse for creating a 32-bit instruction word.
// Each method places a value into its correct bitfield location according
// to the Elson-V ISA specification.
struct InstructionBits {
    IData bits{};

    constexpr InstructionBits(IData b = 0) : bits(b) {}
    operator IData() const { return bits; }

    // --- Field Setters ---
    constexpr auto set_opcode(Opcode opcode) -> InstructionBits& {
        bits |= (IData)opcode << 29u;
        return *this;
    }
    // constexpr auto set_scalar_flag() -> InstructionBits& {
    //     bits |= 1u << 28u;
    //     return *this;
    // }
    constexpr auto set_rd(Register rd) -> InstructionBits& {
        assert_or_err(rd.num < 32, "Register number must be < 32");
        bits |= rd.bits() << 0u;
        return *this;
    }
    constexpr auto set_rs1(Register rs1) -> InstructionBits& {
        assert_or_err(rs1.num < 32, "Register number must be < 32");
        bits |= rs1.bits() << 5u;
        return *this;
    }
    constexpr auto set_rs2(Register rs2) -> InstructionBits& {
        assert_or_err(rs2.num < 32, "Register number must be < 32");
        bits |= rs2.bits() << 14u;
        return *this;
    }
    constexpr auto set_funct4(Funct4 funct4) -> InstructionBits& {
        bits |= (IData)funct4 << 10u;
        return *this;
    }
    constexpr auto set_funct3(Funct3 funct3) -> InstructionBits& {
        bits |= (IData)funct3 << 10u;
        return *this;
    }

    // --- Immediate Format Setters ---
    constexpr auto set_i_imm(IData imm) -> InstructionBits& {
        bits |= (imm & 0x3FFFu) << 14u; // 14 bits for I-Type
        return *this;
    }
    constexpr auto set_m_load_imm(IData imm) -> InstructionBits& {
        bits |= (imm & 0x7FFFu) << 14u; // 15 bits for M-Type Load
        return *this;
    }
    constexpr auto set_m_store_imm(IData imm) -> InstructionBits& {
        // For M-Type Store, the immediate is split.
        bits |= ((imm >> 5) & 0x3FFu) << 19u; // imm[14:5] -> instr[28:19]
        bits |= (imm & 0x1Fu) << 0u;         // imm[4:0]  -> instr[4:0]
        return *this;
    }
    constexpr auto set_p_lui_imm(IData imm) -> InstructionBits& {
        // For P-Type LUI, 20-bit immediate.
        bits |= (imm & 0xFFFFFu) << 9u;
        return *this;
    }
    constexpr auto set_c_jump_imm(IData imm) -> InstructionBits& {
        // For C-Type Jump, immediate is split.
        bits |= ((imm >> 12) & 0xFFFFu) << 13u; // imm[27:12] -> instr[28:13]
        bits |= ((imm >> 2) & 0x3FFu) << 0u;   // imm[11:2]  -> instr[9:0]
        return *this;
    }
    constexpr auto set_c_branch_imm(IData imm) -> InstructionBits& {
        // For C-Type Branch, immediate is split.
        bits |= ((imm >> 8) & 0x3FFu) << 19u;  // imm[17:8] -> instr[28:19]
        bits |= ((imm >> 7) & 0x1u) << 13u;    // imm[7]    -> instr[13]
        bits |= ((imm >> 2) & 0x1Fu) << 0u;    // imm[6:2]  -> instr[4:0]
        return *this;
    }
    // constexpr auto set_c_call_imm(IData imm) -> InstructionBits& {
    //     // For C-Type Call, immediate is placed.
    //     bits |= ((imm >> 2) & 0xFFFFu) << 13u; // imm[17:2] -> instr[28:13]
    //     return *this;
    // }
};

// High-level instruction constructors - this is the public API for tests.
namespace instructions {

#define R_TYPE_INSTR(name, funct4) \
    constexpr auto name(Register rd, Register rs1, Register rs2, bool is_scalar = false) -> InstructionBits { \
        auto instr = InstructionBits().set_opcode(Opcode::R_TYPE).set_funct4(Funct4::funct4).set_rd(rd).set_rs1(rs1).set_rs2(rs2); \
        return instr; \        
    }
R_TYPE_INSTR(add, ADD) R_TYPE_INSTR(sub, SUB) R_TYPE_INSTR(mul, MUL) R_TYPE_INSTR(div, DIV)
R_TYPE_INSTR(slt, SLT) R_TYPE_INSTR(sgt, SGT) R_TYPE_INSTR(seq, SEQ) R_TYPE_INSTR(snez, SNEZ)
R_TYPE_INSTR(min, MIN) R_TYPE_INSTR(abs, ABS)

#define I_TYPE_INSTR(name, funct4) \
    constexpr auto name(Register rd, Register rs1, IData imm, bool is_scalar = false) -> InstructionBits { \
        auto instr = InstructionBits().set_opcode(Opcode::I_TYPE).set_funct4(Funct4::funct4).set_rd(rd).set_rs1(rs1).set_i_imm(imm); \
        //if (is_scalar) instr.set_scalar_flag(); 
        return instr; \
    }
I_TYPE_INSTR(addi, ADDI) I_TYPE_INSTR(muli, MULI) I_TYPE_INSTR(divi, DIVI) I_TYPE_INSTR(slli, SLLI)

#define F_TYPE_INSTR(name, funct4) \
    constexpr auto name(Register rd, Register rs1, Register rs2, bool is_scalar = false) -> InstructionBits { \
        auto instr = InstructionBits().set_opcode(Opcode::F_TYPE).set_funct4(Funct4::funct4).set_rd(rd).set_rs1(rs1).set_rs2(rs2); \
        //if (is_scalar) instr.set_scalar_flag(); 
        return instr; \
    }
F_TYPE_INSTR(fadd_s, FADD_S) F_TYPE_INSTR(fsub_s, FSUB_S) F_TYPE_INSTR(fmul_s, FMUL_S) F_TYPE_INSTR(fdiv_s, FDIV_S)
F_TYPE_INSTR(flt_s,  FLT_S) F_TYPE_INSTR(fneg_s, FNEG_S) F_TYPE_INSTR(feq_s, FEQ_S) F_TYPE_INSTR(fmin_s, FMIN_S)
F_TYPE_INSTR(fabs_s, FABS_S) F_TYPE_INSTR(fcvt_w_s, FCVT_W_S) F_TYPE_INSTR(fcvt_s_w, FCVT_S_W)

// M-Type (Memory) constructors
constexpr auto lw(Register rd, Register rs1, IData imm, bool is_scalar=false) -> InstructionBits { 
    auto i = InstructionBits().set_opcode(Opcode::M_TYPE).set_funct4(Funct4::M_LW).set_rd(rd).set_rs1(rs1).set_m_load_imm(imm);
    //if(is_scalar) i.set_scalar_flag(); 
    return i;
}
constexpr auto sw(Register rs2, Register rs1, IData imm, bool is_scalar=false) -> InstructionBits {
    auto i = InstructionBits().set_opcode(Opcode::M_TYPE).set_funct4(Funct4::M_SW).set_rs2(rs2).set_rs1(rs1).set_m_store_imm(imm);
    //if(is_scalar) i.set_scalar_flag(); 
    return i;
}
constexpr auto flw(Register rd, Register rs1, IData imm, bool is_scalar=false) -> InstructionBits {
    auto i = InstructionBits().set_opcode(Opcode::M_TYPE).set_funct4(Funct4::M_FLW).set_rd(rd).set_rs1(rs1).set_m_load_imm(imm);
    //if(is_scalar) i.set_scalar_flag(); 
    return i;
}
constexpr auto fsw(Register rs2, Register rs1, IData imm, bool is_scalar=false) -> InstructionBits {
    auto i = InstructionBits().set_opcode(Opcode::M_TYPE).set_funct4(Funct4::M_FSW).set_rs2(rs2).set_rs1(rs1).set_m_store_imm(imm);
    //if(is_scalar) i.set_scalar_flag(); 
    return i;
}

// P-Type constructor
constexpr auto lui(Register rd, IData upimm, bool is_scalar=false) -> InstructionBits {
    auto i = InstructionBits().set_opcode(Opcode::P_TYPE).set_rd(rd).set_p_lui_imm(upimm);
    //if(is_scalar) i.set_scalar_flag(); 
    return i;
}

// C-Type (Control) constructors - these are always scalar
constexpr auto j(IData imm) -> InstructionBits { return InstructionBits().set_opcode(Opcode::C_TYPE).set_funct3(Funct3::JUMP).set_c_jump_imm(imm).set_scalar_flag(); }
constexpr auto beqz(Register rs1, IData imm) -> InstructionBits { return InstructionBits().set_opcode(Opcode::C_TYPE).set_funct3(Funct3::BEQZ).set_rs1(rs1).set_c_branch_imm(imm).set_scalar_flag(); }
constexpr auto call(Register rd, Register rs1, IData imm) -> InstructionBits { return InstructionBits().set_opcode(Opcode::C_TYPE).set_funct3(Funct3::CALL).set_rd(rd).set_rs1(rs1).set_c_call_imm(imm).set_scalar_flag(); }
constexpr auto ret() -> InstructionBits { return call({0, 'x'}, {1, 'x'}, 0); }
constexpr auto sync() -> InstructionBits { return InstructionBits().set_opcode(Opcode::C_TYPE).set_funct3(Funct3::SYNC).set_scalar_flag(); }
constexpr auto exit() -> InstructionBits { return InstructionBits().set_opcode(Opcode::C_TYPE).set_funct3(Funct3::EXIT).set_scalar_flag(); }

} // namespace instructions

// Suffixes for creating register objects easily in tests, e.g., 5_x or 10_f
inline sim::Register operator ""_x(unsigned long long reg) { return sim::Register{.num = (IData)reg, .type = 'x'}; }
inline sim::Register operator ""_f(unsigned long long reg) { return sim::Register{.num = (IData)reg, .type = 'f'}; }

} // namespace sim