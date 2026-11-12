/* 每 hart 定时器陷阱：只保存/恢复 t0-t2，计数到 g_traps[hart]。 */
    .section .text
    .align 2
    .globl harts_trap
harts_trap:
    csrrw   t0, mscratch, t0
    sw      t1, 4(t0)
    sw      t2, 8(t0)
    csrr    t1, mhartid
    slli    t1, t1, 2
    la      t2, g_traps
    add     t2, t2, t1
    lw      t1, 0(t2)
    addi    t1, t1, 1
    sw      t1, 0(t2)
    lw      t1, 4(t0)
    lw      t2, 8(t0)
    csrrw   t0, mscratch, t0
    mret
