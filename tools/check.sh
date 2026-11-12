#!/bin/sh
# tools/check.sh —— 三方对拍：native 模拟器 / wasm32 模拟器 / qemu-riscv32。
# 比较 stdout 与退出码（guest 输出必须逐字节一致）。
# 缺 qemu 或 node 时明确提示并跳过对应一侧（不静默通过）。

set -u

BUILD=${BUILD:-build}
EMU=${EMU:-$BUILD/risc6}
WASM=${WASM:-$BUILD/risc6.wasm}
QEMU=${QEMU:-qemu-riscv32}
OUT=$BUILD/out
mkdir -p "$OUT"

have_qemu=0
command -v "$QEMU" >/dev/null 2>&1 && have_qemu=1

have_node=0
command -v node >/dev/null 2>&1 && have_node=1

have_wasm=0
if [ -f "$WASM" ] && [ "$have_node" -eq 1 ]; then
    have_wasm=1
fi

[ "$have_wasm" -eq 0 ] && echo "note: wasm 产物或 node 缺失，跳过 wasm 对拍（make wasm）"
[ "$have_qemu" -eq 0 ] && echo "note: 没有 $QEMU，跳过 qemu 对拍"

fail=0
total=0

for elf in "$@"; do
    name=$(basename "$elf")
    total=$((total + 1))

    "$EMU" --quiet "$elf" >"$OUT/$name.native.out" 2>"$OUT/$name.native.err"
    nrc=$?

    ref_out="$OUT/$name.native.out"
    ref_rc=$nrc
    ref_name=native

    if [ "$have_wasm" -eq 1 ]; then
        node tools/run-wasm.mjs "$WASM" "$elf" --quiet \
            >"$OUT/$name.wasm.out" 2>"$OUT/$name.wasm.err"
        wrc=$?
        if ! cmp -s "$ref_out" "$OUT/$name.wasm.out" || [ "$wrc" -ne "$ref_rc" ]; then
            echo "FAIL $name: wasm 与 native 不一致 (wasm rc=$wrc native rc=$ref_rc)"
            diff -u "$ref_out" "$OUT/$name.wasm.out" | head -20
            fail=$((fail + 1))
        fi
        ref_name="$ref_name+wasm"
    fi

    if [ "$have_qemu" -eq 1 ]; then
        "$QEMU" "$elf" >"$OUT/$name.qemu.out" 2>"$OUT/$name.qemu.err"
        qrc=$?
        if ! cmp -s "$ref_out" "$OUT/$name.qemu.out" || [ "$qrc" -ne "$ref_rc" ]; then
            echo "FAIL $name: qemu 与 $ref_name 不一致 (qemu rc=$qrc rc=$ref_rc)"
            diff -u "$ref_out" "$OUT/$name.qemu.out" | head -20
            fail=$((fail + 1))
        fi
        ref_name="$ref_name+qemu"
    fi

    if [ "$ref_rc" -ne 0 ]; then
        echo "FAIL $name: 退出码 $ref_rc（见 $OUT/$name.native.err）"
        fail=$((fail + 1))
    else
        echo "ok   $name ($ref_name, rc=0)"
    fi
done

echo "---"
if [ "$fail" -eq 0 ]; then
    echo "check: $total 用例全部一致"
    exit 0
fi
echo "check: $fail 处不一致"
exit 1
