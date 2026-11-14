# risc6-packaging：CPack 配置，产出 TGZ / DEB / RPM。
# 复用 risc6-install.cmake 的 install() 清单，故包内容与 `cmake --install` 一致。
# 默认只开 TGZ，避免本机无 rpmbuild/dpkg 时 cpack 直接失败；CI 用 -G 显式覆盖。

set(RISC6_PACKAGE_RELEASE "1" CACHE STRING
    "DEB/RPM 的 Release 字段；CI 可注入构建号，例如 42.1a2b3c4")

set(CPACK_PACKAGE_NAME "risc6")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_VENDOR "risc6")
set(CPACK_PACKAGE_CONTACT "taotieren <admin@taotieren.com>")
set(CPACK_PACKAGE_HOMEPAGE_URL "https://github.com/c6lang/risc6")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY
    "risc6 - RV32IMAC machine emulator (native + wasm32), written in c6")
string(CONCAT CPACK_PACKAGE_DESCRIPTION
  "risc6 is a standalone RISC-V RV32IMAC machine emulator implemented in the c6 language.\n"
  "It runs RV32 static ELFs natively and as a wasm32 module, with instruction-boundary\n"
  "interrupt injection, without requiring qemu.\n")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
set(CPACK_RESOURCE_FILE_README "${CMAKE_CURRENT_SOURCE_DIR}/README.md")

# install() 用相对 prefix 路径，包内根须与配置时一致，否则 DESTDIR/CPack 装到不同前缀。
set(CPACK_PACKAGING_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")
set(CPACK_GENERATOR "TGZ" CACHE STRING "cpack 默认生成器，分号分隔")
set(CPACK_SOURCE_GENERATOR "TGZ")
set(CPACK_SOURCE_IGNORE_FILES
    "/\\.git/" "/build/" "/build-.*/" "/packages/" "/stage/"
    "/\\.o$" "/\\.a$" "/\\.so" "/\\.dylib$" "/\\.exe$" "/\\.core$")

# ---------------------------------------------------------------------------
# DEB
# ---------------------------------------------------------------------------
set(CPACK_DEBIAN_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_DEBIAN_PACKAGE_RELEASE "${RISC6_PACKAGE_RELEASE}")
set(CPACK_DEBIAN_FILE_NAME "DEB-DEFAULT")
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "taotieren <admin@taotieren.com>")
set(CPACK_DEBIAN_PACKAGE_SECTION "devel")
set(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")
set(CPACK_DEBIAN_PACKAGE_HOMEPAGE "${CPACK_PACKAGE_HOMEPAGE_URL}")
# 运行时仅依赖 glibc；wasm 运行需 node、对拍需 qemu/clang，均为可选。
set(CPACK_DEBIAN_PACKAGE_DEPENDS "libc6 (>= 2.17)")
set(CPACK_DEBIAN_PACKAGE_RECOMMENDS "nodejs, qemu-user, clang")
# 不探测本机 dpkg：直接从 CMake 目标架构映射，保证打包机架构正确。
set(_risc6_deb_arch "${CMAKE_SYSTEM_PROCESSOR}")
if(_risc6_deb_arch STREQUAL "x86_64")
  set(_risc6_deb_arch "amd64")
elseif(_risc6_deb_arch MATCHES "^(aarch64|arm64)$")
  set(_risc6_deb_arch "arm64")
elseif(_risc6_deb_arch MATCHES "^armv")
  set(_risc6_deb_arch "armhf")
elseif(_risc6_deb_arch MATCHES "^(i[3-6]86|x86)$")
  set(_risc6_deb_arch "i386")
endif()
set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "${_risc6_deb_arch}")
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS OFF)

# ---------------------------------------------------------------------------
# RPM
# ---------------------------------------------------------------------------
set(CPACK_RPM_PACKAGE_RELEASE "${RISC6_PACKAGE_RELEASE}")
set(CPACK_RPM_FILE_NAME "RPM-DEFAULT")
set(CPACK_RPM_PACKAGE_LICENSE "Apache-2.0")
set(CPACK_RPM_PACKAGE_GROUP "System/Emulators")
set(CPACK_RPM_PACKAGE_URL "${CPACK_PACKAGE_HOMEPAGE_URL}")
set(CPACK_RPM_PACKAGE_SUMMARY "${CPACK_PACKAGE_DESCRIPTION_SUMMARY}")
set(CPACK_RPM_PACKAGE_DESCRIPTION "${CPACK_PACKAGE_DESCRIPTION}")
set(CPACK_RPM_PACKAGE_REQUIRES "glibc")
set(CPACK_RPM_PACKAGE_AUTOREQ OFF)
set(CPACK_RPM_PACKAGE_AUTOPROV OFF)
set(CPACK_RPM_PACKAGE_RELOCATABLE OFF)

include(CPack)
