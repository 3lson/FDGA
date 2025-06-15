#include <iostream>
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <bitset>
#include <vector>
#include <iomanip>
#include <filesystem>
#include <map>
#include <string_view> // NEW: For efficient prefix checking

using namespace std;

// The assembler works by breaking down the instr sw ra, 268(sp)
// args[0] = sw, args[1] = ra, args[2] = 268(sp)

// MODIFIED: Renamed maps for clarity
unordered_map<string, int> int_scalar_registerMap;
unordered_map<string, int> float_scalar_registerMap;
unordered_map<string, int> int_vector_registerMap;
unordered_map<string, int> float_vector_registerMap;

// MODIFIED: No need for opcode in the value, it's determined by the map name
unordered_map<string, int> rTypeFunctMap = {
    {"add", 0b0000}, {"sub", 0b0001}, {"mul", 0b0010},
    {"div", 0b0011}, {"slt", 0b0100}, {"sll", 0b0101},
    {"seq", 0b0110}, {"snez", 0b0111}, {"min", 0b1000},
    {"abs", 0b1001}
};

unordered_map<string, int> iTypeFunctMap = {
    {"addi", 0b0000}, {"muli", 0b0010}, {"slli", 0b1010},
    {"divi", 0b0011}, {"seqi", 0b1011}
};

unordered_map<string, int> fTypeFunctMap = {
    {"fadd.s", 0b0000}, {"fsub.s", 0b0001}, {"fmul.s", 0b0010},
    {"fdiv.s", 0b0011}, {"flt.s", 0b0100}, {"fneg.s", 0b0101},
    {"feq.s",  0b0110}, {"fmin.s", 0b0111}, {"fabs.s", 0b1000},
    {"fcvt.w.s", 0b1001}, {"fcvt.s.w", 0b1010}
};

unordered_map<string, int> cTypeFunctMap = {
    {"j", 0b000}, {"beqz", 0b001}, {"beqo", 0b010},
    {"ret", 0b011}, {"sync", 0b110}, {"exit", 0b111}
};

unordered_map<string, int> xTypeFunctMap = {
    {"slt", 0b0000} // Our funct4 for sx.slt
};


map<string, int> labelMap;
map<string, uint32_t> dataMap; 
map<string, uint32_t> labelDataValues;

void initregisterMap() {
    // Int scalar register file
    for (int i = 0; i < 32; ++i) int_scalar_registerMap["x" + to_string(i)] = i;
    int_scalar_registerMap["zero"] = 0; int_scalar_registerMap["ra"] = 1; int_scalar_registerMap["sp"] = 2;
    int_scalar_registerMap["gp"] = 3; int_scalar_registerMap["tp"] = 4; int_scalar_registerMap["s0"] = 5;
    for(int i = 1; i <= 26; ++i) int_scalar_registerMap["s" + to_string(i)] = 5 + i;
    // Special int scalar regs (compiler will use these names with s.)
    int_scalar_registerMap["threadIdx"] = 29;
    int_scalar_registerMap["blockIdx"] = 30;
    int_scalar_registerMap["block_size"] = 31;


    // Float scalar register file
    for(int i = 0; i <= 31; ++i) float_scalar_registerMap["f" + to_string(i)] = i; // Use 'f' or 'fs' for clarity
    for(int i = 0; i <= 31; ++i) float_scalar_registerMap["fs" + to_string(i)] = i; // Alias with fs prefix

    // Int vector register file
    for(int i = 0; i <= 31; ++i) int_vector_registerMap["x" + to_string(i)] = i; // Placeholder for xN mapping
    int_vector_registerMap["zero"] = 0; // Vector zero
    for(int i = 1; i <= 31; ++i) int_vector_registerMap["v" + to_string(i)] = i;

    // Float vector register file
    for(int i = 0; i <= 31; ++i) float_vector_registerMap["f" + to_string(i)] = i; // Placeholder for fN mapping
    float_vector_registerMap["zero"] = 0; // Vector zero
    for(int i = 1; i <= 31; ++i) float_vector_registerMap["fv" + to_string(i)] = i;
    
}

int resolveSymbol(const string& expr, const map<string, int>& symbolTable) {
    if (expr.rfind("%hi(", 0) == 0 && expr.back() == ')') {
        string sym = expr.substr(4, expr.size() - 5);
        auto it = symbolTable.find(sym);
        if (it == symbolTable.end()) {
            cerr << "Undefined symbol in %hi(): " << sym << endl;
            return 0;
        }
        int32_t addr = it->second;
        int32_t result = (addr + 0x800) >> 12;
        // cout << "  %hi(" << sym << "): addr=0x" << hex << addr << " => upper 20 bits=0x" << result << dec << endl;
        return result;
    }
    if (expr.rfind("%lo(", 0) == 0 && expr.back() == ')') {
        string sym = expr.substr(4, expr.size() - 5);
        auto it = symbolTable.find(sym);
        if (it == symbolTable.end()) {
            cerr << "Undefined symbol in %lo(): " << sym << endl;
            return 0;
        }
        int32_t addr = it->second;
        int32_t result = addr & 0xFFF;
        // cout << "  %lo(" << sym << "): addr=0x" << hex << addr << " => lower 12 bits=0x" << result << dec << endl;
        return result;
    }
    if (expr.find_first_not_of("0123456789-") == string::npos) {
         try { return stoi(expr); } catch(...) { return 0; } // Added try/catch
    }
    auto it = symbolTable.find(expr);
    if (it != symbolTable.end()) return it->second;
    cerr << "Undefined symbol: " << expr << endl;
    return 0;
}

