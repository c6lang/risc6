# risc6 设计文档

> 状态：设计定稿（施工前评审版）· 索引：[README](../README.md) · 任务板：[todo.md](todo.md)
>
> 一句话：**用 c6 写的 RV32IMAC 机器模拟器**，同一份源码出两个产物——native x86-64
> 与 wasm32 模块——给 c6LOS 当不依赖 qemu 的参考机（L1.5），并能在 wasm 宿主里
> 提供「指令边界注入中断」的真抢占。

---

## 1. 为什么需要它

c6LOS 现在的参考机矩阵（`rtos-c6/docs/testing.md` §1）：

| 层 | 现状 | 缺口 |
|---|---|---|
| L1 host | `port/sim`（ucontext 协作切换） | 无真抢占、无 ISA 保真、不能分发 |
| L2 rv32 | qemu-riscv32 user-mode | **依赖 qemu**；`csrr time` 是宿主时钟（不确定）；user-mode 无中断，**证明不了抢占** |
| L3 x86-64 裸机 | qemu-system-x86_64 | 只证明 x86 移植层，不证明 ILP32/32 位地址语义 |
| L4 S3 真板 | 手工上板阶梯 | 慢、不可在 CI 跑 |

risc6 补的正是 L2 的两个缺口：

1. **不依赖 qemu**：模拟器是自己的一段 c6 程序，`make` 出来就能跑，CI、笔记本、
   浏览器都一样；`qemu-riscv32` 只作为**对拍 oracle**（可选）。
2. **能真抢占**：模拟器是「机器」而不是「宿主」——它在**指令边界**注入
   `mtime`/`mtip` 中断，被模拟的内核真的跑在陷阱帧上。wasm 宿主没有栈切换能力
   （见 §6.3），但**模拟出来的机器**不需要宿主具备这个能力。这是「wasm 不能做抢占」
   这条硬限制的正解。
3. **确定性**：`time`/`cycle`/`instret` 由**指令计数**推导，不读 wall clock；
   同一 ELF + 同一注入切片 → 逐字节可复现，`make check` 可以当门禁用。

顺带两个副产品：

- c6 语言第一次被一个 ~1500 行的**真实程序**检验（此前只有 c6LOS 内核 + 用例），
  switch 密集的译码器正好压 c6 的 C99 后端与 LLVM IR 后端。
- 一个能在浏览器里跑的 RISC-V 机器，可以给 c6LOS 做屏/仪表盘 demo 的载体（三期）。

### 非目标

- 不追求跑通任意 Linux 用户态程序（那是 qemu-user 的活）：只支持 **静态 ELF、
  少量 syscall**（见 §5.3）。
- 不做 F/D 浮点（rv32 参考机用 `-mabi=ilp32` 软浮点）、不做向量、B 扩展、
  S 态/U 态、MMU、Sv32、调试模块、DMA/外设。
- 不替代 L3（x86-64 真抢占）与 L4（真板）：它证明的是 32 位地址/ILP32 语义 +
  陷阱/调度交互，不是 Xtensa call0 ABI、FPU 陷阱帧、esp_image 启动链。
- 不做 JIT（先把解释器做对、做确定；性能不达标再谈打包指令缓存，见 §11）。

---

## 2. 实测过的前置事实（决定设计的那几条）

全部在本机验证过（`~/c6`、clang 19.1.4、node v24.13.1、qemu-riscv32）：

