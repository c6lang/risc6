/* tests/t_mem.c —— 访存与指针密集用例：各宽度存取、结构体数组、指针链、
 * 重叠 memmove、字符串、函数指针比较器排序。 */

#include "testlib.h"

#define POOL 64

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

struct Node {
    struct Node *next;
    u32 key;
    u8 tag;
};

static struct Node g_pool[POOL];
static u32 g_used = 0;

static struct Node *alloc_node(u32 key, u8 tag)
{
    struct Node *n = &g_pool[g_used];
    if (g_used >= POOL) {
        return (struct Node *)0;
    }
    g_used++;
    n->key = key;
    n->tag = tag;
    n->next = (struct Node *)0;
    return n;
}

static void push(struct Node **head, u32 key)
{
    struct Node *n = alloc_node(key, (u8)(key & 0xFFu));
    if (n == (struct Node *)0) {
        return;
    }
    n->next = *head;
    *head = n;
}

static u32 sum_list(const struct Node *h)
{
    u32 s = 0;
    while (h != (const struct Node *)0) {
        s += h->key;
        h = h->next;
    }
    return s;
}

static int cmp_u32(const void *a, const void *b)
{
    u32 x = *(const u32 *)a;
    u32 y = *(const u32 *)b;
    if (x < y) {
        return -1;
    }
    if (x > y) {
        return 1;
    }
    return 0;
}

static void sort_u32(u32 *v, int n, int (*cmp)(const void *, const void *))
{
    int i = 0;
    int j = 0;
    for (i = 1; i < n; i++) {
        u32 key = v[i];
        j = i - 1;
        while (j >= 0 && cmp(&v[j], &key) > 0) {
            v[j + 1] = v[j];
            j--;
        }
        v[j + 1] = key;
    }
}

void os_main(void)
{
    struct Node *head = (struct Node *)0;
    u32 nums[8] = {5u, 3u, 9u, 1u, 0u, 7u, 2u, 8u};
    char buf[32];
    u8 bytes[8];
    u16 halfs[4];
    u32 words[4];
    int i = 0;

    /* 各宽度读写 */
    bytes[0] = 0x12u;
    bytes[1] = 0x34u;
    bytes[2] = 0x56u;
    bytes[3] = 0x78u;
    ck((u32)bytes[0] == 0x12u, "u8 store/load");
    halfs[0] = 0xABCDu;
    ck((u32)halfs[0] == 0xABCDu, "u16 store/load");
    words[0] = 0x89ABCDEFu;
    ck(words[0] == 0x89ABCDEFu, "u32 store/load");
    ck(((u32)bytes[0] | ((u32)bytes[1] << 8)) == 0x3412u, "little endian");

    /* 指针链 */
    for (i = 0; i < 10; i++) {
        push(&head, (u32)(i + 1));
    }
    ck(sum_list(head) == 55u, "list sum 1..10");
    ck(head != (struct Node *)0 && head->key == 10u, "list head");

    /* 重叠 movemove：向前/向后 */
    for (i = 0; i < 8; i++) {
        buf[i] = (char)('a' + i);
    }
    buf[8] = '\0';
    memmove(buf + 2, buf, 6);
    ck(buf[0] == 'a' && buf[2] == 'a' && buf[7] == 'f', "memmove fwd overlap");
    memmove(buf, buf + 2, 6);
    ck(buf[0] == 'a' && buf[1] == 'b', "memmove back overlap");

    /* 排序（函数指针 + 比较器） */
    sort_u32(nums, 8, cmp_u32);
    ck(nums[0] == 0u && nums[7] == 9u, "sort bounds");
    for (i = 1; i < 8; i++) {
        ck(nums[i - 1] <= nums[i], "sort order");
    }

    /* 结构体数组与按值拷贝 */
    {
        struct Node local[4];
        for (i = 0; i < 4; i++) {
            local[i].key = (u32)(i * 7 + 1);
            local[i].tag = (u8)i;
            local[i].next = (struct Node *)0;
        }
        local[1] = local[3];
        ck(local[1].key == 22u && local[1].tag == 3u, "struct array copy");
        memcpy(&local[2], &local[0], sizeof(local[0]));
        ck(local[2].key == 1u, "memcpy struct");
    }

    /* 字符串 */
    {
        char s[16];
        memset(s, 0, sizeof(s));
        s[0] = 'o';
        s[1] = 'k';
        s[2] = '\0';
        ck(memcmp(s, "ok", 2) == 0, "memcmp equal");
        ck(memcmp(s, "on", 2) != 0, "memcmp differ");
    }

    puts_("t_mem: ");
    put_u32((u32)g_checks);
    puts_(" check(s), ");
    put_u32((u32)g_fail);
    puts_(" failure(s)\n");
    exit_(g_fail);
}
