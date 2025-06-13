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
    // ... ports are unchanged ...
    input wire clk,
    input wire reset,
    input logic [31:0] base_instr,
    input logic [31:0] base_data,
    input logic [31:0] num_blocks,
    input logic [31:0] warps_per_block,
    input wire execution_start,
    output wire execution_done,
    output wire [INSTRUCTION_MEM_NUM_CHANNELS-1:0] instruction_mem_read_valid,
    output instruction_memory_address_t instruction_mem_read_address [INSTRUCTION_MEM_NUM_CHANNELS],
    input wire [INSTRUCTION_MEM_NUM_CHANNELS-1:0] instruction_mem_read_ready,
    input instruction_t instruction_mem_read_data [INSTRUCTION_MEM_NUM_CHANNELS],
    output logic data_mem_read_valid,
    output data_memory_address_t data_mem_read_address [DATA_MEM_NUM_CHANNELS],
    input logic data_mem_read_ready,
    input data_t data_mem_read_data [DATA_MEM_NUM_CHANNELS],
    output logic data_mem_write_valid,
    output data_memory_address_t data_mem_write_address [DATA_MEM_NUM_CHANNELS],
    output data_t data_mem_write_data [DATA_MEM_NUM_CHANNELS],
    input logic data_mem_write_ready
);

    // --- Dispatcher and Core management signals (unchanged) ---
    kernel_config_t kernel_config_reg;
    logic start_execution;
    logic [NUM_CORES-1:0] core_done;
    logic [NUM_CORES-1:0] core_start;
    logic [NUM_CORES-1:0] core_reset;
    data_t core_block_id [NUM_CORES];
    logic core_start_mcu_transaction;
    logic mcu_busy;

    always_ff @(posedge clk or posedge reset) begin
        if (reset) begin
            start_execution <= 1'b0;
            kernel_config_reg <= '0;
        end else begin
            if (execution_start && !start_execution) begin
                start_execution <= 1'b1;
            end
            kernel_config_reg.base_instructions_address <= base_instr;
            kernel_config_reg.base_data_address <= base_data;
            kernel_config_reg.num_blocks <= num_blocks;
            kernel_config_reg.num_warps_per_block <= warps_per_block;
        end
    end

    dispatcher #(.NUM_CORES(NUM_CORES)) dispatcher_inst (
        .clk(clk), .reset(reset), .start(start_execution), .kernel_config(kernel_config_reg),
        .core_done(core_done), .core_start(core_start), .core_reset(core_reset),
        .core_block_id(core_block_id), .done(execution_done)
    );

    // --- Core <-> MCU Interface Signals ---
    localparam int NUM_LSUS_PER_CORE = THREADS_PER_WARP + 1;
    logic [NUM_LSUS_PER_CORE-1:0] lsu_read_valid;
    data_memory_address_t         lsu_read_address [NUM_LSUS_PER_CORE];
    logic [NUM_LSUS_PER_CORE-1:0] lsu_read_ready;
    data_t                        lsu_read_data [NUM_LSUS_PER_CORE];
    logic [NUM_LSUS_PER_CORE-1:0] lsu_write_valid;
    data_memory_address_t         lsu_write_address [NUM_LSUS_PER_CORE];
    data_t                        lsu_write_data [NUM_LSUS_PER_CORE];
    logic [NUM_LSUS_PER_CORE-1:0] lsu_write_ready;
    
    // --- AXI Bridge Wires (unchanged) ---
    logic [31:0] m_axi_awaddr, m_axi_araddr, m_axi_wdata, m_axi_rdata;
    logic m_axi_awvalid, m_axi_wvalid, m_axi_arvalid;
    logic m_axi_bvalid_to_mcu, m_axi_rvalid_to_mcu;

    // --- FINAL FIX: Explicit port connections ---
    mcu #(
        .THREADS_PER_WARP(THREADS_PER_WARP)
    ) mcu_inst (
        .clk(clk),
        .reset(reset),
        .start_mcu_transaction(core_start_mcu_transaction),
        .mcu_is_busy(mcu_busy),

        // Connect the consumer array ports to the LSU wires
        .consumer_read_valid     (lsu_read_valid),
        .consumer_read_address   (lsu_read_address),
        .consumer_read_ready     (lsu_read_ready),
        .consumer_read_data      (lsu_read_data),
        .consumer_write_valid    (lsu_write_valid),
        .consumer_write_address  (lsu_write_address),
        .consumer_write_data     (lsu_write_data),
        .consumer_write_ready    (lsu_write_ready),

        // AXI Master Data Interface (unchanged)
        .m_axi_awaddr(m_axi_awaddr), .m_axi_awvalid(m_axi_awvalid), .m_axi_awready(data_mem_write_ready),
        .m_axi_wdata(m_axi_wdata), .m_axi_wvalid(m_axi_wvalid), .m_axi_wready(data_mem_write_ready),
        .m_axi_bvalid(m_axi_bvalid_to_mcu), .m_axi_araddr(m_axi_araddr), .m_axi_arvalid(m_axi_arvalid),
        .m_axi_arready(data_mem_read_ready), .m_axi_rdata(m_axi_rdata), .m_axi_rvalid(m_axi_rvalid_to_mcu)
    );

    // Simplified connection logic from MCU to top-level memory ports (unchanged)
    assign data_mem_write_address[0] = m_axi_awaddr;
    assign data_mem_write_data[0]    = m_axi_wdata;
    assign data_mem_write_valid      = m_axi_awvalid | m_axi_wvalid;
    assign m_axi_bvalid_to_mcu       = data_mem_write_ready; 
    assign data_mem_read_address[0]  = m_axi_araddr;
    assign m_axi_rdata               = data_mem_read_data[0];
    assign data_mem_read_valid       = m_axi_arvalid;
    assign m_axi_rvalid_to_mcu       = data_mem_read_ready;

    // --- Core Instantiation ---
    logic [WARPS_PER_CORE-1:0] fetcher_read_valid_core;
    instruction_memory_address_t fetcher_read_address_core [WARPS_PER_CORE];
    logic [WARPS_PER_CORE-1:0] fetcher_read_ready_core;
    instruction_t fetcher_read_data_core [WARPS_PER_CORE];

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

        // Core connects to the LSU signals
        .data_mem_read_valid(lsu_read_valid),
        .data_mem_read_address(lsu_read_address),
        .data_mem_read_ready(lsu_read_ready),
        .data_mem_read_data(lsu_read_data),
        .data_mem_write_valid(lsu_write_valid),
        .data_mem_write_address(lsu_write_address),
        .data_mem_write_data(lsu_write_data),
        .data_mem_write_ready(lsu_write_ready),

        .start_mcu_transaction(core_start_mcu_transaction)
    );
    
endmodule