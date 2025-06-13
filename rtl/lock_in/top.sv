`timescale 1ns / 1ps
`default_nettype none

`include "common.svh"

module top #(
    parameter int DATA_MEM_NUM_CHANNELS  = 8,
    parameter int INSTRUCTION_MEM_NUM_CHANNELS = 8,
    parameter int NUM_CORES = 1,
    parameter int WARPS_PER_CORE = 2,
    parameter int THREADS_PER_WARP = 32
)(
    input  wire clk,
    input  wire reset,

    input  wire execution_start,
    output wire execution_done,

    // Flattened kernel config
    input  wire [`INSTRUCTION_MEMORY_ADDRESS_WIDTH-1:0] base_instructions_address,
    input  wire [`DATA_MEMORY_ADDRESS_WIDTH-1:0] base_data_address,
    input  wire [`DATA_WIDTH-1:0] num_blocks,
    input  wire [`DATA_WIDTH-1:0] num_warps_per_block,

    // Program Memory
    output wire [INSTRUCTION_MEM_NUM_CHANNELS-1:0] instruction_mem_read_valid,
    output instruction_memory_address_t instruction_mem_read_address [INSTRUCTION_MEM_NUM_CHANNELS],
    input  wire [INSTRUCTION_MEM_NUM_CHANNELS-1:0] instruction_mem_read_ready,
    input  instruction_t instruction_mem_read_data [INSTRUCTION_MEM_NUM_CHANNELS],

    // Data Memory
    output wire [DATA_MEM_NUM_CHANNELS-1:0] data_mem_read_valid,
    output data_memory_address_t data_mem_read_address [DATA_MEM_NUM_CHANNELS],
    input  wire [DATA_MEM_NUM_CHANNELS-1:0] data_mem_read_ready,
    input  data_memory_address_t data_mem_read_data [DATA_MEM_NUM_CHANNELS],
    output wire [DATA_MEM_NUM_CHANNELS-1:0] data_mem_write_valid,
    output data_memory_address_t data_mem_write_address [DATA_MEM_NUM_CHANNELS],
    output data_t data_mem_write_data [DATA_MEM_NUM_CHANNELS],
    input  wire [DATA_MEM_NUM_CHANNELS-1:0] data_mem_write_ready
);

    // Bundle kernel_config_t struct
    kernel_config_t kernel_config;
    assign kernel_config.base_instructions_address = base_instructions_address;
    assign kernel_config.base_data_address         = base_data_address;
    assign kernel_config.num_blocks                = num_blocks;
    assign kernel_config.num_warps_per_block       = num_warps_per_block;

    // Instantiate GPU
    gpu #(
        .DATA_MEM_NUM_CHANNELS(DATA_MEM_NUM_CHANNELS),
        .INSTRUCTION_MEM_NUM_CHANNELS(INSTRUCTION_MEM_NUM_CHANNELS),
        .NUM_CORES(NUM_CORES),
        .WARPS_PER_CORE(WARPS_PER_CORE),
        .THREADS_PER_WARP(THREADS_PER_WARP)
    ) gpu_inst (
        .clk(clk),
        .reset(reset),
        .execution_start(execution_start),
        .execution_done(execution_done),
        .kernel_config(kernel_config),
        .instruction_mem_read_valid(instruction_mem_read_valid),
        .instruction_mem_read_address(instruction_mem_read_address),
        .instruction_mem_read_ready(instruction_mem_read_ready),
        .instruction_mem_read_data(instruction_mem_read_data),
        .data_mem_read_valid(data_mem_read_valid),
        .data_mem_read_address(data_mem_read_address),
        .data_mem_read_ready(data_mem_read_ready),
        .data_mem_read_data(data_mem_read_data),
        .data_mem_write_valid(data_mem_write_valid),
        .data_mem_write_address(data_mem_write_address),
        .data_mem_write_data(data_mem_write_data),
        .data_mem_write_ready(data_mem_write_ready)
    );

endmodule
