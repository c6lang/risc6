/* wasm32 宿主：把 c6 侧的数据符号地址与入口转成可导出函数。
 * 地址一律在 C 侧取（>risc6_mem 是 c6 定义的数组，这里只有 extern 符号），
 * 指针不出这个 TU（docs/design.md §5.2）。 */

#include "risc6.h"

unsigned long wx_in_addr(void);
unsigned long wx_run(unsigned long len);
unsigned long wx_out_addr(void);
unsigned long wx_out_len(void);
unsigned long wx_diag_addr(void);
unsigned long wx_diag_len(void);
unsigned long wx_insns_lo(void);
unsigned long wx_insns_hi(void);
void wx_set_limit(unsigned long long n);

unsigned long wx_in_addr(void)
{
    return (unsigned long)(risc6_in);
}

unsigned long wx_run(unsigned long len)
{
    return (unsigned long)risc6_run((u32)len);
}

unsigned long wx_out_addr(void)
{
    return (unsigned long)(risc6_out);
}

unsigned long wx_out_len(void)
{
    return (unsigned long)risc6_out_len;
}

unsigned long wx_diag_addr(void)
{
    return (unsigned long)(risc6_diag);
}

unsigned long wx_diag_len(void)
{
    return (unsigned long)risc6_diag_len;
}

unsigned long wx_insns_lo(void)
{
    return (unsigned long)(risc6_stat_insns & 0xFFFFFFFFull);
}

unsigned long wx_insns_hi(void)
{
    return (unsigned long)(risc6_stat_insns >> 32);
}

void wx_set_limit(unsigned long long n)
{
    risc6_set_limit((u64)n);
}
