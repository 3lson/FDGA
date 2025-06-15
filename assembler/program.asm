.text
.text
.globl main
.align 2
.type main, @function
main:
s.li s17, 1472
s.li sp, 1472
s.addi sp, sp, -272
s.sw ra, 268(sp)
s.sw s17, 264(sp)
s.addi s17, sp, 272
s.li s18, 10
s.sw s18, -132(s17)
s.li s18, 9
s.sw s18, -128(s17)
s.li s18, 1
s.sw s18, -112(s17)
lui s18, %hi(.LC0)
s.flw fs2, %lo(.LC0)(s18)
s.fsw fs2, -104(s17)
lui s18, %hi(.LC1)
s.flw fs2, %lo(.LC1)(s18)
s.fsw fs2, -100(s17)
lui s18, %hi(.LC2)
s.flw fs2, %lo(.LC2)(s18)
s.fsw fs2, -96(s17)
lui s18, %hi(.LC3)
s.flw fs2, %lo(.LC3)(s18)
s.fsw fs2, -92(s17)
lui s18, %hi(.LC4)
s.flw fs2, %lo(.LC4)(s18)
s.fsw fs2, -88(s17)
lui s18, %hi(.LC5)
s.flw fs2, %lo(.LC5)(s18)
s.fsw fs2, -84(s17)
lui s18, %hi(.LC6)
s.flw fs7, %lo(.LC6)(s18)
s.fneg.s fs2, fs7
s.fsw fs2, -80(s17)
lui s18, %hi(.LC7)
s.flw fs7, %lo(.LC7)(s18)
s.fneg.s fs2, fs7
s.fsw fs2, -76(s17)
lui s18, %hi(.LC8)
s.flw fs7, %lo(.LC8)(s18)
s.fneg.s fs2, fs7
s.fsw fs2, -72(s17)
lui s18, %hi(.LC9)
s.flw fs2, %lo(.LC9)(s18)
s.fsw fs2, -68(s17)
lui s18, %hi(.LC10)
s.flw fs2, %lo(.LC10)(s18)
s.fsw fs2, -64(s17)
lui s18, %hi(.LC11)
s.flw fs2, %lo(.LC11)(s18)
s.fsw fs2, -60(s17)
lui s18, %hi(.LC12)
s.flw fs2, %lo(.LC12)(s18)
s.fsw fs2, -56(s17)
lui s18, %hi(.LC13)
s.flw fs2, %lo(.LC13)(s18)
s.fsw fs2, -52(s17)
lui s18, %hi(.LC14)
s.flw fs2, %lo(.LC14)(s18)
s.fsw fs2, -48(s17)
lui s18, %hi(.LC15)
s.flw fs7, %lo(.LC15)(s18)
s.fneg.s fs2, fs7
s.fsw fs2, -44(s17)
lui s18, %hi(.LC16)
s.flw fs7, %lo(.LC16)(s18)
s.fneg.s fs2, fs7
s.fsw fs2, -40(s17)
lui s18, %hi(.LC17)
s.flw fs7, %lo(.LC17)(s18)
s.fneg.s fs2, fs7
s.fsw fs2, -36(s17)
s.li s18, 0
s.sw s18, -116(s17)
for_start0:
s.lw s19, -116(s17)
s.li s20, 3
s.slt s18, s19, s20
beqz s18, for_end1
s.lw s18, -116(s17)
s.slli s18, s18, 2
s.add s18, s18, s17
s.addi s18, s18, -104
s.flw fs1, 0(s18)
s.lw s18, -116(s17)
s.slli s18, s18, 2
s.add s18, s18, s17
s.addi s18, s18, -264
s.fsw fs1, 0(s18)
s.lw s18, -116(s17)
s.slli s18, s18, 2
s.add s18, s18, s17
s.addi s18, s18, -68
s.flw fs1, 0(s18)
s.lw s18, -116(s17)
s.slli s18, s18, 2
s.add s18, s18, s17
s.addi s18, s18, -252
s.fsw fs1, 0(s18)
for_update2:
s.lw s18, -116(s17)
s.addi s19, s18, 1
s.sw s19, -116(s17)
j for_start0
for_end1:
s.li s18, 0
s.sw s18, -124(s17)
for_start3:
s.lw s20, -124(s17)
s.lw s1, -132(s17)
s.slt s18, s20, s1
beqz s18, for_end4
s.li s18, 1
s.sw s18, -112(s17)
s.li s18, 0
s.sw s18, -120(s17)
for_start6:
s.lw s20, -120(s17)
s.li s1, 3
s.slt s18, s20, s1
beqz s18, for_end7
s.lw s18, -120(s17)
s.slli s18, s18, 2
s.add s18, s18, s17
s.addi s18, s18, -264
s.flw fs1, 0(s18)
s.lw s18, -120(s17)
s.slli s18, s18, 2
s.add s18, s18, s17
s.addi s18, s18, -240
s.fsw fs1, 0(s18)
s.lw s18, -120(s17)
s.slli s18, s18, 2
s.add s18, s18, s17
s.addi s18, s18, -252
s.flw fs1, 0(s18)
s.lw s18, -120(s17)
s.slli s18, s18, 2
s.add s18, s18, s17
s.addi s18, s18, -228
s.fsw fs1, 0(s18)
s.li s18, 0
s.lw s20, -120(s17)
s.slli s20, s20, 2
s.add s20, s20, s17
s.addi s20, s20, -144
s.sw s18, 0(s20)
for_update8:
s.lw s18, -120(s17)
s.addi s20, s18, 1
s.sw s20, -120(s17)
j for_start6
for_end7:
s.li s18, 0
s.sw s18, -116(s17)
for_start9:
s.lw s1, -116(s17)
s.lw s21, -128(s17)
s.slt s18, s1, s21
beqz s18, for_end10
s.li s18, 0
s.slli s18, s18, 2
s.add s18, s18, s17
s.addi s18, s18, -264
s.flw fs3, 0(s18)
s.lw s18, -116(s17)
s.slli s18, s18, 2
s.add s18, s18, s17
s.addi s18, s18, -104
s.flw fs10, 0(s18)
s.fsub.s fs7, fs3, fs10
s.fabs.s fs2, fs7
s.li s18, 0
s.slli s18, s18, 2
s.add s18, s18, s17
s.addi s18, s18, -252
s.flw fs10, 0(s18)
s.lw s18, -116(s17)
s.slli s18, s18, 2
s.add s18, s18, s17
s.addi s18, s18, -68
s.flw fs11, 0(s18)
s.fsub.s fs3, fs10, fs11
s.fabs.s fs7, fs3
s.fadd.s fs1, fs2, fs7
s.fsw fs1, -32(s17)
s.li s18, 0
s.sw s18, -108(s17)
s.li s18, 1
s.sw s18, -120(s17)
for_stars192:
s.lw s1, -120(s17)
s.li s21, 3
s.slt s18, s1, s21
beqz s18, for_end13
s.lw s18, -120(s17)
s.slli s18, s18, 2
s.add s18, s18, s17
s.addi s18, s18, -264
s.flw fs3, 0(s18)
s.lw s18, -116(s17)
s.slli s18, s18, 2
s.add s18, s18, s17
s.addi s18, s18, -104
s.flw fs10, 0(s18)
s.fsub.s fs7, fs3, fs10
s.fabs.s fs2, fs7
s.lw s18, -120(s17)
s.slli s18, s18, 2
s.add s18, s18, s17
s.addi s18, s18, -252
s.flw fs10, 0(s18)
s.lw s18, -116(s17)
s.slli s18, s18, 2
s.add s18, s18, s17
s.addi s18, s18, -68
s.flw fs11, 0(s18)
s.fsub.s fs3, fs10, fs11
s.fabs.s fs7, fs3
s.fadd.s fs1, fs2, fs7
s.fsw fs1, -28(s17)
s.flw fs1, -28(s17)
s.flw fs2, -32(s17)
s.flt.s s18, fs1, fs2
beqz s18, else15
s.flw fs1, -28(s17)
s.fsw fs1, -32(s17)
s.lw s1, -120(s17)
s.sw s1, -108(s17)
j end_if16
else15:
end_if16:
for_update14:
s.lw s18, -120(s17)
s.addi s1, s18, 1
s.sw s1, -120(s17)
j for_stars192
for_end13:
s.lw s18, -116(s17)
s.slli s18, s18, 2
s.add s18, s18, s17
s.addi s18, s18, -104
s.flw fs1, 0(s18)
s.lw s21, -108(s17)
s.slli s21, s21, 2
s.add s21, s21, s17
s.addi s21, s21, -144
s.lw s18, 0(s21)
s.slli s18, s18, 2
s.add s18, s18, s17
s.addi s18, s18, -216
s.fsw fs1, 0(s18)
s.lw s18, -116(s17)
s.slli s18, s18, 2
s.add s18, s18, s17
s.addi s18, s18, -68
s.flw fs1, 0(s18)
s.lw s21, -108(s17)
s.slli s21, s21, 2
s.add s21, s21, s17
s.addi s21, s21, -144
s.lw s18, 0(s21)
s.slli s18, s18, 2
s.add s18, s18, s17
s.addi s18, s18, -180
s.fsw fs1, 0(s18)
s.lw s21, -108(s17)
s.slli s21, s21, 2
s.add s21, s21, s17
s.addi s21, s21, -144
s.lw s18, 0(s21)
s.addi s21, s18, 1
for_update11:
s.lw s18, -116(s17)
s.addi s22, s18, 1
s.sw s22, -116(s17)
j for_start9
for_end10:
s.li s18, 0
s.sw s18, -116(s17)
for_stars197:
s.lw s23, -116(s17)
s.li s24, 3
s.slt s18, s23, s24
beqz s18, for_end18
s.li s18, 0
s.fcvt.s.w fs1, s18
s.fsw fs1, -32(s17)
s.li s23, 0
s.fcvt.s.w fs1, s23
s.fsw fs1, -28(s17)
s.lw s25, -116(s17)
s.slli s25, s25, 2
s.add s25, s25, s17
s.addi s25, s25, -144
s.lw s24, 0(s25)
s.sw s24, -24(s17)
s.li s24, 0
s.sw s24, -120(s17)
for_stars200:
s.lw s25, -120(s17)
s.lw s12, -24(s17)
s.slt s24, s25, s12
beqz s24, for_end21
s.flw fs2, -32(s17)
s.lw s24, -120(s17)
s.slli s24, s24, 2
s.add s24, s24, s17
s.addi s24, s24, -216
s.flw fs7, 0(s24)
s.fadd.s fs1, fs2, fs7
s.fsw fs1, -32(s17)
s.flw fs2, -28(s17)
s.lw s24, -120(s17)
s.slli s24, s24, 2
s.add s24, s24, s17
s.addi s24, s24, -180
s.flw fs7, 0(s24)
s.fadd.s fs1, fs2, fs7
s.fsw fs1, -28(s17)
for_update22:
s.lw s24, -120(s17)
s.addi s25, s24, 1
s.sw s25, -120(s17)
j for_stars200
for_end21:
s.lw s12, -24(s17)
s.li s13, 0
s.slt s24, s13, s12
beqz s24, else23
s.flw fs2, -32(s17)
s.lw s12, -24(s17)
s.fcvt.s.w fs7, s12
s.fdiv.s fs1, fs2, fs7
s.lw s13, -116(s17)
s.slli s13, s13, 2
s.add s13, s13, s17
s.addi s13, s13, -264
s.fsw fs1, 0(s13)
s.flw fs2, -28(s17)
s.lw s13, -24(s17)
s.fcvt.s.w fs7, s13
s.fdiv.s fs1, fs2, fs7
s.lw s2, -116(s17)
s.slli s2, s2, 2
s.add s2, s2, s17
s.addi s2, s2, -252
s.fsw fs1, 0(s2)
j end_if24
else23:
end_if24:
for_update19:
s.lw s24, -116(s17)
s.addi s2, s24, 1
s.sw s2, -116(s17)
j for_stars197
for_end18:
s.li s24, 0
s.sw s24, -116(s17)
for_stars205:
s.lw s3, -116(s17)
s.li s4, 3
s.slt s24, s3, s4
beqz s24, for_end26
s.lw s24, -116(s17)
s.slli s24, s24, 2
s.add s24, s24, s17
s.addi s24, s24, -264
s.flw fs7, 0(s24)
s.lw s24, -116(s17)
s.slli s24, s24, 2
s.add s24, s24, s17
s.addi s24, s24, -240
s.flw fs3, 0(s24)
s.fsub.s fs2, fs7, fs3
s.fabs.s fs1, fs2
s.fsw fs1, -32(s17)
s.lw s24, -116(s17)
s.slli s24, s24, 2
s.add s24, s24, s17
s.addi s24, s24, -252
s.flw fs7, 0(s24)
s.lw s24, -116(s17)
s.slli s24, s24, 2
s.add s24, s24, s17
s.addi s24, s24, -228
s.flw fs3, 0(s24)
s.fsub.s fs2, fs7, fs3
s.fabs.s fs1, fs2
s.fsw fs1, -28(s17)
s.flw fs7, -32(s17)
lui s3, %hi(.LC18)
s.flw fs3, %lo(.LC18)(s3)
s.flt.s s2, fs7, fs3
s.flw fs3, -28(s17)
lui s3, %hi(.LC19)
s.flw fs10, %lo(.LC19)(s3)
s.flt.s s7, fs3, fs10
beqz s2, false_label
s.snez s1, s7
j end_label
false_label:
s.li s3, 0
s.addi s1, s3, 0
end_label:
s.li s4, 0
s.fcvt.s.w fs2, s4
s.feq.s s24, fs1, fs2
beqz s24, else28
s.li s5, 0
s.sw s5, -112(s17)
j for_end26
j end_if29
else28:
end_if29:
for_update27:
s.lw s24, -116(s17)
s.addi s5, s24, 1
s.sw s5, -116(s17)
j for_stars205
for_end26:
s.lw s24, -112(s17)
beqz s24, else30
j for_end4
j end_if31
else30:
end_if31:
for_update5:
s.lw s24, -124(s17)
s.addi s6, s24, 1
s.sw s6, -124(s17)
j for_start3
for_end4:
s.li s24, 0
s.sw s24, -116(s17)
for_start32:
s.lw s7, -116(s17)
s.li s8, 3
s.slt s24, s7, s8
beqz s24, for_end33
s.lw s24, -116(s17)
s.slli s24, s24, 2
s.add s24, s24, s17
s.addi s24, s24, -264
s.flw fs1, 0(s24)
s.lw s24, -116(s17)
s.slli s24, s24, 2
lui s7, %hi(global_OUT_centroids_x)
s.add s7, s7, s24
s.fsw fs1, %lo(global_OUT_centroids_x)(s7)
s.lw s24, -116(s17)
s.slli s24, s24, 2
s.add s24, s24, s17
s.addi s24, s24, -252
s.flw fs1, 0(s24)
s.lw s24, -116(s17)
s.slli s24, s24, 2
lui s7, %hi(global_OUT_centroids_y)
s.add s7, s7, s24
s.fsw fs1, %lo(global_OUT_centroids_y)(s7)
s.lw s7, -116(s17)
s.slli s7, s7, 2
s.add s7, s7, s17
s.addi s7, s7, -144
s.lw s24, 0(s7)
s.lw s7, -116(s17)
s.slli s7, s7, 2
lui s8, %hi(global_OUT_cluster_sizes)
s.add s8, s8, s7
s.sw s24, %lo(global_OUT_cluster_sizes)(s8)
s.li s24, 0
s.sw s24, -120(s17)
for_start35:
s.lw s7, -120(s17)
s.lw s9, -116(s17)
s.slli s9, s9, 2
s.add s9, s9, s17
s.addi s9, s9, -144
s.lw s8, 0(s9)
s.slt s24, s7, s8
beqz s24, for_end36
s.lw s24, -120(s17)
s.slli s24, s24, 2
s.add s24, s24, s17
s.addi s24, s24, -216
s.flw fs1, 0(s24)
s.lw s24, -120(s17)
s.slli s24, s24, 2
lui s7, %hi(global_OUT_clusters_x)
s.add s7, s7, s24
s.fsw fs1, %lo(global_OUT_clusters_x)(s7)
s.lw s24, -120(s17)
s.slli s24, s24, 2
s.add s24, s24, s17
s.addi s24, s24, -180
s.flw fs1, 0(s24)
s.lw s24, -120(s17)
s.slli s24, s24, 2
lui s7, %hi(global_OUT_clusters_y)
s.add s7, s7, s24
s.fsw fs1, %lo(global_OUT_clusters_y)(s7)
for_update37:
s.lw s24, -120(s17)
s.addi s7, s24, 1
s.sw s7, -120(s17)
j for_start35
for_end36:
for_update34:
s.lw s24, -116(s17)
s.addi s8, s24, 1
s.sw s8, -116(s17)
j for_start32
for_end33:
s.li s14, 5
s.sw s14, 280(sp)
j main_end
main_end:
s.lw s17, 264(sp)
s.lw ra, 268(sp)
s.addi sp, sp, 272
ret
.size main, .-main

    .section .rodata
    .as.lign 2
