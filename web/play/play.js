// Nova Arcade browser player. Each game is its own WebAssembly module
// (built by scripts/build-web.sh from the same C++ as the firmware).
// This page supplies the pad state (keyboard, gamepad or on-screen touch
// controls), draws the frame to a canvas and streams the game's synth
// through Web Audio.

const GAMES = [
  { id: "novalance", title: "NOVA LANCE", desc: "Side-scrolling synthwave shoot-'em-up with a boss every stage." },
  { id: "blockfall", title: "BLOCKFALL", desc: "Falling-blocks puzzler with hold, ghost piece and wall kicks." },
  { id: "brickstorm", title: "BRICK STORM", desc: "Brick-breaker with 8 stages and five kinds of power-up." },
  { id: "alientide", title: "ALIEN TIDE", desc: "Hold back the marching waves from behind your shields." },
  { id: "neonserpent", title: "NEON SERPENT", desc: "Eat, grow longer, and don't bite yourself." },
  { id: "astrodrift", title: "ASTRO DRIFT", desc: "Split the space rocks and dodge the hunter saucers." },
  { id: "hoprush", title: "HOP RUSH", desc: "Cross the road, ride the river, fill all five docks." },
  { id: "voltrally", title: "VOLT RALLY", desc: "Paddle tennis against a ladder of six CPU rivals." },
  { id: "mazemunch", title: "MAZE MUNCH", desc: "Clear the maze while four wisps hunt you down." },
];

// Button bits, matching BTN_* in lib/ArcadeCore/src/arcade/Input.h
const B = {
  UP: 1 << 0, DOWN: 1 << 1, LEFT: 1 << 2, RIGHT: 1 << 3, A: 1 << 4, B: 1 << 5, X: 1 << 6, Y: 1 << 7,
  L1: 1 << 8, R1: 1 << 9, L2: 1 << 10, R2: 1 << 11, START: 1 << 12, SELECT: 1 << 13, SYSTEM: 1 << 14,
};
const KEYS = {
  ArrowUp: B.UP, ArrowDown: B.DOWN, ArrowLeft: B.LEFT, ArrowRight: B.RIGHT,
  KeyZ: B.A, Space: B.A, KeyX: B.B, KeyA: B.X, KeyS: B.Y, KeyQ: B.L1, KeyW: B.R1,
  Enter: B.START, ShiftLeft: B.SELECT, ShiftRight: B.SELECT, Backspace: B.SELECT, Escape: B.SYSTEM,
};
// Standard gamepad mapping -> button bits
const PAD_BUTTONS = [B.A, B.B, B.X, B.Y, B.L1, B.R1, B.L2, B.R2, B.SELECT, B.START, 0, 0, B.UP, B.DOWN, B.LEFT, B.RIGHT, B.SYSTEM];

const $ = (id) => document.getElementById(id);
const canvas = $("canvas"), ctx2d = canvas.getContext("2d");
const image = ctx2d.createImageData(320, 240);
const overlay = $("overlay");

let game = null;        // the running wasm module
let current = null;     // its GAMES entry
let rafId = 0;
let keysHeld = 0;
let muted = false;
try { muted = localStorage.getItem("nova-arcade/player/muted") === "1"; } catch (e) {}

// ------------------------------------------------------------ library
function hiScore(id) {
  try {
    const vals = ["hi", "hiE", "hiH"].map((k) => parseInt(localStorage.getItem(`nova-arcade/${id}/${k}`) || "0", 10));
    return Math.max(...vals);
  } catch (e) { return 0; }
}

function renderLibrary() {
  const lib = $("library");
  lib.innerHTML = "";
  for (const g of GAMES) {
    const b = document.createElement("button");
    b.className = "card game";
    b.innerHTML = `<img src="../img/${g.id}.png" alt="" loading="lazy"><div class="body"><h3>${g.title}</h3><p>${g.desc}</p><div class="hi"></div></div>`;
    const hi = hiScore(g.id);
    // only show scores the player actually set (the defaults are never saved)
    if (hi) b.querySelector(".hi").textContent = `BEST ${String(hi).padStart(7, "0")}`;
    // start the audio inside the tap itself: iOS only unlocks sound in a user gesture
    b.addEventListener("click", () => { startAudio(); location.hash = g.id; });
    lib.appendChild(b);
  }
}