vector<string> tokenize(const string& line) {
    string cleaned;
    for (char c : line) cleaned += (c == ',' ? ' ' : c);
    istringstream iss(cleaned);
    vector<string> tokens;
    string tok;
    while (iss >> tok) tokens.push_back(tok);
    return tokens;
}

int getImm(const string& s) {
    // Using resolveSymbol directly now
    return resolveSymbol(s, labelMap);
}

// MODIFIED: All encode functions now take a `bool is_scalar` argument
uint32_t encodeRType(string op, const vector<string>& args, bool is_scalar) {
    int opcode = 0b000;
    uint32_t scalar_bit = is_scalar ? 1 : 0;
    int funct4 = rTypeFunctMap[op];

    // R-type operates on Integer registers
    int rd  = is_scalar ? int_scalar_registerMap[args[0]] : int_vector_registerMap[args[0]];
    int rs1 = is_scalar ? int_scalar_registerMap[args[1]] : int_vector_registerMap[args[1]];
    // RS2 is don't care for snez and abs, otherwise it's required
    int rs2 = (op == "abs" || op == "snez") ? 0 : (is_scalar ? int_scalar_registerMap[args[2]] : int_vector_registerMap[args[2]]);

    return (opcode << 29) | (scalar_bit << 28) | (rs2 << 14) | (funct4 << 10) | (rs1 << 5) | rd;
}

uint32_t encodeIType(string op, const vector<string>& args, bool is_scalar) {
    int opcode = 0b001;
    uint32_t scalar_bit = is_scalar ? 1 : 0;
    int funct4 = iTypeFunctMap[op];
    
    // I-type operates on Integer registers
    int rd  = is_scalar ? int_scalar_registerMap[args[0]] : int_vector_registerMap[args[0]];
    int rs1 = is_scalar ? int_scalar_registerMap[args[1]] : int_vector_registerMap[args[1]];
    
    int imm_val = getImm(args[2]);
    uint32_t imm = 0;

    if (op == "slli") {
        // slli takes a 5-bit unsigned immediate
        imm = imm_val & 0x1F; 
         // The 5-bit immediate for SLLI is typically in the IMM[4:0] field,
         // but the ISA table says IMM[13:0]. This is a conflict.
         // Assuming the ISA table is correct and it uses IMM[13:0] where only bits 4:0 are relevant.
         // If it means uimm[4:0] *in* the IMM[13:0] field, that's odd.
         // RISC-V RV32I SLLI uses imm[4:0] in the rs2 field (bits 19:15).
         // Your ISA puts IMM[13:0] in bits [27:14]. Let's place the 5-bit uimm there.
         imm = (imm_val & 0x1F); // This seems wrong based on IMM[13:0] mapping...
         // Re-reading the I-type table: IMM [13:0] (14 bits) is bits [27:14].
         // The note "uimm means 5-bit unsigned immediate (i.e IMM[4:0])" suggests only the lower 5 bits of this 14-bit field matter for SLLI.
         // So we mask the immediate to 5 bits, and place it in the lower part of the 14-bit field.
         imm = (imm_val & 0x1F); // Keep only lower 5 bits
         // The rest of the IMM[13:0] field (bits 13:5) should be 0 for SLLI according to RISC-V UIMM convention
         imm = (imm & 0x1F); // Use only bits 4:0, bits 13:5 are implicitly 0 within the 14-bit field

    } else {
        // Other I-types take a 14-bit signed immediate
        imm = imm_val & 0x3FFF; // 14-bit imm
    }

    return (opcode << 29) | (scalar_bit << 28) | (imm << 14) | (funct4 << 10) | (rs1 << 5) | rd;
}

