/* tests/bare_trap.S —— 机器态陷阱入口（bare 模式验证用）。
 *
 * x5(t0) 在陷阱期间寄存在 mscratch 里：进入时与 mscratch 交换，
 * 于是 t0 = 保存区基址、mscratch = 老的 t0；退出时再交换回来。
 * 其余 31 个寄存器逐个存/取，保存区布局：
 *   [0..4*31]  x0..x31（x0 不用）
 *   [4*32]     陷阱计数
 *   [4*33]     最近一次 mcause
 *   [4*34]     最近一次 mepc
 */
    .section .bss
    .align 4
    .globl risc6_trap_save
risc6_trap_save:
    .space 4 * 40

    .section .text
    .align 2
    .globl risc6_trap_entry
risc6_trap_entry:
    csrrw   t0, mscratch, t0
    sw      x1,   4*1(t0)
    sw      x2,   4*2(t0)
    sw      x3,   4*3(t0)
    sw      x4,   4*4(t0)
    sw      x6,   4*6(t0)
    sw      x7,   4*7(t0)
    sw      x8,   4*8(t0)
    sw      x9,   4*9(t0)
    sw      x10,  4*10(t0)
    sw      x11,  4*11(t0)
    sw      x12,  4*12(t0)
    sw      x13,  4*13(t0)
    sw      x14,  4*14(t0)
    sw      x15,  4*15(t0)
    sw      x16,  4*16(t0)
    sw      x17,  4*17(t0)
    sw      x18,  4*18(t0)
    sw      x19,  4*19(t0)
    sw      x20,  4*20(t0)
    sw      x21,  4*21(t0)
    sw      x22,  4*22(t0)
    sw      x23,  4*23(t0)
    sw      x24,  4*24(t0)
    sw      x25,  4*25(t0)
    sw      x26,  4*26(t0)
    sw      x27,  4*27(t0)
    sw      x28,  4*28(t0)
    sw      x29,  4*29(t0)
    sw      x30,  4*30(t0)
    sw      x31,  4*31(t0)

    lw      t1,   4*32(t0)          /* 计数++ */
    addi    t1, t1, 1
    sw      t1,   4*32(t0)
    csrr    t1, mcause
    sw      t1,   4*33(t0)
    csrr    t1, mepc
    sw      t1,   4*34(t0)

    /* 异常（mcause=11 的 ecall）不是中断：返回前要把 mepc 前移一条指令，
     * 否则 mret 会重新执行同一条 ecall。 */
    csrr    t1, mcause
    li      t2, 11
    bne     t1, t2, 1f
    csrr    t1, mepc
    addi    t1, t1, 4
    csrw    mepc, t1
1:

    lw      x1,   4*1(t0)
    lw      x2,   4*2(t0)
    lw      x3,   4*3(t0)
    lw      x4,   4*4(t0)
    lw      x6,   4*6(t0)
    lw      x7,   4*7(t0)
    lw      x8,   4*8(t0)
    lw      x9,   4*9(t0)
    lw      x10,  4*10(t0)
    lw      x11,  4*11(t0)
    lw      x12,  4*12(t0)
    lw      x13,  4*13(t0)
    lw      x14,  4*14(t0)
    lw      x15,  4*15(t0)
    lw      x16,  4*16(t0)
    lw      x17,  4*17(t0)
    lw      x18,  4*18(t0)
    lw      x19,  4*19(t0)
    lw      x20,  4*20(t0)
    lw      x21,  4*21(t0)
    lw      x22,  4*22(t0)
    lw      x23,  4*23(t0)
    lw      x24,  4*24(t0)
    lw      x25,  4*25(t0)
    lw      x26,  4*26(t0)
    lw      x27,  4*27(t0)
    lw      x28,  4*28(t0)
    lw      x29,  4*29(t0)
    lw      x30,  4*30(t0)
    lw      x31,  4*31(t0)
    csrrw   t0, mscratch, t0
    mret
    .size risc6_trap_entry, . - risc6_trap_entry
