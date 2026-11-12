# risc6 —— 用 c6 写的 RV32IMAC 模拟器（native + wasm32）

一个**机器**模拟器，不是宿主模拟器：它模拟 RISC-V RV32IMAC 的 `pc`/寄存器/内存，
因此能在**指令边界**注入定时器中断——被模拟的内核真的跑在陷阱帧上，而宿主
（node / 浏览器 / native 进程）全程不需要栈切换能力。

- 源码：`src/rv32.c6`（单 TU，c6 语言子集：整数 + 数组 + 函数，无 GC/闭包/切片/libc）
- 产物：`build/risc6`（native x86-64）、`build/risc6.wasm`（wasm32，node/浏览器可跑）
- 定位：给 [c6LOS](../rtos-c6) 当 L1.5 参考机——不依赖 qemu 的 rv32 回归，且能证明抢占
- 设计：[docs/design.md](docs/design.md) · 任务板：[docs/todo.md](docs/todo.md)

## 快速开始

```sh
make                      # native 模拟器 -> build/risc6
make wasm                 # wasm32 模块  -> build/risc6.wasm
make check                # 自带用例 + 与 qemu-riscv32 对拍（装了才比）

build/risc6 build/tests/t_hello
node tools/run-wasm.mjs build/risc6.wasm build/tests/t_hello
```

跑任意 RV32 静态 ELF（含 c6LOS 的 rv32 参考机产物）：

```sh
build/risc6 /path/to/c6os/build/rv32/hello
```

## 支持范围

| 项 | 内容 |
|---|---|
| ISA | RV32I + M + A + C + Zicsr（子集）+ Zifencei + Zicntr |
| 运行模式 | **user**（Linux 静态 ELF，`write`/`exit`/`brk`/`read`/`close`）；**bare**（陷阱 + 中断注入，E5） |
| 时间 | `time/cycle/instret = 指令数 × 64`，确定性、可复现 |
| 内存 | 单块 8 MiB 平坦窗（ELF 段 + 1 MiB 栈）；无 MMU/cache |
| 不支持 | F/D 浮点、向量、B 扩展、S/U 态、多核、外设模型 |

## 目录

```
include/risc6.h     两侧共用：类型、尺寸、数据符号、入口
src/rv32.c6         核心：装载/译码/执行/syscall/诊断（单 TU）
port/host_native.c  native 宿主（main、文件、stdout/stderr）
port/host_wasm.c    wasm 宿主（导出：喂 ELF、取输出、取退出码）
tools/run-wasm.mjs  node 运行器
tools/diff-qemu.sh  与 qemu-riscv32 对拍
tests/              RV32 用例（C 写，clang 编成 rv32 ELF）
docs/               设计文档 + 任务板
```

## 依赖

| 依赖 | 版本/位置 | 用途 |
|---|---|---|
| c6 编译器 | `~/c6/tcc/c6`（`C6_ROOT`） | 编 `src/rv32.c6`（两条后端都靠它） |
| clang + lld | ≥ 19（`clang`、`ld.lld`、`wasm-ld`） | IR → x86-64 / wasm32 |
| node | ≥ 18 | 跑 wasm 产物（仅 `make wasm` 后需要） |
| qemu-riscv32 | 可选 | 对拍 oracle（缺失时 `make check` 跳过并提示） |

## 状态

**E0–E2 尚未施工**（骨架刚建）。按 `docs/todo.md` 推进，每个里程碑的验收数字写进提交信息。
