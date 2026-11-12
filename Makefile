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

NATIVE_CFLAGS := -O2 -g -Iinclude
RVFLAGS := --target=riscv32-unknown-linux-gnu -march=rv32imac -mabi=ilp32 \
           -nostdlib -ffreestanding -O1 -Iinclude -Itests -static -fuse-ld=lld
WASMFLAGS := --target=wasm32-unknown-unknown -O2 -ffreestanding -Iinclude

TESTS   := t_hello t_isa t_mem t_atomic
TESTBIN := $(addprefix $(BUILD)/tests/,$(TESTS))

.PHONY: all wasm check clean run run-wasm stats dirs

all: $(BUILD)/risc6
wasm: $(BUILD)/risc6.wasm

dirs:
	@mkdir -p $(BUILD)/native $(BUILD)/wasm $(BUILD)/tests $(BUILD)/out

# ---- native -------------------------------------------------------------
$(BUILD)/native/rv32.ll: src/rv32.c6 include/risc6.h | dirs
	$(C6) -B $(C6B) -Iinclude --emit-llvm -o $@ src/rv32.c6

$(BUILD)/risc6: $(BUILD)/native/rv32.ll port/host_native.c include/risc6.h | dirs
	$(CC) $(NATIVE_CFLAGS) -o $@ $(BUILD)/native/rv32.ll port/host_native.c \
		-L$(C6B) -ltcc1

# ---- wasm32 -------------------------------------------------------------
$(BUILD)/wasm/rv32.ll: src/rv32.c6 include/risc6.h | dirs
	$(C6) -B $(C6B) -Iinclude --target=wasm32-unknown-unknown --emit-llvm \
		-o $@ src/rv32.c6

$(BUILD)/wasm/rv32.o: $(BUILD)/wasm/rv32.ll
	$(CC) $(WASMFLAGS) -c -o $@ $<

$(BUILD)/wasm/host.o: port/host_wasm.c include/risc6.h | dirs
	$(CC) $(WASMFLAGS) -c -o $@ port/host_wasm.c

$(BUILD)/risc6.wasm: $(BUILD)/wasm/rv32.o $(BUILD)/wasm/host.o
	$(WASMLD) --no-entry \
		--export=wx_run --export=wx_in_addr --export=wx_out_addr \
		--export=wx_out_len --export=wx_diag_addr --export=wx_diag_len \
		--export=wx_insns_lo --export=wx_insns_hi --export=wx_set_limit \
		--export-memory --initial-memory=16777216 \
		-o $@ $(BUILD)/wasm/rv32.o $(BUILD)/wasm/host.o

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

stats: $(BUILD)/risc6 $(TESTBIN)
	@for t in $(TESTBIN); do echo "== $$t"; $(BUILD)/risc6 $$t >/dev/null; done

clean:
	rm -rf $(BUILD)
