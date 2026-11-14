# 安装布局（FHS）：可执行 bin/risc6、wasm 模块与 node runner 在 share/risc6/，
# 文档与许可证在 share/doc/risc6/，另按 RPM 惯例放一份 license 到 share/licenses/。
# 这些规则同时被 `cmake --install` 与 CPack 复用，故包内容与手动安装一致。

install(PROGRAMS ${BUILD_DIR}/risc6 DESTINATION bin)
install(FILES    ${BUILD_DIR}/risc6.wasm DESTINATION share/risc6)
# 跑 wasm 必须有 node runner，否则 risc6.wasm 无法使用。
install(PROGRAMS ${SRC}/tools/run-wasm.mjs DESTINATION share/risc6)

install(FILES ${SRC}/README.md ${SRC}/CHANGELOG.md DESTINATION share/doc/risc6)
install(DIRECTORY ${SRC}/docs/ DESTINATION share/doc/risc6/docs
        FILES_MATCHING PATTERN "*.md")

# Debian 政策要求 copyright 在 /usr/share/doc/<pkg>/copyright；
# RPM 惯例在 /usr/share/licenses/<pkg>/LICENSE。
install(FILES ${SRC}/LICENSE DESTINATION share/doc/risc6 RENAME copyright)
install(FILES ${SRC}/LICENSE DESTINATION share/licenses/risc6)
