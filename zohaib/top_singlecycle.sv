/* verilator lint_off SYNCASYNCNET */
module top (
    input logic clk,          // Clock signal
    input logic rst,          // Reset signal
    output logic [31:0] Result    // Contents of result (output)
);

    // Internal Signals
    logic [31:0] PC;                      // Program Counter
    logic [31:0] instr;                   // Current instruction
    logic [31:0] ImmOp;                   // Sign-extended immediate value
    logic [31:0] ALUop1, ALUop2, ALUout;  // ALU operands and result
    logic EQ;                             // Equality output from ALU
    logic [31:0] RD2, WD3;                // Register file read/write data
    logic RegWrite, ALUsrc, WD3Src;               // Control signals
    logic [1:0] PCsrc;                    // PC mux controls signal
    logic [2:0] ImmSrc;                   // 2-bit Immediate source signal
    logic [3:0] ALUctrl;                  // ALU control signal
    logic [31:0] ReadData;                // DataMemory output
    logic ResultSrc;                      // result mux control signal
    logic [4:0] AD3in;                    // AD3 input chooses between return reg and rd
    logic WDME, exit;


    // Program Counter
    program_counter #(.WIDTH(32)) PC_Reg (
        .clk(clk),
        .rst(rst),
        .PCsrc(PCsrc),
        .Result(Result),
        .ImmOp(ImmOp),
        .PC(PC)
    );

    // Instruction Memory 2.0 
    instr_mem #(
        .ADDRESS_WIDTH(32),
        .ADDRESS_REAL_WIDTH(12),
        .DATA_WIDTH(8),
        .DATA_OUT_WIDTH(32)
    ) InstructionMemory (
        .addr(PC),
        .instr(instr)
    );

    // Sign Extension Unit
    signextension #(
        .DATA_WIDTH(32)
    ) SignExtender (
        .instr(instr),
        .ImmSrc(ImmSrc),
        .ImmOp(ImmOp)
    );

    always_comb begin
        case(WD3Src) 
            1'b0: WD3 = Result;
            1'b1: WD3 = PC + 32'd4;
        endcase
    end

    // Register File with reset
    registerfile RegFile (
        .clk(clk),
        .rst(rst),
        .WE3(RegWrite),
        .AD1(instr[9:5]),
        .AD2(instr[18:14]),
        .AD3(instr[4:0]),
        .WD3(Result),
        .RD1(ALUop1),
        .RD2(RD2)
    );

    //Immediate ALU mux
    assign ALUop2 = ALUsrc ? ImmOp : RD2;

    // ALU
    alu ArithmeticLogicUnit (
        .ALUop1(ALUop1),
        .ALUop2(ALUop2),
        .ALUctrl(ALUctrl),
        .Result(ALUout),
        .EQ(EQ)
    );

    // Control Unit
    controlunit controlunit (
        .instr(instr),
        .EQ(EQ),
        .ALUctrl(ALUctrl),
        .ALUsrc(ALUsrc),
        .ImmSrc(ImmSrc),
        .PCsrc(PCsrc),
        .RegWrite(RegWrite),
        .ResultSrc(ResultSrc),
        .WD3Src(WD3Src),
        .WDME(WDME),
        .exit(exit) //currently disconnected (no input terminal)
    );
    
    //Data memory
    data_mem DataMemory (
        .clk(clk),
        .A(ALUout),
        .WDME(WDME),
        .WD(RD2),
        .RD(ReadData)
    );

    //Result mux
    always_comb begin
        case(ResultSrc)
            1'b0: Result = ALUout;
            1'b1: Result = ReadData;
        endcase
    end

    // Debug Output

    always_ff @(posedge clk) begin
        if (!rst) begin
            $display("PC = 0x%08x | Instr = 0x%08x | ALUop1 = %0d | ALUop2 = %0d | ALUout = %0d | Result = %0d | RegWrite = %0d",
                    PC, instr, ALUop1, ALUop2, ALUout, Result, RegWrite);
        end
    end

    
endmodule
