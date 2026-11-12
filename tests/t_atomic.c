/* tests/t_atomic.c —— A 扩展：lr.w/sc.w 与 amo*.w（经 clang 的 __atomic 内建）。 */

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

static u32 g_counter = 0;
static u32 g_target = 0;

void os_main(void)
{
    u32 old = 0;
    u32 expected = 0;
    u32 desired = 0;
    int ok = 0;
    int i = 0;

    /* amoadd.w */
    __atomic_store_n(&g_counter, 10u, __ATOMIC_SEQ_CST);
    old = __atomic_fetch_add(&g_counter, 5u, __ATOMIC_SEQ_CST);
    ck(old == 10u, "fetch_add old");
    ck(__atomic_load_n(&g_counter, __ATOMIC_SEQ_CST) == 15u, "fetch_add new");

    /* amoand/amoor/amoxor */
    __atomic_fetch_or(&g_counter, 0x100u, __ATOMIC_SEQ_CST);
    ck(__atomic_load_n(&g_counter, __ATOMIC_SEQ_CST) == 0x10Fu, "fetch_or");
    __atomic_fetch_and(&g_counter, 0xFFu, __ATOMIC_SEQ_CST);
    ck(__atomic_load_n(&g_counter, __ATOMIC_SEQ_CST) == 0x0Fu, "fetch_and");
    __atomic_fetch_xor(&g_counter, 0x03u, __ATOMIC_SEQ_CST);
    ck(__atomic_load_n(&g_counter, __ATOMIC_SEQ_CST) == 0x0Cu, "fetch_xor");

    /* amoswap / amomin / amomax */
    old = __atomic_exchange_n(&g_counter, 77u, __ATOMIC_SEQ_CST);
    ck(old == 0x0Cu && g_counter == 77u, "exchange");
    __atomic_store_n(&g_counter, 42u, __ATOMIC_SEQ_CST);
    __atomic_fetch_min(&g_counter, 7u, __ATOMIC_SEQ_CST);
    ck(g_counter == 7u, "fetch_min");
    __atomic_fetch_max(&g_counter, 99u, __ATOMIC_SEQ_CST);
    ck(g_counter == 99u, "fetch_max");

    /* 比较交换（lr/sc 循环） */
    __atomic_store_n(&g_target, 1234u, __ATOMIC_SEQ_CST);
    expected = 1234u;
    desired = 5678u;
    ok = __atomic_compare_exchange_n(&g_target, &expected, desired, 0,
                                     __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
    ck(ok != 0 && g_target == 5678u, "cas success");

    expected = 1u;
    desired = 2u;
    ok = __atomic_compare_exchange_n(&g_target, &expected, desired, 0,
                                     __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
    ck(ok == 0 && g_target == 5678u && expected == 5678u, "cas fail");

    /* 单线程递增循环：没有任何丢失 */
    __atomic_store_n(&g_counter, 0u, __ATOMIC_SEQ_CST);
    for (i = 0; i < 1000; i++) {
        __atomic_fetch_add(&g_counter, 1u, __ATOMIC_SEQ_CST);
    }
    ck(g_counter == 1000u, "loop add");

    puts_("t_atomic: ");
    put_u32((u32)g_checks);
    puts_(" check(s), ");
    put_u32((u32)g_fail);
    puts_(" failure(s)\n");
    exit_(g_fail);
}
