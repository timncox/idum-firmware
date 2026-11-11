// node web/test/bank-test.mjs <built beatgen .hex>
// Checks that the web programmer can find, read, edit and re-encode the pattern bank without
// touching any other byte of the firmware image.
import { readFileSync } from 'node:fs';
import { parseIntelHex } from '../webserial-jtag2updi.js';
import { readBank, writeBank, toIntelHex, findBank, BANK_BYTES } from '../beatgen-bank.js';

const file = process.argv[2];
if (!file) { console.error('usage: node web/test/bank-test.mjs <alt-dum-beatgen.hex>'); process.exit(2); }

const image = parseIntelHex(readFileSync(file, 'utf8'));
const patterns = readBank(image);
console.log(`bank at 0x${findBank(image).toString(16)}, ${image.length} byte image`);
console.log('house OUT1', patterns[0][0]);

// Round trip with no edits must reproduce the exact image.
const same = parseIntelHex(toIntelHex(writeBank(image, patterns)));
if (same.length !== image.length || same.some((b, i) => b !== image[i])) throw new Error('round trip changed the image');

// Edit the User pattern and confirm only bank bytes moved.
const edited = patterns.map((rows) => rows.slice());
edited[4][0] = 'X..X..X.' + 'X'.repeat(24);
const patched = parseIntelHex(toIntelHex(writeBank(image, edited)));
const start = findBank(image);
let changed = 0;
for (let i = 0; i < image.length; i++) {
  if (patched[i] !== image[i]) {
    if (i < start || i >= start + BANK_BYTES) throw new Error(`byte 0x${i.toString(16)} outside the bank changed`);
    changed++;
  }
}
if (readBank(patched)[4][0] !== edited[4][0]) throw new Error('edit did not read back');
console.log(`PASS bank round trip; edit changed ${changed} bytes, all inside the bank`);
