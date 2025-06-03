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

    // Internal reset
    logic rst;
    assign rst = ~rst_n;

    // Output from CPU for debugging
    logic [31:0] a0;

    // Instantiate CPU core
    top cpu (
        .clk(clk),
        .rst(rst),
        .a0(a0)
    );

    // Internal AXI <-> memory interface
    logic [31:0] axi_addr;
    logic [31:0] axi_rdata;
    logic [31:0] axi_wdata;
    logic        axi_write_en;

    // -------------------- AXI READ FSM --------------------

    logic        read_valid;
    logic [31:0] read_addr_reg;

    always_ff @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
            read_valid     <= 1'b0;
            read_addr_reg  <= 32'h0;
            s_axi_rvalid   <= 1'b0;
        end else begin
            if (s_axi_arvalid && !read_valid) begin
                read_addr_reg <= s_axi_araddr;
                read_valid    <= 1'b1;
                s_axi_rvalid  <= 1'b1;
            end else if (s_axi_rvalid && s_axi_rready) begin
                s_axi_rvalid <= 1'b0;
                read_valid   <= 1'b0;
            end
        end
    end

    assign s_axi_arready = !read_valid;
    assign s_axi_rdata   = axi_rdata;
    assign s_axi_rresp   = 2'b00;

    // -------------------- AXI WRITE FSM --------------------

    logic        write_pending;
    logic [31:0] write_addr_reg;
    logic [31:0] write_data_reg;

    always_ff @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
            write_pending   <= 1'b0;
            write_addr_reg  <= 32'h0;
            write_data_reg  <= 32'h0;
            s_axi_bvalid    <= 1'b0;
        end else begin
            if (s_axi_awvalid && s_axi_wvalid && !write_pending) begin
                write_addr_reg <= s_axi_awaddr;
                write_data_reg <= s_axi_wdata;
                write_pending  <= 1'b1;
                s_axi_bvalid   <= 1'b1;
            end else if (s_axi_bvalid && s_axi_bready) begin
                write_pending <= 1'b0;
                s_axi_bvalid  <= 1'b0;
            end
        end
    end

    assign s_axi_awready = !write_pending;
    assign s_axi_wready  = !write_pending;
    assign s_axi_bresp   = 2'b00;

    // -------------------- AXI-to-Memory Wiring --------------------

    assign axi_write_en = write_pending;
    assign axi_wdata    = write_data_reg;
    assign axi_addr     = read_valid ? read_addr_reg : write_addr_reg;

    // -------------------- Memory Instance --------------------

    data_mem data_memory (
        .clk(clk),
        .WDME(axi_write_en),      // Write enable
        .A(axi_addr),             // Address (shared for read/write)
        .WD(axi_wdata),           // Write data
        .RD(),                    // CPU internal read (unused here)

        .axi_A(axi_addr),         // AXI read address
        .axi_RD(axi_rdata)        // AXI read data
    );

endmodule
