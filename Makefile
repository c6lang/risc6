# risc6 —— 用 c6 写的 RV32IMAC 模拟器（native + wasm32）
#
#   make           native  -> build/risc6
#   make wasm      wasm32  -> build/risc6.wasm
#   make check     自带用例 + 与 qemu-riscv32 对拍
#   make run ELF=path/to/x.elf
#   make clean

C6_ROOT ?= $(HOME)/c6
C6      := $(C6_ROOT)/tcc/c6
C6B     := $(C6_ROOT)/tcc
CC      := clang
WASMLD  := wasm-ld
BUILD   := build
QEMU    ?= qemu-riscv32
PYTHON  ?= python3

NATIVE_CFLAGS := -O2 -g -Iinclude
RVFLAGS := --target=riscv32-unknown-linux-gnu -march=rv32imac -mabi=ilp32 \
           -nostdlib -ffreestanding -O1 -Iinclude -Itests -static -fuse-ld=lld \
           -msmall-data-limit=0
WASMFLAGS := --target=wasm32-unknown-unknown -O2 -ffreestanding -Iinclude

TESTS   := t_hello t_isa t_mem t_atomic
TESTBIN := $(addprefix $(BUILD)/tests/,$(TESTS))

.PHONY: all wasm check selfcheck bare harts clean run run-wasm stats dirs

all: $(BUILD)/risc6
wasm: $(BUILD)/risc6.wasm

dirs:
	@mkdir -p $(BUILD)/native $(BUILD)/wasm $(BUILD)/tests $(BUILD)/out

# ---- native -------------------------------------------------------------
# 核心分两个翻译单元：rv32.c6（热路径）+ sys.c6（装载/系统调用/诊断）
CORE_HDR := include/risc6.h include/risc6_core.h
CORE_SRC := src/rv32.c6 src/sys.c6

$(BUILD)/native/rv32.ll: src/rv32.c6 $(CORE_HDR) | dirs
	$(C6) -B $(C6B) -Iinclude --emit-llvm -o $@ src/rv32.c6

$(BUILD)/native/sys.ll: src/sys.c6 $(CORE_HDR) | dirs
	$(C6) -B $(C6B) -Iinclude --emit-llvm -o $@ src/sys.c6

$(BUILD)/risc6: $(BUILD)/native/rv32.ll $(BUILD)/native/sys.ll \
		port/host_native.c include/risc6.h | dirs
	$(CC) $(NATIVE_CFLAGS) -o $@ $(BUILD)/native/rv32.ll \
		$(BUILD)/native/sys.ll port/host_native.c -L$(C6B) -ltcc1

# ---- wasm32 -------------------------------------------------------------
$(BUILD)/wasm/rv32.ll: src/rv32.c6 $(CORE_HDR) | dirs
	$(C6) -B $(C6B) -Iinclude --target=wasm32-unknown-unknown --emit-llvm \
		-o $@ src/rv32.c6

$(BUILD)/wasm/sys.ll: src/sys.c6 $(CORE_HDR) | dirs
	$(C6) -B $(C6B) -Iinclude --target=wasm32-unknown-unknown --emit-llvm \
		-o $@ src/sys.c6

$(BUILD)/wasm/rv32.o: $(BUILD)/wasm/rv32.ll
	$(CC) $(WASMFLAGS) -c -o $@ $<

$(BUILD)/wasm/sys.o: $(BUILD)/wasm/sys.ll
	$(CC) $(WASMFLAGS) -c -o $@ $<

$(BUILD)/wasm/host.o: port/host_wasm.c include/risc6.h | dirs
	$(CC) $(WASMFLAGS) -c -o $@ port/host_wasm.c

$(BUILD)/risc6.wasm: $(BUILD)/wasm/rv32.o $(BUILD)/wasm/sys.o $(BUILD)/wasm/host.o
	$(WASMLD) --no-entry \
		--export=wx_run --export=wx_in_addr --export=wx_out_addr \
		--export=wx_out_len --export=wx_diag_addr --export=wx_diag_len \
		--export=wx_insns_lo --export=wx_insns_hi --export=wx_set_limit \
		--export=wx_stat_addr --export=wx_set_mode --export=wx_set_harts --export=wx_version \
		--export-memory --initial-memory=16777216 \
		-o $@ $(BUILD)/wasm/rv32.o $(BUILD)/wasm/sys.o $(BUILD)/wasm/host.o

# ---- 用例（rv32 user-mode ELF，clang 编） --------------------------------
$(BUILD)/tests/%: tests/%.c tests/start.S tests/support.c tests/testlib.h | dirs
	$(CC) $(RVFLAGS) -o $@ tests/start.S tests/support.c $<

tests: $(TESTBIN)

# ---- 跑与对拍 -----------------------------------------------------------
run: $(BUILD)/risc6
	@test -n "$(ELF)" || { echo "用法: make run ELF=path/to/rv32.elf"; exit 2; }
	$(BUILD)/risc6 $(ELF)

run-wasm: wasm
	@test -n "$(ELF)" || { echo "用法: make run-wasm ELF=path/to/rv32.elf"; exit 2; }
	node tools/run-wasm.mjs $(BUILD)/risc6.wasm $(ELF)

check: $(BUILD)/risc6 $(TESTBIN)
	@sh tools/check.sh $(TESTBIN)

# ---- bare 模式用例（T5）：只在 risc6 上跑，qemu user-mode 不支持 CSR ----
$(BUILD)/tests/t_bare: tests/t_bare.c tests/bare_start.S tests/bare_trap.S | dirs
	$(CC) $(RVFLAGS) -o $@ tests/bare_start.S tests/bare_trap.S tests/t_bare.c

$(BUILD)/tests/t_harts: tests/t_harts.c tests/bare_start.S | dirs
	$(CC) $(RVFLAGS) -o $@ tests/bare_start.S tests/t_harts.c

$(BUILD)/tests/t_harts_irq: tests/t_harts_irq.c tests/harts_trap.S tests/bare_start.S | dirs
	$(CC) $(RVFLAGS) -o $@ tests/bare_start.S tests/harts_trap.S tests/t_harts_irq.c

bare: $(BUILD)/risc6 wasm $(BUILD)/tests/t_bare
	@echo "== bare native（靠 ELF 里的 .risc6_bare 标记切模式）"
	@$(BUILD)/risc6 $(BUILD)/tests/t_bare || true
	@echo "== bare wasm"
	@node tools/run-wasm.mjs $(BUILD)/risc6.wasm $(BUILD)/tests/t_bare || true

harts: $(BUILD)/risc6 wasm $(BUILD)/tests/t_harts $(BUILD)/tests/t_harts_irq
	@echo "== two harts native"
	@$(BUILD)/risc6 --bare --harts 2 $(BUILD)/tests/t_harts
	@echo "== two harts wasm"
	@node tools/run-wasm.mjs $(BUILD)/risc6.wasm $(BUILD)/tests/t_harts --bare --harts 2
	@echo "== per-hart timer irq native"
	@$(BUILD)/risc6 --bare --harts 2 $(BUILD)/tests/t_harts_irq
	@echo "== per-hart timer irq wasm"
	@node tools/run-wasm.mjs $(BUILD)/risc6.wasm $(BUILD)/tests/t_harts_irq --bare --harts 2

selfcheck: $(BUILD)/risc6 wasm $(TESTBIN)
	@sh tools/selfcheck.sh $(BUILD)/tests/t_hello

stats: $(BUILD)/risc6 $(TESTBIN)
	@for t in $(TESTBIN); do echo "== $$t"; $(BUILD)/risc6 $$t >/dev/null; done

clean:
	rm -rf $(BUILD)
