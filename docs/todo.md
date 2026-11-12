# risc6 任务板（E0–E6）

> 状态列：`未开始` / `进行中` / `完成` / `阻塞` · 设计：[design.md](design.md)
> 规则：一个里程碑一次提交；验收数字写进提交信息；上一里程碑未验收不进下一个。

## 施工状态（最新一次）

- 已落盘：`include/risc6.h`、`src/rv32.c6`（单 TU：装载/RV32IMAC+Zicsr/RVC 展开/
  user-mode syscall/诊断/预算/bare 模式 CSR 与 MTIP 注入）、`port/host_native.c`、
  `port/host_wasm.c`、`tools/run-wasm.mjs`、`tools/check.sh`、4 个用例、`Makefile`。
- **未验证**：`make` 首次构建暴露一个 Makefile 问题（`CC ?= clang` 被 make 内建 `CC=cc`
  顶掉，导致把 `.ll` 当链接脚本），已改成 `CC := clang`；`make && make check` 尚未跑通。
- 因此 E0-1..E0-4 实际是「代码已写、待首次编译验证」，状态统一记 `进行中`。
- 下一步（按顺序）：`make` → 修 c6 编译/IR 问题 → `make tests` → `make check`（native
  与 qemu 对拍）→ `make wasm` + node 对拍 → 回到 E3（c6LOS 真用例）。

---

## E0 骨架与工具链（打通两条后端）

| 编号 | 任务 | 产出 | 验收 | 状态 |
|---|---|---|---|---|
| E0-1 | 目录 + 两侧共用头 | `include/risc6.h`（类型/尺寸/数据符号/入口声明） | `c6 --emit-llvm` 与 `clang` 都能处理该头 | 未开始 |
| E0-2 | native 流水线 | `Makefile`：`c6 --emit-llvm` → `clang -O2 -fuse-ld=lld + port/host_native.c + libtcc1.a` | `make` 产出 `build/risc6`，`build/risc6` 无参数时打印用法并退出 | 未开始 |
| E0-3 | wasm 流水线 | `c6 --target=wasm32-unknown-unknown --emit-llvm` → `clang --target=wasm32 -c` → `wasm-ld`；`port/host_wasm.c` 导出 | `make wasm` 产出 `build/risc6.wasm`；`node tools/run-wasm.mjs` 能实例化并拿到退出码 | 未开始 |
| E0-4 | 空机器跑通 | 装载器 + 只认 `ecall 93` 的最小执行器 | 一个「`_start` 直接 `exit(7)`」的 ELF 在 native 与 wasm 下都返回 7 | 未开始 |

## E1 user-mode RV32I（能跑 hello）

| 编号 | 任务 | 验收 | 状态 |
|---|---|---|---|
| E1-1 | ELF32 装载（PT_LOAD、bss 清零、`__global_pointer$`、栈） | 装载失败给 200 + 诊断；成功时 `--trace` 首行 = `e_entry` | 未开始 |
| E1-2 | RV32I 全部 32 位指令 | `tests/t_isa.c` 的算术/移位/比较/跳转全部对 | 未开始 |
| E1-3 | syscall：`write(64)` / `exit(93,94)` | `tests/t_hello.c` 输出正确、退出码 0 | 未开始 |
| E1-4 | 诊断通道 | 非法指令 → 202、越界 → 203，诊断行含 `pc/insn/task` 风格上下文 | 未开始 |
| **E1 验收** | | `build/risc6 build/tests/t_hello` 输出与 `qemu-riscv32` **逐字节一致** | 未开始 |

## E2 M / A / C 扩展 + 对拍

| 编号 | 任务 | 验收 | 状态 |
|---|---|---|---|
| E2-1 | M 扩展 | `div/rem` 除零与溢出按规范；`mulh/mulhsu/mulhu` 边界（0x8000_0000） | 未开始 |
| E2-2 | RVC 展开表 | 全部 16 位指令展开正确；非法编码 → 202 | 未开始 |
| E2-3 | A 扩展 | `lr.w/sc.w` + 10 条 `amo*.w`；`tests/t_atomic.c` 通过 | 未开始 |
| E2-4 | 64 位软算法 | `__udivdi3/__umoddi3` 路径（c6LOS `rt/x86_libc.c` 的算法）结果对 | 未开始 |
| E2-5 | 对拍脚本 | `tools/diff-qemu.sh`：4 个用例 stdout+rc 全等 | 未开始 |
| **E2 验收** | | `make check` 全绿；覆盖率闸门无零命中 | 未开始 |

