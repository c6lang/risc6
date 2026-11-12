#ifndef RISC6_H
#define RISC6_H

/* risc6：RV32IMAC 机器模拟器 —— 两侧共用头。
 * c6（src/rv32.c6）与 C（port/host_*.c）都包含本文件。
 * 硬规则（docs/design.md §5.2/§6.1）：
 *   - 指针永不过边界；跨界只允许 int/u32/u64/void；
 *   - c6 侧不调用 C 函数；共享通道只有下面的数据符号。
 */

/* 版本号：发布 tag vX.Y.Z 与此处同步（CHANGELOG.md 有对应条目） */
#define RISC6_VERSION "0.1.0"

typedef unsigned char      u8;
typedef unsigned short     u16;
typedef unsigned int       u32;
typedef unsigned long long u64;
typedef signed int         i32;
typedef signed long long   i64;

#define RISC6_MEM_SIZE   (8u * 1024u * 1024u)   /* guest 平坦窗 */
#define RISC6_IN_MAX     (1u * 1024u * 1024u)   /* ELF 装载输入 */
#define RISC6_OUT_MAX    (64u * 1024u)          /* guest 控制台 */
#define RISC6_DIAG_MAX   (4u * 1024u)           /* 模拟器自身诊断 */

#define RISC6_STACK_SIZE (1u * 1024u * 1024u)   /* guest 栈 */
#define RISC6_STACK_GAP  (64u * 1024u)

#define RISC6_TIME_SCALE 64u                    /* time = 指令数 × 64 */
#define RISC6_SLICE      50000u                 /* bare 模式中断注入切片 */
#define RISC6_MAX_INSNS  2000000000ull          /* 默认指令预算 */

/* MMIO：bare 模式没有系统调用，guest 用这两条写来输出/退出（§5.3） */
#define RISC6_MMIO_BASE    0x40000000u
#define RISC6_MMIO_CONSOLE RISC6_MMIO_BASE          /* 写：输出一个字节 */
#define RISC6_MMIO_EXIT    (RISC6_MMIO_BASE + 4u)   /* 写 32 位：退出码（低 8 位） */

/* 覆盖率统计槽位（docs/design.md §10 E4）。宿主只读，c6 侧累加。 */
#define RISC6_STAT_N     24
#define ST_LUI        0
#define ST_AUIPC      1
#define ST_JAL        2
#define ST_JALR       3
#define ST_BRANCH     4
#define ST_LOAD       5
#define ST_STORE      6
#define ST_OPIMM      7
#define ST_OP         8
#define ST_FENCE      9
#define ST_AMO        10
#define ST_SYSTEM     11
#define ST_BAD        12   /* 非法/未支持指令 */
#define ST_RVC        13   /* 压缩指令条数 */
#define ST_ECALL      14
#define ST_TRAP       15   /* 进入陷阱次数 */
#define ST_MTIP       16   /* 注入的定时器中断次数 */
#define ST_CSRUNK     17   /* 未实现 CSR 访问次数 */

/* ---- 与宿主共享的数据符号：由 src/rv32.c6 定义（定义侧定义 RISC6_CORE） ---- */
#ifndef RISC6_CORE
extern u8  risc6_mem[RISC6_MEM_SIZE];   /* guest 内存窗（宿主只读） */
extern u8  risc6_in[RISC6_IN_MAX];      /* ELF 装载输入（宿主写） */
extern u8  risc6_out[RISC6_OUT_MAX];    /* guest 输出（宿主读） */
extern u32 risc6_out_len;
extern u8  risc6_diag[RISC6_DIAG_MAX];  /* 模拟器诊断（宿主读） */
extern u32 risc6_diag_len;
extern u64 risc6_stat_insns;            /* 指令计数（宿主读） */
extern u32 risc6_stat[RISC6_STAT_N];    /* 覆盖率统计（宿主读） */
#endif /* RISC6_CORE */

/* ---- 入口：c6 定义、宿主调用 ---- */
int  risc6_run(u32 in_len);
void risc6_set_limit(u64 max_insns);
void risc6_set_trace(u32 on);
void risc6_set_mode(u32 bare);          /* 0 = user（默认），1 = bare */

/* 退出码（模拟器自身，guest 退出码 0..255 透传） */
#define RISC6_RC_LOAD    200
#define RISC6_RC_BUDGET  201
#define RISC6_RC_ILLEGAL 202
#define RISC6_RC_MEM     203
#define RISC6_RC_INTERNAL 204

#endif /* RISC6_H */
