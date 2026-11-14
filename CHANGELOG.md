# 变更记录

格式：Keep a Changelog 风格；版本号遵循语义化版本，发布 tag 为 `vX.Y.Z`。

## [Unreleased]

### 变更

- **构建系统**：`Makefile` 迁移为模块化 `CMakeLists.txt` + `cmake/*.cmake`（toolchain / build / targets 三模块）。c6 经其 CMake 包（`find_package(c6)`）、pkgconf（`c6.pc`）或源码树（`C6_ROOT`，等价于旧 Makefile 的 `C6_ROOT`）三种方式定位，优先用 cmake / pkgconf 提供的接口。原有 `make` / `make wasm` / `check` / `selfcheck` / `bare` / `harts` / `run` / `stats` 对应 `cmake --build build [--target <name>]`，并登记为 CTest 用例。

## [0.2.0] — 2026-11-13

### 新增

- **多 hart**：`--harts N`（1–4）。每个 hart 有独立的 32 个寄存器、PC、mstatus/mie/mip/
  mtvec/mepc/mcause/mtval/mscratch、lr/sc 保留地址和 MTIP 切片；主循环按 2000 条指令
  切片轮转，共享 guest 内存、控制台、MMIO 与全局指令预算。`mhartid` 返回当前 hart。
  native 与 wasm 的 `t_harts` 都是 4078 条指令、`PASS: two harts`；
  `t_harts_irq` 验证每 hart 独立 `mtvec/mscratch/mie/mstatus` 与 MTIP，
  native/wasm 都是 4,001,339 条指令、`PASS: per-hart timer irq`；
  `t_harts_atomic` 两 hart 各 100k 次 AMO 加到共享变量，native/wasm 都是
  604,118 条指令、`PASS: two-hart atomic add`。

### 变更

- 核心拆成两个翻译单元：`src/rv32.c6`（热路径：取指/译码/访存/陷阱/主循环）与
  `src/sys.c6`（冷路径：诊断/控制台/系统调用/ELF 装载/参数入口），共享状态走
  `include/risc6_core.h`。两个后端都变成「多 `.ll` → 一次链接」。性能 47.5 → 44.0 Minsn/s
  （-7%，热循环没有跨 TU 调用）。
- `include/risc6_core.h`：新增的核心内部接口（不含宿主可见契约）。

### 说明

- c6 现在按输出 triple 取指针/`long`/`size_t` 宽度（wasm32 上是 4 字节），所以
  `design.md §6.1` 里「不用 `long`」的规矩从「必须」变成「风格」；risc6 仍然只用
  `u32`/`u8`，两个后端因此逐字节可比。

### 修复

- 不再需要 `tools/ll-zero.py`：c6 的 LLVM 后端现在把数据段里的长全零段（≥32 字节）
  切成独立的 `zeroinitializer` 成员，`1 MiB 全零数组 + 非零标量` 的 `.ll` 从 3.1 MB
  降到 11 KB。risc6 的 `Makefile` 因此回到「c6 直出 `.ll` → clang/wasm-ld」两步，
  小工具与其在 `Makefile` 里的两次调用一起删掉。

## [0.1.0] — 2026-11-12

首个可用版本：`c6` 写的 RV32IMAC+Zicsr 机器模拟器，native 与 wasm32 双后端功能等价。

### 新增

- **执行器**：RV32I 全部 32 位指令、M（`mul/div/rem`）、A（`lr/sc` + 10 条 `amo*.w`）、
  C（完整 RVC 展开表）、Zicsr、Zifencei、Zicntr；非对齐访问按「放行」处理（见偏差）。
- **装载器**：ELF32（EM_RISCV=243、ET_EXEC、PT_LOAD、拒绝 PT_INTERP）、bss 清零、
  4 KiB 对齐基址、栈顶自动布局、`__global_pointer$` 取 gp、brk 缓冲区；
  失败返回 200 并给诊断行。
- **两种运行模式**：user 模式（`ecall` 系统调用：`write/read/close/exit/exit_group/brk`，
  未知调用返回 `-ENOSYS` 并打印一次诊断）；bare 模式（`mtvec` 直连/向量、`mepc`、
  `mcause`、`mtval`、`mscratch`、`mret`、`mstatus.MIE`、`mie.MTIE`，指令边界注入
  `mip.MTIP`——宿主不需要栈切换能力就能真抢占）。选择方式：`--bare` 或 ELF 内
  `.risc6_bare` 段。
- **控制台/退出（bare）**：MMIO `0x4000_0000`（写 1 字节）/ `0x4000_0004`（写退出码）。
- **确定性**：`time/cycle/instret = 指令数 × 64`；`RISC6_SLICE=50000` 条指令注入一次中断。
- **预算保护**：默认 2e9 条指令，超出返回 201（`--limit N` 可调；`--limit 0` = 默认值）。
- **可观测性**：`--stats` 输出 24 个覆盖率计数器（opcode 分类、RVC、ecall、trap、mtip、
  未实现 CSR），`--time` 输出墙钟与 Minsn/s，`--trace` 逐指令跟踪，`--version`。
- **宿主**：`port/host_native.c`（native，链接 c6 的 `libtcc1.a`）、
  `port/host_wasm.c` + `tools/run-wasm.mjs`（wasm32，node 里跑）。
- **工具**：`tools/check.sh`（native / wasm / qemu-riscv32 三方对拍）、
  `tools/selfcheck.sh`（预算与装载错误码自检）、`tools/ll-zero.py`（把 c6 生成的
  全零 `.data` blob 改写成 `zeroinitializer`，`.wasm` 从 9.5 MB 降到 16 KB）。
- **用例**：`tests/t_hello.c`、`t_isa.c`（53 项）、`t_mem.c`（20 项）、`t_atomic.c`（11 项）、
  `t_bare.c`（bare 模式：陷阱/抢占/寄存器帧/`ecall` 异常）。

### 实测（clang 19.1.4、node v24.13.1、本机 x86-64）

| 项 | 结果 |
|---|---|
| `make check` | 4 用例 native / wasm / qemu-riscv32 stdout+退出码全一致 |
| `make selfcheck` | 8 项全通过（预算 201、装载 200、空文件、用法） |
| `make bare` | native 与 wasm 均 `PASS: bare traps`；801k 条指令 → 16 次 MTIP 注入，`mcause=0x80000007`，陷阱帧完好 |
| c6LOS `build/rv32/{hello,sched,tick}` | 与 qemu 逐字节一致（1776 / 35854 / 1594931 条指令） |
| 性能 | native 47.5 Minsn/s，wasm（node）35.5 Minsn/s |

### 已知偏差（详见 docs/design.md §5.4）

- 非对齐访存放行（真机会触发异常）。
- `time` 由指令数换算（×64），不是真实时钟。
- `brk` 是固定窗口，不是真实堆增长。
- 单核语义：`lr/sc` 只保证单 hart 的保留集。
- `fence` 按 nop 处理。
- `wfi` 按 nop 处理（没有事件模型）。
- 未知 CSR 读写：读 0、忽略写，并计入 `csr-unimpl` 计数器。

### 已知限制

- c6 的 C0-1/C0-2（外部 declare 带类型、逐 TU emit）未合入，因此核心是**单 TU**
  （`src/rv32.c6`），且 c6 侧不调用 C；宿主与核心只通过共享数据符号通信。
- 浏览器演示（`tools/index.html`）未提供：本机无浏览器可验证。

[0.1.0]: https://github.com/c6lang/risc6/releases/tag/v0.1.0
