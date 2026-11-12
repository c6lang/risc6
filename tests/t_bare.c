/* tests/t_bare.c —— bare 模式验证（T5）：CSR / 陷阱 / MTIP 注入 / 抢占。
 *
 * 与 user 模式用例不同，这里没有系统调用：
 *   输出 = 写 0x4000_0000（低 8 位一个字符）
 *   退出 = 写 0x4000_0004（低 8 位是退出码）
 *
 * 验证点：
 *   1. mtvec 直连模式能接到定时器中断（mcause = 0x8000_0007）
 *   2. 中断在指令边界被注入，且次数与 SLICE 成正比（>0）
 *   3. 陷阱返回后寄存器帧完好（s2/s3 标记值不变，循环次数精确）
 *   4. bare 模式下 ecall = 机器态陷阱（mcause = 11）
 *
 * 本文件不包含 tests/testlib.h 的 ecall 输出层：bare 没有系统调用。
 */

typedef unsigned char u8;
typedef unsigned int u32;

#define MMIO_CONSOLE 0x40000000u
#define MMIO_EXIT    0x40000004u

#define SAVE_WORDS 40

extern u32 risc6_trap_save[SAVE_WORDS];
extern void risc6_trap_entry(void);

#define TRAP_COUNT  (risc6_trap_save[32])
#define TRAP_CAUSE  (risc6_trap_save[33])
#define TRAP_EPC    (risc6_trap_save[34])

static void bputc(char c)
{
    *(volatile u32 *)MMIO_CONSOLE = (u32)(u8)c;
}

static void bputs(const char *s)
{
    while (*s != '\0') {
        bputc(*s);
        s++;
    }
}

static void bput_hex(u32 v)
{
    int i = 7;
    bputs("0x");
    while (i >= 0) {
        u32 d = (v >> (i * 4)) & 0xFu;
        bputc((char)(d < 10u ? ('0' + d) : ('a' + d - 10u)));
        i--;
    }
}

static void bput_u32(u32 v)
{
    char buf[12];
    int i = 0;
    if (v == 0u) {
        bputc('0');
        return;
    }
    while (v > 0u) {
        buf[i] = (char)('0' + (v % 10u));
        v = v / 10u;
        i++;
    }
    while (i > 0) {
        i--;
        bputc(buf[i]);
    }
}

static int failures;

static void ck(int cond, const char *what)
{
    if (cond == 0) {
        bputs("FAIL: ");
        bputs(what);
        bputs("\n");
        failures++;
    }
}

/* 在寄存器里放标记值，跑一段足够长的循环（会跨过多个 SLICE），
 * 结束后校验标记值与循环次数——陷阱进出不许破坏寄存器帧。 */
static u32 busy_loop(u32 n)
{
    u32 ok = 0;
    __asm__ volatile(
        "li  s2, 0x5A5A5A5A\n\t"
        "li  s3, 0x12345678\n\t"
        "mv  t3, %1\n\t"
        "1:\n\t"
        "addi t3, t3, -1\n\t"
        "bnez t3, 1b\n\t"
        "li  t4, 0x5A5A5A5A\n\t"
        "bne s2, t4, 2f\n\t"
        "li  t4, 0x12345678\n\t"
        "bne s3, t4, 2f\n\t"
        "li  %0, 1\n\t"
        "j 3f\n\t"
        "2:\n\t"
        "li  %0, 0\n\t"
        "3:\n\t"
        : "=r"(ok)
        : "r"(n)
        : "s2", "s3", "t3", "t4", "memory");
    return ok;
}

static u32 rd_mcause(void)
{
    u32 v;
    __asm__ volatile("csrr %0, mcause" : "=r"(v));
    return v;
}

static u32 rd_mie(void)
{
    u32 v;
    __asm__ volatile("csrr %0, mie" : "=r"(v));
    return v;
}

static u32 rd_mstatus(void)
{
    u32 v;
    __asm__ volatile("csrr %0, mstatus" : "=r"(v));
    return v;
}

void os_main(void)
{
    u32 ok = 0;
    u32 traps = 0;
    u32 cause = 0;
    u32 i = 0;
    u32 sum = 0;

    bputs("bare: setup mtvec/mscratch\n");
    __asm__ volatile("csrw mtvec, %0" : : "r"(&risc6_trap_entry));
    __asm__ volatile("csrw mscratch, %0" : : "r"(risc6_trap_save));

    /* mepc/mscratch 读回自检 */
    {
        u32 ms;
        __asm__ volatile("csrr %0, mscratch" : "=r"(ms));
        ck(ms == (u32)risc6_trap_save, "mscratch 读回");
        __asm__ volatile("csrr %0, mtvec" : "=r"(ms));
        ck((ms & 0xFFFFFFFCu) == ((u32)&risc6_trap_entry & 0xFFFFFFFCu),
           "mtvec 读回");
    }
    ck((rd_mstatus() & 0x8u) == 0u, "复位后 mstatus.MIE=0");

    /* 打开 MIE + MTIE，然后跑长循环：定时器中断会打断它 */
    __asm__ volatile("csrsi mstatus, 8");   /* 或上 8 = 置 MIE(bit3) */
    {
        u32 mtie = 0x80u;                    /* 不要手写 t0：会踩编译器的小数据基址 */
        __asm__ volatile("csrs mie, %0" : : "r"(mtie));
    }

    bputs("bare: mstatus=");
    bput_hex(rd_mstatus());
    bputs(" mie=");
    bput_hex(rd_mie());
    bputs("\n");

    ok = busy_loop(400000u);
    ck(ok == 1u, "陷阱返回后寄存器帧完好");

    traps = TRAP_COUNT;
    cause = TRAP_CAUSE;
    bputs("bare: traps=");
    bput_u32(traps);
    bputs(" last mcause=");
    bput_hex(cause);
    bputs(" (SLICE=50000)\n");
    ck(traps > 0u, "注入过定时器中断");
    ck(cause == 0x80000007u, "mcause = 0x80000007（机器定时器中断）");
    ck(TRAP_EPC >= 0x1000u, "mepc 在 guest 地址空间内");

    /* 关掉中断，验证 mcause=11：bare 模式下 ecall 是机器态陷阱 */
    __asm__ volatile("csrci mstatus, 8");
    bputs("bare: ecall -> mcause 11\n");
    __asm__ volatile("ecall");
    ck(TRAP_CAUSE == 11u, "bare ecall -> mcause=11");

    /* 顺手做一点纯计算，确保陷阱之后计算仍然正确 */
    i = 0;
    while (i < 1000u) {
        sum = sum + i;
        i++;
    }
    ck(sum == 499500u, "陷阱之后整数运算正确");

    if (failures == 0) {
        bputs("PASS: bare traps\n");
        *(volatile u32 *)MMIO_EXIT = 0u;
    } else {
        bputs("FAIL: bare\n");
        *(volatile u32 *)MMIO_EXIT = 1u;
    }
    for (;;) {
    }
}