| 事实 | 证据 | 对设计的约束 |
|---|---|---|
| c6 编译器在 `~/c6/tcc/c6`（tcc 0.9.28rc） | `-v` | `C6_ROOT=~/c6`，`-B ~/c6/tcc` 必需 |
| LLVM IR 后端可用 | `c6 --emit-llvm -o x.ll` | native 路径：IR → `clang -fuse-ld=lld -ltcc1` |
| wasm32 目标**内建**且有运行库 | `--print-targets` 显示 `wasm32-unknown-unknown  libtcc1-wasm32.a  ok` | 不需要手写 triple/datalayout 替换 |
| **定义**带真实类型 | `define i32 @os_task_create(ptr %a0, ptr %a1, …)` | c6→c6 跨 TU 调用本身可以是对的 |
| **外部 declare 不带类型**（C0-1 未合入） | `declare i64 @port_puts(...)` | **禁止 c6 调 C**；见 §6.1 |
| rv32 c6LOS 目标 = Linux user-mode ELF | `port/rv32/start.S`（`ecall 93`）、`port/rv32/console.c`（`ecall 64`、`csrr time`） | 模拟器 v1 必须实现 user-mode ABI 才能当 drop-in |
| clang 19 有 wasm32 + `wasm-ld` | `clang --print-targets`、`/usr/lib/llvm-19/bin/wasm-ld` | wasm 产物可控导出 |
| node v24 支持 wasm | `node -e 'WebAssembly.instantiate'` | 运行器用 node（不依赖 wasmtime） |
| qemu-riscv32 在 | `/usr/bin/qemu-riscv32` | 对拍 oracle |

---

## 3. 机器模型

### 3.1 ISA 子集

| 扩展 | 内容 | 说明 |
|---|---|---|
| RV32I | 全（含 `fence`/`fence.i` = nop、`ecall`/`ebreak`） | `-march=rv32imac` 的基础 |
| M | `mul/mulh/mulhsu/mulhu/div/divu/rem/remu` | 除零/溢出按规范（`div` 溢出 → 被除数，除零 → -1） |
| A | `lr.w/sc.w` + 10 条 `amo*.w` | 单核语义：`lr` 记录保留地址，`sc` 校验 |
| C | 全部 16 位指令（RVC） | **展开成等价 32 位指令**后走同一执行器（见 §7.2） |
| Zicsr | `csrrw/csrrs/csrrc` + 立即数形式 | 只实现 §3.4 列出的 CSR，其余读 0/写忽略（可配严格模式） |
| Zifencei | `fence.i` = nop | 无 I-cache 模型 |
| Zicntr | `cycle/instret/time` = `insns × SCALE` | 确定性时钟，见 §4 |

不支持：F/D（`-mabi=ilp32`，无 FPU 寄存器）、Q、V、B、Zba/Zbb、Ztso、调试。

### 3.2 寄存器与复位状态

- `x0..x31`，`x0` 恒 0（写 rd==0 直接丢弃）。
- 复位：`pc = e_entry`；`sp(x2) = 栈顶 - 16`；`gp(x3) = 符号 __global_pointer$`（若 ELF 有）；
  `a0/a1 = 0`（argc/argv）；其余 0。

### 3.3 内存

- **单块平坦窗**：从 ELF 所有 `PT_LOAD` 的 `[min(vaddr), max(vaddr+memsz))` 取
  `base`（4 KiB 下对齐），窗尾追加 `RISC6_STACK_GAP`(64 KiB) + `RISC6_STACK_SIZE`(1 MiB)
  作为栈，整块落在一个静态数组 `risc6_mem[RISC6_MEM_SIZE]`（8 MiB）里。
- guest 地址 → 宿主索引：`off = addr - base`；越界即陷阱。
- 允许非对齐访问（逐字节装/取）——比严格 ISA 宽松，换取用例不被打包结构误伤；
  这是**有意偏离**，记在 §5.4。
- 无 MMU、无 cache、无 PSRAM/flash 区分；`fence*` 为 nop。

### 3.4 CSR

| CSR | 号 | 行为 |
|---|---|---|
| `mstatus` | 0x300 | 只实现 `MIE(3)`/`MPIE(7)`/`MPP(12:11)`（bare 模式）；其余位读 0 |
| `misa` | 0x301 | 常量 `0x4000_0114`（RV32 + I/M/A/C） |
| `mie` | 0x304 | 只实现 `MTIE(7)` |
| `mtvec` | 0x305 | direct（`mode=0`）与 vectored（`mode=1`：`base + 4*cause`） |
| `mip` | 0x344 | 只实现 `MTIP(7)`（由时间事件置位） |
| `mscratch` | 0x340 | 普通读写 |
| `mepc` | 0x341 | 陷阱返回地址 |
| `mcause` | 0x342 | `0x8000_0007` = 机器定时器中断；`11` = ecall from M；`2` = illegal；`5/7` = load/store access fault |
| `mtval` | 0x343 | 非法指令字 / 出错地址 |
| `mhartid` | 0xF14 | 0 |
| `cycle`/`time`/`instret` | 0xC00/0xC01/0xC02 | `insns × RISC6_TIME_SCALE`（u32 截断） |
| `mcycle`/`minstret` | 0xB00/0xB02 | 同 `insns`（低 32 位） |