// MODIFIED: encodeFType updated to handle mixed register types correctly
uint32_t encodeFType(string op, const vector<string>& args, bool is_scalar) {
    int opcode = 0b010;
    uint32_t scalar_bit = is_scalar ? 1 : 0;
    int funct4 = fTypeFunctMap[op];
    int rd = 0, rs1 = 0, rs2 = 0;

    // Determine the register map to use for each operand based on instruction and scalar flag
    auto get_int_reg = [&](const string& reg_name) {
        return is_scalar ? int_scalar_registerMap.at(reg_name) : int_vector_registerMap.at(reg_name);
    };
    auto get_float_reg = [&](const string& reg_name) {
        return is_scalar ? float_scalar_registerMap.at(reg_name) : float_vector_registerMap.at(reg_name);
    };
    
    // Ensure correct number of arguments based on the operation
    if (op == "fneg.s" || op == "fabs.s" || op == "fcvt.w.s" || op == "fcvt.s.w") {
         if (args.size() != 3) { cerr << "Error: Instruction '" << op << "' expects 2 register arguments." << endl; return 0; }
    } else { // Binary float ops, comparisons
         if (args.size() != 4) { cerr << "Error: Instruction '" << op << "' expects 3 register arguments." << endl; return 0; }
    }


    if (op == "fcvt.w.s") { // Convert Float to Int: rd (Int), rs1 (Float)
        rd = get_int_reg(args[1]);
        rs1 = get_float_reg(args[2]);
        rs2 = 0; // Don't care
    } else if (op == "fcvt.s.w") { // Convert Int to Float: rd (Float), rs1 (Int)
        rd = get_float_reg(args[1]);
        rs1 = get_int_reg(args[2]);
        rs2 = 0; // Don't care
    } else if (op == "fneg.s" || op == "fabs.s") { // Unary Float Ops: rd (Float), rs1 (Float)
        rd = get_float_reg(args[1]);
        rs1 = get_float_reg(args[2]);
        rs2 = 0; // Don't care
    } else if (op == "flt.s" || op == "feq.s") { // Float Comparisons: rd (Int), rs1 (Float), rs2 (Float)
        rd = get_int_reg(args[1]);       // Result is 0/1, goes to INT register
        rs1 = get_float_reg(args[2]);    // Operands are FLOAT registers
        rs2 = get_float_reg(args[3]);
    } else if (op == "fadd.s" || op == "fsub.s" || op == "fmul.s" || op == "fdiv.s" || op == "fmin.s") { // Binary Float Ops: rd (Float), rs1 (Float), rs2 (Float)
        rd = get_float_reg(args[1]);
        rs1 = get_float_reg(args[2]);
        rs2 = get_float_reg(args[3]);
    } else {
        // This case should ideally not be reached if the map lookup works,
        // but serves as a fallback error.
        cerr << "Internal Error: Logic missing for F-type instruction '" << op << "'" << endl;
        return 0;
    }

    return (opcode << 29) | (scalar_bit << 28) | (rs2 << 14) | (funct4 << 10) | (rs1 << 5) | rd;
}

uint32_t encodeLoad(string op, const vector<string>& args, bool is_scalar) {
    int opcode = 0b100;
    uint32_t scalar_bit_pos13 = is_scalar ? 1 : 0; // Scalar bit is at [13] for M-type
    // Corrected funct3 values based on ISA table
    int funct3;
    if (op == "lw") funct3 = 0b000;
    else if (op == "sw") funct3 = 0b001; // Note: encodeStore should handle this
    else if (op == "flw") funct3 = 0b010;
    else if (op == "fsw") funct3 = 0b011; // Note: encodeStore should handle this
    else { cerr << "Internal Error: Unknown load opcode '" << op << "'" << endl; return 0; }


    // RD register is the destination register for loads
    int rd;
    if (op == "flw") { // Float Load
        rd = is_scalar ? float_scalar_registerMap.at(args[0]) : float_vector_registerMap.at(args[0]);
    } else { // Integer Load (lw)
        rd = is_scalar ? int_scalar_registerMap.at(args[0])   : int_vector_registerMap.at(args[0]);
    }

    string fullExpr = args[1];
    size_t paren = fullExpr.rfind('(');
    if (paren == string::npos || fullExpr.back() != ')') {
        cerr << "Invalid load format: " << fullExpr << endl;
        return 0;
    }

    string immStr = fullExpr.substr(0, paren);
    string rs1Str = fullExpr.substr(paren + 1, fullExpr.size() - paren - 2);

    int imm = resolveSymbol(immStr, labelMap) & 0x7FFF; // 15-bit imm for Loads
    
    // RS1 (base address register) is always an Integer Scalar register according to typical ISA practice and your M-type formats
    int rs1;
    if (is_scalar) {
        // For s.lw / s.flw, the base register MUST be a scalar integer register.
        if (int_scalar_registerMap.find(rs1Str) == int_scalar_registerMap.end()) {
            cerr << "Error: Invalid base register for '" << op << "'. Must be a scalar int (e.g., sp, s0-s26). Found: '" << rs1Str << "'" << endl;
            return 0;
        }
        rs1 = int_scalar_registerMap.at(rs1Str);
    } else {
        // For v.lw / v.flw, we now allow the base register to be a VECTOR integer register.
        if (int_vector_registerMap.find(rs1Str) == int_vector_registerMap.end()) {
            cerr << "Error: Invalid base register for '" << op << "'. Must be a vector int (e.g., v1-v31). Found: '" << rs1Str << "'" << endl;
            return 0;
        }
        rs1 = int_vector_registerMap.at(rs1Str);
    }
    return (opcode << 29) | (imm << 14) | (scalar_bit_pos13 << 13) | (funct3 << 10) | (rs1 << 5) | rd;
}