// ------------------------------------------------------------ audio
// the board's speaker runs the codec hot; a little extra gain matches it on a computer
const PAGE_GAIN = 2.0;
const audio = { ctx: null, node: null, gain: null, src: new Float32Array(1), pos: 0, rate: 22050 };

function startAudio() {
  if (!audio.ctx) {
    const AC = window.AudioContext || window.webkitAudioContext;
    if (!AC) return;
    audio.ctx = new AC();
    audio.gain = audio.ctx.createGain();
    audio.gain.connect(audio.ctx.destination);
    audio.node = audio.ctx.createScriptProcessor(2048, 0, 1);
    audio.node.onaudioprocess = (e) => fillAudio(e.outputBuffer.getChannelData(0));
    audio.node.connect(audio.gain);
  }
  audio.gain.gain.value = muted ? 0 : PAGE_GAIN;
  if (audio.ctx.state === "suspended") audio.ctx.resume();
}

// Pull samples from the game's synth at 22.05 kHz and resample linearly
// to whatever rate the audio device runs at.
function fillAudio(out) {
  if (!game) { out.fill(0); return; }
  const step = audio.rate / audio.ctx.sampleRate;
  for (let i = 0; i < out.length; i++) {
    let k = Math.floor(audio.pos);
    if (k + 1 >= audio.src.length) {
      const keep = audio.src.subarray(k);
      const n = 1024;
      const ptr = game._web_audio(n);
      const fresh = game.HEAPF32.subarray(ptr >> 2, (ptr >> 2) + n);
      const next = new Float32Array(keep.length + n);
      next.set(keep); next.set(fresh, keep.length);
      audio.src = next; audio.pos -= k; k = 0;
    }
    const f = audio.pos - k;
    out[i] = audio.src[k] * (1 - f) + audio.src[k + 1] * f;
    audio.pos += step;
  }
}

function setMuted(m) {
  muted = m;
  try { localStorage.setItem("nova-arcade/player/muted", m ? "1" : "0"); } catch (e) {}
  if (audio.gain) audio.gain.gain.value = m ? 0 : PAGE_GAIN;
  $("sound").innerHTML = m ? "&#128263;<span class=\"label\"> Sound off</span>" : "&#128266;<span class=\"label\"> Sound on</span>";
}

// ------------------------------------------------------------ input
let padIndex = -1;
function readPad() {
  let held = 0, ax = 0, ay = 0;
  const pads = navigator.getGamepads ? navigator.getGamepads() : [];
  let pad = null;
  for (const p of pads) if (p && p.connected) { pad = p; break; }
  const status = $("padStatus");
  if (pad && pad.index !== padIndex) {
    padIndex = pad.index;
    document.body.classList.add("pad-connected");   // a real pad takes over from the touch controls
    status.textContent = `Connected: ${pad.id.replace(/\(.*?\)/g, "").trim() || "gamepad"}`;
    status.classList.add("on");
  } else if (!pad && padIndex !== -1) {
    padIndex = -1;
    document.body.classList.remove("pad-connected");
    status.textContent = "No gamepad yet: plug one in or pair it, then press a button.";
    status.classList.remove("on");
  }
  if (pad) {
    pad.buttons.forEach((b, i) => { if (b && b.pressed && PAD_BUTTONS[i]) held |= PAD_BUTTONS[i]; });
    const dz = (v) => (Math.abs(v) < 0.18 ? 0 : Math.sign(v) * Math.min(1, (Math.abs(v) - 0.18) / 0.82));
    ax = dz(pad.axes[0] || 0);
    ay = dz(pad.axes[1] || 0);
  }
  held |= keysHeld | touchHeld();
  // like the hardware: the D-pad overrides the stick
  const dx = (held & B.RIGHT ? 1 : 0) - (held & B.LEFT ? 1 : 0);
  const dy = (held & B.DOWN ? 1 : 0) - (held & B.UP ? 1 : 0);
  if (dx) ax = dx;
  if (dy) ay = dy;
  return { held, ax, ay };
}

