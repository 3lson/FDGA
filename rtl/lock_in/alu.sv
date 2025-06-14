`default_nettype none
`timescale 1ns/1ns

`include "common.sv"

module alu (
    input instruction_memory_address_t pc,
    input   alu_instruction_t instruction,
    input   data_t ALUop1,
    input   data_t ALUop2,
    input   data_t IMM,
    output  data_t Result
    // output  logic EQ
);

    // CHANGE: The sequential block is replaced with a combinational one.
    // The ALU output will now be calculated in the same cycle as its inputs change.
    always_comb begin
        case (instruction)
            // --- R-Type: Control Flow ---
            ADD: Result = ALUop1 + ALUop2; // ADD
            SUB: Result = ALUop1 - ALUop2; // SUB
            MUL: Result = ALUop1 * ALUop2; // MULTIPLY
            DIV: Result = ALUop1 / ALUop2; // DIVIDE
            ABS: Result = (ALUop1[31]) ? -ALUop1 : ALUop1;
            SLT: Result = (ALUop1 < ALUop2) ? 1:0; //LESS THAN
            SEQ: Result = (ALUop1 == ALUop2) ? 1:0; //EQUALS
            SNEZ: Result = (ALUop1 != 0) ? 1 : 0; //Set if Not Equals
            MIN: Result = (ALUop1 < ALUop2) ? ALUop1:ALUop2; //MINIMUM INSTRUCTION

            SLL: Result = ALUop1 << ALUop2; //SHIFT LEFT REGISTERS

            // --- I-Type: Control Flow ---
            ADDI: Result = ALUop1 + IMM;
            MULI: Result = ALUop1 * IMM; // MULTIPLY
            DIVI: Result = ALUop1 / IMM; // DIVIDE
            SLLI: Result = ALUop1 << IMM[4:0];
            SEQI: Result = (ALUop1 == IMM) ? 1 : 0;

            // --- C-Type: Control Flow ---
            BEQO: begin 
                Result = (ALUop1 == 32'd1) ? 32'd1 : 32'd0;
            end

            BEQZ: begin
                // The ALU's job is just to check the condition
                Result = (ALUop1 == 32'd0) ? 32'd1 : 32'd0;
            end
            JAL: begin
                // The result is the target address: PC + offset
                Result = pc + IMM;
            end
            default: Result = 32'b0;
        endcase
    end

endmodule