uint32_t encodeStore(string op, const vector<string>& args, bool is_scalar) {
    int opcode = 0b100;
    uint32_t scalar_bit_pos13 = is_scalar ? 1 : 0; // Scalar bit is at [13] for M-type
    // Corrected funct3 values based on ISA table
    int funct3;
    if (op == "lw") funct3 = 0b000; // Note: encodeLoad should handle this
    else if (op == "sw") funct3 = 0b001; 
    else if (op == "flw") funct3 = 0b010; // Note: encodeLoad should handle this
    else if (op == "fsw") funct3 = 0b011; 
     else { cerr << "Internal Error: Unknown store opcode '" << op << "'" << endl; return 0; }


    // RS2 register is the source register for stores
    int rs2;
    if (op == "fsw") { // Float Store
        rs2 = is_scalar ? float_scalar_registerMap.at(args[0]) : float_vector_registerMap.at(args[0]);
    } else { // Integer Store (sw)
        rs2 = is_scalar ? int_scalar_registerMap.at(args[0])   : int_vector_registerMap.at(args[0]);
    }
        
    string fullExpr = args[1];
    size_t paren = fullExpr.rfind('(');
    if (paren == string::npos || fullExpr.back() != ')') {
        cerr << "Invalid store format: " << fullExpr << endl;
        return 0;
    }

    string immStr = fullExpr.substr(0, paren);
    string rs1Str = fullExpr.substr(paren + 1, fullExpr.size() - paren - 2);

    int imm = resolveSymbol(immStr, labelMap);
    // RS1 (base address register) is always an Integer Scalar register
    int rs1;
    if (is_scalar) {
        // For s.sw / s.fsw, the base register MUST be a scalar integer register.
        if (int_scalar_registerMap.find(rs1Str) == int_scalar_registerMap.end()) {
            cerr << "Error: Invalid base register for '" << op << "'. Must be a scalar int (e.g., sp, s0-s26). Found: '" << rs1Str << "'" << endl;
            return 0;
        }
        rs1 = int_scalar_registerMap.at(rs1Str);
    } else {
        // For v.sw / v.fsw, we now allow the base register to be a VECTOR integer register.
        if (int_vector_registerMap.find(rs1Str) == int_vector_registerMap.end()) {
            cerr << "Error: Invalid base register for '" << op << "'. Must be a vector int (e.g., v1-v31). Found: '" << rs1Str << "'" << endl;
            return 0;
        }
        rs1 = int_vector_registerMap.at(rs1Str);
    }

    // Store immediate format is different: Imm[14:5] at [28:19], Imm[4:0] at [4:0]
    uint32_t imm14_5 = (static_cast<uint32_t>(imm) >> 5) & 0x3FF; // 10 bits
    uint32_t imm4_0  = static_cast<uint32_t>(imm) & 0x1F;        // 5 bits

    return (opcode << 29) | (imm14_5 << 19) | (rs2 << 14) | (scalar_bit_pos13 << 13) | (funct3 << 10) | (rs1 << 5) | imm4_0;
}