window.addEventListener("keydown", (e) => {
  if (!game) return;
  if (e.code === "KeyF" && !e.repeat) { toggleFullscreen(); return; }
  if (e.code === "KeyM" && !e.repeat) { setMuted(!muted); return; }
  const bit = KEYS[e.code];
  if (bit === undefined) return;
  e.preventDefault();
  keysHeld |= bit;
});
window.addEventListener("keyup", (e) => {
  const bit = KEYS[e.code];
  if (bit !== undefined) keysHeld &= ~bit;
});
window.addEventListener("blur", () => { keysHeld = 0; });

// ------------------------------------------------------------ touch controls
// Every finger on a control is tracked separately, so you can hold the D-pad
// and press A at the same time, or slide a thumb from one button to the next.
const touches = new Map();   // pointerId -> button bits
let touchMode = false;
function touchHeld() { let h = 0; for (const v of touches.values()) h |= v; return h; }

function setTouchMode(on) {
  if (on === touchMode) return;
  touchMode = on;
  document.body.classList.toggle("touch", on);
  lockZoom(touchPlaying());
  fitCanvas();
}

function showPressed() {
  const h = touchHeld();
  document.querySelectorAll(".dir").forEach((el) => el.classList.toggle("on", !!(h & B[el.dataset.d])));
  document.querySelectorAll(".fb, .sb").forEach((el) => el.classList.toggle("on", !!(h & B[el.dataset.b])));
}

function setTouch(id, bits) {
  const before = touches.get(id) || 0;
  if (bits) touches.set(id, bits); else touches.delete(id);
  if (bits & ~before && navigator.vibrate) navigator.vibrate(8);   // a little click on Android
  showPressed();
}

// D-pad: the direction comes from where the thumb is relative to the centre,
// with diagonals, so rolling the thumb round works like a real pad.
function dpadBits(e, el) {
  const r = el.getBoundingClientRect();
  const x = e.clientX - (r.left + r.width / 2), y = e.clientY - (r.top + r.height / 2);
  if (Math.hypot(x, y) < r.width * 0.12) return 0;
  const a = Math.atan2(y, x) * 180 / Math.PI;   // 0 = right, 90 = down
  let bits = 0;
  if (a > -67.5 && a < 67.5) bits |= B.RIGHT;
  if (a > 22.5 && a < 157.5) bits |= B.DOWN;
  if (a > 112.5 || a < -112.5) bits |= B.LEFT;
  if (a > -157.5 && a < -22.5) bits |= B.UP;
  return bits;
}
const dpadEl = $("dpadCtl");
dpadEl.addEventListener("pointerdown", (e) => { dpadEl.setPointerCapture(e.pointerId); setTouch(e.pointerId, dpadBits(e, dpadEl)); e.preventDefault(); });
dpadEl.addEventListener("pointermove", (e) => { if (touches.has(e.pointerId) || e.buttons) setTouch(e.pointerId, dpadBits(e, dpadEl)); });

// Face and system buttons: whichever button is under the finger is held.
function buttonAt(x, y) {
  const el = document.elementFromPoint(x, y);
  const b = el && el.closest && el.closest(".fb, .sb");
  return b ? B[b.dataset.b] : 0;
}
for (const id of ["abxy", "sys"]) {
  const el = $(id);
  el.addEventListener("pointerdown", (e) => { el.setPointerCapture(e.pointerId); setTouch(e.pointerId, buttonAt(e.clientX, e.clientY)); e.preventDefault(); });
  el.addEventListener("pointermove", (e) => { if (touches.has(e.pointerId)) setTouch(e.pointerId, buttonAt(e.clientX, e.clientY)); });
}
for (const ev of ["pointerup", "pointercancel", "lostpointercapture"])
  window.addEventListener(ev, (e) => { if (touches.has(e.pointerId)) setTouch(e.pointerId, 0); });
document.addEventListener("contextmenu", (e) => { if (touchMode && document.body.classList.contains("playing")) e.preventDefault(); });

