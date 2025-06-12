`default_nettype none
`timescale 1ns/1ns

`include "common.sv"
`include "gpu_defines.svh"

module gpu #(
    parameter int DATA_MEM_NUM_CHANNELS = 1,
    parameter int INSTRUCTION_MEM_NUM_CHANNELS = 1,
    parameter int NUM_CORES = 1,
    parameter int WARPS_PER_CORE = 1,
    parameter int THREADS_PER_WARP = 16
) (
    input wire clk,
    input wire reset,
    input logic [31:0] base_instr,
    input logic [31:0] base_data,
    input logic [31:0] num_blocks,
    input logic [31:0] warps_per_block,
    input wire execution_start,
    output wire execution_done,

    // Program Memory
    output wire [INSTRUCTION_MEM_NUM_CHANNELS-1:0] instruction_mem_read_valid,
    output instruction_memory_address_t instruction_mem_read_address [INSTRUCTION_MEM_NUM_CHANNELS],
    input wire [INSTRUCTION_MEM_NUM_CHANNELS-1:0] instruction_mem_read_ready,
    input instruction_t instruction_mem_read_data [INSTRUCTION_MEM_NUM_CHANNELS],

    // Data Memory
    output wire [DATA_MEM_NUM_CHANNELS-1:0] data_mem_read_valid,
    output data_memory_address_t data_mem_read_address [DATA_MEM_NUM_CHANNELS],
    input wire [DATA_MEM_NUM_CHANNELS-1:0] data_mem_read_ready,
    input data_memory_address_t data_mem_read_data [DATA_MEM_NUM_CHANNELS],
    output wire [DATA_MEM_NUM_CHANNELS-1:0] data_mem_write_valid,
    output data_memory_address_t data_mem_write_address [DATA_MEM_NUM_CHANNELS],
    output data_t data_mem_write_data [DATA_MEM_NUM_CHANNELS],
    input wire [DATA_MEM_NUM_CHANNELS-1:0] data_mem_write_ready
);

    kernel_config_t kernel_config_reg;
    logic start_execution;

    always @(posedge clk) begin
        if (reset) begin
            start_execution <= 1'b0;
            kernel_config_reg <= '0;
        end else begin
            kernel_config_reg.base_instructions_address <= base_instr;
            kernel_config_reg.base_data_address <= base_data;
            kernel_config_reg.num_blocks <= num_blocks;
            kernel_config_reg.num_warps_per_block <= warps_per_block;
            if (execution_start && !start_execution) begin
                start_execution <= 1'b1;
            end
        end
    end

    logic [NUM_CORES-1:0] core_done;
    logic [NUM_CORES-1:0] core_start;
    logic [NUM_CORES-1:0] core_reset;
    data_t core_block_id [NUM_CORES];

    dispatcher #(.NUM_CORES(NUM_CORES)) dispatcher_inst (
        .clk(clk), .reset(reset), .start(start_execution), .kernel_config(kernel_config_reg),
        .core_done(core_done), .core_start(core_start), .core_reset(core_reset),
        .core_block_id(core_block_id), .done(execution_done)
    );

    localparam int NUM_LSUS_PER_CORE = THREADS_PER_WARP + 1;

    logic [NUM_LSUS_PER_CORE-1:0] core_lsu_read_valid;
    data_memory_address_t core_lsu_read_address [NUM_LSUS_PER_CORE];
    logic [NUM_LSUS_PER_CORE-1:0] core_lsu_read_ready;
    data_t core_lsu_read_data [NUM_LSUS_PER_CORE];
    logic [NUM_LSUS_PER_CORE-1:0] core_lsu_write_valid;
    data_memory_address_t core_lsu_write_address [NUM_LSUS_PER_CORE];
    data_t core_lsu_write_data [NUM_LSUS_PER_CORE];
    logic [NUM_LSUS_PER_CORE-1:0] core_lsu_write_ready;

    localparam int SCALAR_LSU_IDX = THREADS_PER_WARP;
    
    // Arbiter for the 16 VECTOR LSUs (Thread-Local Memory)
    logic [$clog2(THREADS_PER_WARP)-1:0] grant_ptr = '0;
    logic [$clog2(THREADS_PER_WARP)-1:0] vector_granted_idx;
    logic vector_grant_valid;

    always_comb begin
        vector_grant_valid = 1'b0;
        vector_granted_idx = grant_ptr; 
        for (int i = 0; i < THREADS_PER_WARP; i++) begin
            int current_idx = (grant_ptr + i) % THREADS_PER_WARP;
            if (core_lsu_read_valid[current_idx] || core_lsu_write_valid[current_idx]) begin
                vector_granted_idx = current_idx;
                vector_grant_valid = 1'b1;
                break;
            end
        end
    end

    always @(posedge clk) begin
        if (reset) grant_ptr <= '0;
        else if (vector_grant_valid) grant_ptr <= vector_granted_idx + 1;
    end

    // Top-level arbiter: Scalar LSU (Global) gets priority
    logic scalar_request = core_lsu_read_valid[SCALAR_LSU_IDX] || core_lsu_write_valid[SCALAR_LSU_IDX];

    assign data_mem_read_valid[0]  = (scalar_request && core_lsu_read_valid[SCALAR_LSU_IDX]) || (!scalar_request && vector_grant_valid && core_lsu_read_valid[vector_granted_idx]);
    assign data_mem_write_valid[0] = (scalar_request && core_lsu_write_valid[SCALAR_LSU_IDX]) || (!scalar_request && vector_grant_valid && core_lsu_write_valid[vector_granted_idx]);

    // Address Translation Logic
    always_comb begin
        if (scalar_request) begin // Global Access
            data_mem_write_address[0] = core_lsu_write_address[SCALAR_LSU_IDX];
            data_mem_write_data[0]    = core_lsu_write_data[SCALAR_LSU_IDX];
            data_mem_read_address[0]  = core_lsu_read_address[SCALAR_LSU_IDX];
        end else if (vector_grant_valid) begin // Thread-Local Access
            data_memory_address_t local_addr = core_lsu_read_valid[vector_granted_idx] ? core_lsu_read_address[vector_granted_idx] : core_lsu_write_address[vector_granted_idx];
            data_memory_address_t physical_addr = THREAD_LOCAL_MEM_BASE_ADDR + (vector_granted_idx * THREAD_LOCAL_MEM_PARTITION_SIZE_WORDS) + local_addr;
            data_mem_write_address[0] = physical_addr;
            data_mem_write_data[0]    = core_lsu_write_data[vector_granted_idx];
            data_mem_read_address[0]  = physical_addr;
        end else begin
            data_mem_write_address[0] = '0; data_mem_write_data[0] = '0; data_mem_read_address[0] = '0;
        end
    end

    // Route ready/data signals back to the correct LSU
    for (genvar j = 0; j < NUM_LSUS_PER_CORE; j++) begin
        if (j == SCALAR_LSU_IDX) begin
            assign core_lsu_read_ready[j]  = data_mem_read_ready[0] && scalar_request;
            assign core_lsu_write_ready[j] = data_mem_write_ready[0] && scalar_request;
            assign core_lsu_read_data[j]   = (scalar_request) ? data_mem_read_data[0] : '0;
        end else begin
            assign core_lsu_read_ready[j]  = data_mem_read_ready[0]  && !scalar_request && vector_grant_valid && (j == vector_granted_idx);
            assign core_lsu_write_ready[j] = data_mem_write_ready[0] && !scalar_request && vector_grant_valid && (j == vector_granted_idx);
            assign core_lsu_read_data[j]   = (!scalar_request && vector_grant_valid && (j == vector_granted_idx)) ? data_mem_read_data[0] : '0;
        end
    end
    
    // Since NUM_CORES=1, we can simplify the connections.
    
    logic [WARPS_PER_CORE-1:0] fetcher_read_valid_core;
    instruction_memory_address_t fetcher_read_address_core [WARPS_PER_CORE];
    logic [WARPS_PER_CORE-1:0] fetcher_read_ready_core;
    instruction_t fetcher_read_data_core [WARPS_PER_CORE];

    // Instruction Memory Pass-through (connects core to top-level ports)
    assign instruction_mem_read_valid = fetcher_read_valid_core;
    assign instruction_mem_read_address = fetcher_read_address_core;
    assign fetcher_read_ready_core = instruction_mem_read_ready;
    assign fetcher_read_data_core = instruction_mem_read_data;

    compute_core #(
        .WARPS_PER_CORE(WARPS_PER_CORE),
        .THREADS_PER_WARP(THREADS_PER_WARP)
    ) core_instance (
        .clk(clk),
        .reset(core_reset[0]),
        .start(core_start[0]),
        .done(core_done[0]),
        .block_id(core_block_id[0]),
        .kernel_config(kernel_config_reg),

        .instruction_mem_read_valid(fetcher_read_valid_core),
        .instruction_mem_read_address(fetcher_read_address_core),
        .instruction_mem_read_ready(fetcher_read_ready_core),
        .instruction_mem_read_data(fetcher_read_data_core),

        .data_mem_read_valid(core_lsu_read_valid),
        .data_mem_read_address(core_lsu_read_address),
        .data_mem_read_ready(core_lsu_read_ready),
        .data_mem_read_data(core_lsu_read_data),
        .data_mem_write_valid(core_lsu_write_valid),
        .data_mem_write_address(core_lsu_write_address),
        .data_mem_write_data(core_lsu_write_data),
        .data_mem_write_ready(core_lsu_write_ready)
    );
    
endmodule