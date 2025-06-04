module top #(
    parameter WIDTH = 32
)(
    input logic clk,
    input logic rst,

    //BRAM interface to data memory (Data BRAM)
    output logic dm_we,
    output logic [WIDTH-1:0] dm_addr,
    output logic [WIDTH-1:0] dm_wdata,
    input logic [WIDTH-1:0] dm_rdata,

    //BRAM interface to instruction memory (Instr BRAM)
    output logic [WIDTH-1:0] im_addr,
    input logic [WIDTH-1:0] im_instr

    //Output for debug
    //output logic [WIDTH-1:0] a0
);
    // Program counter
    logic [1:0] PCsrcE;

    logic [WIDTH-1:0] ImmExtD;
    logic [WIDTH-1:0] ImmExtE;

    logic [WIDTH-1:0] PCPlus4F;
    logic [WIDTH-1:0] PCPlus4D;
    logic [WIDTH-1:0] PCPlus4E;
    logic [WIDTH-1:0] PCPlus4M;
    logic [WIDTH-1:0] PCPlus4W;

    // ALU    
    logic [3:0] ALUctrlD;
    logic [3:0] ALUctrlE;
    
    logic [WIDTH-1:0] SrcAE;
    logic [WIDTH-1:0] SrcBE;
    
    logic ALUsrcD;
    logic ALUsrcE;
    
    logic EQ;

    //floating point compare flag
    logic cmp;
    
    logic [WIDTH-1:0] PCF;
    logic [WIDTH-1:0] PCD;
    logic [WIDTH-1:0] PCE;

    //respective outputs of both ALU's
    logic [WIDTH-1:0] ALUint;
    logic [WIDTH-1:0] ALUfloat;

    logic [WIDTH-1:0] ALUResultE;
    logic [WIDTH-1:0] ALUResultM;
    logic [WIDTH-1:0] ALUResultW;

    logic WD3SrcD;
    logic WD3SrcE;
    logic WD3SrcW;
    logic WD3SrcM;

    // Instruction Memory
    logic [WIDTH-1:0] instrF;
    logic [WIDTH-1:0] instrD;

    // Regfile
    logic [4:0] Rs1D;
    logic [4:0] Rs1E;
    logic [4:0] Rs2D;
    logic [4:0] Rs2E;
    logic [4:0] Rs2M;

    logic [4:0] RdD;
    logic [4:0] RdE;
    logic [4:0] RdM;
    logic [4:0] RdW;

    logic [4:0] WriteAddr;


    //outputs of reg files
    logic [WIDTH-1:0] floatingRD1;
    logic [WIDTH-1:0] floatingRD2;
    logic [WIDTH-1:0] integerRD1;
    logic [WIDTH-1:0] integerRD2;

    logic [WIDTH-1:0] RD1D;
    logic [WIDTH-1:0] RD1E;
    logic [WIDTH-1:0] RD2D;
    logic [WIDTH-1:0] RD2E;

    // Data memory
    logic  WDMED;
    logic  WDMEE;
    logic  WDMEM;

    logic isLoadD;
    logic isLoadE;
    logic isLoadM;

    logic [WIDTH-1:0] WriteDataE;
    logic [WIDTH-1:0] WriteDataM;
    
    //intermediary value
    logic [WIDTH-1:0] WriteDataIN;

    logic [WIDTH-1:0] ReadDataM;
    logic [WIDTH-1:0] ReadDataW;

    logic RegWriteD;
    logic RegWriteE;
    logic RegWriteM;
    logic RegWriteW;

    //Write back to correct register type
    logic IntWE;
    logic FloatWE;

    // Sign Extend
    logic [2:0] ImmSrcD;

    // Result
    logic ResultSrcD;
    logic ResultSrcE;
    logic ResultSrcM;
    logic ResultSrcW;

    // Branch
    logic  branchD;
    logic  branchE;

    // Jump
    logic [1:0] JumpD;
    logic [1:0] JumpE;

    // Hazard Unit
    logic [1:0] forwardAE;
    logic [1:0] forwardBE;
    logic stall;

    //flush relatead signals
    logic flush; 

    //UNIQUE INTERNAL TOP SIGNAL
    logic [WIDTH-1:0] WD3W;

    logic exit;

    logic floatingALUD;
    logic floatingALUE;

    //floating Reg control signals 
    logic [1:0]  floatingReadD;

    logic floatingWriteD;
    logic floatingWriteE;
    logic floatingWriteW;

    //initialize pipeline
    initial begin
        WDMED = 1'b0;
        WDMEE = 1'b0;
        WDMEM = 1'b0;
        isLoadD = 1'b0;
        isLoadE = 1'b0;
        isLoadM = 1'b0;
    end

    // Pipeline Stage 1 - Fetch (FEC)
    
    //Completed 
    assign PCPlus4F = PCF + 4;    
    program_counter program_counter_inst (
        .clk(clk),
        .rst(rst),
        .stall(stall),
        .PCsrc(PCsrcE),
        .ImmOp(ImmExtE),
        .PCE(PCE),
        .Result(ALUResultE), //JALR and RET instructions
        .PC(PCF)
    );

    // Replaced with BRAM mapping 
    assign instrF = im_instr; // Internal instruction signal
    assign im_addr = PCF; // Address into instruction BRAM

    //*
    pipeline_FECtoDEC pipeline_FECtoDEC (
        .clk(clk),
        .flush(flush),
        .stall(stall),
        .instrF(instrF),
        .PCF(PCF),
        .PCPlus4F(PCPlus4F),

        .instrD(instrD),
        .PCD(PCD),
        .PCPlus4D(PCPlus4D)
    );

    // Pipeline Stage 2 - Decode (DEC)

    //Come back to this
    controlunit controlunit (
        .instr(instrD),
       
        .ALUctrl(ALUctrlD),
        .ALUsrc(ALUsrcD),
        .ImmSrc(ImmSrcD),
        .RegWrite(RegWriteD),
        .branch(branchD),
        .Jump(JumpD),
        .WDME(WDMED),
        .isLoadE(isLoadD),
        .ResultSrc(ResultSrcD),
        .WD3Src(WD3SrcD),
        .exit(exit),
        .floatingALU(floatingALUD),
        .floatingRead(floatingReadD),
        .floatingWrite(floatingWriteD)

    );

    //Completed
    signextension #(
        .DATA_WIDTH(32)
    ) SignExtender (
        .instr(instrD),
        .ImmSrc(ImmSrcD),

        .ImmOp(ImmExtD)
    );

    assign Rs1D = instrD[9:5];
    assign Rs2D = instrD[18:14];
    assign RdD = instrD[4:0];
    
    //Completed
    registerfile IntegerRegFile (
        .clk(clk),
        .rst(rst),
        .AD1(Rs1D),
        .AD2(Rs2D),
        .AD3(WriteAddr),
        .WE3(IntWE),
        .WD3(WD3W), //will be subject to change

        .RD1(integerRD1),
        .RD2(integerRD2)
        //.a0(a0)
    );

    // Register type selection muxes:

    //Read operand mux
    always_comb begin
        case(floatingReadD)
        2'b00: {RD1D,RD2D} = {integerRD1,integerRD2};
        2'b01: {RD1D,RD2D} = {floatingRD1,floatingRD2};
        2'b10: {RD1D,RD2D} = {integerRD1,floatingRD2}; //FLOATING STORE 
        default: {RD1D,RD2D} = {integerRD1,integerRD2};
        endcase
    end

    //Write operand mux
    assign {IntWE,FloatWE} = (floatingWriteW) ? {1'b0,RegWriteW}:{RegWriteW,1'b0};

    registerfile FloatingRegFile (
        .clk(clk),
        .rst(rst),
        .AD1(Rs1D),
        .AD2(Rs2D),
        .AD3(WriteAddr),
        .WE3(FloatWE),
        .WD3(WD3W), //will be subject to change

        .RD1(floatingRD1),
        .RD2(floatingRD2)
    );

    //JUMP/CALL instruction write register mux
    assign WriteAddr = WD3SrcW ? 5'd1:RdW;

    //Completed
    pipeline_DECtoEXE pipeline_DECtoEXE (
        .clk(clk),
        .stall(stall),
        .flush(flush),
        .RD1D(RD1D),
        .RD2D(RD2D),
        .PCD(PCD),
        .Rs1D(Rs1D),
        .Rs2D(Rs2D),
        .RdD(RdD),
        .ExtImmD(ImmExtD),
        .PCPlus4D(PCPlus4D),
        .floatingWriteD(floatingWriteD),

        .RD1E(RD1E),
        .RD2E(RD2E),
        .PCE(PCE),
        .Rs1E(Rs1E),
        .Rs2E(Rs2E),
        .RdE(RdE),
        .ExtImmE(ImmExtE),
        .PCPlus4E(PCPlus4E),
        .floatingWriteE(floatingWriteE),

        .RegWriteD(RegWriteD),
        .ResultSrcD(ResultSrcD),
        .WDMED(WDMED),
        .isLoadD(isLoadD),
        .ALUctrlD(ALUctrlD),
        .ALUsrcD(ALUsrcD),
        .WD3SrcD(WD3SrcD),
        .branchD(branchD),
        .JumpD(JumpD),
        .floatingALUD(floatingALUD),

        .RegWriteE(RegWriteE),
        .ResultSrcE(ResultSrcE),
        .WDMEE(WDMEE),
        .isLoadE(isLoadE),
        .ALUctrlE(ALUctrlE),
        .ALUsrcE(ALUsrcE),
        .WD3SrcE(WD3SrcE),
        .branchE(branchE),
        .JumpE(JumpE),
        .floatingALUE(floatingALUE)
    );

    // Pipeline Stage 3 - Execute (EXE)

    //PCsrcE logic
    always_comb begin
        //JALR/RET PC value implementation
        if(JumpE == 2'b11) begin
            PCsrcE = 2'b10;
        end
        else if(JumpE == 2'b10)begin
            PCsrcE = 2'b01;
        end
        else if((branchE == 1'b1) && (EQ == 1'b1)) begin
            PCsrcE = 2'b01;
        end
        //standard PC incrementation for normal instructions
        else begin
            PCsrcE = 2'b00;
        end
    end

    //forwarding mux logic

    //RD1 mux
    always_comb begin
        case(forwardAE)
        2'b00: SrcAE = preserveAE ? reserveAE:RD1E;
        2'b01: SrcAE = ALUResultM;
        2'b10: SrcAE = WD3W;
        default: SrcAE = RD1E;
        endcase
    end

        //RD2 mux
    always_comb begin
        case(forwardBE)
        2'b00: WriteDataE = preserveBE ? reserveBE:RD2E;
        2'b01: WriteDataE = ALUResultM;
        2'b10: WriteDataE = WD3W;
        default: WriteDataE = RD2E;
        endcase
    end


    logic [31:0] reserveAE, reserveBE;
    logic reqAE, reqBE;
    logic preserveAE, preserveBE;

    always_comb begin
        reserveAE = 32'b0;
        reqAE = 1'b0;
        if((forwardAE == 2'b10)  && stall)begin
            reserveAE = SrcAE;
            reqAE = 1'b1;
        end
        else begin
            reserveAE = SrcAE;
            reqAE = 1'b0;
        end
    end

    always_comb begin
        reserveBE = 32'b0;
        reqBE = 1'b0;
        if((forwardBE == 2'b10)  && stall)begin
            reserveBE = SrcBE;
            reqBE = 1'b1;
        end
        else begin
            reserveBE = SrcBE;
            reqBE = 1'b0;
        end
    end

    preserveFSM preserveA(
        .clk(clk),
        .rst(rst),
        .req(reqAE),
        .preserve(preserveAE)
    );

    preserveFSM preserveB(
        .clk(clk),
        .rst(rst),
        .req(reqBE),
        .preserve(preserveBE)
    );

    //Completed
    assign SrcBE = ALUsrcE ? ImmExtE : WriteDataE;

    //Completed
    alu IntArithLogicUnit (
        .ALUop1(SrcAE),
        .ALUop2(SrcBE),
        .ALUctrl(ALUctrlE),

        .EQ(EQ),
        .Result(ALUint)
    );

    floating_alu FloatingArithLogicUnit (
        .op1(SrcAE),
        .op2(SrcBE),
        .alu_op(ALUctrlE),

        .cmp(cmp),
        .result(ALUfloat)
    );

    assign ALUResultE = floatingALUE ? ALUfloat:ALUint ;

    //Completed
    pipeline_EXEtoMEM pipeline_EXEtoMEM (
        .clk(clk),
        .stall(stall),
        .ALUResultE(ALUResultE),
        .WriteDataE(WriteDataE),
        .RdE(RdE),
        .PCPlus4E(PCPlus4E),
        .Rs2E(Rs2E),

        .ALUResultM(ALUResultM),
        .WriteDataM(WriteDataM),
        .RdM(RdM),
        .PCPlus4M(PCPlus4M),
        .Rs2M(Rs2M),

        .RegWriteE(RegWriteE),
        .ResultSrcE(ResultSrcE),
        .WDMEE(WDMEE),
        .isLoadE(isLoadE),
        .WD3SrcE(WD3SrcE),
        .floatingWriteE(floatingWriteE),

        .RegWriteM(RegWriteM),
        .ResultSrcM(ResultSrcM),
        .WDMEM(WDMEM),
        .isLoadM(isLoadM),
        .WD3SrcM(WD3SrcM),
        .floatingWriteM(floatingWriteM) 
    );

    // Pipeline Stage 4 - Memory (MEM)

    // BRAM Data Memory

    assign dm_we = WDMEM;
    assign dm_addr = ALUResultM;
    assign dm_wdata = WriteDataIN;
    assign ReadDataM = dm_rdata; // Feed BRAM read value back into pipeline

    assign WriteDataIN = (Rs2M == RdW) ? WD3W:WriteDataM;

    

    //Completed
    pipeline_MEMtoWB pipeline_MEMtoWB (
        .clk(clk),
        .ALUResultM(ALUResultM),
        .ReadDataM(ReadDataM),
        .RdM(RdM),
        .PCPlus4M(PCPlus4M),

        .ALUResultW(ALUResultW),
        .ReadDataW(ReadDataW),
        .RdW(RdW),
        .PCPlus4W(PCPlus4W),

        .RegWriteM(RegWriteM),
        .ResultSrcM(ResultSrcM),
        .WD3SrcM(WD3SrcM),
        .floatingWriteM(floatingWriteM),

        .RegWriteW(RegWriteW),
        .ResultSrcW(ResultSrcW),
        .WD3SrcW(WD3SrcW),
        .floatingWriteW(floatingWriteW),

        .flush(flush)
    );

    // Pipeline Stage 5 - Writeback (WB)

    // Hazard Unit
    hazardunit hazard_unit (
        .Rs1E(Rs1E),
        .Rs2E(Rs2E),
        .RdM(RdM),
        .RdW(RdW),
        .WDMEM(WDMEM),
        .isLoadM(isLoadM),
        .branchE(branchE),
        .JumpE(JumpE),
        .EQ(EQ),

        .forwardAE(forwardAE),
        .forwardBE(forwardBE),
        .stall(stall),
        .flush(flush)
    );


    //Register Write back logic (Muxes)

    //WD3 mux
    always_comb begin
        casez({WD3SrcW,ResultSrcW})
            2'b1?: WD3W = PCPlus4W;
            2'b01: WD3W = ReadDataW;
            2'b00: WD3W = ALUResultW;
            default: WD3W = ALUResultW;
        endcase
    end


    // always_ff @(posedge clk) begin
    //     if (!rst) begin
    //         $display("PCF=%h, instrF=%h", PCF, instrF);
    //         $display("PCD=%h, instrD=%h, ImmSrcD=%h", PCD, instrD, ImmSrcD);
    //         $display("PCE=%h, ALUResultE=%h, ALUfloat=%h", PCE, ALUResultE, ALUfloat);
    //         $display("ALUctrlE=%b, SrcAE=%h, SrcBE=%h", ALUctrlE, SrcAE, SrcBE);
    //         $display("ALUResultM=%h, ReadDataM=%h", ALUResultM, ReadDataM);
    //         $display( "WD3W=%h", WD3W);
    //         $display("Register a0 output: a0=%h", a0);
    //         $display("------------------------------------------------");
    //     end
    // end


endmodule
