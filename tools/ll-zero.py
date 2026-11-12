#!/usr/bin/env python3
"""把 IR 里「全零」的字符串常量改写成 zeroinitializer。

c6 的 LLVM 后端把整个 .data 段吐成一个
`@__llvm_d2 = internal global [N x i8] c"\\00\\00..."` 大字符串，
里面绝大部分是全零的静态数组（risc6_mem/in/out/diag ...）。
这会让 .ll 膨胀到 28MB、.wasm 膨胀到 9MB。语义上没有区别：
- 只有整块精确为全零时才写成 zeroinitializer（进 bss，不占 data 段）；
- 有任何非零字节就原样保留（非零初值是真的初始状态，不能猜）。
  所以 c6 侧要避免给大静态数组/状态变量写非零初值（见 docs/design.md §6.1）。

用法： ll-zero.py <in.ll> <out.ll>    （out 可与 in 相同）
"""
import re
import sys

# @x = alias [SIZE x i8], ptr getelementptr inbounds ([TOTAL x i8], ptr @BLOB, i64 0, i64 OFF)
ALIAS = re.compile(
    rb'@(\w+) = alias \[(\d+) x i8\], ptr getelementptr inbounds '
    rb'\(\[(\d+) x i8\], ptr @(\w+), i64 0, i64 (\d+)\)')
GLOBAL = re.compile(rb'@(\w+) = internal global \[(\d+) x i8\] c"((?:[^"\\]|\\.)*)"')
ESC = re.compile(rb'\\([0-9A-Fa-f]{2}|\\)')


def decode(s):
    out = bytearray()
    i = 0
    n = len(s)
    while i < n:
        c = s[i:i + 1]
        if c == b"\\":
            h = s[i + 1:i + 3]
            if len(h) == 2 and all(chr(b) in "0123456789abcdefABCDEF" for b in h):
                out.append(int(h, 16))
                i += 3
            else:
                out.append(s[i + 1])
                i += 2
        else:
            out.append(s[i])
            i += 1
    return bytes(out)


def main():
    if len(sys.argv) != 3:
        sys.exit("usage: ll-zero.py <in.ll> <out.ll>")
    src, dst = sys.argv[1], sys.argv[2]
    data = open(src, "rb").read()

    # 每个 blob 被符号引用的最大末尾偏移
    end = {}
    for m in ALIAS.finditer(data):
        size, blob, off = int(m.group(2)), m.group(4), int(m.group(5))
        if off + size > end.get(blob, 0):
            end[blob] = off + size

    stats = {"zero": 0, "trim": 0, "keep": 0}

    def sub(m):
        name, total, body = m.group(1), int(m.group(2)), m.group(3)
        raw = decode(body)
        if len(raw) != total:
            stats["keep"] += 1
            return m.group(0)
        if raw.count(0) == total:
            stats["zero"] += 1
            return b"@%s = internal global [%d x i8] zeroinitializer" % (name, total)
        stats["keep"] += 1
        return m.group(0)

    out = GLOBAL.sub(sub, data)
    open(dst, "wb").write(out)
    print("ll-zero: %d 个全零 blob -> zeroinitializer, %d 个保留 (%.1f MB -> %.1f MB)"
          % (stats["zero"], stats["keep"],
             len(data) / 1048576.0, len(out) / 1048576.0))


if __name__ == "__main__":
    main()
