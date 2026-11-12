/* t_harts_atomic.c —— 两个 hart 对共享内存做 AMO 原子加。 */
#include "risc6.h"

#define N 100000u
static volatile u32 g_count;
static volatile u32 g_done[4];

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
    u32 i;
    u32 guard = 0;

    for (i = 0; i < N; i++) {
        (void)__atomic_fetch_add((u32 *)&g_count, 1u, __ATOMIC_SEQ_CST);
    }
    g_done[id] = 1u;
    if (id == 1u) {
        for (;;) {
        }
    }
    while (g_done[1] == 0u) {
        guard++;
        if (guard > 40000000u) {
            bputs("FAIL: hart1 atomic\n");
            bexit(1u);
        }
    }
    if (g_count == (N * 2u)) {
        bputs("PASS: two-hart atomic add\n");
        bexit(0u);
    }
    bputs("FAIL: atomic count\n");
    bexit(1u);
}
