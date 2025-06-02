#include <iostream>
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <bitset>
#include <vector>
#include <iomanip>
#include <filesystem>
#include <map>

using namespace std;

// The assembler works by breaking down the instr sw ra, 268(sp)
// args[0] = sw, args[1] = ra, args[2] = 268(sp)

unordered_map<string, int> intregisterMap;
unordered_map<string, int> floatregisterMap;
unordered_map<string, pair<int, int>> rTypeFunctMap = {
    {"add", {0b000, 0b0000}}, {"sub", {0b000, 0b0001}}, {"mul", {0b000, 0b0010}},
    {"div", {0b000, 0b0011}}, {"slt", {0b000, 0b0100}}, {"sgt", {0b000, 0b0101}},
    {"seq", {0b000, 0b0110}}, {"snez", {0b000, 0b0111}}, {"min", {0b000, 0b1000}},
    {"abs", {0b000, 0b1001}}
};

unordered_map<string, pair<int, int>> iTypeFunctMap = {
    {"addi", {0b001, 0b0000}}, {"muli", {0b001, 0b0010}}, {"slli", {0b001, 0b1010}},
    {"divi", {0b001, 0b0011}}
};

unordered_map<string, pair<int, int>> fTypeFunctMap = {
    {"fadd.s", {0b010, 0b0000}}, {"fsub.s", {0b010, 0b0001}}, {"fmul.s", {0b010, 0b0010}},
    {"fdiv.s", {0b010, 0b0011}}, {"flt.s", {0b010, 0b0100}}, {"fneg.s", {0b010, 0b0101}},
    {"feq.s",  {0b010, 0b0110}}, {"fmin.s", {0b010, 0b0111}}, {"fabs.s", {0b010, 0b1000}},
    {"fcvt.w.s", {0b010, 0b1001}}, {"fcvt.s.w", {0b010, 0b1010}}
};

unordered_map<string, int> cTypeFunctMap = {
    {"j", 0b000}, {"beqz", 0b001}, {"call", 0b010},
    {"ret", 0b011}, {"sync", 0b110}, {"exit", 0b111}
};

map<string, int> labelMap;
map<string, uint32_t> dataMap; // For storing .word values
map<string, uint32_t> labelDataValues; // For storing data values associated with labels

