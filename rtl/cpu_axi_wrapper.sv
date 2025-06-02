module cpu_axi_wrapper (
    input  logic        clk,
    input  logic        rst_n,

    // AXI-Lite slave interface - READ CHANNELS
    input  logic [31:0] s_axi_araddr,
    input  logic        s_axi_arvalid,
    output logic        s_axi_arready,
    output logic [31:0] s_axi_rdata,
    output logic [1:0]  s_axi_rresp,
    output logic        s_axi_rvalid,
    input  logic        s_axi_rready,
    
    // AXI-Lite slave interface - WRITE CHANNELS
    input  logic [31:0] s_axi_awaddr,
    input  logic        s_axi_awvalid,
    output logic        s_axi_awready,
    input  logic [31:0] s_axi_wdata,
    input  logic [3:0]  s_axi_wstrb,
    input  logic        s_axi_wvalid,
    output logic        s_axi_wready,
    output logic [1:0]  s_axi_bresp,
    output logic        s_axi_bvalid,
    input  logic        s_axi_bready
);

    // Invert reset for active-high inside core
    logic rst;
    assign rst = ~rst_n;

    // Output wire (for debugging)
    logic [31:0] a0;

    // Instantiate CPU core
    top cpu (
        .clk(clk),
        .rst(rst),
        .a0(a0)
    );

    // AXI signals
    logic [31:0] axi_addr;
    logic [31:0] axi_rdata;
    logic [31:0] axi_wdata;
    logic        axi_write_en;

    // Read transaction handling
    assign axi_addr = s_axi_arvalid ? s_axi_araddr : s_axi_awaddr;
    
    // Write transaction handling
    assign axi_wdata = s_axi_wdata;
    assign axi_write_en = s_axi_awvalid & s_axi_wvalid;

    // Access shared port from data_mem
    data_mem data_memory (
        .clk(clk),
        .WDME(axi_write_en),           // Enable write when AXI write transaction
        .A(axi_addr),                  // Address from AXI (read or write)
        .WD(axi_wdata),                // Write data from AXI
        .RD(),                         // CPU read data (not used here)

        .axi_A(axi_addr),              // AXI address
        .axi_RD(axi_rdata)             // AXI read data
    );

    // Read channel responses
    assign s_axi_arready = 1'b1;
    assign s_axi_rvalid  = s_axi_arvalid;
    assign s_axi_rdata   = axi_rdata;
    assign s_axi_rresp   = 2'b00;  // OKAY response

    // Write channel responses
    assign s_axi_awready = 1'b1;
    assign s_axi_wready  = 1'b1;
    assign s_axi_bvalid  = s_axi_awvalid & s_axi_wvalid;
    assign s_axi_bresp   = 2'b00;  // OKAY response

endmodule