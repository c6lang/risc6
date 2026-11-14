# risc6 —— 用 c6 写的 RV32IMAC 模拟器（native + wasm32）

> 版本 **0.2.0** · [CHANGELOG](CHANGELOG.md) · Apache-2.0 · `risc6 --version`

一个**机器**模拟器，不是宿主模拟器：它模拟 RISC-V RV32IMAC 的 `pc`/寄存器/内存，
因此能在**指令边界**注入定时器中断——被模拟的内核真的跑在陷阱帧上，而宿主
（node / 浏览器 / native 进程）全程不需要栈切换能力。

- 源码：`src/rv32.c6`（热路径）+ `src/sys.c6`（装载/系统调用/诊断），c6 语言子集：
  整数 + 数组 + 函数，无 GC/闭包/切片/libc
- 产物：`build/risc6`（native x86-64）、`build/risc6.wasm`（wasm32，node/浏览器可跑）
- 定位：给 [c6LOS](../rtos-c6) 当 L1.5 参考机——不依赖 qemu 的 rv32 回归，且能证明抢占
- 设计：[docs/design.md](docs/design.md) · 任务板：[docs/todo.md](docs/todo.md)

## 快速开始

```sh
cmake -S . -B build       # 配置（c6 经 find_package / pkgconf / C6_ROOT 定位）
cmake --build build       # native 模拟器 -> build/risc6
cmake --build build --target risc6-wasm   # wasm32 模块 -> build/risc6.wasm
cmake --build build --target check         # 自带用例 + 与 qemu-riscv32 对拍（装了才比）
cmake --build build --target run           # 跑默认用例 t_hello（指定其他 ELF 用 -DRISC6_RUN_ELF=...，或直接 ./build/risc6 <elf>）

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
| 多 hart | `--harts N`（1–4）：每 hart 独立寄存器/CSR/MTIP 切片，共享内存与 MMIO；native 与 wasm 同指令数 |
| 时间 | `time/cycle/instret = 指令数 × 64`，确定性、可复现 |
| 内存 | 单块 8 MiB 平坦窗（ELF 段 + 1 MiB 栈）；无 MMU/cache |
| 不支持 | F/D 浮点、向量、B 扩展、S/U 态、多核、外设模型 |

## 目录

```
include/risc6.h     两侧共用：类型、尺寸、数据符号、入口
src/rv32.c6         热路径：取指/译码/访存/陷阱/主循环
src/sys.c6          冷路径：诊断/控制台/syscall/ELF 装载/参数入口
port/host_native.c  native 宿主（main、文件、stdout/stderr）
port/host_wasm.c    wasm 宿主（导出：喂 ELF、取输出、取退出码）
tools/run-wasm.mjs  node 运行器
tools/diff-qemu.sh  与 qemu-riscv32 对拍
tests/              RV32 用例（C 写，clang 编成 rv32 ELF）
docs/               设计文档 + 任务板；随包装到 /usr/share/doc/risc6/docs/
```

## 依赖

| 依赖 | 版本/位置 | 用途 |
|---|---|---|
| c6 编译器 | cmake 包 `find_package(c6)` / pkgconf `c6.pc`（或 `C6_ROOT=~/c6` 源码树） | 编 `src/rv32.c6`（两条后端都靠它） |
| clang + lld | ≥ 19（`clang`、`ld.lld`、`wasm-ld`） | IR → x86-64 / wasm32 |
| node | ≥ 18 | 跑 wasm 产物（仅 `make wasm` 后需要） |
| qemu-riscv32 | 可选 | 对拍 oracle（缺失时 `make check` 跳过并提示） |

## 与旧 Makefile 的差异

构建系统已迁移到 CMake（`CMakeLists.txt` + `cmake/*.cmake` 模块），旧 `Makefile` 已删除。构建逻辑拆分为：

| 模块 | 职责 |
|---|---|
| `CMakeLists.txt` | 顶层装配：定义目录变量、include 各模块 |
| `cmake/risc6-toolchain.cmake` | 工具链探测 + c6 定位（find_package / pkgconf / C6_ROOT） |
| `cmake/risc6-build.cmake` | 核心翻译单元、native/wasm 模拟器、测试 ELF |
| `cmake/risc6-targets.cmake` | 高层 target、安装与 CTest |

目标对应关系：

| 旧 `make` 目标 | CMake 等价 |
|---|---|
| `make` | `cmake --build build`（产出 `build/risc6`） |
| `make wasm` | `cmake --build build --target risc6-wasm` |
| `make check` | `cmake --build build --target check` |
| `make selfcheck` | `cmake --build build --target selfcheck` |
| `make bare` / `make harts` | `cmake --build build --target bare` / `harts` |
| `make run ELF=x` | `cmake -DRISC6_RUN_ELF=x -B build && cmake --build build --target run`（或直接 `./build/risc6 x`） |
| `make tests` | `cmake --build build --target tests` |
| `make clean` | `cmake --build build --target clean-all`（清掉整个 `build/`） |

c6 经其 CMake 包（`find_package(c6)`）、pkgconf（`c6.pc`）或源码树（`-DC6_ROOT=...`，等价于旧 `C6_ROOT`）三种方式定位，优先用 cmake / pkgconf 提供的接口。

## 打包（deb / rpm / tar.gz）

构建系统复用同一套 `install()` 清单产出包，安装布局：

| 路径 | 内容 |
|---|---|
| `bin/risc6` | native 模拟器 |
| `share/risc6/risc6.wasm` | wasm32 模块 |
| `share/risc6/run-wasm.mjs` | 跑 wasm 的 node runner |
| `share/doc/risc6/` | README / CHANGELOG / docs/ |
| `share/doc/risc6/copyright` | Apache-2.0 许可（Debian 惯例） |
| `share/licenses/risc6/LICENSE` | Apache-2.0 许可（RPM 惯例） |

本地打包：先构建两个产物再 `cpack`

```sh
cmake -S . -B build-pkg -DCMAKE_INSTALL_PREFIX=/usr
cmake --build build-pkg --target risc6 risc6-wasm
cpack --config build-pkg/CPackConfig.cmake -G DEB    -B packages   # 同理 RPM / TGZ
```

GitHub Actions 在 push / PR / `v*` tag 上自动测试并产出三种包，tag 时发布到 Release。

## 状态

**E0–E7 已完成**（E7 多 hart）（E6-3 浏览器演示为可选项，见 `docs/todo.md`）。核心已是两个 TU。

实测（本机 clang 19.1.4 / node v24.13.1 / c6 @ `~/c6`）：

| 项 | 数字 |
|---|---|
| `check`（native + wasm + qemu 三方对拍） | 4 用例全一致 |
| `selfcheck`（预算/装载错误码等自检） | 8 项全通过 |
| `bare`（bare 模式陷阱/抢占） | 16 次 MTIP 注入，寄存器帧完好，比 native 与 wasm 逐字节相同 |
| `harts`（双 hart） | native/wasm 均 `PASS: two harts`，4078 条指令一致 |
| 双 hart 定时器隔离 | native/wasm 均 `PASS: per-hart timer irq`，4,001,339 条指令一致 |
| 双 hart 原子加 | native/wasm 均 `PASS: two-hart atomic add`，604,118 条指令一致 |
| c6LOS `rv32/{hello,sched,tick}` | 输出与 `qemu-riscv32` 逐字节一致（1776 / 35854 / 1594931 条指令） |
| 性能 native | ~44 Minsn/s（单 TU 时 47.5） |
| 性能 wasm（node） | ~35 Minsn/s |
| 用例指令数 | t_hello 352、t_mem 1459、t_atomic 3659、t_isa 5891、t_bare 802158 |

## 许可证

Apache-2.0（与 c6LOS 一致，见 `LICENSE`）。

- 本仓库代码全部自研：c6 核心 `src/rv32.c6`、宿主管线、工具脚本、测试用例。
- 工具链依赖不属于本仓库：native 后端链接 c6 的 `libtcc1.a`（LGPL），它只是**工具链
  运行时**，和 c6LOS 的 host 单测用法相同；wasm 后端不链接 `libtcc1`。
- 测试 ELF 来自本仓库 `tests/` 与 c6LOS（同为 Apache-2.0）。
