`default_nettype none
`timescale 1ns/1ns

`include "common.sv"
`include "gpu_defines.svh" // Make sure you have this for constants like THREAD_LOCAL_MEM_BASE_ADDR

module gpu #(
    parameter int NUM_CORES /*verilator public*/ = 1,                 // Number of cores to include in this GPU
    parameter int WARPS_PER_CORE /*verilator public*/ = 4,            // Match the compute_core parameter
    parameter int THREADS_PER_WARP /*verilator public*/ = 16          // Match the compute_core parameter
) (
    input wire clk,
    input wire reset,
    input logic [31:0] base_instr,
    input logic [31:0] base_data,
    input logic [31:0] num_blocks,
    input logic [31:0] warps_per_block,

    input wire execution_start,
    output wire execution_done,

    // Instruction Memory (Single Port ROM BRAM Interface)
    output logic                                 imem_en,
    output instruction_memory_address_t          imem_addr,
    input  instruction_t                         imem_dout,
    
    // Data Memory (True Dual Port BRAM Interface)
    // Port A: Scalar/Global Memory Access
    output logic                                 dmem_a_en,
    output logic [`DATA_MEMORY_WE_WIDTH-1:0]      dmem_a_we, // Use a write-enable per byte
    output data_memory_address_t                 dmem_a_addr,
    output data_t                                dmem_a_din,
    input  data_t                                dmem_a_dout,

    // Port B: Vector/Thread-Local Memory Access
    output logic                                 dmem_b_en,
    output logic [`DATA_MEMORY_WE_WIDTH-1:0]      dmem_b_we,
    output data_memory_address_t                 dmem_b_addr,
    output data_t                                dmem_b_din,
    input  data_t                                dmem_b_dout
);

// --- Kernel Configuration & Dispatcher ---
kernel_config_t kernel_config_reg;
logic start_execution;

always @(posedge clk) begin
    if (reset) begin
        start_execution <= 1'b0;
        kernel_config_reg <= '0;
    end else begin
        kernel_config_reg.base_instructions_address <= base_instr;
        kernel_config_reg.base_data_address      <= base_data;
        kernel_config_reg.num_blocks             <= num_blocks;
        kernel_config_reg.num_warps_per_block    <= warps_per_block;

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


// --- Core and Memory Interface Signal Declarations ---
// These are the flattened buses that connect the cores to the arbiters.

// LSU <> Data Memory Signals
localparam int NUM_LSUS_PER_CORE = THREADS_PER_WARP + 1; // 16 vector + 1 scalar
localparam int NUM_LSUS = NUM_CORES * NUM_LSUS_PER_CORE;
localparam int SCALAR_LSU_OFFSET = THREADS_PER_WARP; // The index of the scalar LSU within a core's ports

logic [NUM_LSUS-1:0] lsu_read_valid;
logic [NUM_LSUS-1:0] lsu_write_valid;
data_memory_address_t lsu_read_address [NUM_LSUS];
data_memory_address_t lsu_write_address [NUM_LSUS];
data_t lsu_write_data [NUM_LSUS];
logic [NUM_LSUS-1:0] lsu_read_ready;  // Driven by arbiters
logic [NUM_LSUS-1:0] lsu_write_ready; // Driven by arbiters
data_t lsu_read_data [NUM_LSUS];   // Driven by BRAM output

// Fetcher <> Instruction Memory Signals
localparam NUM_FETCHERS = NUM_CORES * WARPS_PER_CORE;
logic [NUM_FETCHERS-1:0] fetcher_read_valid;
instruction_memory_address_t fetcher_read_address [NUM_FETCHERS];
logic [NUM_FETCHERS-1:0] fetcher_read_ready; // Driven by arbiter
instruction_t fetcher_read_data [NUM_FETCHERS]; // Driven by BRAM output


// --- Data Memory Arbitration & Connection ---
// This arbiter is designed for a SINGLE CORE (NUM_CORES=1).
// It maps the scalar LSU to Port A and arbitrates the vector LSUs to Port B.
// To support NUM_CORES > 1, this arbiter would need to be hierarchical.

// Port A (Scalar) Logic - Direct Connection from core 0's scalar LSU
localparam int SCALAR_LSU_IDX = SCALAR_LSU_OFFSET; // For core 0
assign lsu_read_ready[SCALAR_LSU_IDX]  = dmem_a_en; // Ready when BRAM port is used for this request
assign lsu_write_ready[SCALAR_LSU_IDX] = dmem_a_en;

assign dmem_a_en = lsu_read_valid[SCALAR_LSU_IDX] || lsu_write_valid[SCALAR_LSU_IDX];
assign dmem_a_we = {`DATA_MEMORY_WE_WIDTH{lsu_write_valid[SCALAR_LSU_IDX]}};
assign dmem_a_addr = lsu_write_valid[SCALAR_LSU_IDX] ? lsu_write_address[SCALAR_LSU_IDX] : lsu_read_address[SCALAR_LSU_IDX];
assign dmem_a_din  = lsu_write_data[SCALAR_LSU_IDX];

reg dmem_a_dout_reg;
always @(posedge clk) dmem_a_dout_reg <= dmem_a_dout;
assign lsu_read_data[SCALAR_LSU_IDX] = dmem_a_dout_reg;


// Port B (Vector) Logic - Round-Robin Arbiter for one core's vector LSUs (indices 0 to THREADS_PER_WARP-1)
logic [$clog2(THREADS_PER_WARP)-1:0] grant_ptr_vec = '0;
logic [$clog2(THREADS_PER_WARP)-1:0] granted_idx_vec;
logic grant_valid_vec;

always_comb begin
    grant_valid_vec = 1'b0;
    granted_idx_vec = grant_ptr_vec;
    for (int i = 0; i < THREADS_PER_WARP; i = i + 1) begin
        int current_idx = (grant_ptr_vec + i) % THREADS_PER_WARP;
        if (lsu_read_valid[current_idx] || lsu_write_valid[current_idx]) begin
            granted_idx_vec = current_idx;
            grant_valid_vec = 1'b1;
            break;
        end
    end
end

always @(posedge clk) begin
    if (reset) grant_ptr_vec <= '0;
    else if (grant_valid_vec) grant_ptr_vec <= granted_idx_vec + 1;
end

assign dmem_b_en = grant_valid_vec;
assign dmem_b_we = {`DATA_MEMORY_WE_WIDTH{lsu_write_valid[granted_idx_vec]}};
assign dmem_b_din = lsu_write_data[granted_idx_vec];

