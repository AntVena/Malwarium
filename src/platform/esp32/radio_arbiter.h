// radio_arbiter.h — the single owner of the Wi-Fi radio (RF arbitration).
//
// AUDIT SCAN (net_sniff.h, passive discovery), AUDIT CAPTURE (net_capture.h,
// promiscuous EAPOL sniff), pet-to-pet LINK (net_link.h, ESP-NOW broadcast), the
// 'Pedia AP (ap_server.h) and the home-network association (net_sta.h) all want the
// radio and CANNOT run at once — they variously drive the Arduino WiFi scan stack,
// raw esp_wifi promiscuous mode, a channel-parked ESP-NOW session, a SoftAP, and a
// station association. This is the arbiter that guarantees they never both power it:
// each loop it picks ONE owner (or none), forces every non-owner fully down
// (WIFI_OFF), then services the owner. Nobody wanting the radio ⇒ fully powered down.
//
// Priority: UPDATE > AP > LINK > CAPTURE > SCAN.
//
// UPDATE (the STA association) sits ABOVE the AP, which is the one place a lower
// layer outranks a user-facing service, and it is deliberate: an association is only
// ever wanted while an update job the operator just started is running
// (Game::netConnectWanted), and a job somebody is waiting on outranks a toggle
// somebody left on. It also cannot become a standing claim — there is no permission
// to leave switched on, so the job ending is the radio coming back, and whichever
// toggle was already on takes it again with no bookkeeping. The visible consequence:
// checking for updates while the 'Pedia AP is up drops the AP, so a phone browsing
// the 'Pedia loses its connection until the check finishes.
//
// AP and STA want the Wi-Fi stack in incompatible modes, so they can never be
// co-resident — the stand-down-then-service ordering below is what enforces that.
// Provisioning has the opposite requirement (the AP must be up before there are any
// credentials to use), which is why the setup portal scans in WIFI_AP_STA inside the
// AP owner and never associates: storing credentials and using them are two separate
// flows, and only the second one comes through here as the Update owner.
//
// Below the AP: LINK only ever asks while the PEERS screen is open, which is a live,
// seconds-long, user-present act — the same reasoning that already lets the AP
// preempt capture, and a sealed capture re-arms by itself the moment the screen
// closes. Below that, CAPTURE outranks SCAN (a live/armed handshake session is the
// higher-intent, time-sensitive activity; discovery resumes when it seals).
//
// AMBIENT LINK IS NOT AN OWNER. Leaving the LINK toggle on never claims the radio:
// an unassociated ESP-NOW listener cannot use modem sleep, so an always-on ambient
// mode would simply hold the receiver open. Instead the arbiter lends it the SCAN
// owner's already-hot window between sweeps (piggyback below), which costs the
// window rather than a whole power cycle.
//
// A low-battery guard sits in front of capture: below kAuditMinBatteryPct SoC on
// battery, capture is refused and any live session is sealed (SM sealActive keeps
// the toggle intent, so it re-arms once charged) — the radio is the dominant
// draw, so we don't let a headless capture flatten the pack.
//
// Device-only (references the esp32 net_sniff / net_capture tiers); compiled only
// into the esp32 build, so no native stub is needed.
#pragma once

#include <cstdint>

#include "config.h"
#include "core/app/game.h"
#include "platform/esp32/ap_server.h"
#include "platform/esp32/net_capture.h"
#include "platform/esp32/net_link.h"
#include "platform/esp32/net_sniff.h"
#include "platform/esp32/net_sta.h"
#include "tunables.h"

#if MAL_NET_CAPTURE
#  include <Arduino.h>  // Serial (one-line owner-change trace)
#endif

namespace mal {

class RadioArbiter {
public:
    enum class Owner : uint8_t { None, Scan, Capture, Link, Ap, Update };

    void begin(Game* game, NetSniff* sniff, NetCapture* capture, ApServer* ap,
               NetLink* link, NetSta* sta) {
        game_ = game;
        sniff_ = sniff;
        capture_ = capture;
        ap_ = ap;
        link_ = link;
        sta_ = sta;
    }