// ... encodeControl remains the same as it's always scalar ...
uint32_t encodeControl(string op, const vector<string>& args, int pc) {
    int opcode = 0b111;
    int funct3 = cTypeFunctMap[op];
    if (op == "ret") return (opcode << 29) | (funct3 << 10) | (1 << 5); // RA (x1) register is implicit rs1
    if (op == "j") {
        if (args.size() != 2) { cerr << "Error: Instruction '" << op << "' expects 1 argument (label)." << endl; return 0; }
        auto it = labelMap.find(args[1]);
        if(it == labelMap.end()) { cerr << "Error: Undefined label '" << args[1] << "' for jump." << endl; return 0; }
        int target = it->second;
        int32_t offset = (target - pc);
        // Jumps are PC-relative and word addressed (offset / 4). Immediate is signed.
        uint32_t imm = (offset >> 2) & 0x3FFFFFF; // 26 bits used in ISA (Imm[27:2])
        uint32_t imm_27_12 = (imm >> 10) & 0xFFFF; // corresponds to Imm[27:12] -> bits [28:13]
        uint32_t imm_11_2 = imm & 0x3FF;          // corresponds to Imm[11:2] -> bits [9:0]
        
        return (opcode << 29) | (imm_27_12 << 13) | (funct3 << 10) | imm_11_2;
    }
    if (op == "beqz") {
        if (args.size() != 3) { cerr << "Error: Instruction '" << op << "' expects register and label." << endl; return 0; }
        string rs1_str = args[1];
        string label = args[2];
        
        auto it = int_scalar_registerMap.find(rs1_str);
        if(it == int_scalar_registerMap.end()) { cerr << "Error: Invalid scalar integer register '" << rs1_str << "' for beqz." << endl; return 0; }
        int rs1 = it->second;
        
        auto label_it = labelMap.find(label);
         if(label_it == labelMap.end()) { cerr << "Error: Undefined label '" << label << "' for beqz." << endl; return 0; }
        int target = label_it->second;

        int rs2 = int_scalar_registerMap.at("zero");  // beqz compares RS1 to x0 (zero)
        int32_t offset = (target - pc);
        
        // Branch offsets are PC-relative and word addressed (offset / 4). Immediate is signed.
        // ISA format: Imm[17:8] at [28:19], Imm[7] at [13], Imm[6:2] at [4:0]
        uint32_t imm = (offset>>2) & 0x3FFFF; // 18-bit offset in ISA (Imm[17:2])
    
        uint32_t imm_17_8 = (imm >> 8) & 0x3FF;   // bits [17:8] -> [28:19]
        uint32_t imm_7    = (imm >> 7) & 0x1;     // bit [7]     -> [13] (Mistake in ISA table? Should be Imm[7] -> bit 13, but your table shows Imm[7] -> bit 13. It should be Imm[7] -> bit 13 or Imm[7] -> Imm[7]. Let's assume Imm[7] goes to bit 13)
        uint32_t imm_6_2  = imm & 0x1F;           // bits [6:2]  -> [4:0]
        
        // Check if RS1 is actually zero (beqz x0, label is an unconditional branch in RISC-V)
         if (rs1 == int_scalar_registerMap.at("zero")) {
             cerr << "Warning: 'beqz zero, " << label << "' is an unconditional branch." << endl;
         }

        return (opcode << 29)
             | (imm_17_8 << 19)
             | (rs2 << 14) // This field is RS2(x0) in ISA table, so hardcoded to 0
             | (imm_7 << 13) // Assuming Imm[7] goes to bit 13
             | (funct3 << 10)
             | (rs1 << 5)
             | imm_6_2;
    }
    if (op == "beqo") {
        if (args.size() != 3) { cerr << "Error: Instruction '" << op << "' expects register and label." << endl; return 0; }
        string rs1_str = args[1];
        string label = args[2];
        
        auto it = int_scalar_registerMap.find(rs1_str);
        if(it == int_scalar_registerMap.end()) { cerr << "Error: Invalid scalar integer register '" << rs1_str << "' for beqo." << endl; return 0; }
        int rs1 = it->second;
        
        auto label_it = labelMap.find(label);
         if(label_it == labelMap.end()) { cerr << "Error: Undefined label '" << label << "' for beqo." << endl; return 0; }
        int target = label_it->second;

        int rs2 = int_scalar_registerMap.at("zero");  // beqz compares RS1 to x0 (zero)
        int32_t offset = (target - pc);
        
        // Branch offsets are PC-relative and word addressed (offset / 4). Immediate is signed.
        // ISA format: Imm[17:8] at [28:19], Imm[7] at [13], Imm[6:2] at [4:0]
        uint32_t imm = (offset>>2) & 0x3FFFF; // 18-bit offset in ISA (Imm[17:2])
    
        uint32_t imm_17_8 = (imm >> 8) & 0x3FF;   // bits [17:8] -> [28:19]
        uint32_t imm_7    = (imm >> 7) & 0x1;     // bit [7]     -> [13] (Mistake in ISA table? Should be Imm[7] -> bit 13, but your table shows Imm[7] -> bit 13. It should be Imm[7] -> bit 13 or Imm[7] -> Imm[7]. Let's assume Imm[7] goes to bit 13)
        uint32_t imm_6_2  = imm & 0x1F;           // bits [6:2]  -> [4:0]
        
        // Check if RS1 is actually zero (beqz x0, label is an unconditional branch in RISC-V)
         if (rs1 == int_scalar_registerMap.at("zero")) {
             cerr << "Warning: 'beqo zero, " << label << "' is an unconditional branch." << endl;
         }

        return (opcode << 29)
             | (imm_17_8 << 19)
             | (rs2 << 14) // This field is RS2(x0) in ISA table, so hardcoded to 0
             | (imm_7 << 13) // Assuming Imm[7] goes to bit 13
             | (funct3 << 10)
             | (rs1 << 5)
             | imm_6_2;
    }
     if (op == "call") {
        if (args.size() != 3) { cerr << "Error: Instruction '" << op << "' expects register and label/address." << endl; return 0; }
        string rd_str = args[1];
        string rs1_str = args[2]; // This is the base register in your format call rd, imm(rs1)
        
        auto it_rd = int_scalar_registerMap.find(rd_str);
         if(it_rd == int_scalar_registerMap.end()) { cerr << "Error: Invalid scalar integer register '" << rd_str << "' for call (rd)." << endl; return 0; }
        int rd = it_rd->second;

        // Your format is call rd, imm(rs1). The ISA table shows Imm[17:2], RS1, RD.
        // This looks like a standard RISC-V JALR (Jump and Link Register) format, but with a different opcode/funct3.
        // JALR format is typically rd, offset(rs1), where offset is 12 bits.
        // Your ISA format is Imm[17:2] (16 bits) + RS1 (5 bits) + RD (5 bits). This seems like a JALR but with a larger immediate.
        // Let's assume 'args[2]' is the base register 'rs1' and 'args[1]' is 'rd'.
        // The immediate part is missing in your format. Is 'call' always register-based?
        // The ISA table for 'call' format: opcode | Imm[17:2] | FUNCT3 | RS1 | RD
        // This implies call is `call rd, rs1, imm` or similar, where Imm[17:2] is a *relative* offset.
        // Let's assume the assembler syntax is `call rd, rs1, offset` where offset is a label or immediate.
        // Or maybe it's `call rd, label` (like JAL rd, offset) where RS1 is hardcoded (like x1/ra)?
        // Let's assume the syntax is `call rd, label` similar to JAL, with RS1 hardcoded to x1 (ra).
        // The immediate would be the target address relative to PC, shifted right by 2.
        
        // Assuming syntax `call rd, label`
        // args[1] is rd, args[2] is label
         auto label_it = labelMap.find(args[2]);
         if(label_it == labelMap.end()) { cerr << "Error: Undefined label '" << args[2] << "' for call." << endl; return 0; }
        int target = label_it->second;
        int32_t offset = (target - pc);

        uint32_t imm = (offset >> 2) & 0xFFFF; // Imm[17:2] -> 16 bits

        // The ISA format has RS1. Is it always RA (x1)? Let's assume RS1 is x1.
        int rs1_call = int_scalar_registerMap.at("ra"); // Assuming RS1 is hardcoded to ra (x1) for call

        return (opcode << 29) | (imm << 13) | (funct3 << 10) | (rs1_call << 5) | rd;

    }


    // Fallback for sync, exit
    // These have funct3=110 and 111, respectively.
    // ISA table shows opcode | 16(x) | FUNCT3 | 10(x) or opcode | 16(x) | FUNCT3 | 10(x)
    // This structure doesn't match the C-type format with RS1, RD, or immediates.
    // They seem to be opcode | [28:13] | FUNCT3 | [9:0].
    // Let's encode them simply: opcode | 0...0 | FUNCT3 | 0...0
    if (op == "exit") {
         return (opcode << 29) | (funct3 << 10);
    }


    cerr << "Error: Unknown C-type instruction '" << op << "'" << endl;
    return 0;
}

