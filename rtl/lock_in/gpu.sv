
`timescale 1ns/1ns

`include "common.svh"

module gpu #(
    parameter int DATA_MEM_NUM_CHANNELS /*verilator public*/ = 8,     // Number of concurrent channels for sending requests to data memory
    parameter int INSTRUCTION_MEM_NUM_CHANNELS /*verilator public*/ = 8,     // Number of concurrent channels for sending requests to data memory
    parameter int NUM_CORES /*verilator public*/ = 1,                 // Number of cores to include in this GPU
    parameter int WARPS_PER_CORE /*verilator public*/ = 1,            // Number of warps to in each core
    parameter int THREADS_PER_WARP /*verilator public*/ = 16          // Number of threads per warp (max 32)
) (
    // ... ports are unchanged ...
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
    output logic data_mem_read_valid,
    output data_memory_address_t data_mem_read_address [DATA_MEM_NUM_CHANNELS],
    input wire [DATA_MEM_NUM_CHANNELS-1:0] data_mem_read_ready,
    input data_t data_mem_read_data [DATA_MEM_NUM_CHANNELS],
    output wire [DATA_MEM_NUM_CHANNELS-1:0] data_mem_write_valid,
    output data_memory_address_t data_mem_write_address [DATA_MEM_NUM_CHANNELS],
    output data_t data_mem_write_data [DATA_MEM_NUM_CHANNELS],
    input logic data_mem_write_ready,
    input wire [4:0] debug_reg_addr,
    output wire [`DATA_WIDTH-1:0] debug_reg_data
);

kernel_config_t kernel_config_reg;

logic start_execution; // EDA: Unimportant hack used because of EDA tooling

// save kernel config on execution start to avoid losing data when the kernel is running
always @(posedge clk) begin
    if (reset) begin
        start_execution <= 1'b0;
        kernel_config_reg <= '0; // IMPORTANT: Reset the register!
    end else begin
        // Always latch the inputs into the register on every cycle.
        // This ensures kernel_config_reg is always up-to-date one cycle after the inputs change.
        kernel_config_reg.base_instructions_address <= base_instr;
        kernel_config_reg.base_data_address      <= base_data;
        kernel_config_reg.num_blocks             <= num_blocks;
        kernel_config_reg.num_warps_per_block    <= warps_per_block;

        if (execution_start && !start_execution) begin
            start_execution <= 1'b1;
            // Now, we display the REGISTERED values, which is what the dispatcher actually sees.
            $display("GPU: Kernel configuration (latched):");
            $display("     - Base instruction address: %h", kernel_config_reg.base_instructions_address);
            $display("     - Base data address: %h", kernel_config_reg.base_data_address);
            $display("     - Num %d blocks", kernel_config_reg.num_blocks);
            $display("     - Number of warps per block: %d", kernel_config_reg.num_warps_per_block);
        end
    end
    //$display("Checking dispatch:", kernel_config_reg.num_blocks);
end

logic [NUM_CORES-1:0] core_done;
logic [NUM_CORES-1:0] core_start;
logic [NUM_CORES-1:0] core_reset;
data_t core_block_id [NUM_CORES];

// LSU <> Data Memory Controller Channels
localparam int NUM_LSUS_PER_CORE = THREADS_PER_WARP + 1;
localparam int NUM_LSUS = NUM_CORES * NUM_LSUS_PER_CORE;
typedef logic [NUM_LSUS-1:0] lsu_size_t;
lsu_size_t lsu_read_valid;
lsu_size_t lsu_read_ready;
lsu_size_t lsu_write_valid;
lsu_size_t lsu_write_ready;
data_memory_address_t lsu_read_address [NUM_LSUS];
data_memory_address_t lsu_write_address [NUM_LSUS];
data_t lsu_read_data [NUM_LSUS];
data_t lsu_write_data [NUM_LSUS];

// Fetcher <> Program Memory Controller Channels
localparam NUM_FETCHERS = NUM_CORES * WARPS_PER_CORE;
typedef logic [NUM_FETCHERS-1:0] fetcher_size_t;
fetcher_size_t fetcher_read_valid;
instruction_memory_address_t fetcher_read_address [NUM_FETCHERS];
fetcher_size_t fetcher_read_ready;
instruction_t fetcher_read_data [NUM_FETCHERS];

dispatcher #(
    .NUM_CORES(NUM_CORES)
    ) dispatcher_inst (
    .clk(clk),
    .reset(reset),
    .start(start_execution),

    .kernel_config(kernel_config_reg),

    .core_done(core_done),
    .core_start(core_start),
    .core_reset(core_reset),
    .core_block_id(core_block_id),

    .done(execution_done)
);

// Data Memory Controller
mem_controller #(
    .DATA_WIDTH(`DATA_WIDTH),
    .ADDRESS_WIDTH(`DATA_MEMORY_ADDRESS_WIDTH),
    .NUM_CONSUMERS(NUM_LSUS),
    .NUM_CHANNELS(DATA_MEM_NUM_CHANNELS)
    ) data_memory_controller (
        .clk(clk),
        .reset(reset),

        .consumer_read_valid(lsu_read_valid),
        .consumer_read_address(lsu_read_address),
        .consumer_read_ready(lsu_read_ready),
        .consumer_read_data(lsu_read_data),

        .consumer_write_valid(lsu_write_valid),
        .consumer_write_address(lsu_write_address),
        .consumer_write_data(lsu_write_data),
        .consumer_write_ready(lsu_write_ready),

        .mem_read_valid(data_mem_read_valid),
        .mem_read_address(data_mem_read_address),
        .mem_read_ready(data_mem_read_ready),
        .mem_read_data(data_mem_read_data),

        .mem_write_valid(data_mem_write_valid),
        .mem_write_address(data_mem_write_address),
        .mem_write_data(data_mem_write_data),
        .mem_write_ready(data_mem_write_ready)
    );



// Instruction Memory Controller

// Disconnected write wires

logic [NUM_FETCHERS-1:0] d_consumer_write_valid;
instruction_memory_address_t d_consumer_write_address [NUM_FETCHERS];
instruction_t d_consumer_write_data [NUM_FETCHERS];
logic [NUM_FETCHERS-1:0] d_consumer_write_ready;

logic [INSTRUCTION_MEM_NUM_CHANNELS-1:0] d_mem_write_valid;
logic [`INSTRUCTION_MEMORY_ADDRESS_WIDTH-1:0] d_mem_write_address [INSTRUCTION_MEM_NUM_CHANNELS];
logic [`INSTRUCTION_WIDTH-1:0] d_mem_write_data [INSTRUCTION_MEM_NUM_CHANNELS];

mem_controller #(
    .DATA_WIDTH(`INSTRUCTION_WIDTH),
    .ADDRESS_WIDTH(`INSTRUCTION_MEMORY_ADDRESS_WIDTH),
    .NUM_CONSUMERS(NUM_FETCHERS),
    .NUM_CHANNELS(INSTRUCTION_MEM_NUM_CHANNELS),
    .WRITE_ENABLE(0)
) program_memory_controller (
    .clk(clk),
    .reset(reset),

    .consumer_read_valid(fetcher_read_valid),
    .consumer_read_address(fetcher_read_address),
    .consumer_read_ready(fetcher_read_ready),
    .consumer_read_data(fetcher_read_data),

    .consumer_write_valid(d_consumer_write_valid),
    .consumer_write_address(d_consumer_write_address),
    .consumer_write_data(d_consumer_write_data),
    .consumer_write_ready(d_consumer_write_ready),

    .mem_read_valid(instruction_mem_read_valid),
    .mem_read_address(instruction_mem_read_address),
    .mem_read_ready(instruction_mem_read_ready),
    .mem_read_data(instruction_mem_read_data),

    .mem_write_valid(d_mem_write_valid),
    .mem_write_address(d_mem_write_address),
    .mem_write_data(d_mem_write_data),
    .mem_write_ready(0)
);

initial begin
    $display("Hello, World!");
end

// Compute Cores
generate
    for (genvar i = 0; i < NUM_CORES; i = i + 1) begin : g_cores
        // EDA: We create separate signals here to pass to cores because of a requirement
        // by the OpenLane EDA flow (uses Verilog 2005) that prevents slicing the top-level signals
        logic [NUM_LSUS_PER_CORE-1:0] core_lsu_read_valid;
        data_memory_address_t core_lsu_read_address [NUM_LSUS_PER_CORE];
        logic [NUM_LSUS_PER_CORE-1:0] core_lsu_read_ready;
        data_t core_lsu_read_data [NUM_LSUS_PER_CORE];
        logic [NUM_LSUS_PER_CORE-1:0] core_lsu_write_valid;
        data_memory_address_t core_lsu_write_address [NUM_LSUS_PER_CORE];
        data_t core_lsu_write_data [NUM_LSUS_PER_CORE];
        logic [NUM_LSUS_PER_CORE-1:0] core_lsu_write_ready;

        // Pass through signals between LSUs and data memory controller
        for (genvar j = 0; j < NUM_LSUS_PER_CORE; j = j + 1) begin : g_lsu_connect
            localparam int lsu_index = i * NUM_LSUS_PER_CORE + j;

            // --- Core -> Memory Bus Connections ---
            assign lsu_read_valid[lsu_index]    = core_lsu_read_valid[j];
            assign lsu_read_address[lsu_index]  = core_lsu_read_address[j];

            assign lsu_write_valid[lsu_index]   = core_lsu_write_valid[j];
            assign lsu_write_address[lsu_index] = core_lsu_write_address[j];
            assign lsu_write_data[lsu_index]    = core_lsu_write_data[j];

            // --- Memory Bus -> Core Connections ---
            assign core_lsu_read_ready[j]       = lsu_read_ready[lsu_index];
            assign core_lsu_read_data[j]        = lsu_read_data[lsu_index];
            assign core_lsu_write_ready[j]      = lsu_write_ready[lsu_index];
        end

        localparam fetcher_index = i * WARPS_PER_CORE;

        // Compute Core
        compute_core #(
            .WARPS_PER_CORE(WARPS_PER_CORE),
            .THREADS_PER_WARP(THREADS_PER_WARP)
        ) core_instance (
            .clk(clk),
            .reset(core_reset[i]),

            .start(core_start[i]),
            .done(core_done[i]),

            .block_id(core_block_id[i]),
            .kernel_config(kernel_config_reg),

            .instruction_mem_read_valid(fetcher_read_valid[fetcher_index +: WARPS_PER_CORE]),
            .instruction_mem_read_address(fetcher_read_address[fetcher_index +: WARPS_PER_CORE]),
            .instruction_mem_read_ready(fetcher_read_ready[fetcher_index +: WARPS_PER_CORE]),
            .instruction_mem_read_data(fetcher_read_data[fetcher_index +: WARPS_PER_CORE]),

            .data_mem_read_valid(core_lsu_read_valid),
            .data_mem_read_address(core_lsu_read_address),
            .data_mem_read_ready(core_lsu_read_ready),
            .data_mem_read_data(core_lsu_read_data),
            .data_mem_write_valid(core_lsu_write_valid),
            .data_mem_write_address(core_lsu_write_address),
            .data_mem_write_data(core_lsu_write_data),
            .data_mem_write_ready(core_lsu_write_ready),
            .debug_reg_addr(debug_reg_addr),
            .debug_reg_data(debug_reg_data)
        );
    end
endgenerate


endmodule
