`timescale 1ns/1ns

`include "common.svh"

module scalar_reg_file #(
    parameter int DATA_WIDTH = `DATA_WIDTH
) (
    input   wire                            clk,
    input   wire                            reset,
    input   wire                            enable, // Warp enable signal

    output  data_t                          warp_execution_mask,

    input   warp_state_t                    warp_state,

    // Decoded instruction fields
    input   logic                           decoded_reg_write_enable,           // Write enable for the register file
    input   reg_input_mux_t                 decoded_reg_input_mux,    // Determines the source of data to write
    input   data_t                          decoded_immediate,                 // Immediate value for constant writes
    input   logic [4:0]                     decoded_rd_address,           // Destination register index
    input   logic [4:0]                     decoded_rs1_address,          // Source register 1 index
    input   logic [4:0]                     decoded_rs2_address,          // Source register 2 index

    input   data_t                          alu_out,
    input   data_t                          lsu_out,
    input   instruction_memory_address_t    pc,
    input   data_t                          vector_to_scalar_data,

    output  data_t                          rs1,
    output  data_t                          rs2
);

/* verilator lint_off UNUSED */
// Special-purpose register indices
localparam int ZERO_REG = 0;
localparam int EXECUTION_MASK_REG = 31;
/* verilator lint_on UNUSED */

data_t registers [32];

always_comb begin
    warp_execution_mask = registers[EXECUTION_MASK_REG];
    if (reset) begin
        rs1 = 32'b0;
        rs2 = 32'b0;
    end else begin
        rs1 = (decoded_rs1_address == ZERO_REG) ? {DATA_WIDTH{1'b0}} : registers[decoded_rs1_address];
        rs2 = (decoded_rs2_address == ZERO_REG) ? {DATA_WIDTH{1'b0}} : registers[decoded_rs2_address];
    end
end
// always_comb begin
//     $display("RS1, RS2 from scalar_reg ", rs1, rs2);
// end

// Register file: each warp has its own set of 32 registers

// always_comb begin
//     if (enable) begin
//         $display("Asynchronous read: ", rs1, rs2);
//         rs1 <= registers[decoded_rs1_address];
//         rs2 <= registers[decoded_rs2_address];
//     end
// end

always @(posedge clk) begin
    // $display("Enable: ", warp_state == WARP_REQUEST);
    // $display("Reset: ", reset);
    if (reset) begin
        registers[0] <= {DATA_WIDTH{1'b0}};
        registers[1] <= {DATA_WIDTH{1'b1}};
        for (int i = 2; i < 32; i++) begin
            registers[i] <= {DATA_WIDTH{1'b0}};
        end
        registers[EXECUTION_MASK_REG] <= {DATA_WIDTH{1'b1}};
    end else if (enable) begin
        // if (warp_state == WARP_REQUEST) begin
        //     $display("Asynchronous read: ", rs1, rs2);
        //     rs1 = registers[decoded_rs1_address];
        //     rs2 = registers[decoded_rs2_address];
        // end

        if (warp_state == WARP_UPDATE) begin
            if (decoded_reg_write_enable && decoded_rd_address > 0) begin
                $display("Scalar Reg File: Writing to register %d", decoded_rd_address);
                case (decoded_reg_input_mux)
                    ALU_OUT:    registers[decoded_rd_address] <= alu_out;
                    LSU_OUT:    registers[decoded_rd_address] <= lsu_out;
                    IMMEDIATE:  registers[decoded_rd_address] <= decoded_immediate;
                    PC_PLUS_1:  registers[decoded_rd_address] <= pc + 1;
                    VECTOR_TO_SCALAR: begin
                        $display("Scalar Reg File: Writing vector_to_scalar_data to register %d", decoded_rd_address);
                        registers[decoded_rd_address] <= vector_to_scalar_data;
                    end
                    default: $error("Invalid decoded_reg_input_mux value");
                endcase
            end
        end
    end
end

endmodule
