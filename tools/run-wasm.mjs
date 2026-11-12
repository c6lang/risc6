#!/usr/bin/env node
/* risc6 wasm32 运行器：把 RV32 ELF 喂给 build/risc6.wasm，打印 guest 输出。
 * 用法: node tools/run-wasm.mjs build/risc6.wasm <file.elf> [--limit N] [--quiet] */

import { readFileSync } from 'node:fs';

const argv = process.argv.slice(2);
if (argv.length < 2) {
  console.error('usage: node tools/run-wasm.mjs <risc6.wasm> <file.elf> [--limit N] [--quiet]');
  process.exit(2);
}

const wasmPath = argv[0];
let elfPath = null;
let limit = null;
let quiet = false;
for (let i = 1; i < argv.length; i++) {
  const a = argv[i];
  if (a === '--limit' && i + 1 < argv.length) limit = BigInt(argv[++i]);
  else if (a === '--quiet') quiet = true;
  else if (a.startsWith('-')) { console.error(`risc6: unknown option ${a}`); process.exit(2); }
  else if (elfPath === null) elfPath = a;
  else { console.error('risc6: one ELF at a time'); process.exit(2); }
}
if (elfPath === null) {
  console.error('risc6: missing ELF');
  process.exit(2);
}

const elf = readFileSync(elfPath);
const { instance } = await WebAssembly.instantiate(readFileSync(wasmPath), {});
const ex = instance.exports;
const mem = () => new Uint8Array(ex.memory.buffer);

if (limit !== null) ex.wx_set_limit(limit);

const inAddr = Number(ex.wx_in_addr());
mem().set(elf, inAddr);
const rc = Number(ex.wx_run(elf.length));

const outLen = Number(ex.wx_out_len());
if (outLen > 0) {
  process.stdout.write(Buffer.from(mem().slice(Number(ex.wx_out_addr()), Number(ex.wx_out_addr()) + outLen)));
}
const diagLen = Number(ex.wx_diag_len());
if (diagLen > 0) {
  process.stderr.write(Buffer.from(mem().slice(Number(ex.wx_diag_addr()), Number(ex.wx_diag_addr()) + diagLen)));
}
if (!quiet) {
  const insns = BigInt(ex.wx_insns_lo()) | (BigInt(ex.wx_insns_hi()) << 32n);
  process.stderr.write(`risc6(wasm): insns=${insns} rc=${rc}\n`);
}
process.exit(rc);