void initregisterMap() {
    for (int i = 0; i < 32; ++i) {
        intregisterMap["x" + to_string(i)] = i;
        floatregisterMap["x" + to_string(i)] = i;
    }
    // Int intregisterMap
    intregisterMap["zero"] = 0; intregisterMap["ra"] = 1; intregisterMap["sp"] = 2;
    intregisterMap["gp"] = 3; intregisterMap["tp"] = 4; intregisterMap["t0"] = 5;
    intregisterMap["t1"] = 6; intregisterMap["t2"] = 7; intregisterMap["s0"] = 8;
    intregisterMap["fp"] = 8; intregisterMap["s1"] = 9; intregisterMap["a0"] = 10;
    intregisterMap["a1"] = 11; intregisterMap["a2"] = 12; intregisterMap["a3"] = 13;
    intregisterMap["a4"] = 14; intregisterMap["a5"] = 15; intregisterMap["a6"] = 16;
    intregisterMap["a7"] = 17; intregisterMap["s2"] = 18; intregisterMap["s3"] = 19;
    intregisterMap["s4"] = 20; intregisterMap["s5"] = 21; intregisterMap["s6"] = 22;
    intregisterMap["s7"] = 23; intregisterMap["s8"] = 24; intregisterMap["s9"] = 25;
    intregisterMap["s10"] = 26; intregisterMap["s11"] = 27; intregisterMap["t3"] = 28;
    intregisterMap["t4"] = 29; intregisterMap["t5"] = 30; intregisterMap["t6"] = 31;

    // Float intregisterMap
    floatregisterMap["ft0"] = 0; floatregisterMap["ft1"] = 1; floatregisterMap["ft2"] = 2;
    floatregisterMap["ft3"] = 3; floatregisterMap["ft4"] = 4; floatregisterMap["ft5"] = 5;
    floatregisterMap["ft6"] = 6; floatregisterMap["ft7"] = 7; floatregisterMap["fs0"] = 8;
    floatregisterMap["fs1"] = 9; floatregisterMap["fa0"] = 10;
    floatregisterMap["fa1"] = 11; floatregisterMap["fa2"] = 12; floatregisterMap["fa3"] = 13;
    floatregisterMap["fa4"] = 14; floatregisterMap["fa5"] = 15; floatregisterMap["fa6"] = 16;
    floatregisterMap["fa7"] = 17; floatregisterMap["fs2"] = 18; floatregisterMap["fs3"] = 19;
    floatregisterMap["fs4"] = 20; floatregisterMap["fs5"] = 21; floatregisterMap["fs6"] = 22;
    floatregisterMap["fs7"] = 23; floatregisterMap["fs8"] = 24; floatregisterMap["fs9"] = 25;
    floatregisterMap["fs10"] = 26; floatregisterMap["fs11"] = 27; floatregisterMap["ft8"] = 28;
    floatregisterMap["ft9"] = 29; floatregisterMap["ft10"] = 30; floatregisterMap["ft11"] = 31;
    
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
        int32_t result = (addr + 0x800) >> 12;  // with rounding for proper relocation
        cout << "  %hi(" << sym << "): addr=0x" << hex << addr 
             << " => upper 20 bits=0x" << result << dec << endl;
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
        cout << "  %lo(" << sym << "): addr=0x" << hex << addr 
             << " => lower 12 bits=0x" << result << dec << endl;
        return result;
    }

    // Direct integer
    if (expr.find_first_not_of("0123456789-") == string::npos) {
        return stoi(expr);
    }

    // Symbolic label
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
    try {
        return resolveSymbol(s, labelMap);
    } catch (...) {
        return 0;
    }
}

uint32_t encodeRType(string op, const vector<string>& args) {
    int opcode = 0b000, pred = 0;
    int rd = intregisterMap[args[0]], rs1 = intregisterMap[args[1]], rs2 = intregisterMap[args[2]];
    int funct4 = rTypeFunctMap[op].second;
    return (opcode << 29) | (pred << 28) | (0 << 19) | (rs2 << 14) | (funct4 << 10) | (rs1 << 5) | rd;
}

uint32_t encodeIType(string op, const vector<string>& args) {
    int opcode = 0b001, pred = 0;
    int rd = intregisterMap[args[0]], rs1 = intregisterMap[args[1]];
    int imm = getImm(args[2]) & 0x3FFF;
    int funct4 = iTypeFunctMap[op].second;
    return (opcode << 29) | (pred << 28) | (imm << 14) | (funct4 << 10) | (rs1 << 5) | rd;
}

uint32_t encodeFType(string op, const vector<string>& args) {
    int opcode = 0b010, pred = 0;
    int rd, rs1, rs2 = 0;
    int funct4 = fTypeFunctMap[op].second;

    if (op == "fcvt.w.s") {
        // int rd = xN, float rs1 = fN
        rd = intregisterMap[args[1]];
        rs1 = floatregisterMap[args[2]];
    } else if (op == "fcvt.s.w") {
        // float rd = fN, int rs1 = xN
        rd = floatregisterMap[args[1]];
        rs1 = intregisterMap[args[2]];
    } else if (op == "fneg.s" || op == "fabs.s") {
        // unary float op: rd = fN, rs1 = fN
        rd = floatregisterMap[args[1]];
        rs1 = floatregisterMap[args[2]];
        rs2 = 0; // don't care
    } else {
        // binary float op: rd = fN, rs1 = fN, rs2 = fN
        rd = floatregisterMap[args[1]];
        rs1 = floatregisterMap[args[2]];
        rs2 = floatregisterMap[args[3]];
    }

    return (opcode << 29) | (pred << 28) | (0 << 19) |
           (rs2 << 14) | (funct4 << 10) | (rs1 << 5) | rd;
}

