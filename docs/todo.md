# risc6 任务板（E0–E6）

> 状态列：`未开始` / `进行中` / `完成` / `阻塞` · 设计：[design.md](design.md)
> 规则：一个里程碑一次提交；验收数字写进提交信息；上一里程碑未验收不进下一个。

## 施工状态（E6 收尾）

- 全部里程碑 E0–E6 已完成，代码在 `include/risc6.h` + `src/rv32.c6`（单 TU）+ 两个宿主 +
  4 个工具脚本 + 5 个 RV32 用例；总计约 1400 行 c6 + 300 行宿主/脚本。
- 验收（本机 clang 19.1.4 / node v24.13.1）：
  - `make check` → `check: 4 用例全部一致`（native / wasm / qemu-riscv32 三方对拍）
  - `make selfcheck` → `selfcheck: 8 项全部通过`（预算 201、装载 200、空文件、参数）
  - `make bare` → native 与 wasm 都是 `PASS: bare traps`（16 次 MTIP 注入、`mcause=0x80000007`、
    `ecall`→`mcause=11`、陷阱帧完好）
  - c6LOS `build/rv32/{hello,sched,tick}` 输出与 qemu 逐字节一致（1776 / 35854 / 1594931 指令）
  - 性能：native ~47 Minsn/s，wasm（node）~35 Minsn/s；两后端指令计数器逐项相同
- 施工期踩到并修掉的真问题：`g_limit` 非零静态初始化把整个 .data 内联进 .wasm（9.5 MB）、
  RVC `c.jal`/`c.lui`/misaligned 取指、ELF 段名扫描提前 break 导致 gp 找不到、
  `csrsi` 的 uimm 是「或上的值」不是位号、测试里手写 `t0` 踩了编译器小数据基址。
- 剩余可选项：E6-2（等 c6 C0-1/C0-2 合入后拆多 TU）、E6-3（浏览器演示）。

---

## E0 骨架与工具链（打通两条后端）

| 编号 | 任务 | 产出 | 验收 | 状态 |
|---|---|---|---|---|
| E0-1 | 目录 + 两侧共用头 | `include/risc6.h`（类型/尺寸/数据符号/入口声明） | `c6 --emit-llvm` 与 `clang` 都能处理该头 | 完成 |
| E0-2 | native 流水线 | `Makefile`：`c6 --emit-llvm` → `clang -O2 -fuse-ld=lld + port/host_native.c + libtcc1.a` | `make` 产出 `build/risc6`，`build/risc6` 无参数时打印用法并退出 | 完成 |
| E0-3 | wasm 流水线 | `c6 --target=wasm32-unknown-unknown --emit-llvm` → `clang --target=wasm32 -c` → `wasm-ld`；`port/host_wasm.c` 导出 | `make wasm` 产出 `build/risc6.wasm`；`node tools/run-wasm.mjs` 能实例化并拿到退出码 | 完成 |
| E0-4 | 空机器跑通 | 装载器 + 只认 `ecall 93` 的最小执行器 | 一个「`_start` 直接 `exit(7)`」的 ELF 在 native 与 wasm 下都返回 7 | 完成 |

## E1 user-mode RV32I（能跑 hello）

| 编号 | 任务 | 验收 | 状态 |
|---|---|---|---|
| E1-1 | ELF32 装载（PT_LOAD、bss 清零、`__global_pointer$`、栈） | 装载失败给 200 + 诊断；成功时 `--trace` 首行 = `e_entry` | 完成 |
| E1-2 | RV32I 全部 32 位指令 | `tests/t_isa.c` 的算术/移位/比较/跳转全部对 | 完成 |
| E1-3 | syscall：`write(64)` / `exit(93,94)` | `tests/t_hello.c` 输出正确、退出码 0 | 完成 |
| E1-4 | 诊断通道 | 非法指令 → 202、越界 → 203，诊断行含 `pc/insn/task` 风格上下文 | 完成 |
| **E1 验收** | | `build/risc6 build/tests/t_hello` 输出与 `qemu-riscv32` **逐字节一致** | 完成（352 条指令） |

## E2 M / A / C 扩展 + 对拍

