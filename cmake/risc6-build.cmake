# 核心翻译单元：c6 把 .c6 翻成 LLVM IR；native 与 wasm 后端只差 --target。
set(_core_hdr ${SRC}/include/risc6.h ${SRC}/include/risc6_core.h)

foreach(_mod rv32 sys)
  add_custom_command(
    OUTPUT ${NATIVE_DIR}/${_mod}.ll
    COMMAND ${C6_COMPILER} -B ${C6_TCCDIR} -I${SRC}/include
            --emit-llvm -o ${NATIVE_DIR}/${_mod}.ll ${SRC}/src/${_mod}.c6
    DEPENDS ${SRC}/src/${_mod}.c6 ${_core_hdr}
    VERBATIM)
  add_custom_command(
    OUTPUT ${WASM_DIR}/${_mod}.ll
    COMMAND ${C6_COMPILER} -B ${C6_TCCDIR} -I${SRC}/include
            --target=wasm32-unknown-unknown --emit-llvm
            -o ${WASM_DIR}/${_mod}.ll ${SRC}/src/${_mod}.c6
    DEPENDS ${SRC}/src/${_mod}.c6 ${_core_hdr}
    VERBATIM)
  add_custom_command(
    OUTPUT ${WASM_DIR}/${_mod}.o
    COMMAND ${CLANG} --target=wasm32-unknown-unknown -O2 -ffreestanding
            -I${SRC}/include -c -o ${WASM_DIR}/${_mod}.o ${WASM_DIR}/${_mod}.ll
    DEPENDS ${WASM_DIR}/${_mod}.ll
    VERBATIM)
endforeach()

# ---- native 模拟器（直接 COMMAND，避免与产物同名循环）-------------------
add_custom_target(risc6 ALL
  COMMAND ${CLANG} -O2 -g -I${SRC}/include
          -o ${BUILD_DIR}/risc6
          ${NATIVE_DIR}/rv32.ll ${NATIVE_DIR}/sys.ll
          ${SRC}/port/host_native.c
          -L${C6_TCCDIR} -ltcc1
  DEPENDS ${NATIVE_DIR}/rv32.ll ${NATIVE_DIR}/sys.ll
          ${SRC}/port/host_native.c ${C6_TCCDIR}/libtcc1.a
  VERBATIM)

# ---- wasm32 模拟器 --------------------------------------------------------
add_custom_command(OUTPUT ${WASM_DIR}/host.o
  COMMAND ${CLANG} --target=wasm32-unknown-unknown -O2 -ffreestanding
          -I${SRC}/include -c -o ${WASM_DIR}/host.o ${SRC}/port/host_wasm.c
  DEPENDS ${SRC}/port/host_wasm.c
  VERBATIM)
add_custom_target(risc6-wasm
  COMMAND ${WASMLD} --no-entry
          --export=wx_run --export=wx_in_addr --export=wx_out_addr
          --export=wx_out_len --export=wx_diag_addr --export=wx_diag_len
          --export=wx_insns_lo --export=wx_insns_hi --export=wx_set_limit
          --export=wx_stat_addr --export=wx_set_mode --export=wx_set_harts --export=wx_version
          --export-memory --initial-memory=16777216
          -o ${BUILD_DIR}/risc6.wasm
          ${WASM_DIR}/rv32.o ${WASM_DIR}/sys.o ${WASM_DIR}/host.o
  DEPENDS ${WASM_DIR}/rv32.o ${WASM_DIR}/sys.o ${WASM_DIR}/host.o
  VERBATIM)

# ---- 测试 ELF（clang 编成 rv32 user-mode 静态 ELF）----------------------
set(RVFLAGS --target=riscv32-unknown-linux-gnu -march=rv32imac -mabi=ilp32
  -nostdlib -ffreestanding -O1 -I${SRC}/include -I${SRC}/tests
  -static -fuse-ld=lld -msmall-data-limit=0)

set(_tests t_hello t_isa t_mem t_atomic)
set(_test_bins "")
foreach(_t ${_tests})
  add_custom_command(
    OUTPUT ${TESTS_DIR}/${_t}
    COMMAND ${CLANG} ${RVFLAGS} -o ${TESTS_DIR}/${_t}
            ${SRC}/tests/start.S ${SRC}/tests/support.c ${SRC}/tests/${_t}.c
    DEPENDS ${SRC}/tests/start.S ${SRC}/tests/support.c ${SRC}/tests/${_t}.c
            ${SRC}/tests/testlib.h
    VERBATIM)
  list(APPEND _test_bins ${TESTS_DIR}/${_t})
endforeach()
add_custom_target(risc6-tests DEPENDS ${_test_bins})

# bare / harts 模式用例（CSR、中断注入、多 hart）：qemu user-mode 不支持，
# 只在 risc6 上跑（见 docs/design.md）。
add_custom_command(OUTPUT ${TESTS_DIR}/t_bare
  COMMAND ${CLANG} ${RVFLAGS} -o ${TESTS_DIR}/t_bare
          ${SRC}/tests/bare_start.S ${SRC}/tests/bare_trap.S ${SRC}/tests/t_bare.c
  DEPENDS ${SRC}/tests/bare_start.S ${SRC}/tests/bare_trap.S ${SRC}/tests/t_bare.c VERBATIM)
add_custom_command(OUTPUT ${TESTS_DIR}/t_harts
  COMMAND ${CLANG} ${RVFLAGS} -o ${TESTS_DIR}/t_harts
          ${SRC}/tests/bare_start.S ${SRC}/tests/t_harts.c
  DEPENDS ${SRC}/tests/bare_start.S ${SRC}/tests/t_harts.c VERBATIM)
add_custom_command(OUTPUT ${TESTS_DIR}/t_harts_irq
  COMMAND ${CLANG} ${RVFLAGS} -o ${TESTS_DIR}/t_harts_irq
          ${SRC}/tests/bare_start.S ${SRC}/tests/harts_trap.S ${SRC}/tests/t_harts_irq.c
  DEPENDS ${SRC}/tests/bare_start.S ${SRC}/tests/harts_trap.S ${SRC}/tests/t_harts_irq.c VERBATIM)
add_custom_command(OUTPUT ${TESTS_DIR}/t_harts_atomic
  COMMAND ${CLANG} ${RVFLAGS} -o ${TESTS_DIR}/t_harts_atomic
          ${SRC}/tests/bare_start.S ${SRC}/tests/t_harts_atomic.c
  DEPENDS ${SRC}/tests/bare_start.S ${SRC}/tests/t_harts_atomic.c VERBATIM)
add_custom_target(risc6-bare-tests DEPENDS
  ${TESTS_DIR}/t_bare ${TESTS_DIR}/t_harts ${TESTS_DIR}/t_harts_irq ${TESTS_DIR}/t_harts_atomic)
