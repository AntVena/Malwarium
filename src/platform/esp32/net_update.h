// net_update.h — device-tier update CHECK: fetch the published list, compare it
// with what is installed, report.
//
// NOT a RadioArbiter owner. It rides the association net_sta.h already owns: an
// update job makes Game::netConnectWanted() true (game.h), the arbiter grants the
// STA owner the radio, and this waits for NetState::Online and then does one HTTP
// GET. One fetch per job — the job ends on its own outcome, which is what hands
// the radio back.
//
// WHY A JOB AND NOT A SCREEN. Anything nav-derived dies when the operator walks
// away, which is fatal for a transfer. So a check is latched (Game::updateJobLive)
// and runs to a terminal state whether or not anyone is looking — leaving the
// UPDATES screen mid-check finds the verdict waiting on the way back. It is also
// what makes the association's lifetime honest: the job is the permission, so the
// device is on the network for exactly as long as the job needs it.
//
// setInsecure(), DELIBERATELY. There is no CA bundle to keep current on a device
// that may sit in a drawer for a year, and no code signing either. What protects
// an install is the per-artifact SHA-256 in the manifest, which catches the
// realistic failure — a corrupted or truncated download — and NOT a hostile
// network, since the manifest rides the same unauthenticated channel as the
// artifacts. That trade is spelled out in core/net/update_manifest.h.
//
// NOTHING IS INSTALLED THAT HASN'T BEEN VERIFIED FIRST, and the two targets get
// there differently because their commit points differ:
//
//   FIRMWARE streams into the INACTIVE OTA slot while hashing. A slot is only
//   bootable once Update.end() sets the boot partition, so a digest mismatch just
//   calls Update.abort() — the half-written slot is never selected and the running
//   build is untouched. There is no way to buffer 1.7MB to hash it first, and no
//   need to.
//
//   THE WEB BUNDLE lands as one temp file on the card, is hashed there, and is
//   only unpacked over the live /web folder once it matches. The bundle is many
//   small files, so verifying after extraction would mean discovering the problem
//   with half of it already replaced.
//
// AND A VERIFIED IMAGE STILL HAS TO EARN ITS KEEP. A digest only says the bytes
// arrived intact; it cannot say the build works. So a firmware install boots on
// trial and is thrown away unless it proves itself — main.cpp's rollback gate,
// which commits the image only after the device reaches the main loop, paints a
// frame and stays up. Nothing here is signed and there is no second way into the
// device, so an image that boots into a crash-loop would otherwise mean a USB
// cable, which the person this path exists for does not have.
//
// A check's fetch BLOCKS for the length of its (sub-kilobyte) transaction. A
// download does NOT: it moves a bounded chunk per service() call and pushes
// progress, so the panel keeps repainting and the operator can walk away — the
// job holds the radio on its own.
#pragma once

#include <cstdint>
#include <cstring>

#include "config.h"
#include "core/app/game.h"
#include "core/app/update_status.h"
#include "core/net/tar_reader.h"
#include "core/net/update_manifest.h"
#include "platform/esp32/update_source.h"
#include "platform/esp32/web_bundle.h"
#include "version.h"

#if UPDATE_ENABLED
#  include <HTTPClient.h>
#  include <Update.h>
#  include <WiFi.h>
#  include <WiFiClientSecure.h>
#  include <mbedtls/sha256.h>
#  include <sys/stat.h>
#endif

namespace mal {

class NetUpdate {
public:
    void begin(Game* game) { game_ = game; }