其余 CSR：读 0、写忽略；`RISC6_STRICT_CSR=1` 时触发非法指令陷阱（默认关，便于先跑通）。

### 3.5 两种运行模式

| 模式 | 用途 | 系统调用 | 陷阱 |
|---|---|---|---|
| **user**（v1） | drop-in 替代 `qemu-riscv32`：跑 c6LOS 的 `build/rv32/*` | `ecall` = Linux 进程 ABI（§5.3） | 非法访存 → 诊断 + 退出码 203 |
| **bare**（v2） | 真抢占 / 中断 / CSR 语义回归 | `ecall` = 机器态陷阱（cause 11） | 完整 `mtvec`/`mret`/`mip.MTIP` 注入 |

选择方式：默认 user；`--bare` 或 ELF 带 `.risc6_bare` 标记节 → bare（见 todo E5）。

---

## 4. 时间、预算与确定性

- **指令预算**：`risc6_set_limit(u64)`，默认 `2e9`。耗尽 → 退出码 201 + 诊断行
  （不是挂死）。
- **时间**：`time/cycle/instret = insns × RISC6_TIME_SCALE`（默认 64）。选 64 的理由：
  c6LOS 的 `port/rv32/console.c:port_idle()` 以「`time` 差 ≥ 1e6」推进一次 tick，
  64 换算 → 每 ~15625 条指令一次 tick，100 个 tick ≈ 1.6M 指令，秒级可测；
  `u32` 截断后差值仍正确（只比较差值）。
- **确定性**：无 wall clock、无随机、无宿主输入（v1 无 stdin）、单线程解释。
  同一 ELF 两次运行输出逐字节相同；不同后端（native / wasm32）也相同（T4）。
- **中断注入切片**（bare 模式）：`RISC6_SLICE`（默认 50000）条指令一次到期检查；
  到期且 `mstatus.MIE && mie.MTIE` → 置 `mip.MTIP` 并在**下一条指令边界**取陷阱，
  然后 `next_event += SLICE`。检查是「每条指令一次比较 + 偶尔分支」，O(1)。

---

## 5. 装载与宿主契约

### 5.1 ELF 要求

ELF32 / little-endian / `ET_EXEC` / `e_machine = 243 (EM_RISCV)` / 无 `PT_INTERP`。
任意 `vaddr`（c6LOS 现在是 `0x10000` 起）。`memsz > filesz` 的尾部清零（.bss）。
`gp` 从 `.symtab` 里找 `__global_pointer$`（链接器松弛会把 `lui+addi` 变成 `gp`
相对访问，没有 crt0 设 gp 时会读飞）。

装载失败 → 退出码 200 + 诊断行，不进入执行。

### 5.2 跨边界契约（关键设计）

**c6 侧不调用任何 C 函数**（C0-1 未合入时外部 declare 一律 `i64(...)`，在 wasm32 上
签名对不上）。两侧唯一的通道是**共享数据符号**：

| 符号 | 方向 | 定义者 | 用途 |
|---|---|---|---|
| `risc6_mem[8 MiB]` | C 只读 | c6 | guest 内存窗（宿主可据此做屏/调试视图） |
| `risc6_in[1 MiB]` | C 写、c6 读 | c6（C 侧 `extern`） | ELF 装载输入 |
| `risc6_out[64 KiB]` / `risc6_out_len` | c6 写、C 读 | c6 | guest 的 stdout/stderr |
| `risc6_diag[4 KiB]` / `risc6_diag_len` | c6 写、C 读 | c6 | 模拟器自身诊断（非法指令、越界、装载错） |
| `risc6_stat_insns` | c6 写、C 读 | c6 | 指令计数（CI 打印/回归） |
| `int risc6_run(u32 in_len)` | C 调 c6 | c6 | 装载 + 运行，返回退出码 |
| `void risc6_set_limit(u64)` / `risc6_set_trace(u32)` | C 调 c6 | c6 | 配置 |

