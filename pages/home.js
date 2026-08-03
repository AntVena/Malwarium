// home.js — fill the landing page from the artifact list it is published beside.
//
// Nothing here is authored: the versions and the manifest address are read out of
// the same manifest.json a device fetches, so the page cannot claim a release
// that was never published, and a fork's copy describes the fork's own host.

const $ = (id) => document.getElementById(id);

const kib = (n) => (n < 1024 * 1024)
  ? `${(n / 1024).toFixed(0)} KB`
  : `${(n / 1024 / 1024).toFixed(2)} MB`;

// What each artifact id IS, in the operator's words. The manifest names them for
// the device's parser; a landing page owes a reader something better than "web".
const kNames = {
  firmware: "FIRMWARE",
  web: "'PEDIA SITE",
};

// The manifest sits beside this page, so its address is this page's own — which
// is also the address to paste into the device's setup portal, and the reason it
// is derived rather than written down. A URL typed into two files is a URL that
// eventually disagrees with itself.
const manifestUrl = new URL("manifest.json", location.href).href;
$("manifest-url").textContent = manifestUrl;

fetch(manifestUrl, { cache: "no-store" })
  .then((res) => {
    if (!res.ok) throw new Error(`the manifest answered ${res.status}`);
    return res.json();
  })
  .then((manifest) => {
    const artifacts = manifest.artifacts || [];
    const fw = artifacts.find((a) => a.id === "firmware");
    $("stamp").textContent = fw ? `firmware v${fw.version}` : "nothing published";
    $("published").innerHTML = artifacts.map((a) =>
      `<div class="kv">
         <span class="k">${kNames[a.id] || a.id.toUpperCase()}</span>
         <span class="v lit">v${a.version} <span class="dim">&middot; ${kib(a.size)}</span></span>
       </div>`).join("");
  })
  .catch((e) => {
    $("stamp").textContent = "no manifest";
    $("published").innerHTML =
      `<div class="kv"><span class="k">NOTHING PUBLISHED YET</span>
         <span class="v">${e.message}</span></div>`;
  });
