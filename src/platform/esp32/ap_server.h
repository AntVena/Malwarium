// ap_server.h — device-tier 'Pedia local Access Point.
//
// Stands up a standalone 2.4GHz SoftAP (no internet) plus a tiny HTTP server
// that serves the web 'Pedia. A phone/PC joins the "Malwarium" network and
// browses http://192.168.4.1 to see the roster/archive/HackerTag. It serves the
// real static /web bundle straight off the SD card
// (mirrors web/ in the repo — see web/README.md) plus the live JSON state +
// the two write endpoints app.js expects; a card-less/SD-absent board falls
// back to the inline terminal landing page so it still shows
// SOMETHING ("SD absent → minimal fallback").
//
// Deliberately first-cut: files are read fresh off SD on every request (no
// RAM cache), so updating the SD's /web folder shows up on the very next
// request — no reboot needed to see a new bundle (only toggling the AP off/on
// re-triggers the heavier SoftAP bring-up, which is unrelated to file content).
//
// Radio ownership: like NetSniff/NetCapture this is a RadioArbiter owner, and it
// outranks everything except an on-the-spot connect request (net_sta.h) — the
// player explicitly turned on a user-facing service, so it preempts the audit
// scan/capture. wants() reads the persisted CFG opt-in (Game::apEnabled, save v20);
// the arbiter services this owner while it wants the radio and stands it down
// (handing the radio back to a still-enabled audit toggle) the moment the player
// switches the AP off. SoftAP and promiscuous capture cannot coexist, so this
// mutual exclusion is mandatory, not just tidy.
//
// SETUP PORTAL. The AP is also where the operator's home network gets provisioned,
// because the alternative is entering a WPA2 passphrase with three buttons — which
// is not a thing anyone should be asked to do. A phone joins this AP, the captive-
// portal DNS lands it on /setup, it picks a network from a live scan and types the
// passphrase on a real keyboard, and net_credentials.h stores it. The page is
// INLINE, not part of the SD /web bundle: provisioning has to work on a device with
// no card in it. RE-provisioning self-presents the same way — see handleProbe for
// what tells a device that has moved house apart from one that is simply offline.
//
// Storing credentials deliberately does NOT connect. The phone submitting the form
// is attached to THIS AP, and joining a home network tears the AP down — so the
// association is a separate act, raised later by an update job that needs it
// (net_sta.h). Scanning, unlike joining, is compatible with the AP: the
// portal flips the radio to WIFI_AP_STA so the STA side can sweep while the SoftAP
// keeps serving the phone.
//
// Whether it is compiled at all is the AP_ENABLED board-capability gate (config.h);
// on a radio-less board it stubs out to nothing. The setup half additionally needs
// STA_ENABLED (there is nothing to provision on a board that cannot associate).
#pragma once

#include <cstdint>
#include <cstdio>

#include "config.h"
#include "core/app/game.h"
#include "core/app/pedia_state.h"
#include "core/content/defs.h"
#include "platform/esp32/net_credentials.h"
#include "platform/esp32/update_source.h"

#if AP_ENABLED
#  include <DNSServer.h>
#  include <WiFi.h>
#  include <WebServer.h>
#endif

namespace mal {

class ApServer {
public:
    // Wire the engine seam. Does NOT power the radio — the RadioArbiter decides
    // when this owner may run (service) vs must stand down (ensureDown).
    void begin(Game* game) { game_ = game; }

    // Whether the player wants the AP up (the runtime CFG toggle). The arbiter
    // reads this to pick the radio owner.
    bool wants() const {
#if AP_ENABLED
        return game_ && game_->apEnabled();
#else
        return false;
#endif
    }

