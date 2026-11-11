// AudioWorklet that runs the IDUM firmware (compiled to WebAssembly from sim/emulator/emu.cpp)
// one audio block at a time, so every output edge lands on the sample it happened on.
//
// Sound modes:
//   drums: each trigger output plays a drum voice (OUT1 kick, OUT2 snare, OUT3 hat, OUT4 perc)
//   raw:   the four outputs are heard as the square waves they are at the jack
//   auto:  raw while VCO mode (slot 8) is running, drums otherwise

const STATE_EVERY = 6;  // post LEDs and scope data every 6 blocks (~16 ms at 48 kHz)

class Voice {
  constructor(kind, rate) { this.kind = kind; this.rate = rate; this.t = 1e9; this.phase = 0; this.hp = 0; this.last = 0; }
  hit() { this.t = 0; this.phase = 0; }
  next() {
    const t = this.t / this.rate;
    if (t > 0.6) return 0;
    this.t++;
    const noise = Math.random() * 2 - 1;
    switch (this.kind) {
      case 0: {  // kick: falling sine
        const f = 45 + 110 * Math.exp(-t * 30);
        this.phase += f / this.rate;
        return Math.sin(2 * Math.PI * this.phase) * Math.exp(-t * 7) * 0.9;
      }
      case 1: {  // snare: tone plus noise
        this.phase += 190 / this.rate;
        return (Math.sin(2 * Math.PI * this.phase) * Math.exp(-t * 30) * 0.35 + noise * Math.exp(-t * 18) * 0.35);
      }
      case 2: {  // hat: high-passed noise
        const hp = noise - this.last; this.last = noise;
        return hp * Math.exp(-t * 60) * 0.18;
      }
      default: {  // perc: short pitched blip with a little noise
        this.phase += 820 / this.rate;
        return (Math.sin(2 * Math.PI * this.phase) * 0.3 + noise * 0.08) * Math.exp(-t * 35);
      }
    }
  }
}

class IdumProcessor extends AudioWorkletProcessor {
  constructor(options) {
    super();
    this.e = new WebAssembly.Instance(options.processorOptions.module, {}).exports;
    this.e.emu_boot();
    const bankPtr = this.e.emu_bank() + 9;  // skip the IDUMBEAT1 marker
    this.builtInBank = this.mem().slice(bankPtr, bankPtr + 640);
    this.voices = [0, 1, 2, 3].map((k) => new Voice(k, sampleRate));
    this.mode = 'auto';
    this.mute = [false, false, false, false];
    this.volume = 0.7;
    this.prev = 0;
    this.dc = [0, 0];
    this.blocks = 0;
    this.masks = [];
    this.port.onmessage = (ev) => this.onMessage(ev.data);
    this.port.postMessage({ type: 'ready', bank: this.builtInBank });
  }

  mem() { return new Uint8Array(this.e.memory.buffer); }

  onMessage(m) {
    const e = this.e;
    if (m.type === 'gate') e.emu_set_gate(m.pin, m.high ? 1 : 0);
    else if (m.type === 'analog') e.emu_set_analog(m.pin, m.value);
    else if (m.type === 'clock') e.emu_set_clock(m.period, m.width);
    else if (m.type === 'bank') this.mem().set(m.bytes, e.emu_bank() + 9);
    else if (m.type === 'sound') { this.mode = m.mode; this.mute = m.mute; this.volume = m.volume; }
    else if (m.type === 'boot') e.emu_boot();
  }

  process(inputs, outputs) {
    const out = outputs[0];
    const n = out[0].length;
    const e = this.e;
    const ptr = e.emu_run(n, sampleRate);
    const bits = this.mem().subarray(ptr, ptr + n);
    const leds = e.emu_leds();
    const raw = this.mode === 'raw' || (this.mode === 'auto' && (leds & 32));
    let blockMask = 0;
    for (let i = 0; i < n; i++) {
      const b = bits[i];
      blockMask |= b;
      let s = 0;
      if (raw) {
        let level = 0;
        for (let c = 0; c < 4; c++) if (!this.mute[c]) level += (b >> c & 1) ? 0.12 : -0.12;
        // one-pole high-pass, like the AC coupling on a mixer input
        this.dc[0] = 0.999 * (this.dc[0] + level - this.dc[1]);
        this.dc[1] = level;
        s = this.dc[0];
      } else {
        const rising = b & ~this.prev;
        for (let c = 0; c < 4; c++) {
          if ((rising >> c & 1) && !this.mute[c]) this.voices[c].hit();
          s += this.voices[c].next();
        }
      }
      this.prev = b;
      s *= this.volume;
      for (let ch = 0; ch < out.length; ch++) out[ch][i] = s;
    }
    this.masks.push(blockMask);
    if (++this.blocks % STATE_EVERY === 0) {
      this.port.postMessage({ type: 'state', leds, masks: this.masks, micros: e.emu_micros(), hz: e.emu_out4_hz(), blockMs: 1000 * n / sampleRate });
      this.masks = [];
    }
    return true;
  }
}

registerProcessor('idum-emulator', IdumProcessor);
