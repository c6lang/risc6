/* include/risc6_core.h —— risc6 两个翻译单元之间的接口（只有 c6 侧包含）。
 *
 * 平台胶水层（port/host_*.c）看不到这个头：跨界契约仍然只有 risc6.h §5.2
 * 里那几个数据符号和四个入口。
 *
 * 拆 TU 的边界：热路径（取指/译码/访存/陷阱/主循环）在 src/rv32.c6，
 * 冷路径（诊断、控制台、系统调用、ELF 装载、参数入口）在 src/sys.c6。
 * 机器状态在 rv32.c6 里定义一次，这里给 sys.c6 的 extern 声明。
 */

#ifndef RISC6_CORE_H
#define RISC6_CORE_H

#include "risc6.h"

/* ---- 机器状态（定义在 src/rv32.c6） ---- */
extern u32 g_x[32];
extern u32 g_pc;
extern u32 g_base;
extern u32 g_size;
extern u32 g_brk;
extern u32 g_brk_max;
extern u64 g_insns;
extern u64 g_limit;
extern u64 g_next_event;
extern u32 g_trace;
extern u32 g_stop;
extern u32 g_rc;
extern u32 g_mode;
extern u32 g_reserved;
extern u32 g_in_len;

extern u32 g_mstatus;
extern u32 g_mie;
extern u32 g_mip;
extern u32 g_mtvec;
extern u32 g_mepc;
extern u32 g_mcause;
extern u32 g_mtval;
extern u32 g_mscratch;

extern u32 g_unknown[16];
extern u32 g_unknown_n;

/* ---- src/rv32.c6 提供，src/sys.c6 使用 ---- */
void risc6_cpu_reset(void);                 /* 复位全部机器状态 */
void risc6_cpu_run(void);                   /* 主循环（直到 g_stop） */
u32  risc6_mem_ok(u32 addr, u32 n);         /* guest 窗口内？ */
void risc6_mem_fault(u32 addr, u32 wr);     /* 越界：陷阱或 203 */
void risc6_take_trap(u32 cause, u32 tval, u32 epc);

/* ---- src/sys.c6 提供，src/rv32.c6 使用 ---- */
void risc6_diag_ch(int c);
void risc6_diag_str(const char *s);
void risc6_diag_hex(u32 v);
void risc6_diag_dec(u64 v);
void risc6_out_ch(int c);                   /* guest 控制台（MMIO / write） */
void risc6_ecall(void);                     /* SYSTEM 指令里的 ecall */

#endif /* RISC6_CORE_H */