    // Drive one step of whatever job is live. Cheap and non-blocking until the
    // association is actually up, at which point it performs the one fetch.
    void service(uint32_t nowMs) {
#if UPDATE_ENABLED
        if (!game_) return;
        if (!game_->updateJobLive()) {   // no job, or the last one already finished
            ran_ = false;
            waitStartMs_ = 0;
            phase_ = Phase::Start;
            received_ = 0;
            // A job that ended without going through failInstall (consent
            // withdrawn mid-download) still has a sink open. Discard it — an
            // aborted OTA slot is never bootable and a temp file is just a file.
            closeDownload();
            if (otaOpen_) { Update.abort(); otaOpen_ = false; }
            if (sink_) { std::fclose(sink_); sink_ = nullptr; std::remove(kWebTmpPath); }
            return;
        }
        const bool installing = game_->updateJobTarget() != UpdateTarget::None;
        if (ran_ && !installing) return;   // a check gets exactly one fetch

        if (!online()) {
            const NetStatus& ns = game_->netStatus();
            if (ns.state == NetState::Failed) { abandon(UpdateFail::NoNetwork); return; }
            // Backstop only: net_sta.h times the join out itself and reports
            // Failed. This catches a job that somehow never gets that far, so a
            // latched job can't hold the radio forever.
            if (waitStartMs_ == 0) waitStartMs_ = nowMs;
            if (nowMs - waitStartMs_ >= kOnlineWaitMs) abandon(UpdateFail::NoNetwork);
            return;
        }

        if (!installing) {
            ran_ = true;
            runCheck();
            return;
        }
        // An install runs a chunk at a time so the panel keeps repainting; each
        // call either advances it or ends it.
        stepInstall(nowMs);
#else
        (void)nowMs;
#endif
    }

    // Whether the device should reboot into what was just written. main.cpp reads
    // this after servicing, so the restart happens between frames rather than
    // inside a draw.
    bool restartPending() const {
#if UPDATE_ENABLED
        return restartPending_;
#else
        return false;
#endif
    }

private:
#if UPDATE_ENABLED
    // How long past the association's own timeout to keep waiting before giving up.
    static constexpr uint32_t kOnlineWaitMs =
        static_cast<uint32_t>(STA_CONNECT_TIMEOUT_MS) + 5000u;
    static constexpr uint32_t kHttpTimeoutMs = 10000;
    // A published manifest is a few hundred bytes. This bound is what stops a
    // captive portal's login page from being read into RAM in full — anything
    // longer is truncated here and then rejected by the parser, which requires the
    // closing brace precisely so a cut-off body cannot look complete.
    static constexpr size_t kManifestCap = 2048;
    // Bytes moved per service() call. Small enough that the panel keeps repainting
    // through a multi-megabyte firmware image, large enough that the loop overhead
    // isn't the bottleneck.
    static constexpr size_t kChunk = 4096;
    // How often a download pushes progress into the engine. Every chunk would mark
    // the frame dirty hundreds of times a second for a bar that moves in percent.
    static constexpr uint32_t kProgressMs = 200;
    // The download lands here whole and is verified before a byte of it reaches the
    // live bundle. Alongside /web rather than inside it, so a half-finished file is
    // never something the AP could serve.
    static constexpr const char* kWebTmpPath = "/sdcard/web_update.tar";
    static constexpr const char* kWebRoot = "/sdcard/web";

    bool online() const { return game_->netStatus().state == NetState::Online; }

    // End whatever job is live with a failure, whichever kind it is. One place, so
    // a check and an install can't disagree about what "give up" means.
    void abandon(UpdateFail why) {
        if (game_->updateJobTarget() == UpdateTarget::None) { finishFail(why); return; }
        closeDownload();
        InstallFail f = InstallFail::NoNetwork;
        if (why == UpdateFail::NoSource) f = InstallFail::WriteFailed;
        failInstall(f);
    }

