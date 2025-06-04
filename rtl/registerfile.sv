module registerfile (
    input  logic        clk,
    input  logic        rst,     // Add reset signal
    input  logic        WE3,     // Write enable
    input  logic [4:0]  AD1,     // Read register 1 address
    input  logic [4:0]  AD2,     // Read register 2 address
    input  logic [4:0]  AD3,     // Write register address
    input  logic [31:0] WD3,     // Write data
    output logic [31:0] RD1,     // Read data 1
    output logic [31:0] RD2     // Read data 2
    //output logic [31:0] a0
);

    logic [31:0] registers [31:0];

    // Initialize registers
    initial begin
        for (int i = 0; i < 32; i = i + 1)
            registers[i] = 32'h0;
    end

    // Synchronous write with reset (non-continuous)
    always_ff @(negedge clk) begin
        if (rst) begin
            // Clear all registers on reset
            for (int i = 0; i < 32; i = i + 1)
                registers[i] <= 32'h0;
        end
        else if (WE3 && (AD3 != 5'b0)) begin // Don't write to x0
            registers[AD3] <= WD3;
        end
    end

    // Asynchronous read (continuous)
    assign RD1 = (AD1 == 5'b0) ? 32'h0 : registers[AD1];
    assign RD2 = (AD2 == 5'b0) ? 32'h0 : registers[AD2];

    //assign a0 = registers[10];


endmodule