类型映射硬规则（两侧必须逐字一致）：`int`↔`i32`、`u32`↔`i32`、`u64`↔`i64`、
`void`↔`void`。**指针永不过边界**：宿主拿地址一律「在自己的 TU 里取符号地址」。

产物侧：

- native：`port/host_native.c` 提供 `main()`（读文件 → `risc6_in` → `risc6_run` →
  刷 `risc6_out`/`risc6_diag`）。
- wasm32：`port/host_wasm.c` 提供 `wx_in_addr()`/`wx_run()`/`wx_out_addr()`/
  `wx_out_len()`/`wx_diag_addr()`/`wx_diag_len()`/`wx_insns()` 导出（全在 C 侧取地址，
  规避「指针不过边界」）；`tools/run-wasm.mjs` 负责喂 ELF、取输出、传退出码。

### 5.3 syscall（user 模式）

| a7 | 名 | 实现 |
|---|---|---|
| 64 | `write(fd, buf, len)` | fd 1/2 → 追加到 `risc6_out`；返回写入字节数；越界 → 203 |
| 93 / 94 | `exit` / `exit_group` | 停止，退出码 = `a0 & 0xFF` |
| 63 | `read` | 返回 0（EOF；v1 无 stdin） |
| 57 | `close` | 0 |
| 214 | `brk` | 记录 break 指针；`brk(0)` 返回当前值；窗内则接受，否则返回原值 |
| 其他 | — | `a0 = -38 (ENOSYS)`，并在 `risc6_diag` 记一行（每号一次） |

### 5.4 已知偏离（对拍时若出现差异先查这里）

1. **非对齐访存不报警**（逐字节完成）。
2. **`time` 来自指令计数**，不是宿主时钟；依赖真实时间的用例（c6LOS `tick`）只比
   `PASS` 行。
3. `brk` 不真增长（窗是固定的）。
4. `lr/sc` 无跨核语义（单核解释器）。
5. 无 `fence` 语义（顺序解释，本来就一致）。

### 5.5 退出码约定（模拟器自身）

| 码 | 含义 |
|---|---|
| 0..255 | guest 的退出码（透传） |
| 200 | ELF 装载失败 |
| 201 | 指令预算耗尽 |
| 202 | 非法指令 / 不支持的 CSR（严格模式） |
| 203 | 访存越界 |
| 204 | 内部错误（不可能路径） |

---

## 6. 用 c6 写：语言子集与硬规则

### 6.1 硬规则（CI/lint 可查）

1. ~~**单 TU**~~ → **两 TU**（c6 已修好外部 declare 的类型，见 §6.1 末尾）。热路径留在
   `src/rv32.c6`，冷路径（诊断/系统调用/ELF 装载/参数入口）在 `src/sys.c6`，共享状态走
   `include/risc6_core.h` 的 extern 声明。原设计理由是 C0-1（外部 declare 不带类型）
   与 C0-2（逐 TU emit）未合入，多 TU 在 wasm32 上会签名不匹配。等 C0 合入再拆（E6）。
2. **c6 不调 C**：见 §5.2。C 只出现在 `port/` 的两个 shim 里，且只「读符号 + 调入口」。
3. **禁用**：`@T`（GC）、`async/co`、闭包/函数字面量、`f".."`、切片 `T[:]`、
   `interface`、libc 头。理由：这些都要 `libtcc1` 的运行时（wasm 侧是另一套 wrapper），
   而本程序的 IR 只要「整数 + 数组 + 函数」；同时与 c6LOS 内核核心的禁令一致。
   注意后端已知缺口：**闭包间接调用会崩**、**9–16 字节结构体返回三后端 ABI 不一致**，
   这两类构造一律不写。
4. **不用 `long`/`size_t`/指针做算术**：guest 地址一律 `u32` 偏移。c6 前端曾经是
   LP64（`--target=wasm32` 下 `sizeof(long)` 也是 8）——正是 c6LOS
   `docs/architecture.md §3` 的坑；c6 现在按 triple 取宽度（wasm32 上 4），但
   这条规矩保留：两个后端共用同一份源码、且 `u32` 偏移本来就更贴近 guest 地址模型。