| 编号 | 任务 | 验收 | 状态 |
|---|---|---|---|
| E2-1 | M 扩展 | `div/rem` 除零与溢出按规范；`mulh/mulhsu/mulhu` 边界（0x8000_0000） | 完成 |
| E2-2 | RVC 展开表 | 全部 16 位指令展开正确；非法编码 → 202 | 完成 |
| E2-3 | A 扩展 | `lr.w/sc.w` + 10 条 `amo*.w`；`tests/t_atomic.c` 通过 | 完成 |
| E2-4 | 64 位软算法 | `__udivdi3/__umoddi3` 路径（c6LOS `rt/x86_libc.c` 的算法）结果对 | 完成 |
| E2-5 | 对拍脚本 | `tools/check.sh`（原计划名 diff-qemu.sh）：4 个用例 stdout+rc 全等 | 完成 |
| **E2 验收** | | `make check` 全绿（4 用例三方一致）；覆盖率闸门无零命中 | 完成 |

## E3 c6LOS 真用例（L2 落地）

| 编号 | 任务 | 验收 | 状态 |
|---|---|---|---|
| E3-1 | 跑 `rtos-c6/build/rv32/hello` | 输出 `c6os rv32 boot` / `PASS: rv32 hello`，rc=0 | 完成 |
| E3-2 | 跑 `build/rv32/sched` | `PASS: rv32 sched`（协作切换真的换栈） | 完成 |
| E3-3 | 跑 `build/rv32/tick` | `PASS: rv32 tick`；`elapsed` 在 99..101（时间源差异记 design §5.4） | 完成 |
| E3-4 | 交叉验证 | 三个 ELF 在 native 与 wasm 下输出字节级一致 | 完成 |
| **E3 验收** | | qemu 与 risc6 结论一致（`PASS` 行相同），risc6 不需要安装 qemu 即可跑 | 完成（hello/sched/tick 逐字节一致） |

## E4 覆盖率与性能

| 编号 | 任务 | 验收 | 状态 |
|---|---|---|---|
| E4-1 | `--stats`：opcode / RVC 象限 / CSR / syscall 命中计数 | 全用例跑完后无零命中；结果落 `build/stats.txt` | 完成（24 个计数器，native/wasm 相同） |
| E4-2 | 指令计数 | `risc6_stat_insns` 三个用例的数字记进 README「实测」 | 完成 |
| E4-3 | 性能 | native ≥ 20M instr/s、wasm ≥ 8M instr/s（`time build/risc6 …`） | 完成（47.5 / 35.5 Minsn/s） |
| E4-4 | 预算保护 | 死循环用例在 `--limit 1000000` 下返回 201 而不是挂住 | 完成 |

## E5 bare 模式（真抢占）

| 编号 | 任务 | 验收 | 状态 |
|---|---|---|---|
| E5-1 | CSR / 陷阱 | `mtvec`（direct+vectored）、`mepc`、`mcause`、`mret`、`mscratch` | 完成 |
| E5-2 | 时间事件 | `mtime/mtimecmp`（或 `RISC6_SLICE`）→ `mip.MTIP` → 指令边界取陷阱 | 完成 |
| E5-3 | 抢占用例 | 一个自旋任务 + tick ISR 计数：被抢占次数 ≈ 预算/slice（±1） | 完成（801k 指令 → 16 次 = 802k/50000） |
| E5-4 | 陷阱帧对拍 | ISR 里保存/恢复全部 32 寄存器后自旋任务结果不变 | 完成 |
| **E5 验收** | | 「真抢占」在 wasm 与 native 下都成立（宿主不需要栈切换能力） | 完成（16 次注入，两后端同结果） |

## E6 收尾

| 编号 | 任务 | 验收 | 状态 |
|---|---|---|---|
| E6-1 | c6LOS 集成 | c6LOS 加 `run-rv32-emu` / `RISC6` 变量；`docs/testing.md` L2 行更新 | 完成 |
| E6-2 | 多 TU（可选） | C0-1/C0-2 合入后把 `src/rv32.c6` 拆成 cpu/mem/sys 三个 TU，验收全绿 | 暂缓（等 C0-1/C0-2） |
| E6-3 | 浏览器演示（可选） | `tools/index.html` + canvas 仪表盘；与 native 输出一致 | 暂缓（本机无浏览器） |
| E6-4 | 文档 | design §2 的「实测」表随工具链升级更新；README 状态置「可用」 | 完成 |

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
