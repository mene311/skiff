// SPDX-License-Identifier: GPL-3.0-only
// Phrase-arp in the browser build, in Node: the capture pass on the ARP page (keys sound, the graph changes,
// the phrase recorded), REC elsewhere recording what the arp plays into the sequence, and the pool coming back
// with the page's saved flash.
//   node web/emu/phr_test.mjs build/emu/felucca.wasm
import fs from "fs";
import { storeSectors, restoreSectors } from "./worklet.js";

const wasmPath = process.argv[2] || "build/emu/felucca.wasm";
const bytes = fs.readFileSync(wasmPath);
const B = { FX: 0, SEL: 1, ENV: 2, LFO: 3, EDIT: 4, GLO: 5, HOME: 6, SAVE: 7, ARP: 8, SEQ: 9, PLAY: 10, REC: 11, OCTDN: 12, OCTUP: 13 };
const EN = { SELECT: 0, ALGO: 1, PRESETS: 2, K1: 3, K2: 4, K3: 5, K4: 6 };
let fails = 0;
const check = (what, ok) => { console.log(`phr: ${what.padEnd(72)} ${ok ? "ok" : "FAIL"}`); if (!ok) fails++; };

async function device(saved) {
  const { instance } = await WebAssembly.instantiate(bytes, {});
  const ex = instance.exports, mem = ex.memory;
  ex._initialize();
  ex.web_nor_erase();
  if (saved) restoreSectors(new Uint8Array(mem.buffer, ex.web_nor(), ex.web_nor_size()), saved);
  ex.web_boot();
  const d = { ex, mem, peak: 0 };
  d.render = (ms) => {
    const n = Math.round(ms * 44.1);
    for (let k = 0; k < n; k += 128) {
      const m = Math.min(128, n - k);
      ex.web_render(m);
      const l = new Float32Array(mem.buffer, ex.web_out_l(), m), r = new Float32Array(mem.buffer, ex.web_out_r(), m);
      for (let i = 0; i < m; i++) d.peak = Math.max(d.peak, Math.abs(l[i]), Math.abs(r[i]));
    }
  };
  d.tap = (b) => { ex.web_buttons(1 << b); d.render(60); ex.web_buttons(0); d.render(60); };
  d.turn = (role, n) => { for (let i = 0; i < Math.abs(n); i++) { ex.web_enc(role, Math.sign(n)); d.render(40); } };
  d.key = (k, ms) => { ex.web_keys(1 << k); d.render(ms || 30); ex.web_keys(0); d.render(30); };
  d.screen = () => new Uint16Array(mem.buffer, ex.web_screen(), 240 * 240).slice();
  return d;
}

const a = await device(null);
a.render(500);                                   // boot + splash
a.peak = 0;

// the ARP page, MODE to PH1 (15 detents past OFF)
a.tap(B.ARP);
a.turn(EN.K1, 15);
const empty = a.screen();

// REC arms and starts the transport; a key on the phrase's screen captures (sounds, writes the phrase)
a.tap(B.REC);
a.peak = 0;
a.key(7);
check("capture: the key sounds while it is recorded", a.peak > 0);
const written = a.screen();
let diff = 0;
for (let i = 0; i < written.length; i++) if (written[i] !== empty[i]) diff++;
check("capture: the graph changed (the phrase's step drawn)", diff > 100);
a.tap(B.REC);                                    // disarm
a.tap(B.PLAY);                                   // stop

// HOME: REC records what the arp plays into the sequence (the phrase plays from the held key)
a.tap(B.HOME);
a.peak = 0;
a.tap(B.REC);
a.key(7, 200);
check("REC elsewhere: the arp plays the recorded phrase (sound out)", a.peak > 0);
a.tap(B.REC);
a.tap(B.PLAY);

// a fresh instance with the page's saved flash: the phrase comes back
const saved = storeSectors(new Uint8Array(a.mem.buffer, a.ex.web_nor(), a.ex.web_nor_size()));
const b = await device(saved);
b.render(500);
b.tap(B.ARP);
b.peak = 0;
b.tap(B.REC);
b.tap(B.HOME);                                   // leave the phrase screen: REC records the arp's output
b.key(7, 200);
check("the pool comes back with the saved flash (the phrase plays)", b.peak > 0);
b.tap(B.REC);
b.tap(B.PLAY);

process.exit(fails ? 1 : 0);
