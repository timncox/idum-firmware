// beatgen-bank.js - read and rewrite the Beatgen pattern bank inside a built firmware image.
//
// The firmware stores its five editable patterns as plain text right after the marker
// "IDUMBEAT1" (see firmware/engines/beatgen/c10_BeatgenEngine.ino). Each pattern is four
// 32-character rows (OUT1..OUT4); each character is a step: '.' rest, then '1' '2' '3' 'X'
// from least to most likely to play. Changing those bytes changes nothing else in the image.
//
// CC BY-SA 4.0, part of a derivative of IDUM by Eli Pechman / Mystic Circuits.

export const MAGIC = 'IDUMBEAT1';
export const PATTERN_NAMES = ['House', 'Techno', 'Breaks', 'Hip-hop', 'User'];
export const PATTERN_SLOTS = [1, 2, 3, 4, 7];  // MODE ring position (1-8) on the module
export const CHANNELS = 4;
export const STEPS = 32;
export const LEVELS = ['.', '1', '2', '3', 'X'];
export const LEVEL_WEIGHT = { '.': 0, '1': 64, '2': 128, '3': 192, 'X': 255 };
export const BANK_BYTES = PATTERN_NAMES.length * CHANNELS * STEPS;

const MAGIC_BYTES = new TextEncoder().encode(MAGIC);

export function findBank(bytes) {
  const hits = [];
  outer: for (let i = 0; i + MAGIC_BYTES.length + BANK_BYTES <= bytes.length; i++) {
    for (let j = 0; j < MAGIC_BYTES.length; j++) if (bytes[i + j] !== MAGIC_BYTES[j]) continue outer;
    hits.push(i);
  }
  if (hits.length === 0) throw new Error('This .hex has no Beatgen pattern bank. Build it with ALT_ENGINE=beatgen.');
  if (hits.length > 1) throw new Error('This .hex has more than one pattern marker; refusing to edit it.');
  return hits[0] + MAGIC_BYTES.length;
}

// -> patterns[pattern][channel] = 32-character string
export function readBank(bytes) {
  const start = findBank(bytes);
  const patterns = [];
  for (let p = 0; p < PATTERN_NAMES.length; p++) {
    const rows = [];
    for (let c = 0; c < CHANNELS; c++) {
      const offset = start + (p * CHANNELS + c) * STEPS;
      rows.push(String.fromCharCode(...bytes.subarray(offset, offset + STEPS)));
    }
    patterns.push(rows);
  }
  validatePatterns(patterns);
  return patterns;
}

export function validatePatterns(patterns) {
  if (!Array.isArray(patterns) || patterns.length !== PATTERN_NAMES.length) throw new Error('Expected 5 patterns.');
  for (const rows of patterns) {
    if (!Array.isArray(rows) || rows.length !== CHANNELS) throw new Error('Each pattern needs 4 rows.');
    for (const row of rows) {
      if (typeof row !== 'string' || row.length !== STEPS) throw new Error('Each row needs 32 steps.');
      for (const ch of row) if (!(ch in LEVEL_WEIGHT)) throw new Error(`Unexpected step "${ch}".`);
    }
  }
  return true;
}

// Returns a patched copy; the input is untouched.
export function writeBank(bytes, patterns) {
  validatePatterns(patterns);
  const out = Uint8Array.from(bytes);
  const start = findBank(out);
  patterns.forEach((rows, p) => rows.forEach((row, c) => {
    const offset = start + (p * CHANNELS + c) * STEPS;
    for (let s = 0; s < STEPS; s++) out[offset + s] = row.charCodeAt(s);
  }));
  return out;
}

// Flat image (address 0..n) -> Intel HEX, 16 data bytes per record (as avr-objcopy writes it).
export function toIntelHex(bytes) {
  const hex2 = (n) => n.toString(16).toUpperCase().padStart(2, '0');
  const lines = [];
  for (let addr = 0; addr < bytes.length; addr += 16) {
    const chunk = bytes.subarray(addr, Math.min(addr + 16, bytes.length));
    const rec = [chunk.length, (addr >> 8) & 0xff, addr & 0xff, 0, ...chunk];
    const sum = (0x100 - (rec.reduce((a, b) => a + b, 0) & 0xff)) & 0xff;
    lines.push(':' + rec.map(hex2).join('') + hex2(sum));
  }
  lines.push(':00000001FF');
  return lines.join('\n') + '\n';
}

// Which steps sound at a given PROB density (0-255), matching the firmware with PARAM at zero.
export function plays(level, density) {
  const weight = LEVEL_WEIGHT[level];
  if (!weight) return false;
  return weight === 255 || weight >= 255 - density;
}