data_memory_address_t vector_local_addr = lsu_write_valid[granted_idx_vec] ? lsu_write_address[granted_idx_vec] : lsu_read_address[granted_idx_vec];
assign dmem_b_addr = THREAD_LOCAL_MEM_BASE_ADDR + (granted_idx_vec * THREAD_LOCAL_MEM_PARTITION_SIZE_WORDS) + vector_local_addr;

reg dmem_b_dout_reg;
always @(posedge clk) dmem_b_dout_reg <= dmem_b_dout;

for (genvar j = 0; j < THREADS_PER_WARP; j=j+1) begin
    assign lsu_read_ready[j]  = grant_valid_vec && (j == granted_idx_vec);
    assign lsu_write_ready[j] = grant_valid_vec && (j == granted_idx_vec);
    assign lsu_read_data[j] = (grant_valid_vec && (j == granted_idx_vec)) ? dmem_b_dout_reg : '0;
end


// --- Instruction Memory Arbitration & Connection ---
// Simple Round-Robin Arbiter for all fetcher requests from all cores.
logic [$clog2(NUM_FETCHERS)-1:0] grant_ptr_inst = '0;
logic [$clog2(NUM_FETCHERS)-1:0] granted_idx_inst;
logic grant_valid_inst;

always_comb begin
    grant_valid_inst = 1'b0;
    granted_idx_inst = grant_ptr_inst;
    for (int i = 0; i < NUM_FETCHERS; i = i + 1) begin
        int current_idx = (grant_ptr_inst + i) % NUM_FETCHERS;
        if (fetcher_read_valid[current_idx]) begin
            granted_idx_inst = current_idx;
            grant_valid_inst = 1'b1;
            break;
        end
    end
end

always @(posedge clk) begin
    if (reset) grant_ptr_inst <= '0;
    else if (grant_valid_inst) grant_ptr_inst <= granted_idx_inst + 1;
end

assign imem_en = grant_valid_inst;
assign imem_addr = fetcher_read_address[granted_idx_inst];

reg imem_dout_reg;
always @(posedge clk) imem_dout_reg <= imem_dout;

for (genvar k = 0; k < NUM_FETCHERS; k=k+1) begin
    assign fetcher_read_ready[k] = grant_valid_inst && (k == granted_idx_inst);
    assign fetcher_read_data[k] = (grant_valid_inst && (k == granted_idx_inst)) ? imem_dout_reg : '0;
end


// --- Compute Core Instantiation ---
// This block instantiates the compute cores and connects their memory request
// ports to the flattened buses that are handled by the arbiters above.
generate
    for (genvar i = 0; i < NUM_CORES; i = i + 1) begin : g_cores
        
        // --- Define the base index for this core's signals in the flat arrays ---
        localparam lsu_base_idx = i * NUM_LSUS_PER_CORE;
        localparam fetcher_base_idx = i * WARPS_PER_CORE;

        // --- Instantiate the actual compute core ---
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

            // --- Instruction Memory Connections ---
            // Connect the core's array of fetcher requests to its slice of the flat bus
            .instruction_mem_read_valid(fetcher_read_valid[fetcher_base_idx +: WARPS_PER_CORE]),
            .instruction_mem_read_address(fetcher_read_address[fetcher_base_idx +: WARPS_PER_CORE]),
            // Connect the arbiter's grant/data signals back to the core's slice
            .instruction_mem_read_ready(fetcher_read_ready[fetcher_base_idx +: WARPS_PER_CORE]),
            .instruction_mem_read_data(fetcher_read_data[fetcher_base_idx +: WARPS_PER_CORE]),

            // --- Data Memory Connections ---
            // Connect the core's array of LSU requests to its slice of the flat bus
            .data_mem_read_valid(lsu_read_valid[lsu_base_idx +: NUM_LSUS_PER_CORE]),
            .data_mem_read_address(lsu_read_address[lsu_base_idx +: NUM_LSUS_PER_CORE]),
            .data_mem_write_valid(lsu_write_valid[lsu_base_idx +: NUM_LSUS_PER_CORE]),
            .data_mem_write_address(lsu_write_address[lsu_base_idx +: NUM_LSUS_PER_CORE]),
            .data_mem_write_data(lsu_write_data[lsu_base_idx +: NUM_LSUS_PER_CORE]),
            // Connect the arbiter's grant/data signals back to the core's slice
            .data_mem_read_ready(lsu_read_ready[lsu_base_idx +: NUM_LSUS_PER_CORE]),
            .data_mem_read_data(lsu_read_data[lsu_base_idx +: NUM_LSUS_PER_CORE]),
            .data_mem_write_ready(lsu_write_ready[lsu_base_idx +: NUM_LSUS_PER_CORE])
        );
    end
endgenerate

endmodule
