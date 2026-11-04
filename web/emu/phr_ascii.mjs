import fs from "fs";
const bytes = fs.readFileSync(process.argv[2] || "build/emu/felucca.wasm");
const B = { FX: 0, SEL: 1, ENV: 2, LFO: 3, EDIT: 4, GLO: 5, HOME: 6, SAVE: 7, ARP: 8, SEQ: 9, PLAY: 10, REC: 11, OCTDN: 12, OCTUP: 13 };
const EN = { SELECT: 0, ALGO: 1, PRESETS: 2, K1: 3, K2: 4, K3: 5, K4: 6 };

async function device() {
  const { instance } = await WebAssembly.instantiate(bytes, {});
  const ex = instance.exports, mem = ex.memory;
  ex._initialize();
  ex.web_nor_erase();
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

function stats(fb) {
  const cnt = new Map();
  for (const v of fb) cnt.set(v, (cnt.get(v) || 0) + 1);
  let bg = 0, best = 0;
  for (const [v, n] of cnt) if (n > best) { bg = v; best = n; }
  return { colors: cnt.size, bg: bg.toString(16) };
}
function ascii(fb, x0, x1, y0, y1) {
  const cnt = new Map();
  for (const v of fb) cnt.set(v, (cnt.get(v) || 0) + 1);
  let bg = 0, best = 0;
  for (const [v, n] of cnt) if (n > best) { bg = v; best = n; }
  let out = "";
  for (let y = y0; y <= y1; y += 2) {
    let row = "";
    for (let x = x0; x <= x1; x += 2) {
      let lit = 0;
      for (let dy = 0; dy < 2 && y + dy <= y1; dy++)
        for (let dx = 0; dx < 2 && x + dx <= x1; dx++)
          if (fb[(y + dy) * 240 + x + dx] !== bg) lit++;
      row += lit > 2 ? "#" : lit ? "." : " ";
    }
    out += row + "\n";
  }
  return out;
}

const a = await device();
a.render(500);
a.tap(B.ARP);
a.turn(EN.K1, 15);
const before = a.screen();
console.log("before stats:", JSON.stringify(stats(before)));
console.log("--- before ---");
console.log(ascii(before, 20, 235, 10, 135));
a.tap(B.REC);
a.peak = 0;
a.key(7, 40);
const after = a.screen();
console.log("after stats:", JSON.stringify(stats(after)));
console.log("--- after ---");
console.log(ascii(after, 20, 235, 10, 135));
let diff = 0;
for (let i = 0; i < after.length; i++) if (after[i] !== before[i]) diff++;
console.log("diff px:", diff, "peak:", a.peak);
