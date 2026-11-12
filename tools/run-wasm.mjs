#!/usr/bin/env node
/* risc6 wasm32 运行器：把 RV32 ELF 喂给 build/risc6.wasm，打印 guest 输出。
 * 用法: node tools/run-wasm.mjs build/risc6.wasm <file.elf> [--limit N] [--quiet] [--time] [--stats] [--bare] [--version] */

import { readFileSync } from 'node:fs';

const argv = process.argv.slice(2);
if (argv.length < 2) {
  console.error('usage: node tools/run-wasm.mjs <risc6.wasm> <file.elf> [--limit N] [--quiet] [--time] [--stats] [--bare] [--version]');
  process.exit(2);
}

const wasmPath = argv[0];
let elfPath = null;
let limit = null;
let quiet = false;
let wantTime = false;
let wantStats = false;
let bare = false;
let wantVersion = false;
for (let i = 1; i < argv.length; i++) {
  const a = argv[i];
  if (a === '--limit' && i + 1 < argv.length) limit = BigInt(argv[++i]);
  else if (a === '--quiet') quiet = true;
  else if (a === '--time') wantTime = true;
  else if (a === '--stats') wantStats = true;
  else if (a === '--bare') bare = true;
  else if (a === '--version') wantVersion = true;
  else if (a.startsWith('-')) { console.error(`risc6: unknown option ${a}`); process.exit(2); }
  else if (elfPath === null) elfPath = a;
  else { console.error('risc6: one ELF at a time'); process.exit(2); }
}
const { instance } = await WebAssembly.instantiate(readFileSync(wasmPath), {});
const ex = instance.exports;
const mem = () => new Uint8Array(ex.memory.buffer);

if (wantVersion) {
  const p = Number(ex.wx_version());
  let v = '';
  for (let i = 0; i < 32; i++) {
    const c = new Uint8Array(ex.memory.buffer, p + i, 1)[0];
    if (c === 0) break;
    v += String.fromCharCode(c);
  }
  console.log('risc6(wasm) ' + v);
  process.exit(0);
}
if (elfPath === null) {
  console.error('risc6: missing ELF');
  process.exit(2);
}

let elf;
try {
  elf = readFileSync(elfPath);
} catch {
  process.stderr.write(`risc6: cannot open ${elfPath}\n`);
  process.exit(200);            /* 与 native 宿主一致的加载失败码 */
}
if (elf.length === 0) {
  process.stderr.write(`risc6: ${elfPath} is empty\n`);
  process.exit(200);
}
if (limit !== null) ex.wx_set_limit(limit);
if (bare) ex.wx_set_mode(1n);

const inAddr = Number(ex.wx_in_addr());
mem().set(elf, inAddr);
const t0 = performance.now();
const rc = Number(ex.wx_run(elf.length));
const dt = performance.now() - t0;

const outLen = Number(ex.wx_out_len());
if (outLen > 0) {
  process.stdout.write(Buffer.from(mem().slice(Number(ex.wx_out_addr()), Number(ex.wx_out_addr()) + outLen)));
}
const diagLen = Number(ex.wx_diag_len());
if (diagLen > 0) {
  process.stderr.write(Buffer.from(mem().slice(Number(ex.wx_diag_addr()), Number(ex.wx_diag_addr()) + diagLen)));
}
const insns = BigInt(ex.wx_insns_lo()) | (BigInt(ex.wx_insns_hi()) << 32n);
if (wantStats) {
  const names = ['lui', 'auipc', 'jal', 'jalr', 'branch', 'load', 'store',
    'op-imm', 'op', 'fence', 'amo', 'system', 'bad-insn', 'compressed',
    'ecall', 'trap', 'mtip', 'csr-unimpl'];
  const st = new Uint32Array(ex.memory.buffer, Number(ex.wx_stat_addr()), names.length);
  for (let k = 0; k < names.length; k++) {
    if (st[k] !== 0) process.stderr.write(`risc6(wasm): stat ${names[k].padEnd(11)} ${st[k]}\n`);
  }
}
if (wantTime && !quiet) {
  const rate = dt > 0 ? Number(insns) / dt / 1e3 : 0;
  process.stderr.write(`risc6(wasm): time=${dt.toFixed(3)}ms rate=${rate.toFixed(1)} Minsn/s\n`);
}
if (!quiet) {
  process.stderr.write(`risc6(wasm): insns=${insns} rc=${rc}\n`);
}
process.exit(rc);
