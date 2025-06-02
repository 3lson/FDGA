module data_mem #(
    parameter DATA_WIDTH = 32, 
              ADDR_WIDTH = 32,
              ADDR_REAL_WIDTH = 18, // 2^18 = 1MB memory
              MEM_WIDTH = 32
)(
    input  logic                    clk,
    input  logic                    WDME,
    input  logic [ADDR_WIDTH-1:0]   A, // full address
    input  logic [DATA_WIDTH-1:0]   WD, // write data
    output logic [DATA_WIDTH-1:0]   RD,  // read data

    // AXI I/O access
    input logic [ADDR_WIDTH-1:0] axi_A, // address
    output logic [DATA_WIDTH-1:0] axi_RD // read data
);

    logic [MEM_WIDTH-1:0] array [2**ADDR_REAL_WIDTH-1:0];

    logic [ADDR_REAL_WIDTH-1:0] word_addr, axi_word_addr;
    
    assign word_addr = A[ADDR_REAL_WIDTH+1:2]; // Word address
    assign axi_word_addr = axi_A[ADDR_REAL_WIDTH+1:2];

    initial begin
        $display("Loading program into data memory...");
        for (int i = 0; i < (2**ADDR_REAL_WIDTH); i++) begin
            array[i] = 32'h0;
        end
        $readmemh("../rtl/data.hex", array);  
    end

    // Store instruction
    always_ff @(posedge clk) begin
        if (WDME) begin
            array[word_addr] <= WD;
        end
    end

    assign RD = array[word_addr];
    assign axi_RD = array[axi_word_addr];

endmodule