5. **错误用返回码**，不用 `err`/`try`（避免 `__c6_err_*` 运行时符号）。
6. **全局状态 + 静态数组**，不 `malloc`（wasm 侧只有 4 MiB 静态池，且我们要确定性）。

### 6.2 目录与模块

```
~/risc6/
  include/risc6.h     两侧共用：类型、尺寸常量、数据符号与入口声明
  src/rv32.c6         热路径：取指 / 译码 / 访存 / 陷阱 / 主循环
  src/sys.c6          冷路径：诊断 / 控制台 / syscall / ELF 装载 / 参数入口
  port/host_native.c  native 宿主：main()、文件读、stdout/stderr
  port/host_wasm.c    wasm 宿主：导出（取地址与转发）
  tools/run-wasm.mjs  node 运行器
  tools/diff-qemu.sh  与 qemu-riscv32 对拍
  tests/              RV32 user-mode 用例（C 写，clang 编）
  docs/               本文档 + todo
```

### 6.3 为什么这条路绕过了「wasm 不能抢占」

wasm 模块内无法切换调用栈（`__stack_pointer` 只覆盖线性内存里的影子栈，返回地址在
宿主的 VM 栈里），也没有中断——所以**不能**像 `port/sim` 那样 `swapcontext`，也不能
在 wasm 里被 tick 打断。risc6 的做法是把「被打断的对象」下沉一层：被抢占的是
**被模拟的 RISC-V 机器**，它的 `pc`/寄存器全在我们自己的变量里，注入中断 = 改几个
整数。宿主（node/浏览器）只负责调用 `risc6_run`，全程不需要栈切换能力。

---

## 7. 核心结构

### 7.1 状态

```
g_x[32]  g_pc            CPU
g_mem_base g_mem_size    guest 窗（risc6_mem 里的一段）
g_insns g_max_insns      计量与预算
g_mode g_*CSR            模式与陷阱状态
g_exit g_rc              停止标志与退出码
g_next_event g_slice     bare 模式的时间事件
```

### 7.2 译码

- `step()`：取 16 位；`(h & 3) != 3` → **RVC 展开**成 32 位规范指令（`rvc_expand`），
  否则拼成 32 位。
- 执行器按 `opcode` 分派（`switch`，7 大类 + SYSTEM + AMO），RVC 与 32 位共用同一份
  语义代码——压缩指令 bug 最少的写法（展开表可单独单测，见 T0）。
- 未知 opcode/非法 RVC 编码 → 诊断 `pc / insn` + 退出码 202（user）或 mcause=2（bare）。

### 7.3 访存与陷阱

`load8/16/32`、`store8/16/32` 统一走 `mem_off(addr, len)`：越界 → 置陷阱。
user 模式：写诊断 + 停止（203）。bare 模式：`mcause = 5/7`、`mtval = addr`、跳
`mtvec`。

### 7.4 运行循环

```
int risc6_run(u32 in_len):
    装载 ELF（失败 → 200）
    while (!g_exit && g_insns < g_max_insns):
        if (bare && g_insns >= g_next_event) tick_event();
        step();
        g_insns++;
    return g_rc
```

---

## 8. 性能

| 目标 | 指标 | 手段 |
|---|---|---|
| native | ≥ 20M instr/s | `-O2`、switch 分派、`u32` 运算、避免每指令函数调用 |
| wasm32 | ≥ 8M instr/s | 同上 |
| 典型用例 | c6LOS `build/rv32/{hello,sched,tick}` 每个 < 2e8 指令 | 预算内秒级完成 |

不做的事：打包指令缓存（把译码结果存成 u32 数组）留作性能不达标的退路（§11）。
`risc6_stat_insns` 落指令数，CI 里跟踪回归。

实测（clang 19.1.4，`-O2`，node v24.13.1，同一批 ELF）：

| 后端 | 用例 | 指令数 | 速率 |
|---|---|---|---|
| native | c6LOS `tick/sched` | 1 594 931 | ~47 Minsn/s |
| wasm（node） | c6LOS `tick/sched` | 1 594 931 | ~35 Minsn/s |

