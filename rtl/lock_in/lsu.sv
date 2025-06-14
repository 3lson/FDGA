`timescale 1ns/1ns

`include "common.svh"

// LOAD-STORE UNIT
// > Handles asynchronous memory load and store operations and waits for response
// > Each thread in each core has it's own LSU
// > LDR, STR instructions are executed here
module lsu (
    input wire clk,
    input wire reset,
    input wire enable, // If current block has less threads then block size, some LSUs will be inactive

    // State
    input warp_state_t warp_state,

    // Memory Control Signals
    input reg decoded_mem_read_enable,
    input reg decoded_mem_write_enable,

    // Registers
    input data_t rs1,
    input data_t rs2,
    input data_t imm,

    // Data Memory
    output logic mem_read_valid,
    output data_memory_address_t mem_read_address,
    input logic mem_read_ready,
    input data_t mem_read_data,
    output logic mem_write_valid,
    output data_memory_address_t mem_write_address,
    output data_t mem_write_data,
    input logic mem_write_ready,

    // LSU Outputs
    output lsu_state_t lsu_state,
    output data_t lsu_out
);

data_t offset_address;
assign offset_address = rs1 + imm;

// always_comb begin 
//     // $display("Enable: ", enable);
//     // $display("Mem read enable: ", decoded_mem_read_enable);
//     // $display("Mem write enable: ", decoded_mem_write_enable);
//     // $display("Rs1: ", rs1);
// end

always @(posedge clk) begin
    // $display("Write enable: ", decoded_mem_write_enable);
    // $display("Write enable: ", decoded_mem_write_enable);
    if (reset) begin
        lsu_state <= LSU_IDLE;
        lsu_out <= 0;
        mem_read_valid <= 0;
        mem_read_address <= 0;
        mem_write_valid <= 0;
        mem_write_address <= 0;
        mem_write_data <= 0;
    end else if (enable) begin
        // If memory read enable is triggered (LDR instruction)
        // $display("decoded_mem_read_enable: ", decoded_mem_read_enable);
        if (decoded_mem_read_enable) begin 
            // $display("In decoded_mem_read_enable");
            //$display("LSU_State: ", lsu_state);
            case (lsu_state)
                LSU_IDLE: begin
                    // Only read when warp_state = REQUEST
                    // $display("WARP REQ: ", warp_state == WARP_REQUEST);
                    if (warp_state == WARP_REQUEST) begin 
                        $display("going to request");
                        lsu_state <= LSU_REQUESTING;
                    end
                end
                LSU_REQUESTING: begin 
                    $display("LSU_REQUESTING");
                    mem_read_valid <= 1;
                    mem_read_address <= offset_address;
                    lsu_state <= LSU_WAITING;
                end
                LSU_WAITING: begin
                    // $display("we reach LSU waiting");
                    // $display("mem_read_ready: ", mem_read_ready);
                    if (mem_read_ready == 1) begin
                        $display("LSU: Reading %d from memory address %d", mem_read_data, offset_address);
                        mem_read_valid <= 0;
                        lsu_out <= mem_read_data;
                        lsu_state <= LSU_DONE;
                    end
                end
                LSU_DONE: begin 
                    $display("Warp update: ", warp_state == WARP_UPDATE);
                    // Reset when warp_state = UPDATE
                    if (warp_state == WARP_UPDATE) begin 
                        lsu_state <= LSU_IDLE;
                        $display("LSU_DONE");
                    end
                end
            endcase
        end

        // If memory write enable is triggered (STR instruction)
        if (decoded_mem_write_enable) begin 
            //$display("LSU State: ", lsu_state);
            case (lsu_state)
                LSU_IDLE: begin
                    // Only read when warp_state = REQUEST
                    if (warp_state == WARP_REQUEST) begin 
                        lsu_state <= LSU_REQUESTING;
                    end
                    $display("warp_state in lsu_idle", warp_state);
                end
                LSU_REQUESTING: begin 
                    $display("LSU: Writing %d to memory address %d", rs2, offset_address);
                    mem_write_valid <= 1;
                    $display("mem_write_valid: ",mem_write_valid);
                    mem_write_address <= offset_address;
                    mem_write_data <= rs2;
                    lsu_state <= LSU_WAITING;
                    $display("State Write LSU Request: ", lsu_state);
                end
                LSU_WAITING: begin
                    $display("mem_write_valid: ",mem_write_valid);
                    $display("mem_write_ready: ", mem_write_ready);
                    if (mem_write_ready) begin
                        mem_write_valid <= 0;
                        lsu_state <= LSU_DONE;
                    end
                    // $display("State Write LSU Waiting: ", lsu_state);
                end
                LSU_DONE: begin 
                    // Reset when warp_state = UPDATE
                    if (warp_state == WARP_UPDATE) begin 
                        lsu_state <= LSU_IDLE;
                        // set_reset = 1;
                        $display("LSU_DONE");
                    end
                end
            endcase
        end
    end
end
endmodule