    // Call every loop(). Enforces the battery guard, resolves the single owner,
    // stands the others down, and services the owner. Non-blocking.
    void poll(uint32_t nowMs) {
        if (!game_ || !sniff_ || !capture_ || !ap_ || !link_ || !sta_) return;

        // Low-battery guard (event-driven read of the last PowerStatus — no new
        // timer). Seal a live/armed capture before it drains the pack; keeps the
        // toggle intent so it re-arms when charged.
        if (!batteryOkForCapture() && game_->auditCapture().capturing()) {
            game_->auditCapture().sealActive(nowMs);
#if MAL_NET_CAPTURE
            if (!loggedBattSeal_) {
                Serial.println("[cap] battery low -> capture sealed");
                loggedBattSeal_ = true;
            }
        } else if (batteryOkForCapture()) {
            loggedBattSeal_ = false;
#endif
        }

        const Owner want = desired();
        if (want != owner_) {
#if MAL_NET_CAPTURE
            Serial.printf("[radio] owner %s -> %s free=%u largest=%u\n",
                          name(owner_), name(want),
                          (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getMaxAllocHeap());
#endif
            owner_ = want;
            // Report the hand-off up to the engine, so the CFG RADIO group and
            // System Info can say which of the contenders is actually live
            // rather than each screen claiming its own toggle is running.
            game_->setRadioOwner(reported(want));
        }

        // Force non-owners fully down FIRST, so the radio is never double-owned
        // across the hand-off, then service the owner. This ordering is what makes
        // the mutually-exclusive AP/STA modes safe: the AP is torn all the way down
        // (WIFI_OFF) before the association is ever attempted, and vice versa.
        if (want != Owner::Update) sta_->ensureDown();
        if (want != Owner::Ap) ap_->ensureDown();
        if (want != Owner::Link) link_->ensureDown();
        if (want != Owner::Capture) capture_->ensureDown();
        if (want != Owner::Scan) sniff_->ensureDown();

        switch (want) {
            case Owner::Update: sta_->service(nowMs); break;
            case Owner::Ap: ap_->service(nowMs); break;
            case Owner::Link: link_->service(nowMs); break;
            case Owner::Capture: capture_->service(nowMs); break;
            case Owner::Scan:
                sniff_->service(nowMs);
                // Lend the hot radio to ambient LINK in the gap between sweeps.
                // Never mid-sweep: parking the channel would corrupt the results
                // the scan is in the middle of collecting.
                if (sniff_->idleBetweenSweeps()) link_->piggyback(nowMs);
                break;
            case Owner::None: break;
        }
    }

    Owner owner() const { return owner_; }

    // Take the radio off the air regardless of what any consumer wants, sealing a
    // live capture so its .pcap sink is flushed and closed rather than truncated.
    // poll() would put straight back whatever the persisted opt-ins ask for, so this
    // is for the one caller that is about to stop calling poll() at all: travel sleep
    // (platform/esp32/main.cpp), on its way into a deep sleep the association would
    // not survive anyway. The opt-ins are untouched — a wake resolves them afresh.
    void standDown(uint32_t nowMs) {
        if (!game_ || !sniff_ || !capture_ || !ap_ || !link_ || !sta_) return;
        game_->auditCapture().sealActive(nowMs);
        sta_->ensureDown();
        ap_->ensureDown();
        link_->ensureDown();
        capture_->ensureDown();
        sniff_->ensureDown();
        owner_ = Owner::None;
        game_->setRadioOwner(RadioOwner::None);
    }

private:
    // The engine-facing name for an owner (core/app/radio_status.h) — a straight
    // relabelling, since both enums name the association after the update job that
    // is the only thing which ever raises it.
    static RadioOwner reported(Owner o) {
        switch (o) {
            case Owner::Scan: return RadioOwner::Scan;
            case Owner::Capture: return RadioOwner::Capture;
            case Owner::Link: return RadioOwner::Link;
            case Owner::Ap: return RadioOwner::Ap;
            case Owner::Update: return RadioOwner::Update;
            case Owner::None: break;
        }
        return RadioOwner::None;
    }

    Owner desired() const {
        // The association is the TOP owner — see the file banner. It can only want
        // the radio while an update job is actually running, so this never becomes a
        // standing claim that starves everything below.
        if (sta_->wants()) return Owner::Update;
        // AP is the next owner: a user-facing service the player explicitly turned
        // on preempts the audit radio. When the player switches it off, ap_->wants()
        // goes false and the radio falls through to whichever audit toggle is still
        // on — so audit resumes automatically (no extra bookkeeping needed).
        if (ap_->wants()) return Owner::Ap;
        // LINK asks only while the PEERS screen is open — a live, user-present act
        // measured in seconds. It preempts a capture the same way the AP does, and
        // the sealed capture re-arms on its own the moment the screen closes.
        if (link_->wants()) return Owner::Link;
        if (capture_->wants() && batteryOkForCapture()) return Owner::Capture;
        if (sniff_->wants()) return Owner::Scan;
        return Owner::None;
    }

    // Capture is only power-gated on battery; unknown/absent readings and external
    // power never block it (host/no-monitor builds, or plugged in to charge).
    bool batteryOkForCapture() const {
        const PowerStatus& p = game_->powerStatus();
        if (!p.present || p.charging || p.percent < 0) return true;
        return p.percent >= kAuditMinBatteryPct;
    }

#if MAL_NET_CAPTURE
    static const char* name(Owner o) {
        switch (o) {
            case Owner::Update: return "UPDATE";
            case Owner::Ap: return "AP";
            case Owner::Link: return "LINK";
            case Owner::Capture: return "CAPTURE";
            case Owner::Scan: return "SCAN";
            default: return "none";
        }
    }
    bool loggedBattSeal_ = false;
#endif

    Game* game_ = nullptr;
    NetSniff* sniff_ = nullptr;
    NetCapture* capture_ = nullptr;
    ApServer* ap_ = nullptr;
    NetLink* link_ = nullptr;
    NetSta* sta_ = nullptr;
    Owner owner_ = Owner::None;
};

} // namespace mal