    // Run one AP step, ASSUMING the arbiter has granted this owner the radio.
    // Brings up SoftAP + the HTTP server the first time, then just services
    // pending clients each call. Non-blocking.
    void service(uint32_t nowMs) {
#if AP_ENABLED
        (void)nowMs;
        if (!game_) return;
        if (!up_) {
            WiFi.mode(WIFI_AP);
            // Empty AP_PASSWORD → open network (nullptr); >=8 chars → WPA2.
            const char* pw = (AP_PASSWORD[0] != '\0') ? AP_PASSWORD : nullptr;
            WiFi.softAP(AP_SSID, pw, AP_CHANNEL);
            const IPAddress ip = WiFi.softAPIP();
            // ROUTES ARE REGISTERED ONCE, EVER. WebServer::on() appends to a handler
            // list that only its destructor frees — stop() closes the socket and
            // leaves the list alone — so registering them per bring-up leaked every
            // handler and its captured lambda on each AP off→on cycle (measured at
            // ~1.9KB a cycle, monotonic, never recovered). The list survives
            // stop()/begin(), so one registration serves every later bring-up.
            if (!routesRegistered_) {
                server_.on("/", HTTP_GET, [this]() { handleStaticOrFallback("/"); });
                // The on-device QR + CFG screen advertise.../pedia,
                // but the SPA is a hash-routed single page served at index.html — so
                // /pedia is an ALIAS for the entry point, not a file on the card.
                server_.on("/pedia", HTTP_GET, [this]() { handleStaticOrFallback("/pedia"); });
                server_.on("/pedia_state.json", HTTP_GET, [this]() { handlePediaStateJson(); });
                server_.on("/api/tag", HTTP_POST, [this]() { handleApiTag(); });
#if UPDATE_ENABLED
                server_.on("/api/update-source", HTTP_POST,
                           [this]() { handleUpdateSourcePost(); });
#endif
                server_.on("/api/achievement/DEVTOOLS_INTRUDER", HTTP_POST,
                           [this]() { handleDevToolsAchievement(); });
#if STA_ENABLED
                // The setup portal: the page, the network list it populates from, and
                // the write that stores what the operator picked.
                server_.on("/setup", HTTP_GET, [this]() { handleSetupPage(); });
                server_.on("/api/networks", HTTP_GET, [this]() { handleNetworkList(); });
                server_.on("/api/wifi", HTTP_POST, [this]() { handleWifiPost(); });
                // Captive-portal probes. Phones/laptops fetch one of these expecting a
                // specific tiny response; anything else — a redirect above all — is read
                // as "a portal is in the way", which pops the sign-in sheet and keeps the
                // OS re-probing instead of treating the network as usable.
                //
                // So the answer depends on whether there is anything left to set up. On
                // an UNPROVISIONED device the redirect is the point: the setup page has
                // to present itself to someone who was never told to type an address. Once
                // a network is stored there is nothing to sign in to, and a device that
                // keeps claiming otherwise fights the 'Pedia — the whole reason the
                // operator joined this AP — so the probes get the plain success each OS
                // is looking for. The wildcard DNS below stays either way, so any typed
                // hostname still lands here.
                server_.on("/generate_204", HTTP_GET, [this]() { handleProbe(204, "", ""); });
                server_.on("/gen_204", HTTP_GET, [this]() { handleProbe(204, "", ""); });
                server_.on("/hotspot-detect.html", HTTP_GET, [this]() {
                    handleProbe(200, "text/html",
                                "<HTML><HEAD><TITLE>Success</TITLE></HEAD>"
                                "<BODY>Success</BODY></HTML>");
                });
                server_.on("/ncsi.txt", HTTP_GET,
                           [this]() { handleProbe(200, "text/plain", "Microsoft NCSI"); });
                server_.on("/connecttest.txt", HTTP_GET,
                           [this]() { handleProbe(200, "text/plain", "Microsoft Connect Test"); });
#endif
                // Everything else (style.css, app.js, data/*.js, assets/*.png, …)
                // resolves through the same static-file path.
                server_.onNotFound([this]() { handleStaticOrFallback(server_.uri()); });
                routesRegistered_ = true;
            }
            server_.begin();
#if STA_ENABLED
            // Answer every DNS question with our own address, so whatever hostname
            // the phone probes resolves here and the portal fires.
            dns_.setErrorReplyCode(DNSReplyCode::NoError);
            dns_.start(53, "*", ip);
#endif
            up_ = true;
            Serial.printf("[ap] up ssid=%s ip=%s %s free=%u largest=%u\n", AP_SSID,
                          ip.toString().c_str(), pw ? "wpa2" : "open",
                          (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getMaxAllocHeap());
        }
#if STA_ENABLED
        dns_.processNextRequest();
#endif
        server_.handleClient();
#else
        (void)nowMs;
#endif
    }

    // Fully tear the AP + server down (arbiter hands the radio to another owner,
    // or the player switched the AP off). Idempotent; safe to call every loop.
    void ensureDown() {
#if AP_ENABLED
        if (up_) {
#if STA_ENABLED
            dns_.stop();
            // A scan left in flight would otherwise hold results (and a mode) across
            // the hand-off to whichever owner takes the radio next.
            if (scanning_) { WiFi.scanDelete(); scanning_ = false; }
#endif
            server_.stop();
            WiFi.softAPdisconnect(true);
            WiFi.mode(WIFI_OFF);
            up_ = false;
            Serial.printf("[ap] down free=%u largest=%u\n",
                          (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getMaxAllocHeap());
        }
#endif
    }

    bool up() const {
#if AP_ENABLED
        return up_;
#else
        return false;
#endif
    }

private:
#if AP_ENABLED
    // Resolve `uri` (a request path, no query string — WebServer.uri() already
    // strips it, but a caller-supplied path is defended against too) against the
    // SD-served /web bundle at /sdcard/web/<path>, "/" → index.html. Streams the
    // file straight off SD in small chunks (no RAM cache — see the file banner).
    // Falls back to the inline handleRoot() terminal page ONLY for the index
    // page when SD is absent or the file is missing; any other missing asset is
    // a plain 404 (so a partially-populated SD is still debuggable).
    void handleStaticOrFallback(String uri) {
        const int q = uri.indexOf('?');
        if (q >= 0) uri = uri.substring(0, q);
        // Root and the advertised /pedia alias both resolve to the SPA entry
        // (index.html); real assets keep their own paths (style.css, /data/…).
        if (uri.length() == 0 || uri == "/" || uri == "/pedia" || uri == "/pedia/")
            uri = "/index.html";

        FILE* f = nullptr;
        if (game_->sdStatus().present) {
            const String path = String("/sdcard/web") + uri;
            f = std::fopen(path.c_str(), "rb");
        }
        if (!f) {
            if (uri == "/index.html") { handleRoot(); return; }
            server_.send(404, "text/plain", "not found");
            return;
        }

        std::fseek(f, 0, SEEK_END);
        const long size = std::ftell(f);
        std::fseek(f, 0, SEEK_SET);

        server_.setContentLength(size > 0 ? static_cast<size_t>(size) : 0);
        server_.send(200, mimeType(uri), "");
        char buf[1024];
        size_t n;
        while ((n = std::fread(buf, 1, sizeof(buf), f)) > 0)
            server_.sendContent(buf, n);
        std::fclose(f);
    }

    // Extension → Content-Type, the handful web/ actually ships (the
    // bundle contents: html/css/js/json/png/fonts).
    static const char* mimeType(const String& path) {
        if (path.endsWith(".html")) return "text/html";
        if (path.endsWith(".css")) return "text/css";
        if (path.endsWith(".js")) return "application/javascript";
        if (path.endsWith(".json")) return "application/json";
        if (path.endsWith(".png")) return "image/png";
        if (path.endsWith(".ttf")) return "font/ttf";
        if (path.endsWith(".woff2")) return "font/woff2";
        return "application/octet-stream";
    }

    // GET /pedia_state.json — the live state payload app.js prefers over its
    // embedded fixture (web/fixtures/pedia_state.js). Built fresh from the live
    // Game every request (core/app/pedia_state.h) — never cached.
    void handlePediaStateJson() {
        const std::string json = buildPediaStateJson(*game_);
        server_.send(200, "application/json", json.c_str());
    }

    // POST /api/tag {"tag":"NEWTAG"} — the one safe write: rename the
    // HackerTag. A tiny hand-rolled JSON-value extraction (no library) is fine
    // here — the only field is a short, already-charset-restricted string.
    void handleApiTag() {
        const String body = server_.arg("plain");
        const String tag = extractJsonStringValue(body, "tag");
        if (tag.length() == 0 || !game_->setHackerTag(tag.c_str())) {
            server_.send(400, "text/plain", "invalid tag");
            return;
        }
        server_.send(200, "text/plain", "ok");
    }

#if UPDATE_ENABLED
    // POST /api/update-source (form-encoded: url) — repoint this device at a
    // different manifest, from the 'Pedia's user section.
    //
    // THE REASON THIS IS A UI AT ALL: the compiled-in default lives in a firmware
    // image that only changes via the mechanism it points at, so a device whose
    // publish host moved could never be told where the new one is. Someone who
    // cannot reflash a board can still join its AP and paste a URL, which is the
    // whole point — this is the recovery path, not a convenience.
    //
    // Form-encoded for the same reason /api/wifi is: this is arbitrary operator-
    // typed text, and the WebServer's own arg() parsing handles percent-escapes
    // properly where the hand-rolled JSON extractor below has no escape handling.
    //
    // An empty url CLEARS the override and falls back to the compiled default,
    // so a bad paste is recoverable from the same field that caused it.
    void handleUpdateSourcePost() {
        const String url = server_.arg("url");
        if (url.length() == 0) {
            source_.clear();
        } else if (!source_.store(url.c_str())) {
            server_.send(400, "text/plain",
                         "needs a full http:// or https:// address");
            return;
        }
        // Re-resolve rather than echoing what was posted: a cleared override
        // reports the compiled default, which is what the field should now show.
        char resolved[UpdateSource::kUrlCap];
        if (!source_.url(resolved, sizeof(resolved))) resolved[0] = '\0';
        game_->setUpdateManifestUrl(resolved);
        Serial.printf("[upd] source set to %s\n", resolved[0] ? resolved : "(none)");
        server_.send(200, "text/plain", resolved);
    }
#endif

    // POST /api/achievement/DEVTOOLS_INTRUDER — the DevTools honeytoken callback
    // Now persisted (save v25) via the real achievement
    // seam — a later /pedia_state.json fetch reports it "complete".
    void handleDevToolsAchievement() {
        if (game_) game_->unlockAchievement(ach::kDevtoolsIntruder);
        Serial.println("[pedia] DEVTOOLS_INTRUDER hit — persisted");
        server_.send(200, "text/plain", "ok");
    }

#if STA_ENABLED
    // Answer one OS connectivity probe: 302 to /setup when this device wants
    // provisioning (so the portal self-presents on the phone that just joined),
    // otherwise the `status`/`body` that probe expects — which is a small lie that
    // buys a lot, since it stops the OS flagging the AP as dead and wandering off
    // mid-'Pedia.
    //
    // Two devices want provisioning. One has no stored network at all. The other has
    // one that no longer works — the moved-house case, where credentials exist and
    // are simply wrong now; without this it would answer probes happily, no portal
    // would pop, and re-provisioning would mean knowing to type /setup by hand.
    // A failed association is the only honest signal for that, and an update check
    // is what surfaces it: UPDATES → CHECK NOW joins before it fetches.
    //
    // Reading the live NetStatus makes this THIS BOOT's evidence — a power cycle
    // clears it back to Idle and the portal stops presenting until a check is run
    // again. That is the right trade rather than persisting a "last attempt failed"
    // bit: the retry is one screen away and always tells the truth about now, where
    // a stored verdict would go stale the moment the network came back.
    void handleProbe(int status, const char* mime, const char* body) {
        const bool wantsSetup =
            !creds_.provisioned() ||
            (game_ && game_->netStatus().state == NetState::Failed);
        if (wantsSetup) {
            server_.sendHeader("Location", "http://192.168.4.1/setup");
            server_.send(302, "text/plain", "");
            return;
        }
        server_.send(status, mime[0] ? mime : "text/plain", body);
    }

    // GET /api/networks — what the radio can hear right now, as JSON, for the
    // setup page's picker. The scan runs ASYNC and this endpoint is polled: the
    // first call kicks a sweep off and answers `{"scanning":true}`, later calls
    // return the results once they land. Blocking for the 2-3s a sweep takes
    // would stall the ~4fps render loop, same reason net_sniff.h is async.
    //
    // WIFI_AP_STA, not WIFI_STA: a scan needs the station interface, and dropping
    // the SoftAP to get it would disconnect the very phone that asked. Scanning
    // alongside an AP is fine; ASSOCIATING is not, which is why joining lives in
    // net_sta.h behind its own request instead of happening here.
    void handleNetworkList() {
        if (WiFi.getMode() != WIFI_AP_STA) WiFi.mode(WIFI_AP_STA);
        if (!scanning_) {
            WiFi.scanDelete();               // drop the previous sweep's results
            WiFi.scanNetworks(/*async=*/true, /*show_hidden=*/false);
            scanning_ = true;
            server_.send(200, "application/json", "{\"scanning\":true,\"networks\":[]}");
            return;
        }
        const int n = WiFi.scanComplete();
        if (n == WIFI_SCAN_RUNNING || n < 0) {
            // Negative also covers the window where the sweep has stopped but the
            // scan-done event hasn't dispatched yet (see net_sniff.h) — keep polling
            // rather than deleting results that are about to arrive.
            server_.send(200, "application/json", "{\"scanning\":true,\"networks\":[]}");
            return;
        }
        scanning_ = false;
        String out = F("{\"scanning\":false,\"networks\":[");
        for (int i = 0; i < n; ++i) {
            if (i) out += ',';
            out += F("{\"ssid\":\"");
            out += jsonEscape(WiFi.SSID(i));
            out += F("\",\"rssi\":");
            out += String(WiFi.RSSI(i));
            out += F(",\"open\":");
            out += (WiFi.encryptionType(i) == WIFI_AUTH_OPEN) ? F("true") : F("false");
            out += '}';
        }
        out += F("]}");
        WiFi.scanDelete();
        server_.send(200, "application/json", out);
    }

    // POST /api/wifi (form-encoded: ssid, pass) — store the operator's home network.
    //
    // Form-encoded, NOT JSON, and deliberately: a WPA2 passphrase may contain quotes
    // and backslashes, and extractJsonStringValue() below has no escape handling. The
    // WebServer's own arg() parsing decodes percent-escaped form fields properly, so
    // there is no hand-rolled parser anywhere near a credential.
    //
    // Storing does not connect (see the file banner). The reply says so, so nobody is
    // left waiting for a device that was never going to join while they watched.
    void handleWifiPost() {
        const String ssid = server_.arg("ssid");
        const String pass = server_.arg("pass");
        if (ssid.length() == 0 || ssid.length() >= NetCredentials::kSsidCap ||
            pass.length() >= NetCredentials::kPassCap) {
            server_.send(400, "text/plain", "bad ssid/passphrase");
            return;
        }
        if (!creds_.store(ssid.c_str(), pass.c_str())) {
            server_.send(500, "text/plain", "could not save");
            return;
        }
        // The engine only ever learns THAT a network is stored, never which or what
        // its passphrase is — that is the whole reason the store is device-tier.
        if (game_) game_->setNetProvisioned(true);
        // SSID only. The passphrase is never printed, here or anywhere.
        Serial.printf("[setup] stored network ssid=%s\n", ssid.c_str());
        server_.send(200, "text/plain", "saved");
    }

    // The setup page. Inline and self-contained (no SD, no external assets) because
    // provisioning must work on a card-less device — and because this is the page a
    // non-technical person meets first, so it stays one screen: pick, type, save.
    void handleSetupPage() {
        String p;
        p.reserve(4300);
        p += F("<!DOCTYPE html><html><head><meta charset='utf-8'>"
               "<meta name='viewport' content='width=device-width,initial-scale=1'>"
               "<title>Malwarium :: SETUP</title><style>"
               "body{background:#0a0f0a;color:#39ff14;font-family:monospace;margin:0;"
               "padding:1.5rem;line-height:1.6}.wrap{max-width:30rem;margin:0 auto}"
               "h1{font-size:1.2rem;letter-spacing:.2em;border-bottom:1px solid #1e5c1e;"
               "padding-bottom:.5rem}label{display:block;margin:1rem 0 .3rem;"
               "font-size:.8rem;letter-spacing:.15em;color:#1e8c1e}"
               "select,input,button{width:100%;box-sizing:border-box;padding:.7rem;"
               "font-family:monospace;font-size:1rem;background:#0f1a0f;color:#39ff14;"
               "border:1px solid #1e5c1e;border-radius:2px}"
               "button{margin-top:1.2rem;background:#1e5c1e;cursor:pointer}"
               "#msg{margin-top:1rem;min-height:1.6em}"
               ".hint{color:#1e5c1e;font-size:.75rem;margin-top:1.5rem}"
               "</style></head><body><div class='wrap'>"
               "<h1>MALWARIUM // SETUP</h1>"
               "<p>Pick your home Wi-Fi and enter its password. "
               "The habitat only uses it to check for updates.</p>"
               "<label for='ssid'>NETWORK</label>"
               "<select id='ssid'><option value=''>scanning...</option></select>"
               "<label for='pass'>PASSWORD</label>"
               "<input id='pass' type='password' autocomplete='off'>"
               "<button id='go'>SAVE NETWORK</button>"
               "<div id='msg'></div>"
               // The HackerTag rides along for the same reason the passphrase does:
               // the on-device editor cycles one character at a time with a single
               // button, and there is a keyboard right here. Its own field and its own
               // button, so neither save depends on the other.
               "<label for='tag'>HACKERTAG (A-Z 0-9 _)</label>"
               "<input id='tag' maxlength='12' autocomplete='off'>"
               "<button id='gotag'>SAVE HACKERTAG</button>"
               "<div id='tagmsg'></div>");
#if UPDATE_ENABLED
        // Where this device looks for updates. The 'Pedia's DEVICE SETTINGS page
        // edits the same thing through the same endpoint, but a CARD-LESS device
        // has no 'Pedia — and the compiled-in default lives in a firmware image
        // that only changes via the mechanism it points at, so without a field
        // here such a device could never be told that its publish host moved.
        // This is the card-less half of the recovery path, not a convenience.
        {
            char resolved[UpdateSource::kUrlCap];
            if (!source_.url(resolved, sizeof(resolved))) resolved[0] = '\0';
            p += F("<label for='url'>UPDATE SOURCE</label>"
                   "<input id='url' type='url' maxlength='159' autocomplete='off' "
                   "spellcheck='false' placeholder='");
            p += resolved[0] ? htmlEscape(resolved) : String(F("https://.../manifest.json"));
            p += F("'><button id='gourl'>SAVE UPDATE SOURCE</button>"
                   "<div id='urlmsg'></div>"
                   "<div class='hint'>Full http:// or https:// address. Leave it empty "
                   "and save to go back to the built-in one.</div>");
        }
#endif
        p += F("<div class='hint'>Saving does not connect. Finish here, then run "
               "CHECK NOW on the habitat's UPDATES screen.</div>"
               "</div><script>"
               // Poll the async scan until it reports done, then fill the picker.
               "var sel=document.getElementById('ssid');"
               "function scan(){fetch('/api/networks').then(function(r){return r.json()})"
               ".then(function(d){if(d.scanning){setTimeout(scan,1200);return}"
               "sel.innerHTML='';if(!d.networks.length){"
               "sel.innerHTML='<option value=\"\">no networks found</option>';return}"
               "d.networks.forEach(function(n){var o=document.createElement('option');"
               "o.value=n.ssid;o.textContent=n.ssid+' ('+n.rssi+'dBm)'+(n.open?' open':'');"
               "sel.appendChild(o)})}).catch(function(){setTimeout(scan,2000)})}scan();"
               "document.getElementById('go').onclick=function(){"
               "var s=sel.value,m=document.getElementById('msg');"
               "if(!s){m.textContent='Pick a network first.';return}"
               "m.textContent='Saving...';"
               "fetch('/api/wifi',{method:'POST',"
               "headers:{'Content-Type':'application/x-www-form-urlencoded'},"
               "body:'ssid='+encodeURIComponent(s)+'&pass='+"
               "encodeURIComponent(document.getElementById('pass').value)})"
               ".then(function(r){m.textContent=r.ok?'Saved. You can close this page.'"
               ":'Could not save - try again.';"
               "if(r.ok){document.getElementById('pass').value=''}})"
               ".catch(function(){m.textContent='Could not reach the habitat.'})};"
               // The tag reuses the 'Pedia's existing rename endpoint. Upper-cased
               // client-side so the habitat's own A-Z0-9_ rule doesn't reject a
               // perfectly reasonable thing to type on a phone.
               "document.getElementById('gotag').onclick=function(){"
               "var t=document.getElementById('tag').value.toUpperCase(),"
               "m=document.getElementById('tagmsg');"
               "if(!t){m.textContent='Type a tag first.';return}"
               "fetch('/api/tag',{method:'POST',"
               "body:JSON.stringify({tag:t})})"
               ".then(function(r){m.textContent=r.ok?'Tag set to '+t"
               ":'Letters, numbers and _ only.'})"
               ".catch(function(){m.textContent='Could not reach the habitat.'})};");
#if UPDATE_ENABLED
        // Form-encoded like /api/wifi, and for the same reason: this is arbitrary
        // pasted text, and the WebServer's own arg() parsing decodes percent-escapes
        // where the hand-rolled JSON reader does not. An EMPTY submit is legitimate —
        // it clears the override — so unlike the tag there is no "type something
        // first" guard. The device answers with what it RESOLVED to, which is what
        // the field should now advertise: after a clear, that is the built-in address.
        p += F("document.getElementById('gourl').onclick=function(){"
               "var i=document.getElementById('url'),v=i.value.trim(),"
               "m=document.getElementById('urlmsg');"
               "if(v&&!/^https?:\\/\\/\\S+$/i.test(v)){"
               "m.textContent='Needs a full http:// or https:// address.';return}"
               "m.textContent='Saving...';"
               "fetch('/api/update-source',{method:'POST',"
               "headers:{'Content-Type':'application/x-www-form-urlencoded'},"
               "body:'url='+encodeURIComponent(v)})"
               ".then(function(r){if(!r.ok)throw 0;return r.text()})"
               ".then(function(u){i.value='';"
               "i.placeholder=u||'https://.../manifest.json';"
               "m.textContent=u?'Now checking '+u:'Cleared - this build has no "
               "built-in address.'})"
               ".catch(function(){m.textContent='Could not save - check the address.'})};");
#endif
        p += F("</script></body></html>");
        server_.send(200, "text/html", p);
    }

    // Escape the handful of characters that would break a JSON string. SSIDs are
    // arbitrary bytes off the air, so this runs on every one of them.
    static String jsonEscape(const String& s) {
        String out;
        out.reserve(s.length() + 8);
        for (unsigned i = 0; i < s.length(); ++i) {
            const char c = s[i];
            if (c == '"' || c == '\\') { out += '\\'; out += c; }
            else if (c < 0x20) out += ' ';       // control bytes -> a plain space
            else out += c;
        }
        return out;
    }
#endif  // STA_ENABLED

    // Minimal manual JSON-string-value extractor: find "key", the next ':',
    // then the quoted value between the first pair of quotes after it. No
    // escape handling — fine here since the only caller (the tag rename) also
    // validates the charset (A-Z0-9_, no quotes/backslashes possible anyway).
    static String extractJsonStringValue(const String& body, const char* key) {
        const String needle = String("\"") + key + "\"";
        const int k = body.indexOf(needle);
        if (k < 0) return String();
        const int colon = body.indexOf(':', k + needle.length());
        if (colon < 0) return String();
        const int q1 = body.indexOf('"', colon + 1);
        if (q1 < 0) return String();
        const int q2 = body.indexOf('"', q1 + 1);
        if (q2 < 0) return String();
        return body.substring(q1 + 1, q2);
    }

    // Build + serve the landing page from LIVE engine state, so a reload always
    // reflects the current pet/tag. Deliberately tiny: a terminal-styled identity
    // card (HackerTag + current pet species). This is now purely the SD-absent /
    // missing-bundle FALLBACK — the real 'Pedia is the static /web bundle above.
    void handleRoot() {
        const CreatureDef* pet = game_->pet();
        const char* species = pet ? pet->displayName : "UNHATCHED EGG";

        String tag = htmlEscape(game_->hackerTag());
        String sp = htmlEscape(species);

        // Terminal green-on-black — a dark CRT-wiki look matching the 'Pedia's
        // hacker theme. No external assets — inline CSS only.
        String p;
        p.reserve(1200);
        p += F("<!DOCTYPE html><html><head><meta charset='utf-8'>"
               "<meta name='viewport' content='width=device-width,initial-scale=1'>"
               "<title>Malwarium :: PEDIA</title><style>"
               "body{background:#0a0f0a;color:#39ff14;font-family:monospace;"
               "margin:0;padding:2rem;line-height:1.6}"
               ".wrap{max-width:34rem;margin:0 auto}"
               "h1{font-size:1.4rem;letter-spacing:.2em;border-bottom:1px solid #1e5c1e;"
               "padding-bottom:.5rem}"
               ".row{margin:1.2rem 0}.k{color:#1e8c1e;font-size:.8rem;letter-spacing:.15em}"
               ".v{font-size:1.5rem}.hint{color:#1e5c1e;font-size:.75rem;margin-top:2rem}"
               "</style></head><body><div class='wrap'>"
               "<h1>MALWARIUM // PEDIA</h1>"
               "<div class='row'><div class='k'>HACKERTAG</div><div class='v'>");
        p += tag;
        p += F("</div></div><div class='row'><div class='k'>CURRENT SPECIMEN</div>"
               "<div class='v'>");
        p += sp;
        p += F("</div></div>");
#if STA_ENABLED
        // The one link worth having here: without an SD card this page is the entire
        // site, and provisioning must still be reachable.
        p += F("<div class='hint'><a style='color:#39ff14' href='/setup'>"
               "SET UP HOME NETWORK</a></div>");
#endif
        p += F("<div class='hint'>containment habitat online &middot; more soon</div>"
               "</div></body></html>");
        server_.send(200, "text/html", p);
    }

    // Minimal HTML-escape (the tag is A-Z0-9_ and species come from content, so
    // this is belt-and-braces against a future free-text field).
    static String htmlEscape(const char* s) {
        String out;
        for (const char* p = s; *p; ++p) {
            switch (*p) {
                case '<': out += F("&lt;"); break;
                case '>': out += F("&gt;"); break;
                case '&': out += F("&amp;"); break;
                default: out += *p; break;
            }
        }
        return out;
    }

    bool up_ = false;
    bool routesRegistered_ = false;   // see service() — the handler list is never freed
    WebServer server_{80};
#if STA_ENABLED
    DNSServer dns_;             // captive-portal catch-all, only while the AP is up
    NetCredentials creds_;      // where /api/wifi writes; never read back for display
#if UPDATE_ENABLED
    // Resolves + rewrites the manifest address the 'Pedia's user section edits.
    UpdateSource source_;
#endif
    bool scanning_ = false;     // an async sweep for /api/networks is in flight
#endif
#endif

    Game* game_ = nullptr;
};

} // namespace mal
