// flash.js — drive a full USB flash of a Malwarium from the browser.
//
// Four images at four fixed addresses, which together are a whole working
// device: bootloader, partition table, OTA slot selector, app. That is the same
// set `pio run -t upload` writes, and it is deliberately the WHOLE set rather
// than just the app — the two changes an over-the-air update structurally cannot
// make are a new bootloader and a new partition table, so the page that exists
// for "OTA can't do this one" has to be able to make both.
//
// The addresses are the ESP32-S3's, not a choice this file gets to make: the
// bootloader lives at 0 (0x1000 is the original ESP32 and the S2), the table at
// 0x8000, and the Arduino build hardcodes boot_app0 to 0xE000 and the app to
// 0x10000 — see partitions_malwarium.csv, which explains why the first three
// partitions are pinned to their stock offsets for exactly this reason.
//
// WHAT THIS FILE DOES NOT DO: talk to the chip. `../vendor/esptool-js/bundle.js`
// is Espressif's own loader and owns every byte of the ROM protocol. This is the
// operator's half — pick a port, fetch the images, prove they arrived intact,
// report what is happening, and say something useful when it doesn't work.

import { ESPLoader, Transport } from "../vendor/esptool-js/bundle.js";

// The three images that don't move with a release. Their names are fixed because
// the publish (`make pages`) stages them under exactly these names beside the
// manifest; the app's filename carries its version, so it comes from the manifest.
const kStaticParts = [
  { file: "bootloader.bin", addr: 0x0000, label: "bootloader" },
  { file: "partitions.bin", addr: 0x8000, label: "partition table" },
  { file: "boot_app0.bin",  addr: 0xe000, label: "OTA slot selector" },
];
const kAppAddr = 0x10000;

// Espressif's USB vendor ID — every board this firmware targets enumerates under
// it, so filtering on it turns Chrome's port picker from "every serial device on
// this machine" into "the thing you just plugged in". The unfiltered request is
// still one button away, because a filter that hides the device is worse than a
// list that is too long.
const kVendorEspressif = 0x303a;

// The chip the images are built for. Writing an S3 bootloader onto a different
// ESP32 does not damage it, but it does produce a board that no longer boots and
// an operator with no idea why — so the mismatch is caught before the first write.
const kChipName = "ESP32-S3";

// The ROM speaks at 115200 and this board's serial port is the S3's native USB,
// where the baud rate is a fiction: the bytes move at USB speed whatever number
// is asked for. Matching the ROM's rate means the loader skips its baud-change
// step, which on a native-USB part is a port reopen bought for nothing.
const kBaud = 115200;

const $ = (id) => document.getElementById(id);

// --- page state -------------------------------------------------------------

let app = null;      // the firmware artifact from the manifest
let busy = false;

// --- small helpers ----------------------------------------------------------

const kib = (n) => (n < 1024 * 1024)
  ? `${(n / 1024).toFixed(0)} KB`
  : `${(n / 1024 / 1024).toFixed(2)} MB`;

function log(line, cls) {
  const el = $("log");
  const span = document.createElement("span");
  if (cls) span.className = cls;
  span.textContent = line + "\n";
  el.appendChild(span);
  el.scrollTop = el.scrollHeight;
}

function status(text, kind) {
  $("progress-card").hidden = false;
  $("status").className = `status ${kind}`;
  $("status-text").textContent = text;
}

function progress(frac, detail) {
  const pct = Math.max(0, Math.min(100, Math.round(frac * 100)));
  $("fill").style.width = `${pct}%`;
  $("pct").textContent = `${pct}%`;
  if (detail !== undefined) $("detail").textContent = detail;
}

async function sha256Hex(bytes) {
  const digest = await crypto.subtle.digest("SHA-256", bytes);
  return Array.from(new Uint8Array(digest))
    .map((b) => b.toString(16).padStart(2, "0")).join("");
}

// Fetch one image, reporting as it arrives. `onChunk` gets bytes-so-far and the
// declared total (0 when the server sends no length, which the caller treats as
// an indeterminate leg rather than a failure).
async function fetchImage(url, onChunk) {
  const res = await fetch(url, { cache: "no-store" });
  if (!res.ok) throw new Error(`${url.split("/").pop()} — the server answered ${res.status}`);
  const total = Number(res.headers.get("content-length")) || 0;
  const chunks = [];
  let seen = 0;
  const reader = res.body.getReader();
  for (;;) {
    const { done, value } = await reader.read();
    if (done) break;
    chunks.push(value);
    seen += value.length;
    onChunk(seen, total);
  }
  const out = new Uint8Array(seen);
  let at = 0;
  for (const c of chunks) { out.set(c, at); at += c.length; }
  return out;
}

