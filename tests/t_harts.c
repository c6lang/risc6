/* t_harts.c —— risc6 多 hart 验证：两个 hart 共享内存、交替执行。 */
#include "risc6.h"

static volatile u32 g_flag;
static volatile u32 g_count;

__attribute__((noreturn)) static void spin(void)
{
    for (;;) {
    }
}

static u32 hartid(void)
{
    u32 v;
    __asm__ volatile("csrr %0, mhartid" : "=r"(v));
    return v;
}

static void bputc(char c)
{
    *(volatile u32 *)RISC6_MMIO_CONSOLE = (u32)(u8)c;
}

static void bputs(const char *s)
{
    while (*s != '\0') {
        bputc(*s);
        s++;
    }
}

static void bexit(u32 code)
{
    *(volatile u32 *)RISC6_MMIO_EXIT = code & 0xFFu;
    spin();
}

void os_main(void)
{
    u32 id = hartid();
    u32 guard = 0;

    if (id == 1u) {
        bputs("hart1 up\n");
        g_count = 42u;
        g_flag = 1u;
        spin();
    }
    bputs("hart0 up\n");
    while (g_flag == 0u) {
        guard++;
        if (guard > 20000000u) {
            bputs("FAIL: hart1 never ran\n");
            bexit(1u);
        }
    }
    if (g_count == 42u) {
        bputs("PASS: two harts\n");
        bexit(0u);
    }
    bputs("FAIL: shared memory\n");
    bexit(1u);
}
