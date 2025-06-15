// This wrapper is added solely for module testbenching purposes because fetcher contains structs not supported in Verilator


`timescale 1ns / 1ns
`include "common.svh"

module fetcher_wrapper (
    input  logic clk,
    input  logic reset,

    input  warp_state_t warp_state,
    input  instruction_memory_address_t pc,

    input  logic instruction_mem_read_ready,
    input  instruction_t instruction_mem_read_data,
    output logic instruction_mem_read_valid,
    output instruction_memory_address_t instruction_mem_read_address,

    output fetcher_state_t fetcher_state,
    output instruction_t instruction
);

    fetcher u_fetcher (
        .clk(clk),
        .reset(reset),
        .warp_state(warp_state),
        .pc(pc),
        .instruction_mem_read_ready(instruction_mem_read_ready),
        .instruction_mem_read_data(instruction_mem_read_data),
        .instruction_mem_read_valid(instruction_mem_read_valid),
        .instruction_mem_read_address(instruction_mem_read_address),
        .fetcher_state(fetcher_state),
        .instruction(instruction)
    );

endmodule