    void runCheck() {
        char url[UpdateSource::kUrlCap];
        if (!source_.url(url, sizeof(url))) {
            Serial.println("[upd] no manifest source");
            finishFail(UpdateFail::NoSource);
            return;
        }

        char body[kManifestCap];
        size_t len = 0;
        if (!fetch(url, body, sizeof(body), &len)) {
            finishFail(UpdateFail::NoManifest);
            return;
        }

        UpdateManifest man;
        const ManifestParse r = man.parse(body, len);
        if (r == ManifestParse::Malformed) {
            Serial.printf("[upd] manifest malformed (%u bytes)\n",
                          static_cast<unsigned>(len));
            finishFail(UpdateFail::Malformed);
            return;
        }
        // An empty list is a publish host with nothing on it — that is an answer
        // ("nothing for you"), not a failure, and it reads as up to date.

        UpdateStatus st;
        if (const UpdateArtifact* fw = man.byId("firmware")) {
            if (firmwareUpdateAvailable(fw->code)) {
                st.firmwareNewer = true;
                copyVersion(st.firmwareVersion, fw->version);
            }
        }
        // The web bundle's installed version is a file beside the bundle, since
        // nothing about the SD card is compiled in (web_bundle.h). No marker reads
        // as 0, which offers a re-install — the safe direction.
        VersionMarker web;
        readWebBundleVersion(&web);
        if (const UpdateArtifact* wa = man.byId("web")) {
            if (artifactIsNewer(*wa, web.code)) {
                st.webNewer = true;
                copyVersion(st.webVersion, wa->version);
            }
        }

        st.state = (st.firmwareNewer || st.webNewer) ? UpdateState::Available
                                                     : UpdateState::UpToDate;
        Serial.printf("[upd] checked: fw=%s web=%s\n",
                      st.firmwareNewer ? st.firmwareVersion : "current",
                      st.webNewer ? st.webVersion : "current");
        game_->setUpdateStatus(st);
    }

    // One bounded GET. Reads at most `cap` bytes: a body that overruns is left
    // truncated on purpose, because the parser rejects a manifest with no closing
    // brace and that is a far better outcome than half a login page parsed as an
    // artifact list.
    bool fetch(const char* url, char* out, size_t cap, size_t* outLen) {
        const bool secure = std::strncmp(url, "https://", 8) == 0;
        WiFiClientSecure tls;
        WiFiClient plain;
        HTTPClient http;

        if (secure) tls.setInsecure();   // no CA chain to rot — see the banner
        http.setConnectTimeout(kHttpTimeoutMs);
        http.setTimeout(kHttpTimeoutMs);
        if (!http.begin(secure ? static_cast<WiFiClient&>(tls) : plain, url)) {
            Serial.println("[upd] bad manifest url");
            return false;
        }

        const int code = http.GET();
        if (code != HTTP_CODE_OK) {
            Serial.printf("[upd] manifest http %d\n", code);
            http.end();
            return false;
        }

        const int declared = http.getSize();   // -1 when chunked
        WiFiClient* stream = http.getStreamPtr();
        size_t n = 0;
        const uint32_t start = millis();
        while (n < cap && millis() - start < kHttpTimeoutMs) {
            if (declared > 0 && n >= static_cast<size_t>(declared)) break;
            const int avail = stream->available();
            if (avail <= 0) {
                if (!http.connected()) break;
                delay(10);
                continue;
            }
            const size_t want = (cap - n) < static_cast<size_t>(avail)
                                    ? (cap - n)
                                    : static_cast<size_t>(avail);
            const int got = stream->readBytes(out + n, want);
            if (got <= 0) break;
            n += static_cast<size_t>(got);
        }
        http.end();

        if (n == 0) {
            Serial.println("[upd] empty manifest response");
            return false;
        }
        *outLen = n;
        return true;
    }

    // ---- Install ----------------------------------------------------------
    //
    // Phases, one per service() call until the job ends: Start opens the
    // download and the sink, Body moves a chunk, and the commit happens the
    // moment the last byte has arrived AND the digest matches.
    enum class Phase : uint8_t { Start, Body };

