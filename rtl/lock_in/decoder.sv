`default_nettype none
`timescale 1ns/1ns

`include "common.sv"

module decoder(
    input   wire                clk,
    input   wire                reset,
    input   warp_state_t        warp_state,

    input   instruction_t       instruction,

    output  reg                 decoded_reg_write_enable,
    output  reg                 decoded_mem_write_enable,
    output  reg                 decoded_mem_read_enable,
    output  reg                 decoded_branch,
    output  reg_input_mux_t     decoded_reg_input_mux,
    output  reg                 decoded_scalar_instruction,
    output  data_t              decoded_immediate,
    output  reg [4:0]           decoded_rd_address,
    output  reg [4:0]           decoded_rs1_address,
    output  reg [4:0]           decoded_rs2_address,
    output  alu_instruction_t   decoded_alu_instruction,

    output  reg                 decoded_halt,
    output  reg [1:0]           floatingRead,
    output  reg                 floatingWrite,
    output  reg                 decoded_sync
);
    // Extract fields from instruction
    wire [2:0]   opcode     = instruction[31:29];
    wire [4:0]   rd         = instruction[4:0];
    wire [2:0]   funct3     = instruction[12:10];
    wire [4:0]   rs1        = instruction[9:5];
    wire [3:0]   funct4     = instruction[13:10];
    wire [4:0]   rs2        = instruction[18:14];

    wire [13:0]  imm_i      = instruction[27:14];
    wire [14:0]  imm_load   = instruction[28:14];
    wire [14:0]  imm_s      = {instruction[28:19], instruction[4:0]};
    wire [15:0]  imm_b      = {instruction[28:19], instruction[13], instruction[4:0]};
    wire [31:12] imm_u      = instruction[28:9];
    wire [25:0]  imm_j      = {instruction[28:13], instruction[9:0]};

    always @(posedge clk) begin
        // $display("Instruction: %h", instruction);
        // $display("Decode Rs1: ", rs1);
        if (reset) begin
            // Set outputs to default values
            decoded_scalar_instruction <= 0;
            decoded_reg_write_enable <= 0;
            decoded_mem_write_enable <= 0;
            decoded_mem_read_enable <= 0;
            decoded_branch <= 0;
            decoded_reg_input_mux <= ALU_OUT;
            decoded_immediate <= {`DATA_WIDTH{1'b0}};
            decoded_rd_address <= 5'b0;
            decoded_rs1_address <= 5'b0;
            decoded_rs2_address <= 5'b0;
            decoded_halt <= 0;
            decoded_scalar_instruction <= 0;
            floatingRead <= 2'b00;
            floatingWrite <= 1'b0;
            decoded_sync    <= 0;
        end else if (warp_state == WARP_DECODE) begin
            // Default assignments for new decode
            decoded_reg_write_enable <= 0;
            decoded_reg_input_mux <= ALU_OUT;
            decoded_immediate <= {`DATA_WIDTH{1'b0}};
            decoded_rd_address <= 5'b0;
            decoded_rs1_address <= 5'b0;
            decoded_rs2_address <= 5'b0;
            decoded_mem_read_enable <= 0;
            decoded_mem_write_enable <= 0;
            decoded_branch <= 0;
            decoded_halt <= 0;
            decoded_scalar_instruction <= 0;
            floatingRead <= 2'b00;
            floatingWrite <= 1'b0;
            decoded_sync <= 0;

            if (opcode == `OPCODE_J) begin
                decoded_mem_read_enable     <= 0;
                decoded_mem_write_enable    <= 0;
                unique case (funct3) 
                    3'b000: begin
                        // Jump instr
                        decoded_alu_instruction     <= JAL;
                        $display("Decoding instruction 0b%32b", instruction);
                        decoded_immediate           <= sign_extend_26(imm_j);
                        decoded_scalar_instruction  <= 1;
                    end
                    3'b001: begin
                        // Branch instructions (e.g., BEQZ)
                        decoded_rs1_address         <= rs1;
                        decoded_rs2_address         <= rs2;
                        decoded_immediate           <= sign_extend_16(imm_b);
                        decoded_branch              <= 1;
                        decoded_alu_instruction     <= BEQZ;
                        decoded_scalar_instruction  <= 1;
                    end
                    3'b110: begin
                        // Sync instruction
                        decoded_sync                <= 1;
                        decoded_alu_instruction     <= SYNC;
                        decoded_scalar_instruction  <= 1;
                    end
                    3'b111: begin
                        // Exit instruction
                        decoded_halt <=1;
                        decoded_scalar_instruction <= 1;
                    end

                    default: $error("Invalid R-type instruction with funct3 %b", funct3);
                endcase                
            end else begin
                case (opcode)
                    `OPCODE_R: begin
                        // Vector R-type instructions
                        decoded_rd_address          <= rd;
                        decoded_rs1_address         <= rs1;
                        decoded_rs2_address         <= rs2;
                        decoded_reg_write_enable    <= 1;
                        decoded_reg_input_mux       <= ALU_OUT;
                        decoded_scalar_instruction  <= instruction[28];
                        decoded_mem_read_enable     <= 0;
                        decoded_mem_write_enable    <= 0;
                        floatingRead <= 2'b00;
                        floatingWrite <= 1'b0;

                        // Determine the ALU instruction
                        unique case (funct4)
                            4'b0000: decoded_alu_instruction <= ADD;
                            4'b0001: decoded_alu_instruction <= SUB;
                            4'b0010: decoded_alu_instruction <= MUL;
                            4'b0011: decoded_alu_instruction <= DIV;
                            4'b0100: decoded_alu_instruction <= SLT;
                            4'b0101: decoded_alu_instruction <= SLL;
                            4'b0110: decoded_alu_instruction <= SEQ;
                            4'b0111: decoded_alu_instruction <= SNEZ;
                            4'b1000: decoded_alu_instruction <= MIN;
                            4'b1001: decoded_alu_instruction <= ABS;
                            default: $error("Invalid R-type instruction with funct4 %b", funct4);
                        endcase
                    end
                    `OPCODE_I: begin
                        // Vector I-type instructions
                        decoded_rd_address <= rd;
                        decoded_rs1_address <= rs1;
                        decoded_reg_write_enable <= 1;
                        decoded_reg_input_mux <= ALU_OUT;
                        decoded_immediate <= sign_extend_14(imm_i);
                        decoded_scalar_instruction  <= instruction[28];
                        decoded_mem_read_enable     <= 0;
                        floatingRead <= 2'b00;
                        floatingWrite <= 1'b0;
                        decoded_mem_write_enable    <= 0;

                        unique case (funct4)
                            4'b0000: begin
                                decoded_alu_instruction <= ADDI;
                                if(rs1 == 5'b0) begin 
                                    //decoded_mem_read_enable     <= 1;
                                end else begin
                                    decoded_mem_read_enable     <= 0;
                                end
                            end
                            4'b0010: decoded_alu_instruction <= MULI;
                            4'b0011: decoded_alu_instruction <= DIVI;
                            4'b1010: decoded_alu_instruction <= SLLI;
                            default: $error("Invalid I-type instruction with funct4 %b", funct4);
                        endcase
                    end

                    `OPCODE_F: begin
                        // Vector F-type instructions
                        decoded_rd_address          <= rd;
                        decoded_rs1_address         <= rs1;
                        decoded_rs2_address         <= rs2;
                        decoded_reg_write_enable    <= 1;
                        decoded_reg_input_mux       <= ALU_OUT;
                        decoded_scalar_instruction  <= instruction[28];
                        decoded_mem_read_enable     <= 0;
                        decoded_mem_write_enable    <= 0;

                        unique case (funct4)
                            4'b0000: begin decoded_alu_instruction <= FADD; floatingRead <= 2'b11; floatingWrite <= 1'b1; end
                            4'b0001: begin decoded_alu_instruction <= FSUB; floatingRead <= 2'b11; floatingWrite <= 1'b1; end 
                            4'b0010: begin decoded_alu_instruction <= FMUL; floatingRead <= 2'b11; floatingWrite <= 1'b1; end
                            4'b0011: begin decoded_alu_instruction <= FDIV; floatingRead <= 2'b11; floatingWrite <= 1'b1; end
                            4'b0100: begin decoded_alu_instruction <= FSLT; floatingRead <= 2'b11; floatingWrite <= 1'b0; end // Write to int
                            4'b0101: begin decoded_alu_instruction <= FNEG; floatingRead <= 2'b01; floatingWrite <= 1'b1; end
                            4'b0110: begin decoded_alu_instruction <= FEQ; floatingRead <= 2'b11; floatingWrite <= 1'b0; end // Write to int
                            4'b0111: begin decoded_alu_instruction <= FMIN; floatingRead <= 2'b11; floatingWrite <= 1'b1; end
                            4'b1000: begin decoded_alu_instruction <= FABS; floatingRead <= 2'b01; floatingWrite <= 1'b1; end
                            4'b1001: begin decoded_alu_instruction <= FCVT_W_S; floatingRead <= 2'b01; floatingWrite <= 1'b0; end// float to int
                            4'b1010: begin decoded_alu_instruction <= FCVT_S_W; floatingRead <= 2'b01; floatingWrite <= 1'b1; end// int to floaat
                            default: $error("Invalid F-type instruction with funct4 %b", funct4);
                        endcase
                    end

                    `OPCODE_M: begin
                        unique case (funct3) 
                            3'b000: begin
                                // Load instructions (e.g., LW)
                                decoded_rd_address          <= rd;
                                decoded_rs1_address         <= rs1;
                                decoded_reg_write_enable    <= 1;
                                decoded_reg_input_mux       <= LSU_OUT;
                                decoded_immediate           <= sign_extend_15(imm_load);
                                decoded_mem_read_enable     <= 1;
                                decoded_alu_instruction     <= ADDI; // For computing effective address
                                decoded_scalar_instruction  <= instruction[13];
                                decoded_mem_write_enable    <= 0;
                                floatingRead <= 2'b00;
                                floatingWrite <= 1'b0;
                            end
                            3'b001: begin // SW
                                // $display("Decoded rs1: ", rs1);
                                // $display("Decoded rs2: ", rs2);
                                decoded_rs1_address         <= rs1;
                                decoded_rs2_address         <= rs2;
                                decoded_immediate           <= sign_extend_15(imm_s);
                                decoded_mem_write_enable    <= 1;
                                decoded_reg_write_enable <= 1'b0;
                                decoded_alu_instruction     <= ADDI; // For computing effective address
                                decoded_scalar_instruction  <= instruction[13];
                                floatingRead <= 2'b00;
                                floatingWrite <= 1'b0;
                                decoded_mem_read_enable     <= 0;
                            end
                            3'b010: begin
                                // Load instructions (e.g., FLW)
                                decoded_rd_address          <= rd;
                                decoded_rs1_address         <= rs1;
                                decoded_reg_write_enable    <= 1;
                                decoded_reg_input_mux       <= LSU_OUT;
                                decoded_immediate           <= sign_extend_15(imm_load);
                                decoded_mem_read_enable     <= 1;
                                decoded_alu_instruction     <= ADDI; // For computing effective address
                                decoded_scalar_instruction  <= instruction[13];
                                floatingRead              <= 2'b00; // Base address is INT
                                floatingWrite              <= 1'b1; //Write to float RD
                                decoded_mem_write_enable    <= 0;
                                
                            end
                            3'b011: begin 
                            // (FSW)
                                decoded_rs1_address         <= rs1;
                                decoded_rs2_address         <= rs2;
                                decoded_immediate           <= sign_extend_15(imm_s);
                                decoded_mem_write_enable    <= 1;
                                decoded_reg_write_enable <= 0;
                                decoded_alu_instruction     <= ADDI; // For computing effective address
                                decoded_scalar_instruction  <= instruction[13];
                                floatingRead              <= 2'b10; // Base is INT, data is FLOAT
                                floatingWrite               <= 1'b0;
                                decoded_mem_read_enable     <= 0;
                            end
                            default: $error("Invalid M-type instruction with funct4 %b", funct4);
                        endcase
                    end
                    `OPCODE_UP: begin
                        // LUI instruction
                        decoded_rd_address          <= rd;
                        decoded_immediate           <= {imm_u, 12'b0}; // Immediate value shifted left 12 bits
                        decoded_reg_write_enable    <= 1;
                        decoded_reg_input_mux       <= IMMEDIATE;
                        decoded_scalar_instruction  <= instruction[5];
                        decoded_mem_read_enable     <= 1;
                        floatingRead <= 2'b00;
                        floatingWrite <= 1'b0;
                        decoded_mem_write_enable    <= 0;
                    end
                    `OPCODE_SX_SLT: begin
                        decoded_rd_address          <= rd;
                        decoded_rs1_address         <= rs1;
                        decoded_rs2_address         <= rs2;
                        decoded_reg_write_enable    <= 1;
                        decoded_reg_input_mux       <= VECTOR_TO_SCALAR;
                    
                        decoded_scalar_instruction  <= 1'b0;
                        decoded_mem_read_enable     <= 0;
                        decoded_mem_write_enable    <= 0;

                        unique case (funct4)
                           4'b0000: decoded_alu_instruction <= SLT;
                           default: $error("Invalid X-type instruction with funct4 %b", funct4);
                        endcase
                    end
                    default: begin
                        // No operation or unrecognized opcode
                    end
                endcase
            end
        end
        // $display("Mem Read Enable: ", decoded_mem_write_enable);
        // $display("Opcode: ", opcode);
        // $display("Funct3: ", funct3);
        // $display("Decoded Alu: ", decoded_alu_instruction);
        // $display("Rs1: ", decoded_rs1_address);
        // $display("Branch: ", decoded_branch);
    end
endmodule
