# FPGA Integration Branch

This branch is used for the integration of the custom CPU onto the FPGA
We will make use of the BRAM available on the FPGA and therefore the data_mem and instr_mem files would be different from other branches as this is handled on Vivado. Note this therefore makes this branch a non-simulation check branch (i.e running testbenches are expected to fail due to empty presence of readmemh in the sv files)