// --- the manifest -----------------------------------------------------------

// The same file the device reads on CFG -> UPDATES -> CHECK NOW, so the version
// this page offers and the version a device offers itself can never disagree.
//
// Only the artifact's FILENAME is taken from it, never its URL. The manifest
// embeds absolute addresses built for one publish host, and this page is served
// from whatever host is actually holding it — a fork's Pages site, a laptop, a
// local copy. Resolving the name against this page's own origin means the
// flasher always fetches the artifacts sitting next to it.
async function loadManifest() {
  const res = await fetch("../manifest.json", { cache: "no-store" });
  if (!res.ok) throw new Error(`the manifest answered ${res.status}`);
  const manifest = await res.json();
  const fw = (manifest.artifacts || []).find((a) => a.id === "firmware");
  if (!fw) throw new Error("the manifest lists no firmware artifact");
  return {
    version: fw.version,
    file: fw.url.split("/").pop(),
    size: fw.size,
    sha256: (fw.sha256 || "").toLowerCase(),
  };
}

function renderParts() {
  $("fw-stamp").textContent = `firmware v${app.version}`;
  $("fw-count").textContent = "4 images";
  const rows = [
    ...kStaticParts.map((p) => [p.label, p.file]),
    ["firmware", `${app.file} · ${kib(app.size)}`],
  ];
  $("parts").innerHTML = rows.map(([k, v]) =>
    `<div class="kv"><span class="k">${k}</span><span class="v">${v}</span></div>`).join("");
  $("go").disabled = false;
  $("go").textContent = `FLASH v${app.version}`;
}

// --- the flash --------------------------------------------------------------

// A port the operator picked, or null when they dismissed the picker — which is
// a decision, not a failure, and must not be dressed up as one.
async function pickPort(filtered) {
  try {
    return await navigator.serial.requestPort(
      filtered ? { filters: [{ usbVendorId: kVendorEspressif }] } : {});
  } catch (e) {
    if (e && e.name === "NotFoundError") return null;
    throw e;
  }
}

async function downloadAll() {
  // Weighted by size so the bar tracks bytes rather than files — the app is two
  // orders of magnitude bigger than the three images in front of it, and a bar
  // that jumps to 75% and then sits there is a bar that looks stuck.
  const jobs = [
    ...kStaticParts.map((p) => ({ ...p, url: `../${p.file}`, weight: 20 * 1024 })),
    { file: app.file, addr: kAppAddr, label: "firmware", url: `../${app.file}`, weight: app.size },
  ];
  const totalWeight = jobs.reduce((a, j) => a + j.weight, 0);
  const fileArray = [];
  let done = 0;

  for (const job of jobs) {
    status(`Downloading ${job.label}…`, "busy");
    const data = await fetchImage(job.url, (seen, total) => {
      const frac = total ? seen / total : 0;
      progress((done + frac * job.weight) / totalWeight,
               `${job.file} — ${kib(seen)}${total ? ` of ${kib(total)}` : ""}`);
    });
    done += job.weight;
    log(`fetched ${job.file} (${data.length} bytes)`);
    fileArray.push({ data, address: job.addr });
  }

  // Integrity, before anything is written. The manifest publishes a SHA-256 over
  // the firmware image and this is the same check the device makes on itself
  // during an over-the-air install — a corrupted download should cost a retry,
  // never a half-written app slot.
  status("Checking the firmware…", "busy");
  const got = await sha256Hex(fileArray[fileArray.length - 1].data);
  if (app.sha256 && got !== app.sha256) {
    log(`expected ${app.sha256}`, "hot");
    log(`     got ${got}`, "hot");
    throw new Error("the firmware that downloaded doesn't match the digest the manifest " +
                    "publishes for it. Nothing was written. Reload the page and try again — " +
                    "if it happens twice, the published file is the problem, not your machine.");
  }
  log(`sha-256 ok — ${got}`, "lit");
  return fileArray;
}