uint32_t encodeLUI(const vector<string>& args, bool is_scalar) {
    int opcode = 0b011;
    uint32_t scalar_bit_pos5 = is_scalar ? 1 : 0; // Scalar bit is at [5] for LUI
    
    // RD register is the destination register, can be scalar or vector integer
    int rd = is_scalar ? int_scalar_registerMap.at(args[0]) : int_vector_registerMap.at(args[0]);
    
    // Resolve the immediate using resolveSymbol which handles %hi/%lo
    // LUI takes the *upper 20 bits* of the calculated address/immediate.
    // If the assembler gets `lui rd, %hi(symbol)`, resolveSymbol should return the upper 20 bits.
    // If the assembler gets `lui rd, immediate`, it needs to calculate (immediate + 0x800) >> 12.
    // The assembler syntax is `lui rd, upimm`. 'upimm' is the value *directly* for the upper 20 bits.
    // It's likely the compiler will use `%hi(label)` or just the raw upper bits.
    // Let's assume args[1] is the value directly for the upper 20 bits.
    
    int upimm_val;
     try {
        upimm_val = stoi(args[1]); // Try converting directly first (for raw numbers)
     } catch (...) {
        upimm_val = resolveSymbol(args[1], labelMap); // Fallback for symbols (%hi)
     }

    uint32_t upimm = static_cast<uint32_t>(upimm_val) & 0xFFFFF; // Ensure it's 20 bits

    // ISA: opcode[31:29] | UpIMM[31:12][28:9] | 3(x)[8:6] | scalar[5] | RD[4:0]
    return (opcode << 29) | (upimm << 9) | (0 << 6) | (scalar_bit_pos5 << 5) | rd;
}

uint32_t encodePseudoLI(const vector<string>& args, bool is_scalar) {
    if (args.size() != 3) { cerr << "Error: Pseudo-instruction 'li' expects register and immediate." << endl; return 0; }
    // A li is just an addi from zero. The register type is determined by the prefix (and passed down).
    return encodeIType("addi", {args[1], "zero", args[2]}, is_scalar); // li rd, imm -> addi rd, zero, imm
}


// Add this new function with the other encode functions
uint32_t encodeXType(string op, const vector<string>& args) {
    int opcode = 0b101; // X-Type opcode

    if (!xTypeFunctMap.count(op)) {
        cerr << "Internal Error: Unknown X-type opcode '" << op << "'" << endl;
        return 0;
    }
    int funct4 = xTypeFunctMap.at(op);

    // sx.slt rd, rs1, rs2 requires 3 register arguments
    if (args.size() != 3) {
        cerr << "Error: Instruction 'sx." << op << "' expects 3 arguments (rd, rs1, rs2)." << endl;
        return 0;
    }

    // Argument 1 (rd): The destination is a SCALAR INTEGER register
    if (!int_scalar_registerMap.count(args[0])) {
        cerr << "Error: Invalid destination register '" << args[0] << "' for sx." << op << ". Must be a scalar integer register (e.g., s1)." << endl;
        return 0;
    }
    int rd = int_scalar_registerMap.at(args[0]);

    // Argument 2 (rs1): The source is a VECTOR INTEGER register
    if (!int_vector_registerMap.count(args[1])) {
        cerr << "Error: Invalid source register '" << args[1] << "' for sx." << op << ". Must be a vector integer register (e.g., x5/v5)." << endl;
        return 0;
    }
    int rs1 = int_vector_registerMap.at(args[1]);
    
    // Argument 3 (rs2): The source is a VECTOR INTEGER register
    if (!int_vector_registerMap.count(args[2])) {
        cerr << "Error: Invalid source register '" << args[2] << "' for sx." << op << ". Must be a vector integer register (e.g., x5/v5)." << endl;
        return 0;
    }
    int rs2 = int_vector_registerMap.at(args[2]);


    // | 31-29  | 28-19   | 18-14 | 13-10 | 9-5   | 4-0   |
    // | OPCODE | 10(x)   | RS2   | Funct4| RS1   | RD    |
    return (opcode << 29) | (rs2 << 14) | (funct4 << 10) | (rs1 << 5) | rd;
}

