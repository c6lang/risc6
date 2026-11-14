cmake_minimum_required(VERSION 3.16)

# 版本取自 include/risc6.h 的 RISC6_VERSION 宏（单一事实来源），避免误用旧 tag。
set(RISC6_VERSION "0.2.0")
set(_risc6_hdr "${CMAKE_CURRENT_SOURCE_DIR}/include/risc6.h")
if(EXISTS "${_risc6_hdr}")
  file(STRINGS "${_risc6_hdr}" _risc6_ver REGEX "^#define[ \t]+RISC6_VERSION[ \t]+\"")
  if(_risc6_ver MATCHES "\"([0-9]+\\.[0-9]+\\.[0-9]+)\"")
    set(RISC6_VERSION "${CMAKE_MATCH_1}")
  endif()
endif()
project(risc6 VERSION ${RISC6_VERSION} LANGUAGES NONE)

# risc6 —— 用 c6 写的 RV32IMAC 模拟器（native + wasm32）。
# 构建逻辑拆分到 cmake/*.cmake 模块，本文件只做顶层装配：
#   risc6-toolchain.cmake  工具链探测 + c6 定位（find_package / pkgconf / C6_ROOT）
#   risc6-build.cmake      核心翻译单元、native/wasm 模拟器、测试 ELF
#   risc6-targets.cmake    高层 target 与 CTest
#   risc6-install.cmake    安装路径布局
#   risc6-packaging.cmake  CPack（TGZ / DEB / RPM）
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
include(cmake/risc6-install.cmake)
include(cmake/risc6-packaging.cmake)

message(STATUS "risc6: `cmake --build build` 产出 ${BUILD_DIR}/risc6；"
               "`--target risc6-wasm` 产出 ${BUILD_DIR}/risc6.wasm")
