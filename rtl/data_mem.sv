module data_mem #(
    parameter DATA_WIDTH       = 32,
              ADDR_WIDTH       = 32,
              ADDR_REAL_WIDTH  = 15,  // 2^15 = 32K words = 128KB total
              MEM_WIDTH        = 32
)(
    input  logic                    clk,
    input  logic                    WDME,
    input  logic [ADDR_WIDTH-1:0]   A,        // CPU address
    input  logic [DATA_WIDTH-1:0]   WD,       // Write data
    output logic [DATA_WIDTH-1:0]   RD,       // Read data

    // AXI I/O access (read-only for now)
    input  logic [ADDR_WIDTH-1:0]   axi_A,
    output logic [DATA_WIDTH-1:0]   axi_RD
);

    // Use BRAM explicitly
    (* ram_style = "block" *) logic [MEM_WIDTH-1:0] array [0:(1 << ADDR_REAL_WIDTH) - 1];

    logic [ADDR_REAL_WIDTH-1:0] word_addr, axi_word_addr;

    assign word_addr     = A[ADDR_REAL_WIDTH+1:2];     // word-aligned
    assign axi_word_addr = axi_A[ADDR_REAL_WIDTH+1:2]; // word-aligned

    initial begin
        $display("Loading program into data memory...");
        for (int i = 0; i < (1 << ADDR_REAL_WIDTH); i++) begin
            array[i] = 32'h00000000;
        end
        $readmemh("../rtl/data.hex", array);
    end

    // Synchronous read and write
    always_ff @(posedge clk) begin
        if (WDME) begin
            array[word_addr] <= WD;
        end
        RD      <= array[word_addr];
        axi_RD  <= array[axi_word_addr];
    end

endmodule
