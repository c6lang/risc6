/* tests/t_isa.c —— ISA 回归：算术/逻辑/移位/符号扩展/M 扩展/64 位/RVC/分支/
 * switch/函数指针/递归。任何一条不对就返回失败数（对拍 qemu 时比较退出码）。 */

#include "testlib.h"

static int g_fail = 0;
static int g_checks = 0;

static void ck(int cond, const char *what)
{
    g_checks++;
    if (cond == 0) {
        g_fail++;
        puts_("FAIL: ");
        puts_(what);
        putc_('\n');
    }
}

static void ck_eq(u32 got, u32 want, const char *what)
{
    g_checks++;
    if (got != want) {
        g_fail++;
        puts_("FAIL: ");
        puts_(what);
        puts_(" got=");
        put_hex(got);
        puts_(" want=");
        put_hex(want);
        putc_('\n');
    }
}

static void ck_eq64(u64 got, u64 want, const char *what)
{
    g_checks++;
    if (got != want) {
        g_fail++;
        puts_("FAIL: ");
        puts_(what);
        puts_(" got_lo=");
        put_hex((u32)got);
        puts_(" got_hi=");
        put_hex((u32)(got >> 32));
        puts_(" want_lo=");
        put_hex((u32)want);
        puts_(" want_hi=");
        put_hex((u32)(want >> 32));
        putc_('\n');
    }
}

/* ---- RVC 友好：短函数、紧循环 ---- */
static int add3(int a, int b, int c) { return a + b + c; }
static int imax(int a, int b) { return a > b ? a : b; }

static int fib(int n)
{
    if (n < 2) {
        return n;
    }
    return fib(n - 1) + fib(n - 2);
}

static u32 sum_loop(u32 n)
{
    u32 i = 0;
    u32 s = 0;
    for (i = 1; i <= n; i++) {
        s += i;
    }
    return s;
}

static u32 switchy(u32 v)
{
    switch (v) {
    case 0: return 0x10u;
    case 1: return 0x11u;
    case 2: return 0x12u;
    case 3: return 0x13u;
    case 4: return 0x14u;
    case 5: return 0x15u;
    case 6: return 0x16u;
    case 7: return 0x17u;
    default: return 0xFFu;
    }
}

typedef int (*binop)(int, int);
static int op_add(int a, int b) { return a + b; }
static int op_sub(int a, int b) { return a - b; }
static int op_mul(int a, int b) { return a * b; }
static binop g_ops[3] = {op_add, op_sub, op_mul};

struct Pair {
    u32 a;
    u16 b;
    u8 c;
};

static void test_int(void)
{
    u32 a = 0x12345678u;
    u32 b = 0x0F0F0F0Fu;
    int x = -7;
    int y = 3;

    ck_eq(a + b, 0x21436587u, "u32 add");
    ck_eq(a - b, 0x03254769u, "u32 sub");
    ck_eq(a & b, 0x02040608u, "u32 and");
    ck_eq(a | b, 0x1F3F5F7Fu, "u32 or");
    ck_eq(a ^ b, 0x1D3B5977u, "u32 xor");
    ck_eq(a << 3, 0x91A2B3C0u, "u32 sll");
    ck_eq(a >> 5, 0x0091A2B3u, "u32 srl");
    ck_eq((u32)((int)a >> 5), 0x0091A2B3u, "u32 sra");
    ck_eq((u32)(x / y), (u32)(-2), "sdiv trunc");
    ck_eq((u32)(x % y), (u32)(-1), "srem sign");
    ck_eq((u32)(x < y), 1u, "slt signed");
    ck_eq((u32)((u32)x < (u32)y), 0u, "sltu unsigned");
    ck_eq((u32)(-1) / 2u, 0x7FFFFFFFu, "divu");
    ck_eq(7u % 3u, 1u, "remu");
    ck_eq(a * 3u, 0x369D0368u, "mul");
    ck_eq((u32)(((i64)(i32)0x80000000 * (i64)(i32)0x80000000) >> 32), 0x40000000u, "mulh edge");
    ck_eq((u32)((u64)0xFFFFFFFFu * (u64)0xFFFFFFFFu >> 32), 0xFFFFFFFEu, "mulhu edge");
    ck_eq(0u - 1u, 0xFFFFFFFFu, "wrap sub");
    ck_eq((u32)(signed char)0x80, 0xFFFFFF80u, "sext 8");
    ck_eq((u32)(short)0x8000, 0xFFFF8000u, "sext 16");
    ck_eq((u32)(unsigned char)0x1FF, 0xFFu, "trunc 8");
    ck_eq(add3(1, 2, 3), 6u, "add3");
    ck_eq((u32)imax(-5, -9), (u32)(-5), "imax");
    ck_eq((u32)fib(12), 144u, "fib recursion");
    ck_eq(sum_loop(100), 5050u, "loop sum");
    ck_eq(switchy(0), 0x10u, "switch 0");
    ck_eq(switchy(7), 0x17u, "switch 7");
    ck_eq(switchy(9), 0xFFu, "switch default");
    ck_eq((u32)g_ops[0](7, 5), 12u, "fnptr add");
    ck_eq((u32)g_ops[1](7, 5), 2u, "fnptr sub");
    ck_eq((u32)g_ops[2](7, 5), 35u, "fnptr mul");
}