// Phones and tablets get the touch controls; a desktop with a touch screen
// switches to them the first time it is touched. ?touch=1 forces them on.
const coarse = window.matchMedia ? window.matchMedia("(pointer: coarse)") : { matches: false };
window.addEventListener("pointerdown", (e) => { if (e.pointerType === "touch") setTouchMode(true); }, true);

// ------------------------------------------------------------ no zooming while playing
// iOS Safari ignores touch-action for double-tap zoom, and once it has zoomed
// in, our no-gesture rule also blocks the pinch that would zoom back out. So:
// cancel the second tap of a double tap, block Safari's pinch gestures, lock
// the viewport scale while a game runs, and snap back if a zoom slips through.
const touchPlaying = () => touchMode && document.body.classList.contains("playing");
const vpMeta = document.querySelector('meta[name="viewport"]');
const VP_FREE = vpMeta.content;                                         // the game list can still be zoomed
const VP_LOCKED = `${VP_FREE}, minimum-scale=1, maximum-scale=1, user-scalable=no`;
function lockZoom(on) { vpMeta.content = on ? VP_LOCKED : VP_FREE; }
function resetZoom() {
  if (!window.visualViewport || visualViewport.scale <= 1.01) return;
  // changing the viewport limits makes Safari drop back to 100%
  vpMeta.content = `${VP_FREE}, maximum-scale=1.0001`;
  requestAnimationFrame(() => { lockZoom(touchPlaying()); fitCanvas(); });
}
let lastTouchEnd = 0;
const inBar = (e) => e.target && e.target.closest && e.target.closest(".bar");
document.addEventListener("touchstart", (e) => {
  // the on-screen controls use pointer events, so cancelling the touch only stops browser gestures
  if (touchPlaying() && !inBar(e)) e.preventDefault();
}, { passive: false });
document.addEventListener("touchend", (e) => {
  if (!touchPlaying()) return;
  const now = performance.now();
  if (now - lastTouchEnd < 350 && !inBar(e)) e.preventDefault();   // second tap of a double tap
  lastTouchEnd = now;
  resetZoom();
}, { passive: false });
for (const ev of ["gesturestart", "gesturechange", "gestureend"])      // Safari's pinch events
  document.addEventListener(ev, (e) => { if (touchPlaying()) e.preventDefault(); }, { passive: false });
document.addEventListener("dblclick", (e) => { if (touchPlaying()) e.preventDefault(); }, { passive: false });
if (window.visualViewport) visualViewport.addEventListener("resize", () => { if (touchPlaying()) resetZoom(); });

// keep the phone from dimming while a game runs
let wakeLock = null;
async function keepAwake(on) {
  try {
    if (on && !wakeLock && navigator.wakeLock) { wakeLock = await navigator.wakeLock.request("screen"); wakeLock.addEventListener("release", () => { wakeLock = null; }); }
    else if (!on && wakeLock) { await wakeLock.release(); wakeLock = null; }
  } catch (e) {}
}

// ------------------------------------------------------------ screen
function fitCanvas() {
  const screen = $("screen");
  const full = document.fullscreenElement === screen;
  let availW, availH;
  if (full) { availW = window.innerWidth; availH = window.innerHeight; }
  else if (touchMode && document.body.classList.contains("playing")) {
    // leave room for the controls: below the screen in portrait, beside it in landscape
    const vw = window.innerWidth, vh = window.innerHeight;
    const landscape = vw > vh;
    // portrait: D-pad, START/SELECT and the face buttons share one row, so size them to the width
    const ctl = Math.round(landscape ? Math.max(120, Math.min(176, vh * 0.42))
                                     : Math.max(104, Math.min(176, (vw - 96) / 2.12)));
    document.documentElement.style.setProperty("--dpad", `${ctl}px`);
    document.documentElement.style.setProperty("--abxy", `${Math.round(ctl * 1.08)}px`);
    if (landscape) { availW = vw - 2 * (ctl + 24); availH = vh; }
    else { availW = vw; availH = vh - 44 - ctl - 90; }
  } else {
    availW = screen.clientWidth - 28;
    availH = Math.max(240, window.innerHeight - 170);
  }
  let scale = Math.min(availW / 320, availH / 240);
  // whole-number scaling keeps pixels square on big screens; phones use every pixel they have
  if (!(touchMode && !full)) scale = scale >= 1 ? Math.floor(scale) || 1 : scale;
  scale = Math.max(scale, 0.5);
  canvas.style.width = `${320 * scale}px`;
  canvas.style.height = `${240 * scale}px`;
}
window.addEventListener("resize", fitCanvas);
window.addEventListener("orientationchange", () => setTimeout(fitCanvas, 200));
document.addEventListener("fullscreenchange", fitCanvas);

