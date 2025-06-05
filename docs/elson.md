## Here I will document some special considerations on Vivado for our integration to work

I have opted to put the instr_mem and data_mem into brams respectively in Vivado.

### Problem with instr being garbage

I believe that this maybe due to the PC not resetting properly as we do not have any real way to wait if the user has finished writing into MMIO for data_bram before we start

We need to add a start signal that the user can say when to start execution otherwise:
1) We will end up with garbage due to bad PC value for unstable reset 
2) The CPU starts as soon as the program loads so there is no time for the MMIO to write and will write at the wrong time
