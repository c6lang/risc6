/* tests/support.c —— rv32imuac 上 clang 需要的 mem* 与 64 位 libcall。
 * 自带实现（不用 compiler-rt），顺便当被测代码：每条都是真实的整数/循环/
 * 64 位运算路径。 */

typedef unsigned char u8;
typedef unsigned int u32;
typedef unsigned long long u64;
typedef long long i64;

void *memcpy(void *dst, const void *src, unsigned long n)
{
    u8 *d = (u8 *)dst;
    const u8 *s = (const u8 *)src;

    while (n-- > 0UL) {
        *d++ = *s++;
    }
    return dst;
}

void *memmove(void *dst, const void *src, unsigned long n)
{
    u8 *d = (u8 *)dst;
    const u8 *s = (const u8 *)src;

    if (d < s) {
        while (n-- > 0UL) {
            *d++ = *s++;
        }
    } else {
        d += n;
        s += n;
        while (n-- > 0UL) {
            *--d = *--s;
        }
    }
    return dst;
}

void *memset(void *dst, int c, unsigned long n)
{
    u8 *d = (u8 *)dst;

    while (n-- > 0UL) {
        *d++ = (u8)c;
    }
    return dst;
}

int memcmp(const void *a, const void *b, unsigned long n)
{
    const u8 *x = (const u8 *)a;
    const u8 *y = (const u8 *)b;

    while (n-- > 0UL) {
        if (*x != *y) {
            return (int)*x - (int)*y;
        }
        x++;
        y++;
    }
    return 0;
}

static u64 udiv64(u64 a, u64 b, u64 *rem)
{
    u64 q = 0ull;
    u64 r = 0ull;
    int i = 63;

    if (b == 0ull) {
        if (rem != (u64 *)0) {
            *rem = 0ull;
        }
        return 0ull;
    }
    while (i >= 0) {
        r = (r << 1) | ((a >> (u32)i) & 1ull);
        if (r >= b) {
            r -= b;
            q |= (1ull << (u32)i);
        }
        i--;
    }
    if (rem != (u64 *)0) {
        *rem = r;
    }
    return q;
}

unsigned long long __udivdi3(unsigned long long a, unsigned long long b)
{
    return udiv64(a, b, (u64 *)0);
}

unsigned long long __umoddi3(unsigned long long a, unsigned long long b)
{
    u64 r = 0ull;
    (void)udiv64(a, b, &r);
    return r;
}

long long __divdi3(long long a, long long b)
{
    int neg = 0;
    u64 ua = (u64)a;
    u64 ub = (u64)b;
    u64 q = 0ull;

    if (a < 0) {
        neg = !neg;
        ua = (u64)(-a);
    }
    if (b < 0) {
        neg = !neg;
        ub = (u64)(-b);
    }
    q = udiv64(ua, ub, (u64 *)0);
    return neg ? -(i64)q : (i64)q;
}

long long __moddi3(long long a, long long b)
{
    int neg = 0;
    u64 ua = (u64)a;
    u64 ub = (u64)b;
    u64 r = 0ull;

    if (a < 0) {
        neg = !neg;
        ua = (u64)(-a);
    }
    if (b < 0) {
        ub = (u64)(-b);
    }
    (void)udiv64(ua, ub, &r);
    return neg ? -(i64)r : (i64)r;
}

unsigned long long __ashldi3(unsigned long long v, int s)
{
    if (s <= 0) {
        return v;
    }
    if (s >= 64) {
        return 0ull;
    }
    return v << (u32)s;
}

unsigned long long __lshrdi3(unsigned long long v, int s)
{
    if (s <= 0) {
        return v;
    }
    if (s >= 64) {
        return 0ull;
    }
    return v >> (u32)s;
}

long long __ashrdi3(long long v, int s)
{
    if (s <= 0) {
        return v;
    }
    if (s >= 64) {
        return (v < 0) ? -1ll : 0ll;
    }
    return v >> (u32)s;
}
