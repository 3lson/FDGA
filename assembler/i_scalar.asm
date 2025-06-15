s.li sp, 42
s.li s1, 10
s.li s5, 1
s.sw s1, 0(sp)
s.lw s2, 0(sp)
s.sw s3, 1(sp)
s.addi s3, s2, 10
s.sw s3, 2(sp)
s.muli s3, s2, 10
s.sw s3, 3(sp)
s.divi s3, s2, 10
s.sw s3, 4(sp)
s.slli s3, s2, 2
s.sw s3, 5(sp)
s.seqi s3, s1, 10
s.sw s3, 6(sp) 
s.beqo s5, branch1
branch1: 
exit