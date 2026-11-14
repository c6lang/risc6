# 高层 target（对应旧 Makefile 的 check / run / selfcheck / bare / harts / stats）。
# 脚本内部用相对路径引用 tools/，故统一在源码根目录运行。

add_custom_target(check
  COMMAND ${CMAKE_COMMAND} -E env
          BUILD=${BUILD_DIR} EMU=${BUILD_DIR}/risc6 WASM=${BUILD_DIR}/risc6.wasm
          QEMU=${QEMU}
          ${SH} ${SRC}/tools/check.sh ${_test_bins}
  DEPENDS risc6 risc6-tests
  WORKING_DIRECTORY ${SRC}
  VERBATIM)

# 跑任意 RV32 静态 ELF：默认 t_hello；可用 -DRISC6_RUN_ELF=<绝对路径> 指定，
# 或直接 ./build/risc6 <任意 elf>（cmake 无法在构建时传参，故以缓存变量提供覆盖）。
set(RISC6_RUN_ELF "${TESTS_DIR}/t_hello" CACHE FILEPATH
    "run / run-wasm 默认运行的 ELF；也可直接 ./build/risc6 <任意 elf>")
add_custom_target(run
  COMMAND ${BUILD_DIR}/risc6 ${RISC6_RUN_ELF}
  DEPENDS risc6 risc6-tests ${RISC6_RUN_ELF}
  VERBATIM)
add_custom_target(run-wasm
  COMMAND ${NODE} ${SRC}/tools/run-wasm.mjs ${BUILD_DIR}/risc6.wasm ${RISC6_RUN_ELF}
  DEPENDS risc6-wasm risc6-tests ${RISC6_RUN_ELF}
  VERBATIM)

add_custom_target(stats
  COMMAND ${BUILD_DIR}/risc6 ${TESTS_DIR}/t_hello  >/dev/null
  COMMAND ${BUILD_DIR}/risc6 ${TESTS_DIR}/t_isa    >/dev/null
  COMMAND ${BUILD_DIR}/risc6 ${TESTS_DIR}/t_mem    >/dev/null
  COMMAND ${BUILD_DIR}/risc6 ${TESTS_DIR}/t_atomic >/dev/null
  DEPENDS risc6 risc6-tests
  VERBATIM)

add_custom_target(selfcheck
  COMMAND ${CMAKE_COMMAND} -E env
          BUILD=${BUILD_DIR} EMU=${BUILD_DIR}/risc6 WASM=${BUILD_DIR}/risc6.wasm
          QEMU=${QEMU}
          ${SH} ${SRC}/tools/selfcheck.sh ${TESTS_DIR}/t_hello
  DEPENDS risc6 risc6-wasm risc6-tests
  WORKING_DIRECTORY ${SRC}
  VERBATIM)

add_custom_target(bare
  COMMAND ${CMAKE_COMMAND} -E echo "== bare native"
  COMMAND ${BUILD_DIR}/risc6 ${TESTS_DIR}/t_bare || true
  COMMAND ${CMAKE_COMMAND} -E echo "== bare wasm"
  COMMAND ${NODE} ${SRC}/tools/run-wasm.mjs ${BUILD_DIR}/risc6.wasm ${TESTS_DIR}/t_bare || true
  DEPENDS risc6 risc6-wasm risc6-bare-tests
  WORKING_DIRECTORY ${SRC}
  VERBATIM)

add_custom_target(harts
  COMMAND ${CMAKE_COMMAND} -E echo "== two harts native"
  COMMAND ${BUILD_DIR}/risc6 --bare --harts 2 ${TESTS_DIR}/t_harts
  COMMAND ${CMAKE_COMMAND} -E echo "== two harts wasm"
  COMMAND ${NODE} ${SRC}/tools/run-wasm.mjs ${BUILD_DIR}/risc6.wasm ${TESTS_DIR}/t_harts --bare --harts 2
  COMMAND ${CMAKE_COMMAND} -E echo "== per-hart timer irq native"
  COMMAND ${BUILD_DIR}/risc6 --bare --harts 2 ${TESTS_DIR}/t_harts_irq
  COMMAND ${CMAKE_COMMAND} -E echo "== per-hart timer irq wasm"
  COMMAND ${NODE} ${SRC}/tools/run-wasm.mjs ${BUILD_DIR}/risc6.wasm ${TESTS_DIR}/t_harts_irq --bare --harts 2
  COMMAND ${CMAKE_COMMAND} -E echo "== two-hart atomic native"
  COMMAND ${BUILD_DIR}/risc6 --bare --harts 2 ${TESTS_DIR}/t_harts_atomic
  COMMAND ${CMAKE_COMMAND} -E echo "== two-hart atomic wasm"
  COMMAND ${NODE} ${SRC}/tools/run-wasm.mjs ${BUILD_DIR}/risc6.wasm ${TESTS_DIR}/t_harts_atomic --bare --harts 2
  DEPENDS risc6 risc6-wasm risc6-bare-tests
  WORKING_DIRECTORY ${SRC}
  VERBATIM)

# 对齐旧 Makefile 的 `make tests` / `make clean`
add_custom_target(tests DEPENDS risc6-tests)
add_custom_target(clean-all
  COMMAND ${CMAKE_COMMAND} -E rm -rf ${BUILD_DIR}
  COMMENT "删除整个构建目录（含 out/ 等脚本产物），等价于旧 make clean")

# ---- CTest：把对拍与自检登记为用例，便于 `ctest --test-dir build` --------
if(BUILD_TESTING)
  add_test(NAME risc6-check
    COMMAND ${CMAKE_COMMAND} -E env BUILD=${BUILD_DIR}
            EMU=${BUILD_DIR}/risc6 WASM=${BUILD_DIR}/risc6.wasm QEMU=${QEMU}
            ${SH} ${SRC}/tools/check.sh ${_test_bins})
  set_tests_properties(risc6-check PROPERTIES LABELS "risc6;check" WORKING_DIRECTORY ${SRC})
  add_test(NAME risc6-selfcheck
    COMMAND ${CMAKE_COMMAND} -E env BUILD=${BUILD_DIR}
            EMU=${BUILD_DIR}/risc6 WASM=${BUILD_DIR}/risc6.wasm QEMU=${QEMU}
            ${SH} ${SRC}/tools/selfcheck.sh ${TESTS_DIR}/t_hello)
  set_tests_properties(risc6-selfcheck PROPERTIES LABELS "risc6;selfcheck" WORKING_DIRECTORY ${SRC})
endif()
