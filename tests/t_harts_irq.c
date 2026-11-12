/* t_harts_irq.c —— 每个 hart 独立 mstatus/mie/mip 与 MTIP 切片。 */
#include "risc6.h"

extern void harts_trap(void);
volatile u32 g_traps[4];
static u32 g_save[4][4];

static u32 hartid(void)
{
    u32 v;
    __asm__ volatile("csrr %0, mhartid" : "=r"(v));
    return v;
}

static void bputs(const char *s)
{
    while (*s != '\0') {
        *(volatile u32 *)RISC6_MMIO_CONSOLE = (u32)(u8)*s;
        s++;
    }
}

static void bexit(u32 code)
{
    *(volatile u32 *)RISC6_MMIO_EXIT = code & 0xFFu;
    for (;;) {
    }
}

void os_main(void)
{
    u32 id = hartid();
    u32 ms = 0x80u;
    volatile u32 i;

    __asm__ volatile("csrw mtvec, %0" : : "r"(&harts_trap));
    __asm__ volatile("csrw mscratch, %0" : : "r"(&g_save[id][0]));
    __asm__ volatile("csrs mie, %0" : : "r"(ms));
    __asm__ volatile("csrsi mstatus, 8");

    for (i = 0; i < 400000u; i++) {
    }
    if (g_traps[id] == 0u) {
        bputs("FAIL: hart has no timer irq\n");
        bexit(1u);
    }
    if (id == 0u) {
        u32 guard = 0;
        while (g_traps[1] == 0u) {
            guard++;
            if (guard > 40000000u) {
                bputs("FAIL: hart1 timer irq\n");
                bexit(1u);
            }
        }
        bputs("PASS: per-hart timer irq\n");
        bexit(0u);
    }
    for (;;) {
    }
}
