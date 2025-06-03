# This file is used to convert the hex files to coe files necessary as mem file initialisation for BRAM
# We shall assuming the format of a 32-bit hex value per line 
# Uppercase and lowercase is both ok but no prefixes of 0x just pure hex

def hex_to_coe(hex_path, coe_path, radix=16):
    with open(hex_path, 'r') as hex_file:
        lines = [line.strip() for line in hex_file if line.strip()]
    
    with open(coe_path, 'w') as coe_file:
        coe_file.write(f"memory_initialization_radix={radix};\n")
        coe_file.write("memory_initialization_vector=\n")
        
        for i, line in enumerate(lines):
            sep = ',' if i < len(lines) - 1 else ';'
            coe_file.write(line + sep + '\n')
    
    print(f"Converted {hex_path} to {coe_path}")

# Example usage:
hex_to_coe("rtl/program.hex", "instr.coe")  # for instruction BRAM
hex_to_coe("rtl/data.hex", "data.coe")      # for data BRAM