.LC0:
    .word 1065353216
    .as.lign 2
.LC1:
    .word 1073741824
    .as.lign 2
.LC2:
    .word 1065353216
    .as.lign 2
.LC3:
    .word 1090519040
    .as.lign 2
.LC4:
    .word 1091567616
    .as.lign 2
.LC5:
    .word 1090519040
    .as.lign 2
.LC6:
    .word 1065353216
    .as.lign 2
.LC7:
    .word 1073741824
    .as.lign 2
.LC8:
    .word 1065353216
    .as.lign 2
.LC9:
    .word 1065353216
    .as.lign 2
.LC10:
    .word 1065353216
    .as.lign 2
.LC11:
    .word 1073741824
    .as.lign 2
.LC12:
    .word 1090519040
    .as.lign 2
.LC13:
    .word 1090519040
    .as.lign 2
.LC14:
    .word 1091567616
    .as.lign 2
.LC15:
    .word 1065353216
    .as.lign 2
.LC16:
    .word 1065353216
    .as.lign 2
.LC17:
    .word 1073741824
    .as.lign 2
.LC18:
    .word 897988541
    .as.lign 2
.LC19:
    .word 897988541

    .data
    .as.lign 2
    .type global_OUT_cluster_sizes, @object
    .globl global_OUT_cluster_sizes
global_OUT_cluster_sizes:
    .zero 4
    .zero 4
    .zero 4

    .as.lign 2
    .type global_OUT_clusters_y, @object
    .globl global_OUT_clusters_y
global_OUT_clusters_y:
    .zero 4
    .zero 4
    .zero 4
    .zero 4
    .zero 4
    .zero 4
    .zero 4
    .zero 4
    .zero 4

    .as.lign 2
    .type global_OUT_clusters_x, @object
    .globl global_OUT_clusters_x
global_OUT_clusters_x:
    .zero 4
    .zero 4
    .zero 4
    .zero 4
    .zero 4
    .zero 4
    .zero 4
    .zero 4
    .zero 4

    .as.lign 2
    .type global_OUT_centroids_y, @object
    .globl global_OUT_centroids_y
global_OUT_centroids_y:
    .zero 4
    .zero 4
    .zero 4

    .as.lign 2
    .type global_OUT_centroids_x, @object
    .globl global_OUT_centroids_x
global_OUT_centroids_x:
    .zero 4
    .zero 4
    .zero 4