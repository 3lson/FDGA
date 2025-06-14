// common.sv
`ifndef COMMON_SV
`define COMMON_SV

// Global Macros
`define DATA_WIDTH 32
`define INSTRUCTION_WIDTH 32
`define DATA_MEMORY_ADDRESS_WIDTH 16
`define INSTRUCTION_MEMORY_ADDRESS_WIDTH 16

// Type Definitions
typedef logic [`DATA_WIDTH-1:0] data_t;
typedef logic [`INSTRUCTION_WIDTH-1:0] instruction_t;
typedef logic [`DATA_MEMORY_ADDRESS_WIDTH:0] data_memory_address_t;
typedef logic [`INSTRUCTION_MEMORY_ADDRESS_WIDTH-1:0] instruction_memory_address_t;

typedef struct packed {
    instruction_memory_address_t base_instructions_address;
    data_memory_address_t base_data_address; // This is where kernel function arguments are stored
    data_t num_blocks;
    data_t num_warps_per_block;
} kernel_config_t;

// RISC-V Definitions
`define OPCODE_WIDTH 7
`define FUNCT3_WIDTH 3
`define FUNCT4_WIDTH 4

// Vector-Scalar Instruction Opcodes (SX_SLTI and SX_SLT)
// SX_SLTI sets one bit of a scalar register based on thread's comparison result
`define OPCODE_SX_SLT   3'b101        // SX_SLT rd, rs1, rs2 <=> rd[id] = rs1 < rs2 ? 1 : 0

// Instruction Opcodes
// The entire opcode is 7 bits, the most significant bit decides whether the instruction is vector or scalar
`define OPCODE_R        3'b000          // Used by all R-type instructions (ADD, SUB, SLL, SLT, XOR, SRL, SRA)
`define OPCODE_I        3'b001          // Used by ALU I-type instructions (ADDI, SLTI, XORI, ORI, ANDI, SLLI, SRLI, SRAI)
`define OPCODE_UP       3'b011          // Used by LUI
`define OPCODE_M        3'b100          // Used by load instructions (LB, LH, LW)
`define OPCODE_F        3'b010          // Floating Point F-Type
`define OPCODE_J        3'b111

// Those instructions can only be used by scalar instructions
`define OPCODE_C        3'b111          // Control Flow C-Type

typedef logic [`OPCODE_WIDTH-1:0] opcode_t;
typedef logic [`FUNCT3_WIDTH-1:0] funct3_t;
typedef logic [`FUNCT4_WIDTH-1:0] funct4_t;
typedef logic [11:0] imm12_t;

// alu instructions enum
typedef enum logic [4:0] {
    // register instructions
    ADD, // 00000
    SUB, // 00001
    MUL, // 00010
    DIV, // 00011
    SLT, // 00100
    SLL, // 00101
    SEQ, // 00110
    SNEZ, // 00111
    MIN, // 01000
    ABS, // 01001

    ADDI, // 01010
    MULI, // 01011
    DIVI, // 01100
    SLLI, // 01101

    // F-type instructions
    FADD, // 01110
    FSUB, // 01111
    FMUL, // 10000
    FDIV, // 10001
    FSLT, // 10010
    FNEG, // 10011
    FEQ, // 10100
    FMIN, // 10101
    FABS, // 10110
    FCVT_W_S, // 10111
    FCVT_S_W, // 11000

    // compare instructions
    BEQZ, // 11001

    // jump instructions
    JAL, // 11010
    SEQI, // 11011
    BEQO, // 11100
    SYNC, // 11101
    END_SYNC // 11110
} alu_instruction_t;

// warp state enum
typedef enum logic [3:0] {
    WARP_IDLE,
    WARP_FETCH,
    WARP_DECODE,
    WARP_REQUEST,
    WARP_WAIT,
    WARP_EXECUTE,
    WARP_UPDATE,
    WARP_SYNC_WAIT,
    WARP_DONE
} warp_state_t;

// fetcher state enum
typedef enum logic [1:0] {
    FETCHER_IDLE,
    FETCHER_FETCHING,
    FETCHER_DONE
} fetcher_state_t;

// lsu state enum
typedef enum logic [1:0] {
    LSU_IDLE,
    LSU_REQUESTING,
    LSU_WAITING,
    LSU_DONE
} lsu_state_t;

// reg input mux
typedef enum logic [2:0] {
    ALU_OUT,
    LSU_OUT,
    IMMEDIATE,
    PC_PLUS_1,
    VECTOR_TO_SCALAR
} reg_input_mux_t;

function automatic data_t sign_extend_14(logic [13:0] imm14);
    data_t signed_imm14;
    if (imm14[13]) begin
        signed_imm14 = {{18{1'b1}}, imm14};
    end else begin
        signed_imm14 = {{18{1'b0}}, imm14};
    end
    return signed_imm14;
endfunction

function automatic data_t sign_extend_15(logic[14:0] imm15);
    data_t signed_imm15;
    if (imm15[14]) begin
        signed_imm15 = {{17{1'b1}}, imm15};
    end else begin
        signed_imm15 = {{17{1'b0}}, imm15};
    end
    return signed_imm15;
endfunction

function automatic data_t sign_extend_16(logic[15:0] imm16);
    data_t signed_imm16;
    if (imm16[15]) begin
        signed_imm16 = {{16{1'b1}}, imm16};
    end else begin
        signed_imm16 = {{16{1'b0}}, imm16};
    end
    return signed_imm16;
endfunction

// sign extend function for 21-bit immediate values
function automatic data_t sign_extend_26(logic[25:0] imm26);
    data_t signed_imm26;
    if (imm26[25]) begin
        signed_imm26 = {{6{1'b1}}, imm26};
    end else begin
        signed_imm26 = {{6{1'b0}}, imm26};
    end
    return signed_imm26;
endfunction

`endif // COMMON_SV