int main() {
    initregisterMap();
    //ifstream input("bin/output/algotests/for/for.s"); // Example test file
    ifstream input("assembler/mcu.asm");
    ofstream instrOut("tb/test/tmp_test/mcu.hex");
    ofstream dataOut("tb/test/tmp_test/data_mcu.hex");
    vector<pair<int, string>> instructions;
    vector<pair<int, uint32_t>> data;
    string line;
    uint32_t instr_pc = 0;
    uint32_t data_pc = 0;

    // First pass - collect labels and instructions (same as before)
    string current_section = ".text";
    string pending_label = "";
    while (getline(input, line)) {
        // Trim whitespace and remove comments
        line.erase(0, line.find_first_not_of(" \t"));
        line.erase(line.find_last_not_of(" \t") + 1);
        if (line.empty() || line[0] == '#') continue;

        // Handle section changes
        if (line.rfind(".section", 0) == 0 || line.rfind(".text", 0) == 0 || line.rfind(".data", 0) == 0 || line.rfind(".rodata", 0) == 0) {
            if (line.find(".rodata") != string::npos) current_section = ".rodata";
            else if (line.find(".data") != string::npos) current_section = ".data";
            else if (line.find(".text") != string::npos) current_section = ".text";
            // Ignore .section directives without a known name for now
            continue;
        }
        // Ignore other common directives
        if (line.rfind(".globl", 0) == 0 || line.rfind(".size", 0) == 0 || line.rfind(".type", 0) == 0) {
            continue;
        }

        // Handle alignment
        if (line.rfind(".align", 0) == 0) {
            string alignStr = line.substr(line.find(".align") + 6);
            alignStr.erase(0, alignStr.find_first_not_of(" \t"));
            int align_bits = stoi(alignStr);
            int align_bytes = 1 << align_bits;
            if (current_section == ".text") {
                instr_pc = (instr_pc + align_bytes - 1) & ~(align_bytes - 1);
            } else {
                data_pc = (data_pc + align_bytes - 1) & ~(align_bytes - 1);
            }
            continue;
        }

        // Process labels
        auto colon_pos = line.find(':');
        if (colon_pos != string::npos) {
            string label = line.substr(0, colon_pos);
            // Assign the current address to this label
            if (current_section == ".text") {
                labelMap[label] = instr_pc;
            } else {
                labelMap[label] = data_pc;
            }
            // Move past the label to see if there's a directive on the same line
            line = line.substr(colon_pos + 1);
            line.erase(0, line.find_first_not_of(" \t"));
        }
        
        // If after processing a label, the line is empty, just continue to the next.
        if (line.empty()) continue;

        // Process directives in data sections
        if (current_section == ".rodata" || current_section == ".data") {
            if (line.rfind(".word", 0) == 0) {
                string valueStr = line.substr(line.find(".word") + 5);
                valueStr.erase(0, valueStr.find_first_not_of(" \t"));
                uint32_t value = static_cast<uint32_t>(stoul(valueStr, nullptr, 0)); // Allow hex/dec
                data.emplace_back(data_pc, value);
                data_pc += 4; // Increment AFTER processing the word
            } else if (line.rfind(".zero", 0) == 0) {
                string sizeStr = line.substr(line.find(".zero") + 5);
                sizeStr.erase(0, sizeStr.find_first_not_of(" \t"));
                int size = stoi(sizeStr);
                for (int i = 0; i < size; ++i) {
                    // To keep it simple, we just advance the data_pc.
                    // We could add zero entries to the `data` vector if needed for the hex output.
                }
                data_pc += size;
            }
        } else if (current_section == ".text") {
            // Anything left on the line is an instruction
            instructions.emplace_back(instr_pc, line);
            instr_pc += 4;
        }
    }
    // End of first pass section

    cout << "\nLabel map:" << endl;
    for (const auto& [label, addr] : labelMap) {
        cout << "  " << label << " -> 0x" << hex << addr << dec << endl;
    }

    // Second pass - output instructions
    cout << "\nAssembling instructions:" << endl;
    for (auto& [pc_addr, line] : instructions) {
        vector<string> tokens = tokenize(line);
        if (tokens.empty()) continue;
        
        string op_with_prefix = tokens[0];
        uint32_t instr = 0;
        
        // NEW: Handle s. and v. prefixes
        bool is_scalar = true; // Default to scalar if no prefix? Or require prefix? Let's require prefix.
        string op;

        cout << "PC 0x" << hex << pc_addr << ": " << line << " -> ";

        if (op_with_prefix.rfind("sx.", 0) == 0) {
            op = op_with_prefix.substr(3);
            if (xTypeFunctMap.count(op)) {
                instr = encodeXType(op, {tokens[1], tokens[2], tokens[3]});
                // This instruction is fully handled. Print and continue to the next one.
                cout << "0x" << hex << setw(8) << setfill('0') << instr << dec << endl;
                instrOut << hex << setw(8) << setfill('0') << instr << endl;
                continue; // <<< --- THE FIX
            } else {
                cerr << "Unknown instruction: " << op_with_prefix << endl;
                continue;
            }
        }

        if (op_with_prefix.rfind("s.", 0) == 0) {
            is_scalar = true;
            op = op_with_prefix.substr(2);
        } else if (op_with_prefix.rfind("v.", 0) == 0) {
            is_scalar = false;
            op = op_with_prefix.substr(2);
        } else {
            // Handle instructions that *don't* use the scalar bit (control flow, lui, li)
             op = op_with_prefix;
             // Need to check if it's a control flow instruction explicitly here
             if (!cTypeFunctMap.count(op) && op != "lui" && op != "li" && op != "sync" && op != "exit") {
                cerr << "Error: Instruction '" << op_with_prefix << "' at PC 0x" << hex << pc_addr << dec << " is missing 's.' or 'v.' prefix." << endl;
                continue; // Skip encoding this instruction
             }
             // For control flow, lui, li, the is_scalar flag might still matter for register lookup
             // but the scalar bit in the instruction encoding is handled differently (or not present)
             // Let's set is_scalar based on the *first operand's register type* for these,
             // UNLESS the instruction explicitly defines register types (like beqz takes int scalar).
             // Simpler: Assume control flow is always scalar logic, lui/li can be scalar or vector depending on RD.
             if (cTypeFunctMap.count(op) || op == "sync" || op == "exit") {
                 is_scalar = true; // Control flow, sync, exit are fundamentally scalar
             } else if (op == "lui" || op == "li") {
                 // For lui/li, check the destination register to determine scalar/vector
                 if (tokens.size() > 1) {
                     string rd_str = tokens[1];
                     if (int_scalar_registerMap.count(rd_str) || float_scalar_registerMap.count(rd_str)) {
                         is_scalar = true;
                     } else if (int_vector_registerMap.count(rd_str) || float_vector_registerMap.count(rd_str)) {
                         is_scalar = false;
                     } else {
                         cerr << "Error: Invalid register '" << rd_str << "' for instruction '" << op << "' at PC 0x" << hex << pc_addr << dec << "." << endl;
                         continue;
                     }
                 } else {
                     cerr << "Error: Instruction '" << op << "' requires a destination register at PC 0x" << hex << pc_addr << dec << "." << endl;
                     continue;
                 }
             } else {
                 // This case should not be reached due to the check above, but as a safeguard:
                 cerr << "Internal Error: Unhandled instruction prefix logic for '" << op_with_prefix << "'" << endl;
                 continue;
             }
        }

        tokens[0] = op; // Update token with stripped opcode (or original if no prefix)
        
        cout << "PC 0x" << hex << pc_addr << ": " << line << " -> ";
        if (op == "sync") {
            auto it = labelMap.find("ENDSYNC");
            if (it == labelMap.end()) {
                cerr << "Assembler Error: 'sync' instruction requires a label 'ENDSYNC' to be defined." << endl;
                continue; // Fail to assemble this instruction
            }
            // The immediate for the addi is the address of ENDSYNC
            uint32_t endsync_address = it->second;

            // We will now forge the arguments for an 'addi s25, zero, <endsync_address>' instruction
            vector<string> addi_args = {
                "s25",                       // rd = s25
                "zero",                      // rs1 = zero
                to_string(endsync_address)   // immediate = address of ENDSYNC
            };
            
            cout << " (interpreted as addi s25, zero, 0x" << hex << endsync_address << dec << ") -> ";

            // Reuse the I-Type encoder to generate the instruction word
            instr = encodeIType("addi", addi_args, true); // 'true' for scalar
        }
        else if (rTypeFunctMap.count(op)) instr = encodeRType(op, {tokens[1], tokens[2], tokens[3]}, is_scalar);
        else if (iTypeFunctMap.count(op)) instr = encodeIType(op, {tokens[1], tokens[2], tokens[3]}, is_scalar);
        else if (fTypeFunctMap.count(op)) instr = encodeFType(op, tokens, is_scalar); // F-type takes full tokens vector
        else if (op == "lw" || op == "flw") instr = encodeLoad(op, {tokens[1], tokens[2]}, is_scalar);
        else if (op == "sw" || op == "fsw") instr = encodeStore(op, {tokens[1], tokens[2]}, is_scalar);
        else if (cTypeFunctMap.count(op) || op == "exit") {
            instr = encodeControl(op, tokens, pc_addr); // Control flow is always encoded as scalar operation
        }
        else if (op == "lui") instr = encodeLUI({tokens[1], tokens[2]}, is_scalar);
        else if (op == "li") instr = encodePseudoLI(tokens, is_scalar); // li takes full tokens vector
        else {
            cerr << "Unknown instruction: " << op << " (after potential prefix removal)" << endl;
            continue;
        }

        cout << "0x" << hex << setw(8) << setfill('0') << instr << dec << endl;
        instrOut << hex << setw(8) << setfill('0') << instr << endl;
    }

    // Output data section
    cout << "\nData section:" << endl;
    for (auto& [addr, value] : data) {
        cout << "0x" << hex << setw(8) << setfill('0') << value << dec << endl;
        dataOut << hex << setw(8) << setfill('0') << value << endl;
    }


    input.close();
    instrOut.close();
    dataOut.close();
    return 0;
}