    void stepInstall(uint32_t nowMs) {
        if (phase_ == Phase::Start) { beginInstall(); return; }

        WiFiClient* stream = http_.getStreamPtr();
        const int avail = stream ? stream->available() : 0;
        if (avail > 0) {
            const uint32_t left = art_.size - received_;
            size_t want = static_cast<size_t>(avail) < sizeof(chunk_)
                              ? static_cast<size_t>(avail) : sizeof(chunk_);
            if (want > left) want = left;
            const int got = stream->readBytes(chunk_, want);
            if (got > 0) {
                if (!writeChunk(chunk_, static_cast<size_t>(got))) {
                    closeDownload();
                    failInstall(InstallFail::WriteFailed);
                    return;
                }
                mbedtls_sha256_update_ret(&sha_, chunk_, static_cast<size_t>(got));
                received_ += static_cast<uint32_t>(got);
                lastByteMs_ = nowMs;
            }
        } else if (!http_.connected() || nowMs - lastByteMs_ >= kHttpTimeoutMs) {
            // The stream ended or stalled. Whether that is fine depends entirely on
            // whether every declared byte got here.
            if (received_ < art_.size) {
                closeDownload();
                Serial.printf("[upd] truncated: %u of %u bytes\n",
                              static_cast<unsigned>(received_),
                              static_cast<unsigned>(art_.size));
                failInstall(InstallFail::Truncated);
                return;
            }
        }

        if (received_ >= art_.size) { finishInstall(); return; }

        if (nowMs - lastProgressMs_ >= kProgressMs) {
            lastProgressMs_ = nowMs;
            push(InstallState::Downloading, InstallFail::None);
        }
    }

    // Re-read the manifest rather than trusting a row cached from the check: this
    // installs what is published RIGHT NOW, and the url/size/digest it uses are the
    // ones it just read together, so they cannot be a mismatched pair.
    void beginInstall() {
        const UpdateTarget target = game_->updateJobTarget();
        char url[UpdateSource::kUrlCap];
        if (!source_.url(url, sizeof(url))) { failInstall(InstallFail::WriteFailed); return; }

        char body[kManifestCap];
        size_t len = 0;
        if (!fetch(url, body, sizeof(body), &len)) {
            failInstall(InstallFail::NoNetwork);
            return;
        }
        UpdateManifest man;
        if (man.parse(body, len) == ManifestParse::Malformed) {
            failInstall(InstallFail::NoNetwork);
            return;
        }
        const UpdateArtifact* a =
            man.byId(target == UpdateTarget::Firmware ? "firmware" : "web");
        if (!a || a->size == 0) { failInstall(InstallFail::NoNetwork); return; }
        art_ = *a;

        if (!openSink(target)) return;            // reports its own failure
        if (!openDownload(art_.url)) {
            closeSink(/*keep=*/false);
            failInstall(InstallFail::NoNetwork);
            return;
        }

        mbedtls_sha256_init(&sha_);
        mbedtls_sha256_starts_ret(&sha_, /*is224=*/0);
        received_ = 0;
        phase_ = Phase::Body;
        lastByteMs_ = millis();
        lastProgressMs_ = 0;
        push(InstallState::Downloading, InstallFail::None);
        Serial.printf("[upd] installing %s %s (%u bytes)\n", art_.id, art_.version,
                      static_cast<unsigned>(art_.size));
    }

    // Every byte is here. Hash first, and only then let it become the thing that
    // runs — this is the one gate between a damaged download and a bricked device.
    void finishInstall() {
        closeDownload();
        push(InstallState::Verifying, InstallFail::None);

        uint8_t got[32];
        mbedtls_sha256_finish_ret(&sha_, got);
        mbedtls_sha256_free(&sha_);
        if (std::memcmp(got, art_.sha256, sizeof(got)) != 0) {
            Serial.println("[upd] digest mismatch — discarding");
            closeSink(/*keep=*/false);   // firmware: abort; web: delete the temp file
            failInstall(InstallFail::Corrupt);
            return;
        }

        push(InstallState::Installing, InstallFail::None);
        if (!closeSink(/*keep=*/true)) { failInstall(InstallFail::WriteFailed); return; }

        InstallStatus st;
        st.state = InstallState::Done;
        st.target = game_->updateJobTarget();
        st.received = received_;
        st.total = art_.size;
        st.restartNeeded = st.target == UpdateTarget::Firmware;
        restartPending_ = st.restartNeeded;
        game_->setInstallStatus(st);
        Serial.printf("[upd] installed %s %s\n", art_.id, art_.version);
    }

    // ---- Sinks: where the bytes actually go --------------------------------

