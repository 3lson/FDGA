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
    input  logic   consumer_read_valid [THREADS_PER_WARP:0],
    input  data_memory_address_t            consumer_read_address [THREADS_PER_WARP:0],
    output logic         consumer_read_ready [THREADS_PER_WARP:0],
    output data_t                           consumer_read_data [THREADS_PER_WARP:0],
    input  logic         consumer_write_valid [THREADS_PER_WARP:0],
    input  data_memory_address_t            consumer_write_address [THREADS_PER_WARP:0],
    input  data_t                           consumer_write_data [THREADS_PER_WARP:0],
    output logic        consumer_write_ready [THREADS_PER_WARP:0],
    output logic mcu_is_busy,

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

    // --- Internal State and Buffers ---
    // FIX: Removed the redundant BUFFER_REQS state
    typedef enum logic [2:0] {
        IDLE,
        PROCESS_SCALAR,
        COALESCE_PLAN,
        ISSUE_ADDR_CMD,
        READ_DATA_BURST,
        WRITE_DATA_BURST,
        WAIT_WRITE_RESP
    } mcu_state_t;

    mcu_state_t state, next_state;

    // Internal signals (declarations unchanged)
    logic any_remaining_requests_after_burst;
    logic [$clog2(THREADS_PER_WARP+1):0] burst_start_idx;
    logic [7:0]                         burst_len_count;
    logic [7:0]                         burst_data_counter;
    logic [THREADS_PER_WARP:0] req_valid;
    logic [THREADS_PER_WARP:0] req_is_write;
    data_memory_address_t      req_addr [THREADS_PER_WARP:0];
    data_t                     req_wdata [THREADS_PER_WARP:0];
    logic [THREADS_PER_WARP:0] burst_serviced_mask;

    // --- Continuous Assignments ---
    assign mcu_is_busy = (state != IDLE);
    assign any_remaining_requests_after_burst = |(req_valid & ~burst_serviced_mask);
    always_comb begin 
        $display("req_valid: ", req_valid);
        $display("burst_serviced_mask: ", burst_serviced_mask);
    end
    // --- State Machine ---
    always @(posedge clk or posedge reset) begin
        if (reset) state <= IDLE;
        else       state <= next_state;
    end

    always_comb begin
        // --- Default assignments ---
        next_state = state;
        burst_serviced_mask = '0;
        m_axi_awid    = '0; 
        m_axi_awaddr  = '0; 
        m_axi_awlen   = '0; 
        m_axi_awsize  = $clog2(BYTES_PER_WORD); 
        m_axi_awburst = 2'b01; 
        m_axi_awvalid = 1'b0;
        m_axi_wdata   = '0;
        m_axi_wstrb   = '1; 
        m_axi_wlast   = 1'b0; 
        m_axi_wvalid  = 1'b0; 
        m_axi_bready  = 1'b0;
        m_axi_arid    = '0; 
        m_axi_araddr  = '0; 
        m_axi_arlen   = '0; 
        m_axi_arsize  = $clog2(BYTES_PER_WORD); 
        m_axi_arburst = 2'b01; 
        m_axi_arvalid = 1'b0; 
        m_axi_rready  = 1'b0;
        // for (int i=0; i<=THREADS_PER_WARP; i++) begin
        //     consumer_read_ready[i] = 1'b0; 
        //     consumer_read_data[i]  = '0; 
        //     consumer_write_ready[i]= 1'b0;
        // end

        // --- State Logic ---
        case(state)
            IDLE: begin
                $display("In the IDLE state");
                if (start_mcu_transaction) begin
                    $display("Starting the MCU transaction...");
                    next_state = PROCESS_SCALAR;
                end
            end

            PROCESS_SCALAR: begin
                $display("In the PROCESS_SCALAR state");
                if (req_valid[SCALAR_LSU_IDX]) begin
                    $display("Going to ISSUE_ADDR_CMD");
                    burst_start_idx = SCALAR_LSU_IDX;
                    burst_len_count = 1;
                    burst_serviced_mask[SCALAR_LSU_IDX] = 1'b1;
                    next_state = ISSUE_ADDR_CMD;
                end else if (|req_valid[THREADS_PER_WARP-1:0]) begin
                    $display("Going to COALESCE_PLAN");
                    next_state = COALESCE_PLAN;
                end else begin
                    $display("Going to IDLE");
                    next_state = IDLE;
                end
            end

            // All other states (COALESCE_PLAN, ISSUE_ADDR_CMD, etc.) remain unchanged.
            COALESCE_PLAN: begin
                for (int i = 0; i < THREADS_PER_WARP; i++) begin
                    if (req_valid[i]) begin
                        burst_start_idx = i;
                        burst_len_count = 1;
                        burst_serviced_mask[i] = 1'b1;
                        for (int j = i + 1; j < THREADS_PER_WARP; j++) begin
                            if (req_valid[j] && (req_is_write[j] == req_is_write[i]) && (req_addr[j] == req_addr[i] + (j-i))) begin
                                burst_len_count = burst_len_count + 1;
                                burst_serviced_mask[j] = 1'b1;
                            end
                        end
                        next_state = ISSUE_ADDR_CMD;
                        break;
                    end
                end
                if (next_state == COALESCE_PLAN) begin
                    for (int i = 0; i < THREADS_PER_WARP; i++) begin
                        if (req_valid[i]) begin
                            burst_start_idx = i;
                            burst_len_count = 1;
                            burst_serviced_mask[i] = 1'b1;
                            next_state = ISSUE_ADDR_CMD;
                            break;
                        end
                    end
                end
            end

            ISSUE_ADDR_CMD: begin
                data_memory_address_t word_addr = req_addr[burst_start_idx];
                data_memory_address_t byte_addr = word_addr * BYTES_PER_WORD;
                $display("In ISSUE_ADDR_CMD");
                if (req_is_write[burst_start_idx]) begin
                    m_axi_awvalid = 1'b1; m_axi_awaddr  = C_M_AXI_ADDR_WIDTH'(byte_addr); m_axi_awlen   = burst_len_count - 1;
                    if (m_axi_awready) begin
                        $display("Going to WRITE_DATA_BURST");
                        next_state = WRITE_DATA_BURST;
                    end
                end else begin
                    m_axi_arvalid = 1'b1; m_axi_araddr  = C_M_AXI_ADDR_WIDTH'(byte_addr); m_axi_arlen   = burst_len_count - 1;
                    if (m_axi_arready) begin
                        $display("Going to READ_DATA_BURST");
                        next_state = READ_DATA_BURST;
                    end
                end
            end

            WRITE_DATA_BURST: begin
                int current_thread_idx = burst_start_idx + burst_data_counter;
                m_axi_wvalid = 1'b1; m_axi_wdata  = req_wdata[current_thread_idx]; m_axi_wlast  = (burst_data_counter == burst_len_count - 1);
                if (m_axi_wready && m_axi_wlast) next_state = WAIT_WRITE_RESP;
            end

            WAIT_WRITE_RESP: begin
                m_axi_bready = 1'b1;
                if (m_axi_bvalid) begin
                    if (any_remaining_requests_after_burst) next_state = PROCESS_SCALAR;
                    else next_state = IDLE;
                end
            end

            READ_DATA_BURST: begin
                // $display("In READ_DATA_BURST");
                m_axi_rready = 1'b1;
                // $display("m_axi_rvalid ", m_axi_rvalid);
                // $display("m_axi_rlast ", m_axi_rlast);
                // $display("any_remaining_requests_after_burst: ", any_remaining_requests_after_burst);
                if (m_axi_rvalid) begin
                    int current_thread_idx = burst_start_idx + burst_data_counter;
                    $display("Hello bro");
                    consumer_read_data[current_thread_idx] = m_axi_rdata;
                    consumer_read_ready[current_thread_idx] = 1'b1;
                    if (m_axi_rlast) begin
                        if (any_remaining_requests_after_burst) begin
                            next_state = PROCESS_SCALAR;
                        end
                        else begin
                            $display("Going to IDLE");
                            next_state = IDLE;
                        end
                    end
                    // else begin
                    //     $display("Going to IDLE");
                    //     next_state = IDLE;
                    // end
                end
                // else begin
                //     $display("Going to IDLE");
                //     next_state = IDLE;
                // end
            end
        endcase
    end

    // --- Registered Logic for Buffers and Counters ---
    always @(posedge clk) begin
        if (reset) begin
            req_valid <= '0;
            burst_data_counter <= '0;
        end else begin
            // FIX: Buffer requests when a transaction is started from the IDLE state.
            if (state == IDLE && start_mcu_transaction) begin
                for (int i=0; i<=THREADS_PER_WARP; i++) begin
                    req_valid[i]     <= consumer_read_valid[i] || consumer_write_valid[i];
                    req_is_write[i]  <= consumer_write_valid[i];
                    req_addr[i]      <= consumer_write_valid[i] ? consumer_write_address[i] : consumer_read_address[i];
                    req_wdata[i]     <= consumer_write_data[i];
                end
            end

            // Clear serviced req_valid bits after the burst completes
            if ((state == WAIT_WRITE_RESP && m_axi_bvalid) ||
                (state == READ_DATA_BURST && m_axi_rvalid && m_axi_rlast)) begin
                req_valid <= req_valid & ~burst_serviced_mask;
            end

            // Manage burst data counter
            if ((state == WRITE_DATA_BURST && m_axi_wvalid && m_axi_wready) ||
                (state == READ_DATA_BURST && m_axi_rvalid && m_axi_rready)) begin
                burst_data_counter <= burst_data_counter + 1;
            end else if (next_state == ISSUE_ADDR_CMD || next_state == IDLE) begin
                burst_data_counter <= '0;
            end
        end
    end

endmodule