## E3 c6LOS 真用例（L2 落地）

| 编号 | 任务 | 验收 | 状态 |
|---|---|---|---|
| E3-1 | 跑 `rtos-c6/build/rv32/hello` | 输出 `c6os rv32 boot` / `PASS: rv32 hello`，rc=0 | 未开始 |
| E3-2 | 跑 `build/rv32/sched` | `PASS: rv32 sched`（协作切换真的换栈） | 未开始 |
| E3-3 | 跑 `build/rv32/tick` | `PASS: rv32 tick`；`elapsed` 在 99..101（时间源差异记 design §5.4） | 未开始 |
| E3-4 | 交叉验证 | 三个 ELF 在 native 与 wasm 下输出字节级一致 | 未开始 |
| **E3 验收** | | qemu 与 risc6 结论一致（`PASS` 行相同），risc6 不需要安装 qemu 即可跑 | 未开始 |

## E4 覆盖率与性能

| 编号 | 任务 | 验收 | 状态 |
|---|---|---|---|
| E4-1 | `--stats`：opcode / RVC 象限 / CSR / syscall 命中计数 | 全用例跑完后无零命中；结果落 `build/stats.txt` | 未开始 |
| E4-2 | 指令计数 | `risc6_stat_insns` 三个用例的数字记进 README「实测」 | 未开始 |
| E4-3 | 性能 | native ≥ 20M instr/s、wasm ≥ 8M instr/s（`time build/risc6 …`） | 未开始 |
| E4-4 | 预算保护 | 死循环用例在 `--limit 1000000` 下返回 201 而不是挂住 | 未开始 |

## E5 bare 模式（真抢占）

| 编号 | 任务 | 验收 | 状态 |
|---|---|---|---|
| E5-1 | CSR / 陷阱 | `mtvec`（direct+vectored）、`mepc`、`mcause`、`mret`、`mscratch` | 未开始 |
| E5-2 | 时间事件 | `mtime/mtimecmp`（或 `RISC6_SLICE`）→ `mip.MTIP` → 指令边界取陷阱 | 未开始 |
| E5-3 | 抢占用例 | 一个自旋任务 + tick ISR 计数：被抢占次数 ≈ 预算/slice（±1） | 未开始 |
| E5-4 | 陷阱帧对拍 | ISR 里保存/恢复全部 32 寄存器后自旋任务结果不变 | 未开始 |
| **E5 验收** | | 「真抢占」在 wasm 与 native 下都成立（宿主不需要栈切换能力） | 未开始 |

## E6 收尾

| 编号 | 任务 | 验收 | 状态 |
|---|---|---|---|
| E6-1 | c6LOS 集成 | c6LOS 加 `run-rv32-emu` / `RISC6` 变量；`docs/testing.md` L2 行更新 | 未开始 |
| E6-2 | 多 TU（可选） | C0-1/C0-2 合入后把 `src/rv32.c6` 拆成 cpu/mem/sys 三个 TU，验收全绿 | 未开始 |
| E6-3 | 浏览器演示（可选） | `tools/index.html` + canvas 仪表盘；与 native 输出一致 | 未开始 |
| E6-4 | 文档 | design §2 的「实测」表随工具链升级更新；README 状态置「可用」 | 未开始 |

---

## 施工顺序与依赖

```
E0 ──> E1 ──> E2 ──> E3 ──> E4
                      └──> E5 ──> E6
```

- E1 之前不要碰 RVC：先把 32 位路径与装载器做对，否则 bug 归因不了。
- E2-2（RVC）必须在 E3 之前：c6LOS 的 rv32 目标用 `-march=rv32imac`，clang 必然发压缩指令。
- E3-3（tick）依赖「时间 = 指令计数」这条设计（design §4），不要改用 wall clock。
- E5 依赖 E1–E3 全绿：陷阱与抢占是建在正确的执行器上的。

## 已知阻塞与前提

| 项 | 状态 | 影响 |
|---|---|---|
| C0-1（外部 declare 带类型）/ C0-2（逐 TU emit） | 未合入 | 强制单 TU、c6 不调 C（design §6.1） |
| `~/c6/tcc/libtcc1-wasm32.a` | 已存在（4.5 MB） | wasm 路径可用；若重新构建用 `scripts/build-wasm-runtime.sh` |
| qemu-riscv32 | 本机有 | 对拍；CI 上缺失时脚本应跳过并明确提示（不静默通过） |
