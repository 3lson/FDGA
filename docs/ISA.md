# Elson-V ISA


## Overview  
The custom ISA is designed to efficiently execute K-means clustering algorithms on custom FPGA-based hardware.

## Bit Encoding Overview
All instr are 32 bit wide

| Field | Bits | 
| ------| -----|
| opcode | [31:29]|
| scalar flag | [28] |

**Note:** The scalar flag here will be set high for scalar operations (encodings handled by the assembler)

The compiler will handle the differentiation between a scalar and vector operations as it will add prefixes of `s.` or `v.` in front of the operations.

E.g.

`v.fadd.s fv1, fv2, fv3` - vector operation
`s.addi s1, s2, 3` - scalar operation
`v.addi v1, v2, 3` - vector operation

## Instruction Types

### R-type (Register-Register Arith)
`opcode = 000`

| [31:29] | [28] | [27:19] | [18:14] | [13:10] | [9:5] | [4:0]
|--------|----|-----|------|-----|-----|----|
|opcode| scalar |  9(x) | rs2 | funct4 | rs1 | rd |

|Mnemonic|funct4|Description|
| -------| -----|-----------|
| `add` | 0000| rd = rs1 + rs2 |
| `sub` | 0001 | rd = rs1 - rs2 |
| `mul` | 0010 | rd = rs1 * rs2 |
| `div` | 0011 | rd = rs1 / rs2 |
| `slt` | 0100 | rd = (rs1 < rs2) ? 1 : 0 |
| `sll` | 0101 | rd = rs1 << rs2 |
| `seq` | 0110 | rd = (rs1 == rs2) ? 1 : 0 |
| `snez` | 0111 | rd = (rs1 != 0) ? 1 : 0 [rs2 is don't cares here] |
| `min` | 1000 | rd = min(rs1, rs2) |
| `abs` | 1001 | rd = abs(rs1) [rs2 is don't cares here] |

### I-type (Immediate Arith)
`opcode = 001`

| [31:29] | [28] | [27:14] | [13:10] | [9:5] | [4:0]
|--------|----|-----|---------|----|--|
|opcode| scalar | imm [13:0] (14 bits) | funct4 | rs1 | rd |

|Mnemonic|FUNCT4|Description|
| -------| -----|-----------|
| `addi` | 0000| rd = rs1 + imm |
| `muli` | 0010 | rd = rs1 * imm |
| `divi` | 0011 | rd = rs1 / imm |
| `slli` | 1010 | rd = rs1 << uimm (shift left logical immediate) | 
| `seqi` | 1011 | rd = (rs1 == imm) ? 1 : 0 | 

**Note:** `uimm` means 5-bit unsigned immediate (i.e `IMM[4:0]`)

### M-type (Memory Access)
`opcode = 100`

**LOAD** `lw rd, imm(rs1)`

| [31:29] | [28:14] | [13] | [12:10] | [9:5] | [4:0]
|--------|----|---|-------|--|-----|
|opcode| imm [14:0] (15 bits) | scalar | funct3 = 000 | rs1 | rd |

**STORE** `sw rs2, imm(rs1)`

| [31:29] | [28:19] | [18:14] | [13] | [12:10] | [9:5] | [4:0]|
|--------|----|-----|---|------|---|-----|
|opcode| imm [14:5] | rs2| scalar |funct3=001 | rs1 | imm [4:0] |

**FLOAT LOAD** `flw rd, imm(rs1)`

| [31:29] | [28:14] | [13] | [12:10] | [9:5] | [4:0]
|--------|----|---|-------|--|-----|
|opcode| imm [14:0] (15 bits) | scalar | funct3 = 010 | rs1 | rd |

**FLOAT STORE** `fsw rs2, imm(rs1)`

| [31:29] | [28:19] | [18:14] | [13] | [12:10] | [9:5] | [4:0]|
|--------|----|-----|------|---|---|-----|
|opcode| imm [14:5] | RS2| scalar | funct3=011 | rs1 | imm [4:0] |

### C-type  (Control Flow)
`opcode = 111`

**jump** `j`

| [31:29] | [28:13] | [12:10] | [9:0] |
| -------| -------- | ------- | ------ |
| opcode | imm[27:12] | funct3 = 000 | imm[11:2] |


**branch** `beqz rd, label`
**Note:** The label will be calculated in assembler to give the relative PC offset addressing

| [31:29] | [28:19] | [18:14] | [13] | [12:10] | [9:5] | [4:0] |
| -------| -------- | ------ |---- |------- | ------ | ------- |
| opcode | imm[17:8] |rs2(x0)| imm[7] | funct3 = 001 | rs1 | imm[6:2] |

**branch** `beqo rd, label`
**Note:** The label will be calculated in assembler to give the relative PC offset addressing

| [31:29] | [28:19] | [18:14] | [13] | [12:10] | [9:5] | [4:0] |
| -------| -------- | ------ |---- |------- | ------ | ------- |
| opcode | imm[17:8] |rs2(x0)| imm[7] | funct3 = 010 | rs1 | imm[6:2] |

**ret** `ret`

| [31:29] | [28:13] | [12:10] | [9:5] | [4:0]
| -------| -------- | ------- | ------ | ------ |
| opcode | 16{1'b0} | funct3 = 011 | 5'b00001 | 5(x)|

**sync** 

| [31:29] | [28:13] | [12:10] | [9:0]
| -------| -------- | ------- | ------ |
| opcode | 16(x) | funct3 = 100 | 10(x) |

**endsync** 

| [31:29] | [28:13] | [12:10] | [9:0]
| -------| -------- | ------- | ------ |
| opcode | 16(x) | funct3 = 101 | 10(x) |

**exit** 

| [31:29] | [28:13] | [12:10] | [9:0] |
| -------| -------- | ------- | ------ | 
| opcode | 16(x) | funct3 = 111 | 10(x)|

### P-type (PseudoInstr)
`opcode = 011`

`li t0, 100` 
This is a pseudo-instr that underlying would perform `addi t0, zero, 100`

`lui rd upimm`


| [31:29] | [28:9] | [8:6] | [5] | [4:0] |
| -------| ---- | ------ | ------ | -------|
| opcode | upimm[31:12] | 3(x) | scalar | rd |

**Note:** `upimm` gives 20-bit upper immediate IMM[31:12]


### X-type 
`opcode = 101`

`sx.slt rd, rs1, rs2`

- rd: The destination scalar mask register. This will be a 32-bit register where each bit i holds the comparison result for thread i.
- rs1: The first source integer vector register.
- rs2: The second source integer vector register.

**Note:** This will be left empty but reversed for when we come to SIMD

| [31:29] | [28:19] | [18:14] | [13:10] | [9:5] | [4:0]
|--------|----|-----|----------|--|-------|
| opcode | 10(x) | rs2 (vector) | funct4 (0000) | rs1 (vector)| rd (scalar)| 

### F-type 
These are arithmetic instructions for FPU block
`opcode = 010`

| [31:29] | [28] | [27:19] | [18:14] | [13:10] | [9:5] | [4:0]
|--------|----|-----|------|-----|-----|----|
|opcode| scalar |  9(x) | rs2 | funct4 | rs1 | rd |

|Mnemonic|FUNCT4|Description|
| -------| -----|-----------|
| `fadd.s` | 0000| rd = rs1 + rs2 |
| `fsub.s` | 0001 | rd = rs1 - rs2 |
| `fmul.s` | 0010 | rd = rs1 * rs2 |
| `fdiv.s` | 0011 | rd = rs1 / rs2 |
| `flt.s` | 0100 | rd = (rs1 < rs2) ? 1 : 0 |
| `fneg.s` | 0101 | rd = neg(rs1) [rs2 will be don't cares here] |
| `feq.s` | 0110 | rd = (rs1 == rs2) ? 1 : 0 |
| `fmin.s` | 0111 | rd = min(rs1, rs2) |
| `fabs.s` | 1000 | rd = abs(rs1) [rs2 will be don't cares here] |
| `fcvt.w.s` | 1001 | Convert a floating-point number in floating-point register rs1 to a signed 32-bit in integer register rd. (rs2 will be don't cares here)|
| `fcvt.s.w` | 1010 | Convert a signed 32-bit integer in integer register rs1 to a signed 32-bit in integer floating-point number in floating-point register rd . (rs2 will be don't cares here)|

## Register File Assignment
We have four different set of registers which handle scalar and vector handling both floats and ints.

**Int Scalar Register File**

| Name | Register Number | Usage |
| -----| ----------------| ------|
| zero | x0 | constant value 0 |
| ra | x1 | return address | 
| sp | x2 | stack pointer |
| gp | x3 | global pointer | 
| tp | x4 | thread pointer |
| s0 | x5 | protected |
| s1-s25 | x6-x30 | saved registers | 
| s26 | x31| reserved register | 

**Float Scalar Register File**

| Name | Register Number | Usage |
| -----| ----------------| ------|
| fs0-fs30 | x0-x30 | saved registers | 
| fs31 | x31 | reserved register | 


**Int Vector Register File**

| Name | Register Number | Usage |
| -----| ----------------| ------|
| zero | x0 | constant value 0 |
| v1-v28 | x1-x28 | saved registers | 
| v29-v31 | x29-x31 | special registers | 

**Float Vector Register File**

| Name | Register Number | Usage |
| -----| ----------------| ------|
| zero | x0 | constant value 0 |
| fv1-fv28 | x1-x28 | saved registers | 
| fv29-fv31 | x29-x31 | special registers | 


The following are mapped as the special registers

 | Register Number | Purpose | 
 | ---------------- |--------|
 | x29 | threadIdx | 
 | x30 | blockIdx |
 | x31 | block_size | 

 