uint32_t encodeLoad(string op, const vector<string>& args) {
    int opcode = 0b100;
    int funct4 = (op == "flw") ? 0b1000 : 0b0000;
    int rd = (op == "flw") ? floatregisterMap[args[0]] : intregisterMap[args[0]];

    string fullExpr = args[1];
    size_t paren = fullExpr.rfind('(');  // use last '('
    if (paren == string::npos || fullExpr.back() != ')') {
        cerr << "Invalid load format: " << fullExpr << endl;
        return 0;
    }

    string immStr = fullExpr.substr(0, paren);  // %lo(.LC0)
    string rs1Str = fullExpr.substr(paren + 1, fullExpr.size() - paren - 2);  // t0

    int imm = resolveSymbol(immStr, labelMap);
    int rs1 = intregisterMap[rs1Str];

    return (opcode << 29) | ((imm & 0x7FFF) << 14) | (funct4 << 10) | (rs1 << 5) | rd;
}

uint32_t encodeStore(string op, const vector<string>& args) {
    int opcode = 0b100;
    int funct4 = (op == "fsw") ? 0b1001 : 0b0001;
    int rs2 = (op == "fsw") ? floatregisterMap[args[0]] : intregisterMap[args[0]];

    string fullExpr = args[1];
    size_t paren = fullExpr.rfind('(');
    if (paren == string::npos || fullExpr.back() != ')') {
        cerr << "Invalid store format: " << fullExpr << endl;
        return 0;
    }

    string immStr = fullExpr.substr(0, paren);  // handle %lo(.LC0)
    string rs1Str = fullExpr.substr(paren + 1, fullExpr.size() - paren - 2);  // t0

    int imm = resolveSymbol(immStr, labelMap);
    int rs1 = intregisterMap[rs1Str];
    uint32_t uimm = static_cast<uint32_t>(imm) & 0x7FFF;
    uint32_t imm_hi = (uimm >> 5) & 0x3FF;
    uint32_t imm_lo = uimm & 0x1F;
    return (opcode << 29) | (imm_hi << 19) | (rs2 << 14) | (funct4 << 10) | (rs1 << 5) | imm_lo;
}

uint32_t encodeControl(string op, const vector<string>& args, int pc) {
    int opcode = 0b111;
    int funct3 = cTypeFunctMap[op];

    if (op == "ret") {
        return (opcode << 29) | (funct3 << 10) | (1 << 5);
    }

    if (op == "j") {
        string label = args[1];
        int target = labelMap[label];
        int32_t offset = (target - pc);
        int32_t imm = (offset >> 2) & 0x3FFFFFF;  
        uint32_t imm_hi = (imm >> 10) & 0xFFFF; 
        uint32_t imm_lo = imm & 0x3FF; 
    
        return (opcode << 29) | (imm_hi << 13) | (funct3 << 10) | imm_lo;
    }

    if (op == "beqz") {
        string rs1_str = args[1];
        string label = args[2];
        int rs1 = intregisterMap[rs1_str];
        int rs2 = 0;  // hardcoded to x0
        int target = labelMap[label];
        int32_t offset = (target - pc);
        
        uint32_t imm = (offset>>2) & 0x3FFFF;
    
        uint32_t imm_17_8 = (imm >> 6) & 0x3FF;   // bits [17:8] -> [28:19]
        uint32_t imm_7    = (imm >> 5) & 0x1;     // bit [7]     -> [13]
        uint32_t imm_6_2  = imm & 0x1F;           // bits [6:2]  -> [4:0]
            
        return (opcode << 29)
             | (imm_17_8 << 19)
             | (rs2 << 14)
             | (imm_7 << 13)
             | (funct3 << 10)
             | (rs1 << 5)
             | imm_6_2;
    }

    // Fallback (sync, exit, etc)
    return (opcode << 29) | (funct3 << 10);
}

