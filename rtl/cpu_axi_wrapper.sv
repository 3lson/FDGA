module cpu_axi_wrapper (
    input  logic        clk,
    input  logic        rst_n,

    // AXI-Lite slave interface
    input  logic [31:0] s_axi_araddr,
    input  logic        s_axi_arvalid,
    output logic        s_axi_arready,
    output logic [31:0] s_axi_rdata,
    output logic        s_axi_rvalid,
    input  logic        s_axi_rready
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

    // Read-only memory access via shared port
    logic [31:0] axi_addr;
    logic [31:0] axi_data;

    assign axi_addr = s_axi_araddr;

    // Access shared port from data_mem
    // You must wire this out of `top.sv` to here, OR recreate data_mem here
    data_mem data_memory (
        .clk(clk),
        .WDME(1'b0),
        .A(32'b0),     // not used
        .WD(32'b0),    // not used
        .RD(),         // not used

        .axi_A(axi_addr),
        .axi_RD(axi_data)
    );

    assign s_axi_arready = 1'b1;
    assign s_axi_rvalid  = s_axi_arvalid;
    assign s_axi_rdata   = axi_data;

endmodule