async function flash(filtered) {
  if (busy) return;

  let port = null, transport = null, loader = null;
  const eraseAll = $("erase").checked;

  try {
    port = await pickPort(filtered);
    if (!port) return;                     // picker dismissed; say nothing

    busy = true;
    $("go").disabled = true;
    $("unfiltered").hidden = false;
    $("result-ok").hidden = true;
    $("result-err").hidden = true;
    $("log-wrap").open = true;
    $("log").textContent = "";
    progress(0, "");

    const fileArray = await downloadAll();

    status("Waking the chip…", "busy");
    progress(0, "");
    transport = new Transport(port, false);
    loader = new ESPLoader({
      transport,
      baudrate: kBaud,
      terminal: {
        clean() { $("log").textContent = ""; },
        writeLine(data) { log(data); },
        write(data) {
          // The loader streams its dots and partial lines through here; folding
          // them onto the tail keeps "Connecting....." one line instead of six.
          const el = $("log");
          const last = el.lastChild;
          if (last && last.nodeName === "SPAN" && !last.textContent.endsWith("\n"))
            last.textContent += data;
          else log(data.replace(/\n$/, ""));
        },
      },
    });

    // `default_reset` picks the USB-JTAG-serial sequence on this board, which is
    // what recovers a device whose A button was released too early. Holding A is
    // still the instruction the page gives, because that path works even when the
    // installed firmware is the reason the device is unreachable.
    const chip = await loader.main();
    log(`chip: ${chip}`, "lit");
    if (!loader.chip || loader.chip.CHIP_NAME !== kChipName)
      throw new Error(`that board is ${loader.chip ? loader.chip.CHIP_NAME : "an unknown chip"}, ` +
                      `and this firmware is built for the ${kChipName}. Nothing was written.`);

    const totalBytes = fileArray.reduce((a, f) => a + f.data.length, 0);
    const before = fileArray.map((_, i) =>
      fileArray.slice(0, i).reduce((a, f) => a + f.data.length, 0));

    if (eraseAll) status("Erasing the whole chip — this takes a while…", "busy");
    else status("Writing…", "busy");

    await loader.writeFlash({
      fileArray,
      // Every image already carries the flash mode, frequency and size its own
      // build chose; `keep` writes them through untouched rather than second-
      // guessing a header that is correct by construction.
      flashMode: "keep",
      flashFreq: "keep",
      flashSize: "keep",
      eraseAll,
      compress: true,
      reportProgress(index, written, total) {
        const frac = total ? written / total : 0;
        const bytes = before[index] + frac * fileArray[index].data.length;
        const label = index < kStaticParts.length ? kStaticParts[index].label : "firmware";
        status(`Writing ${label}…`, "busy");
        progress(bytes / totalBytes, `${kib(bytes)} of ${kib(totalBytes)}`);
      },
    });

    progress(1, `${kib(totalBytes)} written`);
    status("Flashed.", "ok");
    $("result-ok").hidden = false;

    // Best-effort: the board reboots out of download mode and re-enumerates as a
    // different USB device, so the handle this page holds dies either way. The
    // operator is told to replug regardless, which is why a failure here is a log
    // line and not an error.
    try { await loader.after("hard_reset", true); }
    catch { log("(the board rebooted before it could be reset from here — replug it)"); }

  } catch (e) {
    const msg = (e && e.message) ? e.message : String(e);
    log(msg, "hot");
    status("Stopped.", "err");
    $("err-text").textContent = friendlyError(msg);
    $("result-err").hidden = false;
  } finally {
    busy = false;
    $("go").disabled = false;
    try { if (transport) await transport.disconnect(); } catch { /* already gone */ }
  }
}

// The loader's own failures are accurate and unhelpful in equal measure, so the
// two that operators actually hit get the sentence that fixes them appended.
// Anything else is passed through as-is rather than guessed at.
function friendlyError(msg) {
  if (/Failed to connect/i.test(msg))
    return `${msg} — this is almost always the board running the game instead of listening. ` +
           `Unplug it, hold A, plug it back in, and press FLASH again.`;
  if (/Failed to open serial port|device has been lost|The port is already open/i.test(msg))
    return `${msg} — something else may have the port open (a serial monitor, the Arduino IDE). ` +
           `Close it, replug the device holding A, and try again.`;
  return msg;
}

// --- boot -------------------------------------------------------------------

// Two ways to have no Web Serial and only one of them is the browser's fault, so
// they are told apart before either message is shown — "use Chrome" is no help to
// someone already in Chrome on an http:// address.
if (!window.isSecureContext) {
  $("gate-insecure").hidden = false;
} else if (!("serial" in navigator)) {
  $("gate-browser").hidden = false;
  $("gate-url").textContent = location.href;
} else {
  $("app").hidden = false;
  $("go").addEventListener("click", () => flash(true));
  $("unfiltered").addEventListener("click", () => flash(false));
  loadManifest().then((fw) => { app = fw; renderParts(); }).catch((e) => {
    $("fw-stamp").textContent = "no manifest";
    $("parts").innerHTML =
      `<div class="kv"><span class="k">error</span><span class="v">${e.message}</span></div>`;
    status("There is nothing published to flash from here.", "err");
    $("detail").textContent =
      "This page reads the same artifact list the device does. If it isn't there, no release " +
      "has been published to this host yet.";
  });
}
