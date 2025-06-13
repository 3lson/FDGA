// bram_adapter.sv
`default_nettype none
`timescale 1ns/1ns

module bram_adapter #(
    parameter int DATA_WIDTH = 32,
    parameter int ADDR_WIDTH = 32,
    parameter int NUM_CHANNELS = 8,
    parameter bit WRITE_ENABLE = 1'b1
) (
    input wire clk,
    input wire reset,

    // GPU-Facing Interface (Ready/Valid)
    input  wire [NUM_CHANNELS-1:0]           gpu_req_valid,
    output wire [NUM_CHANNELS-1:0]           gpu_req_ready,
    input  wire [ADDR_WIDTH-1:0]             gpu_req_addr [NUM_CHANNELS],
    output logic [DATA_WIDTH-1:0]            gpu_resp_data [NUM_CHANNELS],
    // Write signals
    input  wire [NUM_CHANNELS-1:0]           gpu_w_valid,
    input  wire [DATA_WIDTH-1:0]             gpu_w_data [NUM_CHANNELS],
    output wire [NUM_CHANNELS-1:0]           gpu_w_ready,


    // BRAM-Facing Interface (Native)
    output logic                            bram_en,
    output logic [ADDR_WIDTH-1:0]            bram_addr,
    output logic [0:0]                       bram_we, // BRAM write enable is usually a single bit or a byte-mask
    output logic [DATA_WIDTH-1:0]            bram_din,
    input  wire [DATA_WIDTH-1:0]             bram_dout
);

    // Simple round-robin arbiter
    localparam int CH_WIDTH = (NUM_CHANNELS > 1) ? $clog2(NUM_CHANNELS) : 1;
    logic [CH_WIDTH-1:0] last_grant, next_grant;
    logic [NUM_CHANNELS-1:0] req_mask;
    logic grant_valid;
    
    // State machine for handling BRAM latency
    typedef enum logic [1:0] {IDLE, WAIT_DATA} state_t;
    state_t state;
    logic [CH_WIDTH-1:0] granted_ch_reg;

    // --- Arbitration Logic ---
    always_comb begin
        req_mask = gpu_req_valid | (WRITE_ENABLE ? gpu_w_valid : '0);
        grant_valid = 1'b0;
        next_grant = last_grant;
        for (int i = 0; i < NUM_CHANNELS; i++) begin
            int ch = (last_grant + 1 + i) % NUM_CHANNELS;
            if (req_mask[ch]) begin
                next_grant = ch;
                grant_valid = 1'b1;
                break;
            end
        end
    end

    // --- State Machine & BRAM Control ---
    always @(posedge clk) begin
        if (reset) begin
            state <= IDLE;
            last_grant <= '0;
            bram_en <= 1'b0;
            bram_we <= 1'b0;
        end else begin
            // Default assignments
            bram_en <= 1'b0;
            bram_we <= 1'b0;

            case (state)
                IDLE: begin
                    if (grant_valid) begin
                        bram_en   <= 1'b1;
                        bram_addr <= gpu_req_addr[next_grant];
                        
                        if (WRITE_ENABLE && gpu_w_valid[next_grant]) begin
                            bram_we <= 1'b1;
                            bram_din <= gpu_w_data[next_grant];
                            // Write takes 1 cycle, no wait state needed
                            state <= IDLE; 
                        end else begin // It's a read request
                            bram_we <= 1'b0;
                            state <= WAIT_DATA;
                        end
                        granted_ch_reg <= next_grant;
                        last_grant <= next_grant; // Update arbiter priority
                    end
                end
                WAIT_DATA: begin
                    // Data from BRAM is available this cycle.
                    // The machine will be ready for a new request on the next cycle.
                    state <= IDLE;
                end
            endcase
        end
    end

    // --- GPU-Facing Ready/Valid Logic ---
    genvar i;
    generate
        for (i = 0; i < NUM_CHANNELS; i = i + 1) begin : g_gpu_connect
            // A channel can make a request if the adapter is idle and it gets the grant
            assign gpu_req_ready[i] = (state == IDLE) && grant_valid && (next_grant == i);
            assign gpu_w_ready[i]   = (state == IDLE) && grant_valid && (next_grant == i);
            
            // Route the BRAM data back to the channel that made the read request
            // This is valid for one cycle when we are leaving the WAIT_DATA state.
            always @(posedge clk) begin
                if (reset) begin
                    gpu_resp_data[i] <= '0;
                end else if (state == WAIT_DATA && granted_ch_reg == i) begin
                    gpu_resp_data[i] <= bram_dout;
                end
            end
        end
    endgenerate

endmodule
`default_nettype wire