    bool openSink(UpdateTarget target) {
        if (target == UpdateTarget::Firmware) {
            // Update.h picks the inactive OTA slot. A false here means the image
            // does not fit the slot — the one thing worth naming separately,
            // because no amount of retrying fixes it.
            if (!Update.begin(art_.size, U_FLASH)) {
                Serial.printf("[upd] ota begin failed: %s\n", Update.errorString());
                failInstall(InstallFail::NoRoom);
                return false;
            }
            otaOpen_ = true;
            return true;
        }
        if (!game_->sdStatus().present) {
            Serial.println("[upd] no card for the web bundle");
            failInstall(InstallFail::WriteFailed);
            return false;
        }
        sink_ = std::fopen(kWebTmpPath, "wb");
        if (!sink_) {
            failInstall(InstallFail::WriteFailed);
            return false;
        }
        return true;
    }

    bool writeChunk(const uint8_t* p, size_t n) {
        if (otaOpen_) return Update.write(const_cast<uint8_t*>(p), n) == n;
        if (sink_) return std::fwrite(p, 1, n, sink_) == n;
        return false;
    }

    // `keep` commits, !keep discards. Discarding is the important half: an aborted
    // OTA slot is never made bootable, and a deleted temp file never reaches /web.
    bool closeSink(bool keep) {
        if (otaOpen_) {
            otaOpen_ = false;
            if (!keep) { Update.abort(); return true; }
            if (!Update.end(/*evenIfRemaining=*/true)) {
                Serial.printf("[upd] ota end failed: %s\n", Update.errorString());
                return false;
            }
            return true;
        }
        if (sink_) {
            std::fclose(sink_);
            sink_ = nullptr;
            if (!keep) { std::remove(kWebTmpPath); return true; }
            const bool ok = unpackWebBundle();
            std::remove(kWebTmpPath);
            return ok;
        }
        return keep;   // nothing was open; only a discard can succeed vacuously
    }

    // Extract the verified archive over the live bundle and re-stamp it. The stamp
    // is written LAST, so a card whose extraction died half-way still reads as the
    // old version and gets offered the update again, rather than claiming to be new.
    bool unpackWebBundle() {
        mkdir(kWebRoot, 0777);
        if (!untar(kWebTmpPath, kWebRoot)) {
            Serial.println("[upd] unpack failed");
            return false;
        }
        VersionMarker m;
        m.code = art_.code;
        std::strncpy(m.version, art_.version, sizeof(m.version) - 1);
        if (!writeWebBundleVersion(m)) return false;
        game_->setWebBundleVersion(m.version);
        return true;
    }

    // Walk the archive one header at a time. Names are validated by
    // parseTarHeader before anything is opened — an entry that tries to climb out
    // of `root` stops the whole extraction rather than being quietly renamed.
    static bool untar(const char* tarPath, const char* root) {
        FILE* f = std::fopen(tarPath, "rb");
        if (!f) return false;
        uint8_t block[kTarBlock];
        bool ok = true;
        while (ok) {
            if (std::fread(block, 1, kTarBlock, f) != kTarBlock) { ok = false; break; }
            TarEntry e;
            if (!parseTarHeader(block, sizeof(block), &e)) { ok = false; break; }
            if (e.kind == TarEntryKind::End) break;
            if (e.kind == TarEntryKind::Invalid) {
                Serial.println("[upd] rejected a tar entry");
                ok = false;
                break;
            }
            if (e.kind == TarEntryKind::Skip) {
                std::fseek(f, static_cast<long>(tarDataSpan(e.size)), SEEK_CUR);
                continue;
            }

            char path[192];
            std::snprintf(path, sizeof(path), "%s/%s", root, e.name);
            makeParents(path);
            if (e.kind == TarEntryKind::Directory) { mkdir(path, 0777); continue; }

            FILE* out = std::fopen(path, "wb");
            if (!out) { ok = false; break; }
            uint32_t left = e.size;
            while (left > 0) {
                const size_t want = left < sizeof(block) ? left : sizeof(block);
                if (std::fread(block, 1, want, f) != want) { ok = false; break; }
                if (std::fwrite(block, 1, want, out) != want) { ok = false; break; }
                left -= static_cast<uint32_t>(want);
            }
            std::fclose(out);
            if (!ok) break;
            const uint32_t pad = tarDataSpan(e.size) - e.size;
            if (pad) std::fseek(f, static_cast<long>(pad), SEEK_CUR);
        }
        std::fclose(f);
        return ok;
    }

