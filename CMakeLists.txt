cmake_minimum_required(VERSION 3.16)
project(risc6 VERSION 0.2.0 LANGUAGES NONE)

# risc6 —— 用 c6 写的 RV32IMAC 模拟器（native + wasm32）。
# 构建逻辑拆分到 cmake/*.cmake 模块，本文件只做顶层装配：
#   risc6-toolchain.cmake  工具链探测 + c6 定位（find_package / pkgconf / C6_ROOT）
#   risc6-build.cmake      核心翻译单元、native/wasm 模拟器、测试 ELF
#   risc6-targets.cmake    高层 target、安装与 CTest
include(CTest)

set(SRC        ${CMAKE_CURRENT_SOURCE_DIR})
set(BUILD_DIR  ${CMAKE_BINARY_DIR})
set(NATIVE_DIR ${BUILD_DIR}/native)
set(WASM_DIR   ${BUILD_DIR}/wasm)
set(TESTS_DIR  ${BUILD_DIR}/tests)
set(OUT_DIR    ${BUILD_DIR}/out)
file(MAKE_DIRECTORY ${NATIVE_DIR} ${WASM_DIR} ${TESTS_DIR} ${OUT_DIR})

include(cmake/risc6-toolchain.cmake)
include(cmake/risc6-build.cmake)
include(cmake/risc6-targets.cmake)

message(STATUS "risc6: `cmake --build build` 产出 ${BUILD_DIR}/risc6；"
               "`--target risc6-wasm` 产出 ${BUILD_DIR}/risc6.wasm")