两个后端指令数与计数器逐项相同，说明分派逻辑没有后端相关的行为差异。

---

## 9. 构建

```sh
make                 # native：build/risc6
make wasm            # wasm32：build/risc6.wasm
make check           # T1 + T2（对拍 qemu-riscv32，若装了）
make run ELF=...     # 跑任意 rv32 ELF
node tools/run-wasm.mjs build/risc6.wasm build/tests/t_hello
```

流水线（两条，同一份 `.c6`）：

```
src/rv32.c6 ─┐
src/sys.c6  ─┴─c6 --emit-llvm──> build/native/{rv32,sys}.ll
            └─clang -O2 -fuse-ld=lld + port/host_native.c + libtcc1.a ──> build/risc6

src/{rv32,sys}.c6 ──c6 --target=wasm32-unknown-unknown --emit-llvm──> build/wasm/{rv32,sys}.ll
            └─clang --target=wasm32 -c + port/host_wasm.c
              └─wasm-ld --no-entry --export=... --initial-memory=16MiB → build/risc6.wasm
```

---

## 10. 测试与验收

| 层 | 内容 | 通过标准 |
|---|---|---|
| T0 | c6 `test "…"` 块：RVC 展开表、`mulh` 边界、`div` 除零/溢出、sext、CSR 编解码 | `make check-unit` 全绿 |
| T1 | RV32 ELF 用例（`tests/t_*.c`，clang 编）：hello、isa（算术/移位/M/64 位/压缩）、mem（memcpy/memmove/结构体/函数指针表）、atomic（`lr/sc`+`amo*`） | 用例自判并返回失败数 |
| T2 | **对拍 qemu-riscv32**：stdout 与退出码一致 | `tools/check.sh` 0 差异 |
| T3 | c6LOS 真用例：`build/rv32/{hello,sched,tick}` 在 risc6 下 PASS | 三行 `PASS`（`tick` 只比 PASS 行） |
| T4 | wasm 后端：同一批 ELF 在 node 里跑，输出与 native **字节级一致** | diff 为空 |
| T5 | 覆盖率闸门：每个 opcode / RVC 象限 / CSR 命中计数 > 0 | `--stats` 无零命中 |

验收命令（合并前）：

```sh
make clean && make && make wasm && make check && make selfcheck && make bare
```

结果（E6 收尾时实测）：`check` 4 用例 native/wasm/qemu 全一致；`selfcheck` 8 项全过；
`bare` 在 native 与 wasm 下都是 `PASS: bare traps`（16 次 MTIP 注入、`mcause=0x80000007`、
`ecall` → `mcause=11`、陷阱帧完好）。

---

## 11. 风险与回退

| 风险 | 触发条件 | 回退 |
|---|---|---|
| c6 顶不住 switch 密集的热循环 | 编译报错 / 生成代码错 | 热路径已是独立 TU；再不行把 `switch` 拆成两级 `if` 或按 opcode 分组 |
| 8 MiB 静态数组在 wasm 侧超预算 | `wasm-ld --initial-memory` 失败 | 降到 4 MiB，或分段窗口 |
| `gp` 相对寻址 | 用例里出现 `lw a0,-2048(gp)` 且 gp=0 | 已按符号表设 `gp`；保险加 `-Wl,--no-relax` |
| 与 qemu 行为差（`time` 源、非对齐、`brk`） | 对拍差异 | 记进 §5.4「已知偏离」或用例白名单 |
| 性能不达标 | native < 10M instr/s | 打包指令缓存（`(pc,insn)` → 译码结果） |
| 真实内核依赖更多 CSR | c6LOS `sched`/`tick` 跑飞 | 未知 CSR 读 0 + `--stats` 打印未命中 CSR |

---

## 12. 与 c6LOS 的集成（E6，不改 c6LOS 现状）

c6LOS 侧只需两行：

```make
RISC6 ?= $(HOME)/risc6/build/risc6
run-rv32-emu: rv32
	$(RISC6) $(RV32)/hello
```

并把 `docs/testing.md` 的 L2 行改成「qemu-riscv32 **或** risc6（提供真抢占注入）」。
反向不依赖：risc6 自带 C 用例，`make check` 不需要 c6LOS 在场。
