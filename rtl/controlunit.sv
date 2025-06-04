`include "define.sv"

module controlunit #(
    parameter DATA_WIDTH = 32
) (
    /* verilator lint_off UNUSED */
    input logic [DATA_WIDTH-1:0] instr,  // Instruction input
    output logic [3:0]          ALUctrl, // ALU control signal
    output logic                ALUsrc,  // ALU source (1 for immediate, 0 for register)
    output logic [2:0]          ImmSrc,  // Immediate source selection
    output logic                branch, 
    output logic [1:0]          Jump,      // 2 bit Jump signal: MSB for jump confirmation, LSB for Jump type
    output logic                RegWrite, // Register write enable
    output logic                ResultSrc, // control signal for output mux
    output logic                WD3Src,     // control unit signal for write port for register allowing Jump instruction implementation
    output logic                WDME, // Write Data Memory for store
    output logic                isLoadE,
    output logic                exit,
    output logic                floatingALU, //floating point flag
    output logic [1:0]          floatingRead,
    output logic                floatingWrite
);

    // Extract instruction fields
    logic [2:0] op;
    logic [3:0] funct4;
    logic [2:0] funct3;
    /* verilator lint_off UNUSED */
    logic predicate;

    assign op = instr[31:29];
    // predicate not used yet
    assign predicate = instr[28];
    assign funct4 = instr[13:10];
    assign funct3 = instr[12:10];

    // NOP detection: R-type, funct4=0000, rs1=0, rs2=0, rd=0
    logic is_nop;
    assign is_nop = (op == 3'b000) && (funct4 == 4'b0000) && 
                    (instr[9:5] == 5'b00000) && // rs1
                    (instr[18:14] == 5'b00000) && // rs2
                    (instr[4:0] == 5'b00000); // rd

    always_comb begin
        // Default values
        ALUctrl = `ALU_ADD;
        ALUsrc = 1'b0;
        ImmSrc = 3'b000;
        Jump = 2'b00;
        branch = 1'b0;
        ResultSrc = 1'b0;
        RegWrite = 1'b0;
        WD3Src = 1'b0;
        exit = 1'b0;
        WDME = 1'b0;
        isLoadE = 1'b0;
        floatingALU = 1'b0;
        floatingRead = 2'b00;
        floatingWrite = 1'b0;
        if (is_nop) begin
            //Do nothing
        end
        else  begin

            case (op)
                `Rtype: begin 
                    case (funct4)
                        `ALU_ADD: ALUctrl = `ALU_ADD; 
                        `ALU_SUB: ALUctrl = `ALU_SUB; 
                        `ALU_MUL: ALUctrl = `ALU_MUL; 
                        `ALU_DIV: ALUctrl = `ALU_DIV; 
                        `ALU_SLT: ALUctrl = `ALU_SLT; 
                        `ALU_SGT: ALUctrl = `ALU_SGT;
                        `ALU_SEQ: ALUctrl = `ALU_SEQ;
                        `ALU_SNEZ: ALUctrl = `ALU_SNEZ; 
                        `ALU_MIN: ALUctrl = `ALU_MIN;
                        `ALU_ABS: ALUctrl = `ALU_ABS;
                        default: ALUctrl = `ALU_ADD;
                    endcase
                    RegWrite = 1'b1;
                    ALUsrc = 1'b0;
                end

                `Itype: begin 
                    case (funct4)
                        `ALU_ADD: ALUctrl = `ALU_ADD; 
                        `ALU_MUL: ALUctrl = `ALU_MUL; 
                        `ALU_DIV: ALUctrl = `ALU_DIV;
                        `ALU_SLLI: ALUctrl = `ALU_SLLI;
                        default: ALUctrl = `ALU_ADD;
                    endcase
                    RegWrite = 1'b1;
                    ALUsrc = 1'b1;
                    ImmSrc = 3'b000;
                end

                `Mtype: begin 
                    case (funct4)
                        // LOAD
                        4'b0000: begin
                                ALUctrl = `ALU_ADD; // ADD for address calculation
                                RegWrite = 1'b1;
                                ALUsrc = 1'b1;
                                ImmSrc = 3'b001;
                                ResultSrc = 1'b1;
                                WDME = 1'b0;
                                isLoadE = 1'b1;
                        end

                        //FLOATING LOAD
                        4'b1000: begin
                            ALUctrl = `ALU_ADD; // ADD for address calculation
                            RegWrite = 1'b1;
                            ALUsrc = 1'b1;
                            ImmSrc = 3'b001;
                            ResultSrc = 1'b1;
                            WDME = 1'b0;
                            isLoadE = 1'b1;
                            floatingRead = 2'b00;
                            floatingWrite = 1'b1;
                        end

                        // STORE
                        4'b0001: begin
                                WDME = 1'b1;
                                isLoadE = 1'b0;
                                ALUctrl = `ALU_ADD; // ADD for address calculation
                                ALUsrc = 1'b0; // Uses rd2
                                ImmSrc = 3'b010; // Store immediate
                                ALUsrc = 1'b1;
                        end

                        //FLOATING STORE
                        4'b1001: begin
                                WDME = 1'b1;
                                isLoadE = 1'b0;
                                ALUctrl = `ALU_ADD; // ADD for address calculation
                                ALUsrc = 1'b0; // Uses rd2
                                ImmSrc = 3'b010; // Store immediate
                                ALUsrc = 1'b1;
                                floatingRead = 2'b10;
                                floatingWrite = 1'b1;
                        end
                        default: begin
                                WDME = 1'b0;
                                isLoadE = 1'b0;
                                ALUctrl = `ALU_ADD;
                                ALUsrc = 1'b0;
                                ImmSrc = 3'b000;
                        end
                    endcase
                end

                `Ctype: begin
                    case (funct3)
                    // JUMP
                    3'b000: begin
                        ImmSrc = 3'b110;
                        ALUsrc = 1'b1;
                        RegWrite =1'b1;
                        Jump = 2'b10;
                        WD3Src = 1'b1;
                    end

                    // BRANCH
                    3'b001: begin
                        ALUctrl = `ALU_SEQ;
                        branch = 1'b1;
                        ImmSrc = 3'b011;
                    end

                    // CALL 
                    3'b010: begin
                        ImmSrc = 3'b100;
                        Jump = 2'b10;
                        WD3Src = 1'b1;
                        RegWrite = 1'b1;
                    end

                    // RET (ADDI X1 0)
                    3'b011: begin
                        ALUctrl = `ALU_ADD;  
                        Jump = 2'b11;    // return from saved address in register
                        RegWrite = 1'b0;
                        ALUsrc = 1'b1;
                        ImmSrc = 3'b000;
                    end

                    // SYNC
                    3'b110: begin 
                        RegWrite = 1'b0;
                    end

                    // EXIT
                    3'b111: begin 
                        exit = 1'b1;
                        RegWrite = 1'b0;
                    end
                    default: begin
                        exit = 1'b0;
                        RegWrite = 1'b0;
                    end
                    endcase
                end
                `Ptype: begin
                    ImmSrc = 3'b101;
                    ALUsrc = 1'b1;
                    RegWrite = 1'b1;
                end

                `Ftype: begin
                    floatingALU = 1'b1;
                    ALUsrc = 1'b0;
                    RegWrite = 1'b1;
                    floatingRead = 2'b01;
                    floatingWrite = 1'b1;

                    case(funct4)
                    `FALU_ADD: ALUctrl = `FALU_ADD;
                    `FALU_SUB: ALUctrl = `FALU_SUB;
                    `FALU_MUL: ALUctrl = `FALU_MUL;
                    `FALU_DIV: ALUctrl = `FALU_DIV;
                    `FALU_SLT: ALUctrl = `FALU_SLT; 
                    `FALU_EQ: ALUctrl = `FALU_EQ;
                    `FALU_NEG: ALUctrl = `FALU_NEG;
                    `FALU_ABS: ALUctrl = `FALU_ABS; 
                    `FALU_FCVT_WS: begin
                        ALUctrl = `FALU_FCVT_WS;
                        floatingRead = 2'b01;
                        floatingWrite = 1'b0;
                    end
                    `FALU_FCVT_SW: begin
                        ALUctrl = `FALU_FCVT_SW;
                        floatingRead = 2'b00;
                        floatingWrite =  1'b1;
                    end
                    default ALUctrl = `FALU_ADD;
                    endcase
                end

                default: begin
                    ALUctrl = `ALU_ADD;
                    ALUsrc = 1'b0;
                    ImmSrc = 3'b000;
                    branch = 1'b0;
                    Jump = 2'b00;
                    RegWrite = 1'b0;
                    ResultSrc = 1'b0;
                    WD3Src = 1'b0;
                    WDME = 1'b0;
                    isLoadE = 1'b0;
                    exit = 1'b0;
                    floatingALU = 1'b0;
                    floatingRead = 2'b00;
                    floatingWrite = 1'b0;
                end
            endcase
        end
    end
endmodule
