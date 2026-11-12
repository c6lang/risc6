#!/bin/sh
# tools/selfcheck.sh —— 模拟器自身行为的用例（不比对 guest 输出，只看退出码与诊断）。
#
#   T3  指令预算：--limit 太小 -> 201，且诊断里说明是预算耗尽
#   T4  加载失败：文件不存在 / 空文件 -> 200
#   T5  预算默认值：--limit 0 等价于不设限（跑完 guest）
#
# native 与 wasm 两侧都跑（没有 node 时跳过 wasm）。

set -u

BUILD=${BUILD:-build}
EMU=${EMU:-$BUILD/risc6}
WASM=${WASM:-$BUILD/risc6.wasm}
OUT=$BUILD/out
mkdir -p "$OUT"

have_node=0
command -v node >/dev/null 2>&1 && have_node=1
have_wasm=0
if [ -f "$WASM" ] && [ "$have_node" -eq 1 ]; then
    have_wasm=1
fi

fail=0
total=0

# run_native <elf> [额外参数...] -> 设置 rc / err
run_native() {
    "$EMU" "$@" >"$OUT/self.out" 2>"$OUT/self.err"
    rc=$?
}

run_wasm() {
    node tools/run-wasm.mjs "$WASM" "$@" >"$OUT/self.out" 2>"$OUT/self.err"
    rc=$?
}

check() {   # check <名字> <期望 rc> <诊断关键字或 ->
    total=$((total + 1))
    name=$1
    want=$2
    key=$3
    ok=1
    [ "$rc" -eq "$want" ] || ok=0
    if [ "$key" != "-" ]; then
        grep -q "$key" "$OUT/self.err" || ok=0
    fi
    if [ "$ok" -eq 1 ]; then
        echo "ok   $name (rc=$rc)"
    else
        echo "FAIL $name: rc=$rc 期望 $want（见 $OUT/self.err）"
        head -3 "$OUT/self.err" | sed 's/^/     /'
        fail=$((fail + 1))
    fi
}

ELF=${1:-$BUILD/tests/t_hello}
if [ ! -f "$ELF" ]; then
    echo "selfcheck: 缺少 $ELF（先 make tests）"
    exit 2
fi

echo "== native $ELF"
run_native --limit 10 "$ELF"
check "native 预算耗尽" 201 "budget"
run_native --limit 0 "$ELF"
check "native limit=0 用默认预算" 0 "-"
run_native "$BUILD/out/does-not-exist.elf"
check "native 文件不存在" 200 "cannot open"
: >"$OUT/empty.elf"
run_native "$OUT/empty.elf"
check "native 空文件" 200 "empty"

if [ "$have_wasm" -eq 1 ]; then
    echo "== wasm $ELF"
    run_wasm --limit 10 "$ELF"
    check "wasm 预算耗尽" 201 "budget"
    run_wasm --limit 0 "$ELF"
    check "wasm limit=0 用默认预算" 0 "-"
    run_wasm "$BUILD/out/does-not-exist.elf"
    check "wasm 文件不存在" 200 "cannot open"
    : >"$OUT/empty.elf"
    run_wasm "$OUT/empty.elf"
    check "wasm 空文件" 200 "is empty"
else
    echo "note: wasm 产物或 node 缺失，跳过 wasm 自检（make wasm）"
fi

echo "---"
if [ "$fail" -eq 0 ]; then
    echo "selfcheck: $total 项全部通过"
    exit 0
fi
echo "selfcheck: $fail 项失败"
exit 1
