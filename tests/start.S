/* rv32 user-mode 入口：调用 os_main，然后 exit(0)。 */

.section .text
.globl _start
_start:
    call os_main
    li a7, 93          /* SYS_exit */
    li a0, 0
    ecall
1:
    j 1b