uint32_t encodeLUI(const vector<string>& args) {
    int opcode = 0b011;
    int rd = intregisterMap[args[0]];

    string immExpr = args[1];
    int fulladdr = resolveSymbol(immExpr, labelMap);  // Should return (addr + 0x800) >> 12

    // Extract upper 20 bits (already handled in resolveSymbol if %hi)
    uint32_t upimm = static_cast<uint32_t>(fulladdr) & 0xFFFFF;

    cout << "LUI encoding: rd=" << args[0] << " (" << rd << "), " 
         << immExpr << " resolved to 0x" << hex << fulladdr 
         << " (upper 20 bits: 0x" << upimm << ")" << dec << endl;

    return (opcode << 29) | (upimm << 9) | (0b0000 << 5) | rd;
}

uint32_t encodePseudoLI(const vector<string>& args) {
    return encodeIType("addi", {args[0], "zero", args[1]});
}

int main() {
    initregisterMap();
    ifstream input("bin/output/algotests/for/for.s");
    //For testing purposes
    //ifstream input("assembler/program.asm");
    ofstream instrOut("rtl/program.hex");
    ofstream dataOut("rtl/data.hex");
    vector<pair<int, string>> instructions;
    vector<pair<int, uint32_t>> data;
    string line;
    uint32_t instr_pc = 0;
    uint32_t data_pc = 0;  // This will be used for all data (constants + globals)
    uint32_t globalDataStart = 0;

    // First pass - collect labels and instructions
    string current_section = ".text";  // default section
    string pending_label = ""; // To track labels that precede .word directives
    bool in_rodata = false;
    bool in_data = false;
    
    while (getline(input, line)) {
        if (line.empty() || line[0] == '#') continue;

        if (line.find(".section") != string::npos || 
            line.find(".text") != string::npos ||
            line.find(".data") != string::npos ||
            line.find(".globl") != string::npos ||
            line.find(".size") != string::npos ||
            line.find(".type") != string::npos ||
            line.find(".rodata") != string::npos) {
            
            if (line.find(".rodata") != string::npos) {
                current_section = ".rodata";
                in_rodata = true;
                in_data = false;
            } else if (line.find(".data") != string::npos) {
                current_section = ".data";
                in_rodata = false;
                in_data = true;
            } else if (line.find(".text") != string::npos) {
                current_section = ".text";
                in_rodata = false;
                in_data = false;
            }
            continue;
        }

        if (line.find(".align") != string::npos) {
            string alignStr = line.substr(line.find(".align") + 6);
            alignStr.erase(0, alignStr.find_first_not_of(" \t"));
            int align = 1 << stoi(alignStr);
            if (current_section == ".text")
                instr_pc = (instr_pc + align - 1) & ~(align - 1);
            else
                data_pc = (data_pc + align - 1) & ~(align - 1);
            continue;
        }

        auto colon = line.find(':');
        if (colon != string::npos) {
            string label = line.substr(0, colon);
            label.erase(0, label.find_first_not_of(" \t"));
            label.erase(label.find_last_not_of(" \t") + 1);
            
            if (current_section == ".text") {
                labelMap[label] = instr_pc;
            } else {
                // Both .rodata and .data labels get assigned to the same memory space
                // This ensures global arrays come right after constants
                labelMap[label] = data_pc;
                cout << "Assigning " << label << " to data address 0x" << hex << data_pc << dec << endl;
            }
            
            pending_label = label;
            line = line.substr(colon + 1);
            line.erase(0, line.find_first_not_of(" \t"));
        }

        if (current_section == ".text") {
            if (!line.empty() && line.find(".zero") == string::npos) {
                instructions.emplace_back(instr_pc, line);
                instr_pc += 4;
            }
        } else { // .rodata or .data sections
            if (line.find(".word") != string::npos) {
                string valueStr = line.substr(line.find(".word") + 5);
                valueStr.erase(0, valueStr.find_first_not_of(" \t"));
                uint32_t value = static_cast<uint32_t>(stoul(valueStr));
                data.emplace_back(data_pc, value);
                
                if (!pending_label.empty()) {
                    labelMap[pending_label] = data_pc;
                    labelDataValues[pending_label] = value;
                    cout << "Label " << pending_label << " assigned to data address 0x" 
                         << hex << data_pc << " with value 0x" << value << dec << endl;
                    pending_label = "";
                }
                
                data_pc += 4;
            } else if (line.find(".zero") != string::npos) {
                string sizeStr = line.substr(line.find(".zero") + 5);
                sizeStr.erase(0, sizeStr.find_first_not_of(" \t"));
                int size = stoi(sizeStr);

                if (!pending_label.empty()) {
                    labelMap[pending_label] = data_pc;
                    cout << "Global array " << pending_label << " assigned to data address 0x" 
                         << hex << data_pc << dec << " (size: " << size << " bytes)" << endl;
                    pending_label = "";
                }

                // Add zero-initialized data entries
                for (int i = 0; i < size; i += 4) {
                    data.emplace_back(data_pc, 0);
                    data_pc += 4;
                }
            }
        }
    }

    cout << "\nLabel map:" << endl;
    for (const auto& [label, addr] : labelMap) {
        cout << "  " << label << " -> 0x" << hex << addr << dec << endl;
    }

    // Second pass - output instructions
    cout << "\nAssembling instructions:" << endl;
    for (auto& [pc_addr, line] : instructions) {
        vector<string> tokens = tokenize(line);
        if (tokens.empty()) continue;
        string op = tokens[0];
        uint32_t instr = 0;
        
        cout << "PC 0x" << hex << pc_addr << ": " << line << " -> ";
        
        if (rTypeFunctMap.count(op)) instr = encodeRType(op, {tokens[1], tokens[2], tokens[3]});
        else if (iTypeFunctMap.count(op)) instr = encodeIType(op, {tokens[1], tokens[2], tokens[3]});
        else if (fTypeFunctMap.count(op)) instr = encodeFType(op, tokens);
        else if (op == "lw" || op == "flw") instr = encodeLoad(op, {tokens[1], tokens[2]});
        else if (op == "sw" || op == "fsw") instr = encodeStore(op, {tokens[1], tokens[2]});
        else if (cTypeFunctMap.count(op)) instr = encodeControl(op, tokens, pc_addr);
        else if (op == "lui") instr = encodeLUI({tokens[1], tokens[2]});
        else if (op == "li") instr = encodePseudoLI({tokens[1], tokens[2]});
        else {
            cerr << "Unknown instruction: " << op << endl;
            continue;
        }

        instrOut << hex << setw(2) << setfill('0') << (instr & 0xFF) << endl;
        instrOut << hex << setw(2) << setfill('0') << ((instr >> 8) & 0xFF) << endl;
        instrOut << hex << setw(2) << setfill('0') << ((instr >> 16) & 0xFF) << endl;
        instrOut << hex << setw(2) << setfill('0') << ((instr >> 24) & 0xFF) << endl;

        // cout << "0x" << hex << instr << dec << endl;
        // instrOut << hex << setw(8) << setfill('0') << instr << endl;
    }

    // Output data section
    cout << "\nData section:" << endl;
    for (auto& [addr, value] : data) {

        // dataOut << hex << setw(2) << setfill('0') << (value & 0xFF) << endl;
        // dataOut << hex << setw(2) << setfill('0') << ((value >> 8) & 0xFF) << endl;
        // dataOut << hex << setw(2) << setfill('0') << ((value >> 16) & 0xFF) << endl;
        // dataOut << hex << setw(2) << setfill('0') << ((value >> 24) & 0xFF) << endl;

        //cout << "0x" << hex << addr << ": 0x" << value << dec << endl;
        // Changed to 32-bit read to data.hex now
        dataOut << hex << setw(8) << setfill('0') << value << endl;
    }

    input.close();
    instrOut.close();
    dataOut.close();
    return 0;
}