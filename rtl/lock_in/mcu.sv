`default_nettype none
`timescale 1ns/1ns

`include "common.sv"
`include "gpu_defines.svh"

module mcu #(
    parameter THREADS_PER_WARP = 16,
    parameter C_M_AXI_ADDR_WIDTH = 32,
    parameter C_M_AXI_DATA_WIDTH = 32,
    parameter C_M_AXI_ID_WIDTH   = 1
) (
    input wire clk,
    input wire reset,
    input wire start_mcu_transaction,
    
    // FIXED: Changed from unpacked arrays to packed arrays
    input  logic [THREADS_PER_WARP:0] consumer_read_valid,
    input  data_memory_address_t consumer_read_address [THREADS_PER_WARP:0],
    output logic [THREADS_PER_WARP:0] consumer_read_ready,
    output data_t consumer_read_data [THREADS_PER_WARP:0],
    input  logic [THREADS_PER_WARP:0] consumer_write_valid,
    input  data_memory_address_t consumer_write_address [THREADS_PER_WARP:0],
    input  data_t consumer_write_data [THREADS_PER_WARP:0],
    output logic [THREADS_PER_WARP:0] consumer_write_ready,
    
    output logic mcu_is_busy,
    // --- AXI Ports ---
    output logic [C_M_AXI_ID_WIDTH-1:0]     m_axi_awid,
    output logic [C_M_AXI_ADDR_WIDTH-1:0]   m_axi_awaddr,
    output logic [7:0]                      m_axi_awlen,
    output logic [2:0]                      m_axi_awsize,
    output logic [1:0]                      m_axi_awburst,
    output logic                            m_axi_awvalid,
    input  logic                            m_axi_awready,
    output logic [C_M_AXI_DATA_WIDTH-1:0]   m_axi_wdata,
    output logic [C_M_AXI_DATA_WIDTH/8-1:0] m_axi_wstrb,
    output logic                            m_axi_wlast,
    output logic                            m_axi_wvalid,
    input  logic                            m_axi_wready,
    input  logic [C_M_AXI_ID_WIDTH-1:0]      m_axi_bid,
    input  logic [1:0]                       m_axi_bresp,
    input  logic                             m_axi_bvalid,
    output logic                             m_axi_bready,
    output logic [C_M_AXI_ID_WIDTH-1:0]     m_axi_arid,
    output logic [C_M_AXI_ADDR_WIDTH-1:0]   m_axi_araddr,
    output logic [7:0]                      m_axi_arlen,
    output logic [2:0]                      m_axi_arsize,
    output logic [1:0]                      m_axi_arburst,
    output logic                            m_axi_arvalid,
    input  logic                            m_axi_arready,
    input  logic [C_M_AXI_ID_WIDTH-1:0]      m_axi_rid,
    input  logic [C_M_AXI_DATA_WIDTH-1:0]    m_axi_rdata,
    input  logic [1:0]                       m_axi_rresp,
    input  logic                             m_axi_rlast,
    input  logic                             m_axi_rvalid,
    output logic                             m_axi_rready
);

    localparam int SCALAR_LSU_IDX = THREADS_PER_WARP;
    localparam BYTES_PER_WORD = C_M_AXI_DATA_WIDTH / 8;

    typedef enum logic [2:0] {
        IDLE,
        PROCESS_SCALAR,
        COALESCE_PLAN,
        ISSUE_ADDR_CMD,
        READ_DATA_BURST,
        WRITE_DATA_BURST,
        WAIT_WRITE_RESP
    } mcu_state_t;

    // --- Internal Registers ---
    mcu_state_t state, next_state;
    logic [$clog2(THREADS_PER_WARP+1)-1:0] burst_start_idx;
    logic [7:0]                           burst_len_count;
    logic [7:0]                           burst_data_counter;
    logic [THREADS_PER_WARP:0] req_valid;
    logic [THREADS_PER_WARP:0] req_is_write;
    data_memory_address_t      req_addr [THREADS_PER_WARP:0];
    data_t                     req_wdata [THREADS_PER_WARP:0];
    
    logic any_remaining_requests_after_burst;
    logic  [THREADS_PER_WARP:0] req_valid_after_clear;
    
    assign mcu_is_busy = (state != IDLE);

    always_comb begin
        // --- Defaults ---
        next_state = state;
        m_axi_awvalid = 1'b0; m_axi_arvalid = 1'b0; m_axi_wvalid = 1'b0;
        m_axi_bready  = 1'b0; m_axi_rready  = 1'b0;
        m_axi_awid    = '0; m_axi_awaddr  = '0; m_axi_awlen   = '0;
        m_axi_arid    = '0; m_axi_araddr  = '0; m_axi_arlen   = '0;
        m_axi_wdata   = '0; m_axi_wlast   = 1'b0;
        m_axi_awsize  = $clog2(BYTES_PER_WORD); m_axi_awburst = 2'b01; // 32-bit, INCR
        m_axi_arsize  = $clog2(BYTES_PER_WORD); m_axi_arburst = 2'b01; // 32-bit, INCR
        m_axi_wstrb   = '1; // Write all bytes
        
        // FIXED: Initialize packed array outputs
        consumer_write_ready = '0;
        
        for (int i=0; i<=THREADS_PER_WARP; i++) begin 
            req_valid_after_clear[i] = req_valid[i];
        end

        // Debug print for the read data test
        //$display("Value loaded in", consumer_read_data[16]);
        
        // Predictively clear the requests that are part of the *current* burst
        for (int i=0; i < burst_len_count; i++) begin
            req_valid_after_clear[burst_start_idx + i] = 1'b0;
        end
        // Check if any bit is still set after the predictive clear
        any_remaining_requests_after_burst = |req_valid_after_clear;

        // --- FSM ---
        case(state)
            IDLE:
                if (start_mcu_transaction) begin
                    $display("IN IDLE GOING TO PROCESS_SCALAR");
                    next_state = PROCESS_SCALAR;
                end

            PROCESS_SCALAR:
                if (req_valid[SCALAR_LSU_IDX]) begin
                    $display("IN PROCESS_SCALAR GOING TO ISSUE_ADDR_CMD");
                    next_state = ISSUE_ADDR_CMD;
                end else if (|req_valid[THREADS_PER_WARP-1:0]) begin // Any vector requests?
                    $display("IN PROCESS_SCALAR GOING TO COALESCE_PLAN");
                    next_state = COALESCE_PLAN;
                end else begin
                    $display("IN PROCESS_SCALAR GOING TO IDLE");
                    next_state = IDLE; // No requests at all
                end

            COALESCE_PLAN: begin
                $display("IN COALESCE_PLAN GOING TO ISSUE_ADDR_CMD");
                next_state = ISSUE_ADDR_CMD;
            end

            ISSUE_ADDR_CMD: begin
                // MCU addresses are WORD addresses, AXI needs BYTE addresses
                data_memory_address_t byte_addr = req_addr[burst_start_idx] * BYTES_PER_WORD;
                if (req_is_write[burst_start_idx]) begin
                    m_axi_awvalid = 1'b1; 
                    m_axi_awaddr = byte_addr; 
                    m_axi_awlen = burst_len_count - 1;
                    if (m_axi_awready) begin
                        $display("IN ISSUE_ADDR_CMD GOING TO WRITE_DATA_BURST");
                        next_state = WRITE_DATA_BURST;
                    end
                end else begin
                    m_axi_arvalid = 1'b1; 
                    m_axi_araddr = byte_addr; 
                    m_axi_arlen = burst_len_count - 1;
                    if (m_axi_arready) begin
                        $display("IN ISSUE_ADDR_CMD GOING TO READ_DATA_BURST");
                        next_state = READ_DATA_BURST;
                    end
                end
            end

            WRITE_DATA_BURST: begin
                m_axi_wvalid = 1'b1; m_axi_wdata = req_wdata[burst_start_idx + burst_data_counter];
                m_axi_wlast = (burst_data_counter == burst_len_count - 1);
                if (m_axi_wready && m_axi_wlast) begin
                    $display("IN WRITE_DATA_BURST GOING TO WAIT_WRITE_RESP");
                    next_state = WAIT_WRITE_RESP;
                end
            end

            WAIT_WRITE_RESP: begin
                m_axi_bready = 1'b1;
                if (m_axi_bvalid) begin
                    if (any_remaining_requests_after_burst) begin
                        $display("IN WAIT_WRITE_RESP GOING TO PROCESS_SCALAR");
                        next_state = PROCESS_SCALAR; // Go check for more work
                    end else begin
                        $display("IN WAIT_WRITE_RESP GOING TO IDLE");
                        next_state = IDLE; // All done
                    end
                end
            end
            
            READ_DATA_BURST: begin
                m_axi_rready = 1'b1;
                if (m_axi_rvalid && m_axi_rlast) begin
                    // --- SOLUTION: Use the PREDICTED value to decide the next state ---
                    if (any_remaining_requests_after_burst) begin
                        $display("IN READ_DATA_BURST GOING TO PROCESS_SCALAR");
                        next_state = PROCESS_SCALAR; // Go check for more work
                    end else begin
                        $display("IN READ_DATA_BURST GOING TO IDLE");
                        next_state = IDLE; // All done
                    end
                end
            end
        endcase
    end

    // --- Sequential Logic ---
    always_ff @(posedge clk or posedge reset) begin
        if (reset) begin
            state <= IDLE;
            req_valid <= '0;
            burst_data_counter <= '0;
            consumer_read_ready <= '0;  // FIXED: Initialize packed array
            consumer_read_data <= '{default:'0};
        end else begin
            state <= next_state;

            // --- Latch consumer requests at the start of a transaction ---
            if (state == IDLE && start_mcu_transaction) begin
                // On the first cycle, clear old read-ready flags
                consumer_read_ready <= '0;  // FIXED: Clear packed array
                for (int i=0; i<=THREADS_PER_WARP; i++) begin
                    req_valid[i]     <= consumer_read_valid[i] || consumer_write_valid[i];
                    req_is_write[i]  <= consumer_write_valid[i];
                    req_addr[i]      <= consumer_write_valid[i] ? consumer_write_address[i] : consumer_read_address[i];
                    req_wdata[i]     <= consumer_write_data[i];
                end
            end

            // --- Clear the processed requests from the buffer ---
            // This happens AFTER a burst is fully complete
            if ((state == WAIT_WRITE_RESP && m_axi_bvalid) ||
                (state == READ_DATA_BURST && m_axi_rvalid && m_axi_rlast)) begin
                for (int i = 0; i < burst_len_count; i++) begin
                    req_valid[burst_start_idx + i] <= 1'b0;
                end
            end

            // --- Latch incoming read data and signal ready to the specific consumer ---
            if (state == READ_DATA_BURST && m_axi_rvalid) begin
                int current_thread_idx = burst_start_idx + burst_data_counter;
                consumer_read_data[current_thread_idx] <= m_axi_rdata;
                consumer_read_ready[current_thread_idx] <= 1'b1;  // FIXED: Set individual bit
            end

            // --- Manage Burst Counter ---
            if (next_state == ISSUE_ADDR_CMD) begin // Reset counter when we start a new plan
                burst_data_counter <= '0;
            end else if ((state == WRITE_DATA_BURST && m_axi_wready) ||
                       (state == READ_DATA_BURST && m_axi_rvalid)) begin // Increment during a burst
                burst_data_counter <= burst_data_counter + 1;
            end

            // --- Set burst parameters in PROCESS_SCALAR for scalar requests ---
            if (state == PROCESS_SCALAR && req_valid[SCALAR_LSU_IDX]) begin
                burst_start_idx <= SCALAR_LSU_IDX;
                burst_len_count <= 1;
            end

            // --- Set burst parameters in COALESCE_PLAN for vector requests ---
            if (state == COALESCE_PLAN) begin
                logic found_burst = 1'b0;
                for (int i = 0; i < THREADS_PER_WARP; i++) begin
                    if (req_valid[i] && !found_burst) begin
                        burst_start_idx <= i;
                        burst_len_count <= 1;
                        found_burst = 1'b1;
                        // Count consecutive word addresses
                        for (int j = i + 1; j < THREADS_PER_WARP; j++) begin
                            if (req_valid[j] && (req_addr[j] == (req_addr[i] + (j-i)))) begin
                                burst_len_count <= j - i + 1;
                            end else begin
                                break;
                            end
                        end
                    end
                end
            end
        end
    end

endmodule
