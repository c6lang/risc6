/* tests/testlib.h —— rv32 user-mode 用例的公共部分。
 * 只依赖 ecall write(64)/exit(93)，与 c6LOS 的 port/rv32/console.c 同一条路。 */

#ifndef TESTLIB_H
#define TESTLIB_H

typedef unsigned char      u8;
typedef unsigned short     u16;
typedef unsigned int       u32;
typedef unsigned long long u64;
typedef long long          i64;
typedef signed int         i32;

/* support.c 提供 */
void *memcpy(void *dst, const void *src, unsigned long n);
void *memmove(void *dst, const void *src, unsigned long n);
void *memset(void *dst, int c, unsigned long n);
int memcmp(const void *a, const void *b, unsigned long n);

#define OUT_CAP 4096

static char g_out[OUT_CAP];
static int g_out_n = 0;

static long sc3(long n, long a, long b, long c)
{
    register long a0 __asm__("a0") = a;
    register long a1 __asm__("a1") = b;
    register long a2 __asm__("a2") = c;
    register long a7 __asm__("a7") = n;

    __asm__ __volatile__("ecall" : "+r"(a0) : "r"(a1), "r"(a2), "r"(a7) : "memory");
    return a0;
}

static void flush_out(void)
{
    if (g_out_n > 0) {
        sc3(64, 1, (long)g_out, g_out_n);
        g_out_n = 0;
    }
}

static void putc_(int c)
{
    if (g_out_n >= OUT_CAP) {
        flush_out();
    }
    g_out[g_out_n++] = (char)c;
}

static void puts_(const char *s)
{
    while (*s != '\0') {
        putc_((int)*s);
        s++;
    }
}

static void put_hex(u32 v)
{
    int i = 8;
    puts_("0x");
    while (i > 0) {
        u32 d = 0;
        i--;
        d = (v >> (u32)(i * 4)) & 0xFu;
        putc_((int)(d < 10u ? ('0' + d) : ('a' + (d - 10u))));
    }
}

static void put_u32(u32 v)
{
    char buf[16];
    int n = 0;

    if (v == 0u) {
        putc_('0');
        return;
    }
    while (v != 0u && n < 16) {
        buf[n] = (char)('0' + (int)(v % 10u));
        v = v / 10u;
        n++;
    }
    while (n > 0) {
        n--;
        putc_((int)buf[n]);
    }
}

static void put_i32(int v)
{
    if (v < 0) {
        putc_('-');
        put_u32((u32)(-(long)v));
        return;
    }
    put_u32((u32)v);
}

static void exit_(int code)
{
    flush_out();
    sc3(93, code, 0, 0);
    for (;;) {
    }
}

#endif /* TESTLIB_H */