function toggleFullscreen() {
  const screen = $("screen");
  if (document.fullscreenElement) document.exitFullscreen();
  else if (screen.requestFullscreen) screen.requestFullscreen();
}

function frame() {
  rafId = requestAnimationFrame(frame);
  if (!game) return;
  const { held, ax, ay } = readPad();
  game._web_pad(held >>> 0, ax, ay);
  const ptr = game._web_frame();
  if (!game) return;   // the game may have exited during this frame
  image.data.set(game.HEAPU8.subarray(ptr, ptr + 320 * 240 * 4));
  ctx2d.putImageData(image, 0, 0);
}

// ------------------------------------------------------------ switching games
async function launch(entry) {
  stopGame();
  current = entry;
  document.body.classList.add("playing");
  lockZoom(touchMode);
  resetZoom();
  keepAwake(true);
  $("title").textContent = entry.title;
  document.title = `${entry.title} - Nova Arcade Player`;
  overlay.hidden = false;
  overlay.textContent = "LOADING...";
  fitCanvas();
  startAudio();
  try {
    const factory = (await import(`./${entry.id}.js`)).default;
    const mod = await factory();
    if (current !== entry) return;   // the player picked something else meanwhile
    mod.onExit = () => { location.hash = ""; };
    mod._web_init((Math.random() * 4294967296) >>> 0);
    audio.rate = mod._web_audio_rate();
    audio.src = new Float32Array(1); audio.pos = 0;
    game = mod;
    overlay.hidden = true;
    canvas.focus();
    rafId = requestAnimationFrame(frame);
  } catch (err) {
    console.error(err);
    overlay.textContent = "COULD NOT LOAD THIS GAME";
  }
}

function stopGame() {
  cancelAnimationFrame(rafId);
  game = null;
  keysHeld = 0;
  touches.clear();
  showPressed();
  keepAwake(false);
}

function showLibrary() {
  stopGame();
  current = null;
  if (document.fullscreenElement) document.exitFullscreen();
  document.body.classList.remove("playing");
  lockZoom(false);
  document.title = "Nova Arcade Player";
  window.scrollTo(0, 0);
  renderLibrary();
}

function route() {
  const id = location.hash.replace("#", "");
  const entry = GAMES.find((g) => g.id === id);
  if (entry) launch(entry); else showLibrary();
}

$("back").addEventListener("click", () => { location.hash = ""; });
$("full").addEventListener("click", toggleFullscreen);
$("sound").addEventListener("click", () => { startAudio(); setMuted(!muted); });
document.addEventListener("visibilitychange", () => {
  if (!document.hidden && game) keepAwake(true);   // the lock is dropped whenever the page is hidden
  if (!audio.ctx) return;
  if (document.hidden) audio.ctx.suspend(); else if (game) audio.ctx.resume();
});
// browsers only allow sound after a tap, click or key press on the page
// (iOS Safari only counts touchend and click, not touchstart or pointerdown)
for (const ev of ["pointerdown", "touchend", "click", "keydown"])
  window.addEventListener(ev, () => { if (game) startAudio(); }, true);
// iPhone Safari has no full screen API for pages: hide the button there
// (adding the page to the home screen gives a full-screen app instead)
if (!document.fullscreenEnabled) $("full").style.display = "none";
window.addEventListener("hashchange", route);
// handy from the dev tools console: novaPlayer.game is the running wasm module
window.novaPlayer = { get game() { return game; }, get audio() { return audio; } };
setMuted(muted);
setTouchMode(coarse.matches || /[?&]touch=1/.test(location.search));
route();