static void test_u64(void)
{
    u64 m = 0x123456789ABCull;
    u64 n = 0x100000ull;

    ck_eq64(m + n, 0x123456889ABCull, "u64 add");
    ck_eq64(m * 3ull, 0x369D0369D034ull, "u64 mul");
    ck_eq64(m / n, 0x1234567ull, "u64 div");
    ck_eq64(m % n, 0x89ABCull, "u64 mod");
    ck_eq64(m >> 16, 0x12345678ull, "u64 srl");
    ck_eq64(m << 8, 0x123456789ABC00ull, "u64 sll");
    ck_eq64((u64)((i64)-1024 >> 4), (u64)(-64ll), "i64 sra");
    ck_eq((u32)(m > n), 1u, "u64 cmp");
    ck_eq((u32)(m == 0x123456789ABCull), 1u, "u64 eq");
    ck_eq64((u64)(u32)m, 0x56789ABCull, "u64 trunc");
    ck_eq64(m, (u64)0x123456789ABCull, "u64 literal");
}

static void test_mem(void)
{
    struct Pair p;
    struct Pair q;
    u8 raw[8];
    u16 half[4];

    p.a = 0xDEADBEEFu;
    p.b = 0x1234u;
    p.c = 0x5Au;
    q = p;
    ck_eq(q.a, p.a, "struct copy a");
    ck_eq((u32)q.b, 0x1234u, "struct copy b");
    ck_eq((u32)q.c, 0x5Au, "struct copy c");

    memset(raw, 0xAA, sizeof(raw));
    ck_eq((u32)raw[0], 0xAAu, "memset");
    ck_eq((u32)raw[7], 0xAAu, "memset tail");
    memcpy(raw + 1, raw, 3);
    ck_eq((u32)raw[2], 0xAAu, "memcpy");
    memmove(raw + 4, raw + 3, 3);
    ck_eq((u32)raw[4], 0xAAu, "memmove");

    half[0] = 0x1122u;
    half[1] = 0xFFEEu;
    ck_eq((u32)(short)half[1], 0xFFFFFFEEu, "half load");
    ck_eq((u32)half[0], 0x1122u, "half store");
    ck_eq((u32)raw[0] | ((u32)raw[1] << 8), 0xAAAAu, "byte order");
}

static void test_time(void)
{
    u32 t0 = 0;
    u32 t1 = 0;

    __asm__ __volatile__("csrr %0, time" : "=r"(t0));
    t1 = t0;
    __asm__ __volatile__("csrr %0, time" : "=r"(t1));
    ck(t1 >= t0, "csrr time monotonic");
}

void os_main(void)
{
    puts_("risc6 isa test\n");
    test_int();
    test_u64();
    test_mem();
    test_time();
    puts_("t_isa: ");
    put_u32((u32)g_checks);
    puts_(" check(s), ");
    put_u32((u32)g_fail);
    puts_(" failure(s)\n");
    exit_(g_fail);
}
