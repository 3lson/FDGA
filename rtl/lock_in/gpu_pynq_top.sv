// gpu_pynq_top.sv

`timescale 1ns/1ns

`include "common.svh"
`include "gpu_defines.svh" // Include your memory map defines

module gpu_pynq_top #(
    // GPU Parameters
    parameter int DATA_MEM_NUM_CHANNELS      = 8,
    parameter int INSTRUCTION_MEM_NUM_CHANNELS = 8,
    parameter int NUM_CORES                  = 1,
    parameter int WARPS_PER_CORE             = 1,
    parameter int THREADS_PER_WARP           = 16
) (
    input wire clk,
    input wire reset,
    input wire execution_start,
    input wire soft_reset,
    output wire execution_done,

    // Instruction BRAM Interface
    output logic                                instr_bram_en,
    output logic [`INSTRUCTION_MEMORY_ADDRESS_WIDTH-1:0] instr_bram_addr,
    input  wire [`INSTRUCTION_WIDTH-1:0]        instr_bram_dout,

    // Data BRAM Interface
    output logic                                data_bram_en,
    output logic [`DATA_MEMORY_ADDRESS_WIDTH-1:0] data_bram_addr,
    output logic [0:0]                          data_bram_we,
    output logic [`DATA_WIDTH-1:0]              data_bram_din,
    input  wire [`DATA_WIDTH-1:0]               data_bram_dout,
    input wire [4:0] debug_reg_addr,
    output wire [`DATA_WIDTH-1:0]    debug_reg_data
);

 
    localparam [31:0] BASE_INSTR_ADDR = 32'd0;
    localparam [31:0] BASE_DATA_ADDR  = 32'd0;
    localparam [31:0] NUM_BLOCKS      = 32'd1;
    localparam [31:0] WARPS_PER_BLOCK = 32'd1;

    
    // --- Instantiate GPU and Adapters ---

    // Instruction memory signals
    logic [INSTRUCTION_MEM_NUM_CHANNELS-1:0] imem_valid;
    instruction_memory_address_t imem_addr [INSTRUCTION_MEM_NUM_CHANNELS];
    logic [INSTRUCTION_MEM_NUM_CHANNELS-1:0] imem_ready;
    instruction_t imem_data [INSTRUCTION_MEM_NUM_CHANNELS];

    // Data memory signals
    logic [DATA_MEM_NUM_CHANNELS-1:0] dmem_r_valid;
    data_memory_address_t dmem_r_addr [DATA_MEM_NUM_CHANNELS];
    logic [DATA_MEM_NUM_CHANNELS-1:0] dmem_r_ready;
    data_t dmem_r_data [DATA_MEM_NUM_CHANNELS];
    logic [DATA_MEM_NUM_CHANNELS-1:0] dmem_w_valid;
    data_memory_address_t dmem_w_addr [DATA_MEM_NUM_CHANNELS];
    data_t dmem_w_data [DATA_MEM_NUM_CHANNELS];
    logic [DATA_MEM_NUM_CHANNELS-1:0] dmem_w_ready;


    gpu #(
        .DATA_MEM_NUM_CHANNELS(DATA_MEM_NUM_CHANNELS),
        .INSTRUCTION_MEM_NUM_CHANNELS(INSTRUCTION_MEM_NUM_CHANNELS),
        .NUM_CORES(NUM_CORES),
        .WARPS_PER_CORE(WARPS_PER_CORE),
        .THREADS_PER_WARP(THREADS_PER_WARP)
    ) gpu_inst (
        .clk(clk),
        .reset(reset || soft_reset),
        
        .base_instr(base_instr_reg),
        .base_data(base_data_reg),
        .num_blocks(num_blocks_reg),
        .warps_per_block(warps_per_block_reg),
        
        .execution_start(execution_start_pulse),
        .execution_done(execution_done),

        // Instruction Memory
        .instruction_mem_read_valid(imem_valid),
        .instruction_mem_read_address(imem_addr),
        .instruction_mem_read_ready(imem_ready),
        .instruction_mem_read_data(imem_data),

        // Data Memory
        .data_mem_read_valid(dmem_r_valid),
        .data_mem_read_address(dmem_r_addr),
        .data_mem_read_ready(dmem_r_ready),
        .data_mem_read_data(dmem_r_data),
        .data_mem_write_valid(dmem_w_valid),
        .data_mem_write_address(dmem_w_addr),
        .data_mem_write_data(dmem_w_data),
        .data_mem_write_ready(dmem_w_ready),
        .debug_reg_addr(debug_reg_addr),
        .debug_reg_data(debug_reg_data)
    );

    // Adapter for Instruction Memory (Read-Only)
    bram_adapter #(
        .DATA_WIDTH(`INSTRUCTION_WIDTH),
        .ADDR_WIDTH(`INSTRUCTION_MEMORY_ADDRESS_WIDTH),
        .NUM_CHANNELS(INSTRUCTION_MEM_NUM_CHANNELS),
        .WRITE_ENABLE(0)
    ) instr_adapter (
        .clk(clk),
        .reset(reset),
        .gpu_req_valid(imem_valid),
        .gpu_req_ready(imem_ready),
        .gpu_req_addr(imem_addr),
        .gpu_resp_data(imem_data),
        .gpu_w_valid('0), // Not used
        .gpu_w_data('{default:'0}), // Not used
        .gpu_w_ready(),   // Not used
        .bram_en(instr_bram_en),
        .bram_addr(instr_bram_addr),
        .bram_we(),       // Not used
        .bram_din(),      // Not used
        .bram_dout(instr_bram_dout)
    );

    // Adapter for Data Memory (Read/Write)
    // We combine read and write requests for the arbiter
    logic [DATA_MEM_NUM_CHANNELS-1:0] dmem_any_req_valid;
    data_memory_address_t dmem_any_addr [DATA_MEM_NUM_CHANNELS];
    
    for (genvar i = 0; i < DATA_MEM_NUM_CHANNELS; i++) begin
        assign dmem_any_req_valid[i] = dmem_r_valid[i] || dmem_w_valid[i];
        assign dmem_any_addr[i] = dmem_r_valid[i] ? dmem_r_addr[i] : dmem_w_addr[i];
    end

    bram_adapter #(
        .DATA_WIDTH(`DATA_WIDTH),
        .ADDR_WIDTH(`DATA_MEMORY_ADDRESS_WIDTH),
        .NUM_CHANNELS(DATA_MEM_NUM_CHANNELS),
        .WRITE_ENABLE(1)
    ) data_adapter (
        .clk(clk),
        .reset(reset),
        .gpu_req_valid(dmem_any_req_valid), // Combined read/write valid
        .gpu_req_ready(dmem_r_ready), // Connect ready to both read and write sides
        .gpu_req_addr(dmem_any_addr),
        .gpu_resp_data(dmem_r_data),
        .gpu_w_valid(dmem_w_valid),
        .gpu_w_data(dmem_w_data),
        .gpu_w_ready(dmem_w_ready),
        .bram_en(data_bram_en),
        .bram_addr(data_bram_addr),
        .bram_we(data_bram_we),
        .bram_din(data_bram_din),
        .bram_dout(data_bram_dout)
    );

endmodule
