module data_mem #(
    parameter DATA_WIDTH = 32, 
              ADDR_WIDTH = 32,
              ADDR_REAL_WIDTH = 20, // 2^20 = 1MB memory
              MEM_WIDTH = 8
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

    logic [31:0] addr;
    logic [MEM_WIDTH-1:0] array [2**ADDR_REAL_WIDTH-1:0];
    logic [DATA_WIDTH-1:0] temp;
    logic [DATA_WIDTH-1:0] axi_temp;

    assign addr = $unsigned(A);

    initial begin
        for (int i = 0; i < (1 << ADDR_REAL_WIDTH); i++) begin
            array[i] = 8'b0;
        end
        $display("Loading program into data memory...");
        $readmemh("../rtl/data.hex", array);  
    end

    // Load instruction
    always_comb begin
        temp = {array[addr+3], array[addr+2], array[addr+1], array[addr]};
        axi_temp = {array[axi_A+3], array[axi_A+2], array[axi_A+1], array[axi_A]};
    end

    // Store instruction
    always_ff @(posedge clk) begin
        if (WDME) begin
            array[addr]     <= WD[7:0];
            array[addr + 1] <= WD[15:8];
            array[addr + 2] <= WD[23:16];
            array[addr + 3] <= WD[31:24];
        end
    end

    assign RD = temp;
    assign axi_RD = axi_temp;

endmodule
