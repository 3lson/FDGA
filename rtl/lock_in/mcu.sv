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
    output logic                               m_axi_awvalid,
    input  logic                            m_axi_awready,
    output logic [C_M_AXI_DATA_WIDTH-1:0]   m_axi_wdata,
    output logic [C_M_AXI_DATA_WIDTH/8-1:0] m_axi_wstrb,
    output logic                            m_axi_wlast,
    output logic                              m_axi_wvalid,
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
    input logic                              m_axi_rlast,
    input  logic                             m_axi_rvalid,
    output logic                             m_axi_rready
);

// This function is for combinational prediction, which is a good optimisation
function logic calc_remaining_requests(
    logic [THREADS_PER_WARP:0] current_req_valid,
    logic [$clog2(THREADS_PER_WARP+1)-1:0] start_idx,
    logic [7:0] len_count
);
    logic [THREADS_PER_WARP:0] temp_valid;
    temp_valid = current_req_valid;
    
    for (int i=0; i < len_count; i++) begin
        if ((start_idx + i) <= THREADS_PER_WARP) begin
            temp_valid[start_idx + i] = 1'b0;
        end
    end
    
    return |temp_valid;
endfunction

    localparam int SCALAR_LSU_IDX = THREADS_PER_WARP;
    localparam BYTES_PER_WORD = C_M_AXI_DATA_WIDTH / 8;

    typedef enum logic [3:0] {
        IDLE,
        BUFFER_REQS,
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
    
    
    assign mcu_is_busy = (state != IDLE);

    always_comb begin
        // $display("consumer_write_valid[i] always_comb: ", consumer_write_valid[16]);
        // $display("consumer_write_data[i] always_comb: ", consumer_write_data[16]);
        // $display("consumer_write_address[i] always_comb: ", consumer_write_address[16]);

        // $display("consumer_read_valid[i] always_comb: ", consumer_read_valid[16]);
        // $display("consumer_read_data[i] always_comb: ", consumer_read_data[16]);
        // $display("consumer_read_address[i] always_comb: ", consumer_read_address[16]);
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
        
        // for (int i=0; i<=THREADS_PER_WARP; i++) begin 
        //     req_valid_after_clear[i] = req_valid[i];
        // end

        // for (int i=0; i<=THREADS_PER_WARP; i++) begin 
        //     $display("consumer_write_valid: ", consumer_write_valid[i]);
        // end

        // Debug print for the read data test
        //$display("Value loaded in", consumer_read_data[16]);
        
        // Predictively clear the requests that are part of the *current* burst
        // for (int i=0; i < burst_len_count; i++) begin
        //     req_valid_after_clear[burst_start_idx + i] = 1'b0;
        // end
        // // Check if any bit is still set after the predictive clear
        // any_remaining_requests_after_burst = |req_valid_after_clear;

        // --- FSM ---
        case(state)
            IDLE: begin
                // if (start_mcu_transaction && !transaction_started) begin
                //     $display("MCU: IDLE -> PROCESS_SCALAR");
                //     next_state = PROCESS_SCALAR;
                // end
                // $display("consumer_read_valid", consumer_read_valid);
                // $display("|consumer_write_valid", |consumer_write_valid);
                if(|consumer_read_valid || |consumer_write_valid) begin
                    $display("MCU: IDLE -> BUFFER_REQS");
                    next_state = BUFFER_REQS;
                end
            end

            BUFFER_REQS: begin
                // We spend 1 cycle here to guarantee inputs are stable
                // On the next cycle, we will have the latched values
                $display("MCU: BUFFER_REQS -> PROCESS_SCALAR");
                next_state = PROCESS_SCALAR;
            end

            PROCESS_SCALAR:
                if (req_valid[SCALAR_LSU_IDX]) begin
                    $display("MCU: PROCESS_SCALAR -> ISSUE_ADDR_CMD");
                    burst_start_idx = SCALAR_LSU_IDX;
                    burst_len_count = 1;
                    next_state = ISSUE_ADDR_CMD;
                end else if (|req_valid[THREADS_PER_WARP-1:0]) begin // Any vector requests?
                    $display("MCU: PROCESS_SCALAR -> COALESCE_PLAN");
                    next_state = COALESCE_PLAN;
                end else begin
                    $display("MCU: PROCESS_SCALAR -> IDLE");
                    next_state = IDLE; // No requests at all
                end

            COALESCE_PLAN: begin
                logic found_burst;
                found_burst = 1'b0;
                burst_len_count = 1; // Default to burst of 1 for uncoalesced
                for(int i=0; i<=THREADS_PER_WARP; i++) begin
                    if(req_valid[i] && !found_burst) begin
                        found_burst = 1'b1;
                        burst_start_idx = i;
                        for (int j = i + 1; j <= THREADS_PER_WARP; j++) begin
                            if (req_valid[j] && (req_addr[j] == (req_addr[i] + (j-i)))) begin
                                burst_len_count = burst_len_count + 1;
                            end else break;
                        end
                    end
                end
                $display("MCU: COALESCE_PLAN -> ISSUE_ADDR_CMD");
                next_state = ISSUE_ADDR_CMD;
            end

            ISSUE_ADDR_CMD: begin
                // MCU addresses are WORD addresses, AXI needs BYTE addresses
                data_memory_address_t byte_addr = req_addr[burst_start_idx] * BYTES_PER_WORD;
                // $display("burst_start_idx: ", burst_start_idx);
                // $display("req_is_write[burst_start_idx]: ", req_is_write[burst_start_idx]);
                // $display("m_axi_awready: ", m_axi_awready);
                if (req_is_write[burst_start_idx]) begin
                    m_axi_awvalid = 1'b1; 
                    m_axi_awaddr = byte_addr; 
                    m_axi_awlen = burst_len_count - 1;
                    if (m_axi_awready) begin
                        $display("MCU: ISSUE_ADDR_CMD -> WRITE_DATA_BURST");
                        next_state = WRITE_DATA_BURST;
                    end
                end else begin
                    m_axi_arvalid = 1'b1; 
                    m_axi_araddr = byte_addr; 
                    m_axi_arlen = burst_len_count - 1;
                    if (m_axi_arready) begin
                        $display("MCU: ISSUE_ADDR_CMD -> READ_DATA_BURST");
                        next_state = READ_DATA_BURST;
                    end
                end
            end

            WRITE_DATA_BURST: begin
                // $display("m_axi_wready: ", m_axi_wready);
                // $display("m_axi_wlast: ", m_axi_wlast);
                // $display("m_axi_wdata: ", m_axi_wdata);
                // $display("burst_len_count: ", burst_len_count);
                // $display("burst_data_counter: ", burst_data_counter);
                m_axi_wvalid = 1'b1; m_axi_wdata = req_wdata[burst_start_idx + burst_data_counter];
                m_axi_wlast = (burst_data_counter == burst_len_count - 1);
                if (m_axi_wready) begin
                    if (m_axi_wlast) begin 
                        $display("MCU: WRITE_DATA_BURST -> WAIT_WRITE_RESP");
                        next_state = WAIT_WRITE_RESP;
                    end
                    // else stay in this state for multi-beat bursts
                end
            end

            WAIT_WRITE_RESP: begin
                m_axi_bready = 1'b1;
                // Assert consumer_write_ready for the completed transfers
                for (int i = 0; i < burst_len_count; i++) begin
                    if (req_is_write[burst_start_idx + i]) begin
                        consumer_write_ready[burst_start_idx + i] = 1'b1;
                    end
                end
                if (m_axi_bvalid) begin
                    if (calc_remaining_requests(req_valid, burst_start_idx, burst_len_count)) begin
                        $display("MCU: WAIT_WRITE_RESP -> PROCESS_SCALAR");
                        next_state = PROCESS_SCALAR;
                    end
                    else begin
                        $display("MCU: WAIT_WRITE_RESP -> IDLE");
                        next_state = IDLE;
                    end
                end
            end
            
            READ_DATA_BURST: begin
                m_axi_rready = 1'b1;
                if (m_axi_rvalid) begin
                    $display("m_axi_rvalid: ", m_axi_rvalid);
                    $display("m_axi_rlast: ", m_axi_rlast);
                    // Data and Ready are driven in the always_ff block
                    if (m_axi_rlast) begin
                        if (calc_remaining_requests(req_valid, burst_start_idx, burst_len_count)) begin
                            $display("MCU: READ_DATA_BURST -> PROCESS_SCALAR");
                            next_state = PROCESS_SCALAR;
                        end
                        else begin
                            $display("MCU: READ_DATA_BURST -> IDLE");
                            next_state = IDLE;
                        end
                    end
                end
            end
        endcase
    end

    // --- Sequential Logic ---
    always_ff @(posedge clk or posedge reset) begin
        $display("state: ", state);
        $display("next state: ", next_state);
        if (reset) begin
            state <= IDLE;
            req_valid <= '0;
            burst_data_counter <= '0;
            consumer_read_data <= '{default:'0};
            //transaction_started <= 1'b0;
        end else begin
            state <= next_state;
            

            // Latch inputs only when in the BUFFER_REQS state
            if (state == IDLE && next_state == BUFFER_REQS) begin
                $display("where did [16] go: ", consumer_write_valid[16]);
                req_valid <= |consumer_write_valid | |consumer_read_valid;
                $display("req_valid: ",  |consumer_write_valid | |consumer_read_valid);
                for (int i=0; i<=THREADS_PER_WARP; i++) begin
                    $display("consumer_write_valid: ", consumer_write_valid[i]);
                    $display("consumer_write_address: ", consumer_write_address[i]);
                    $display("consumer_write_data: ", consumer_write_data[i]);
                    req_is_write[i]  <= consumer_write_valid[i];
                    req_addr[i]      <= consumer_write_valid[i] ? consumer_write_address[i] : consumer_read_address[i];
                    req_wdata[i]     <= consumer_write_data[i];
                    $display("req_is_write: ", req_is_write[i]);
                    $display("req_addr: ", req_addr[i]);
                    $display("req_wdata: ", req_wdata[i]);
                end
            end

            // Clear the valid bits of the requests that have been fully processed
            if ((state == WAIT_WRITE_RESP && m_axi_bvalid) || (state == READ_DATA_BURST && m_axi_rvalid && m_axi_rlast)) begin
                for (int i = 0; i < burst_len_count; i++) begin
                    req_valid[burst_start_idx + i] <= 1'b0;
                end
            end

            // Latch incoming read data
            if (state == READ_DATA_BURST && m_axi_rvalid) begin
                consumer_read_data[burst_start_idx + burst_data_counter] <= m_axi_rdata;
            end else begin
                consumer_read_ready <= '0;
            end

            // Manage Burst Counter
            if (next_state == IDLE || next_state == PROCESS_SCALAR) begin
                burst_data_counter <= '0;
            end else if ((state == WRITE_DATA_BURST && m_axi_wready) || (state == READ_DATA_BURST && m_axi_rvalid)) begin
                burst_data_counter <= burst_data_counter + 1;
            end
        end
    end

endmodule
