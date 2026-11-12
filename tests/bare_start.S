/* tests/bare_start.S —— bare 模式的入口：没有系统调用，返回后自旋。 */
    .section .text.init,"ax",@progbits
    .globl _start
_start:
    call os_main
1:
    j 1b
    .size _start, . - _start

/* 标记：装载器见到这个段就按 bare 模式跑（docs/design.md §3.5） */
    .section .risc6_bare,"a",@progbits
    .byte 1
