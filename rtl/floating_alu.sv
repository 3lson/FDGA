`include "define.sv"
// =================================================
// A 32-bit Floating Point ALU
// with ADD, SUB, MUL, DIV, EQ, NEQ, ABS and SLT
// =================================================
module floating_alu (
    input  logic [3:0]  alu_op,
    input  logic [31:0] op1,
    input  logic [31:0] op2,
    output logic [31:0] result,
    output logic        cmp
);

// 1 sign bit, 8 exponent bits and 23 mantissa bits
logic op1_sign_bit, op2_sign_bit;
logic [7:0] op1_biased_exponent, op2_biased_exponent;
// 1 implicit leading 1 + 23 fraction bits
logic [23:0] op1_significand, op2_significand;

logic [7:0] exp_diff, result_exp;
logic [24:0] sum, mantissa_shift_op1, mantissa_shift_op2;
logic result_sign_bit;
logic guard_bit, round_bit, round_up;
logic [23:0] rounded_sum;
/* verilator lint_off UNUSED */
logic [47:0] product, quotient, numerator, denominator;
int shift_count;
logic sticky_bit;
logic [7:0] op1_unbiased_exponent;
logic [31:0] op1_int;
logic [31:0] abs_op1;
int significant_one;
/* verilator lint_on UNUSED */

//floating point divs internal regs
logic signed [8:0]  norm_exponent;
logic        [22:0] norm_mantissa;
logic        [8:0] intermediary_exponent;
logic signed [8:0] shift;

always_comb begin
    result = 32'd0;
    cmp = 0;

    op1_sign_bit = op1[31];
    
    // ADD instead
    op2_sign_bit = op2[31];
    op1_biased_exponent = op1[30:23];
    op2_biased_exponent = op2[30:23];
    // do we need a denormal check???
    op1_significand = {1'b1, op1[22:0]};
    op2_significand = {1'b1, op2[22:0]};

    exp_diff = 0;
    mantissa_shift_op1 = 0;
    mantissa_shift_op2 = 0;
    sum = 0;
    round_up = 0;
    rounded_sum = 0;
    product = 0;
    quotient = 0;
    result_exp = 0;
    result_sign_bit = 0;
    guard_bit = 0;
    round_bit = 0;
    shift_count = 0;
    numerator = 0;
    denominator = 0;
    sticky_bit = 0;
    shift_count = 0;
    op1_int = 0;
    op1_unbiased_exponent = 0;
    abs_op1 = 0;
    significant_one = 0;

    //Fdiv internal reg sets to 0
    norm_exponent = 0;
    norm_mantissa = 0;
    intermediary_exponent = 0;
    shift = 0;

    case (alu_op)
        `FALU_ADD, `FALU_SUB: begin
            if(alu_op == `FALU_SUB) op2_sign_bit = ~op2_sign_bit;

            if (op1_biased_exponent > op2_biased_exponent) begin
                exp_diff = op1_biased_exponent - op2_biased_exponent;
                // add 1'b0 to align mantissas after right shift
                mantissa_shift_op1 = {1'b0, op1_significand};
                mantissa_shift_op2 = {1'b0, op2_significand} >> exp_diff;
                result_exp = op1_biased_exponent;
                result_sign_bit = op1_sign_bit;
            end else begin
                exp_diff = op2_biased_exponent - op1_biased_exponent;
                mantissa_shift_op1 = {1'b0, op1_significand} >> exp_diff;
                mantissa_shift_op2 = {1'b0, op2_significand};
                result_exp = op2_biased_exponent;
                result_sign_bit = op2_sign_bit;
            end

            if (op1_sign_bit == op2_sign_bit) begin
                // Addition part
                sum = mantissa_shift_op1 + mantissa_shift_op2;

                if (sum[24]) begin
                    sum = sum >> 1;
                    result_exp = result_exp + 1;
                end else begin
                    for(int i=0; i<24; i++) begin
                        if(sum[23] == 1 || result_exp == 0)
                            break;
                        sum = sum << 1;
                        result_exp = result_exp - 1;
                    end
                end

                // Rounding
                guard_bit = sum[1];
                round_bit = sum[0];
                round_up = guard_bit & (round_bit | sum[2]);

                if (round_up) begin
                    rounded_sum = sum[23:0] + 1;
                    if (rounded_sum[23]) begin
                        result = {result_sign_bit, result_exp, rounded_sum[22:0]};
                    end else begin
                        result_exp = result_exp + 1;
                        result = {result_sign_bit, result_exp, rounded_sum[23:1]};
                    end
                end else begin
                    result = {result_sign_bit, result_exp, sum[22:0]};
                end

            end else begin
                // Subtraction part
                if (mantissa_shift_op1 >= mantissa_shift_op2) begin
                    sum = mantissa_shift_op1 - mantissa_shift_op2;
                    result_sign_bit = op1_sign_bit;
                end else begin
                    sum = mantissa_shift_op2 - mantissa_shift_op1;
                    result_sign_bit = op2_sign_bit;
                end

                if (sum == 0) begin
                    result = 32'd0;
                end else begin
                    for(int i=0; i < 24; i++) begin
                        if(sum[23] == 1 || result_exp ==0)
                            break;
                        sum = sum << 1;
                        result_exp = result_exp - 1;
                    end

                    guard_bit = sum[1];
                    round_bit = sum[0];
                    round_up = guard_bit & (round_bit | sum[2]);

                    if (round_up) begin
                        rounded_sum = sum[23:0] + 1;
                        if (rounded_sum[23]) begin
                            result = {result_sign_bit, result_exp, rounded_sum[22:0]};
                        end else begin
                            result_exp = result_exp + 1;
                            result = {result_sign_bit, result_exp, rounded_sum[23:1]};
                        end
                    end else begin
                        result = {result_sign_bit, result_exp, sum[22:0]};
                    end
                end
            end
        end

        `FALU_MUL: begin
            product = op1_significand * op2_significand;
            if (product[47]) begin
                result_exp = op1_biased_exponent + op2_biased_exponent - 127 + 1;
                rounded_sum = product[47:24];
                guard_bit = product[23];
                round_bit = product[22];
                round_up = guard_bit & (round_bit | product[21]);
            end else begin
                result_exp = op1_biased_exponent + op2_biased_exponent - 127;
                rounded_sum = product[46:23];
                guard_bit = product[22];
                round_bit = product[21];
                round_up = guard_bit & (round_bit | product[20]);
            end

            result_sign_bit = op1_sign_bit ^ op2_sign_bit;
            if (round_up) begin
                rounded_sum = rounded_sum + 1;
                if (rounded_sum[23]) begin
                    result = {result_sign_bit, result_exp, rounded_sum[22:0]};
                end else begin
                    result_exp = result_exp + 1;
                    result = {result_sign_bit, result_exp, rounded_sum[23:1]};
                end
            end else begin
                result = {result_sign_bit, result_exp, rounded_sum[22:0]};
            end
        end

        `FALU_DIV: begin
            numerator = {op1_significand, 24'd0};
            denominator = {24'd0, op2_significand};
            quotient = numerator / denominator;
            intermediary_exponent  = {1'b0, op1_biased_exponent} - {1'b0, op2_biased_exponent} + 9'sd127; //to accomodate for overflow
            result_sign_bit = op1_sign_bit ^ op2_sign_bit;

            /*
            NORMALIZATION
            */

            
            if(quotient[25])begin
                quotient = quotient >> 1;
                norm_exponent = intermediary_exponent + 9'sd1;
                norm_mantissa = quotient[23:1]; //implicit one removed 
            end
            else if(!quotient[24])begin
                shift = 9'sd0;
                for(int i = 24; i >= 0; i-- )begin
                    if(quotient[i])begin
                       break;
                    end
                    shift = shift + 9'sd1;
                end

                quotient = quotient << shift;
                
                norm_exponent = intermediary_exponent - shift;
                norm_mantissa = quotient[23:1];
            end
            else begin
                norm_exponent = intermediary_exponent;
                norm_mantissa = quotient[23:1];
            end
            
            //clamping
            if(norm_exponent <= 0) norm_exponent = 9'sd0;
            else if(norm_exponent >= 255) norm_exponent = 9'sd255;

            result = {result_sign_bit, norm_exponent[7:0], norm_mantissa};

        end

        `FALU_NEG: result = op1 ^ 32'h80000000; //negate the sign bit
        
        `FALU_ABS: result = {0'b0, op1[30:0]};

        `FALU_EQ: begin
            cmp = (op1 == op2) ? 1'b1:1'b0;
            result = (op1 == op2) ? 32'd1:32'd0;
        end

        `FALU_SLT: begin
            if (op1_sign_bit != op2_sign_bit) begin
                cmp = op1_sign_bit;
            end else if (op1_biased_exponent != op2_biased_exponent) begin
                cmp = (op1_biased_exponent < op2_biased_exponent) ^ op1_sign_bit;
            end else begin
                cmp = (op1_significand < op2_significand) ^ op1_sign_bit;
            end
        end

        `FALU_FCVT_WS: begin // Float to int conversion (FCVT_WS)
            if (op1_biased_exponent == 8'hFF) begin
                result = op1_sign_bit ? 32'h80000000 : 32'h7FFFFFFF;
            end else if (op1_biased_exponent == 0) begin
                result = 32'd0;
            end else begin
                op1_unbiased_exponent = op1_biased_exponent - 127;
                
                if (op1_unbiased_exponent < 0 || op1_unbiased_exponent > 127) begin
                    result = 32'd0;
                end else if (op1_unbiased_exponent > 30) begin
                    result = op1_sign_bit ? 32'h80000000 : 32'h7FFFFFFF;
                end else begin
                    if (op1_unbiased_exponent >= 23) begin
                        op1_int = op1_significand << (op1_unbiased_exponent - 23);
                    end else begin
                        op1_int = op1_significand >> (23 - op1_unbiased_exponent);
                    end
                    
                    result = op1_sign_bit ? -op1_int : op1_int;
                end
            end
        end

        `FALU_FCVT_SW: begin
            if (op1 == 32'd0) begin
                result = 32'd0;
            end else begin
                abs_op1 = op1[31] ? -op1 : op1;
                significant_one = -1;
                
                for(int i = 31; i>=0; i--) begin
                    if(abs_op1[i]) begin
                        significant_one = i;
                        break;
                    end
                end

                if(significant_one == -1) begin
                    result = 32'd0;
                end else begin
                
                op1_biased_exponent = 127 + significant_one;
                    if (significant_one >= 23) begin
                        op1_significand = abs_op1 >> (significant_one - 23);
                    end else begin
                        op1_significand = abs_op1 << (23 - significant_one);
                    end
                    
                    result = {op1[31], op1_biased_exponent, op1_significand[22:0]};
                end
            end
        end

        default: begin
            result = 32'd0;
            cmp = 0;
        end
    endcase
end
endmodule