    // mkdir every directory component of `path` except the last. FAT has no
    // "create parents" call, and an archive is free to list a file before the
    // directory it lives in.
    static void makeParents(char* path) {
        for (char* p = path + 1; *p; ++p) {
            if (*p != '/') continue;
            *p = '\0';
            mkdir(path, 0777);
            *p = '/';
        }
    }

    // ---- Download plumbing -------------------------------------------------

    bool openDownload(const char* url) {
        const bool secure = std::strncmp(url, "https://", 8) == 0;
        if (secure) tls_.setInsecure();   // no CA chain to rot — see the banner
        http_.setConnectTimeout(kHttpTimeoutMs);
        http_.setTimeout(kHttpTimeoutMs);
        if (!http_.begin(secure ? static_cast<WiFiClient&>(tls_) : plain_, url))
            return false;
        dlOpen_ = true;
        if (http_.GET() != HTTP_CODE_OK) { closeDownload(); return false; }
        return true;
    }

    void closeDownload() {
        if (!dlOpen_) return;
        http_.end();
        dlOpen_ = false;
    }

    void push(InstallState s, InstallFail f) {
        InstallStatus st;
        st.state = s;
        st.fail = f;
        st.target = game_->updateJobTarget();
        st.received = received_;
        st.total = art_.size;
        game_->setInstallStatus(st);
    }

    void failInstall(InstallFail why) {
        closeDownload();
        if (otaOpen_) { Update.abort(); otaOpen_ = false; }
        if (sink_) { std::fclose(sink_); sink_ = nullptr; std::remove(kWebTmpPath); }
        InstallStatus st;
        st.state = InstallState::Failed;
        st.fail = why;
        st.target = game_->updateJobTarget();
        st.received = received_;
        st.total = art_.size;
        game_->setInstallStatus(st);   // terminal — this is what releases the radio
        phase_ = Phase::Start;
    }

    static void copyVersion(char* dst, const char* src) {
        std::strncpy(dst, src, 15);
        dst[15] = '\0';
    }

    void finishFail(UpdateFail why) {
        ran_ = true;
        UpdateStatus st;
        st.state = UpdateState::Failed;
        st.fail = why;
        game_->setUpdateStatus(st);   // terminal — this is what releases the radio
    }

    UpdateSource source_;
    bool ran_ = false;
    uint32_t waitStartMs_ = 0;

    // Install-in-flight state. Reset by service() the moment no job is live, so a
    // finished or abandoned job never leaves a half-open sink behind.
    Phase phase_ = Phase::Start;
    UpdateArtifact art_{};
    mbedtls_sha256_context sha_{};
    uint32_t received_ = 0;
    uint32_t lastByteMs_ = 0;
    uint32_t lastProgressMs_ = 0;
    HTTPClient http_;
    WiFiClientSecure tls_;
    WiFiClient plain_;
    bool dlOpen_ = false;
    FILE* sink_ = nullptr;
    bool otaOpen_ = false;
    // The download's transfer buffer. A MEMBER, not a stepInstall() local: the
    // Arduino loop task gets an 8KB stack, and kChunk bytes of it live across
    // beginInstall()'s own frame (a 2KB manifest body + a parsed UpdateManifest)
    // plus HTTPClient's — which overflows the stack and double-faults the moment an
    // install starts. Job state belongs beside sha_/art_/received_ anyway; this is
    // the buffer that job streams through.
    uint8_t chunk_[kChunk] = {0};
    bool restartPending_ = false;
#endif

    Game* game_ = nullptr;
};

}  // namespace mal
