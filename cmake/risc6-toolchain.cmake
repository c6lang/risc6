# 工具链探测与 c6 定位。
# c6 经其 CMake 包（find_package）、pkgconf（c6.pc）或源码树（C6_ROOT）三种方式
# 定位，优先用 cmake / pkgconf 提供的接口，最后回退到源码树（等价于旧 Makefile 的
# C6_ROOT），方便尚未安装 c6 时开发。产出 C6_COMPILER（可执行）与 C6_TCCDIR
# （含 libtcc1.a / tcclib.h 的 tcc 运行时库目录）。
find_program(CLANG  NAMES clang REQUIRED)
find_program(WASMLD NAMES wasm-ld REQUIRED)
find_program(NODE   NAMES node)
find_program(QEMU   NAMES qemu-riscv32)
find_program(SH     NAMES sh)

set(_c6_from "")
if(NOT C6_COMPILER)
  find_package(c6 CONFIG QUIET)
  if(c6_FOUND)
    set(C6_COMPILER "${C6_EXECUTABLE}")
    set(C6_TCCDIR   "${C6_TCCDIR}")
    set(_c6_from   "find_package(c6)")
  endif()
endif()
if(NOT C6_COMPILER)
  find_package(PkgConfig QUIET)
  if(PkgConfig_FOUND)
    pkg_check_modules(C6PC c6 QUIET)
    if(C6PC_FOUND)
      set(C6_COMPILER "${C6PC_c6}")
      set(C6_TCCDIR   "${C6PC_tccdir}")
      set(_c6_from   "pkgconf c6.pc")
    endif()
  endif()
endif()
if(NOT C6_COMPILER)
  set(C6_ROOT "$ENV{HOME}/c6" CACHE PATH
      "c6 源码/构建树根（含 tcc/c6 与 tcc/libtcc1.a）；等价于旧 Makefile 的 C6_ROOT")
  find_program(C6_COMPILER NAMES c6
               PATHS "${C6_ROOT}/tcc" NO_DEFAULT_PATH REQUIRED)
  find_path(C6_TCCDIR NAMES libtcc1.a
            PATHS "${C6_ROOT}/tcc" NO_DEFAULT_PATH REQUIRED)
  set(_c6_from "C6_ROOT=${C6_ROOT}")
endif()
set(C6_TCC_INCLUDE "${C6_TCCDIR}/include")
message(STATUS "risc6: c6 来自 ${_c6_from} (c6=${C6_COMPILER}, tcclib=${C6_TCCDIR})")
