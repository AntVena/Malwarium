// net_link.h — device-tier pet-to-pet discovery (ESP-NOW broadcast).
//
// Carries two formats over one radio: the HELLO beacon (core/net/peer_link.h), which
// is broadcast and announces this device, and the 1v1 duel handshake
// (core/net/pvp_link.h), which is unicast to one operator. Every received frame is
// handed to the engine — Game::registerPeer for a beacon, Game::onPvpFrame for a duel
// frame — which owns the decoding, the snapshot, the roster and the session state.
// Nothing here parses a payload or decides what a meeting or a duel is; this file is
// the radio and nothing else. The two formats are told apart by their magic, so the
// engine seam that rejects one simply gets offered the other.
//
// CONNECTIONLESS. Broadcast frames need no pairing and no association, and the
// people we merely HEAR never enter the ESP-NOW peer table — so the met-list is
// bounded by storage rather than by the 20-peer limit. Only the broadcast address
// itself is registered, because sending requires a peer entry even for broadcast.
//
// SAME CHANNEL OR NOTHING. ESP-NOW frames ride whatever channel the radio is parked
// on, and two unassociated devices have no way to converge on one. PEER_LINK_CHANNEL
// (config.h) is the fixed meeting point; a device parked elsewhere is simply
// inaudible, which is why the channel is a config constant rather than a scan.
//
// TWO WAYS TO GET AIRTIME, because an unassociated listener cannot use Wi-Fi modem
// sleep (that needs an AP's DTIM beacons) — its receiver is on continuously, so
// there is no cheap always-on mode:
//   * service() — this tier OWNS the radio, beaconing, listening, and sending queued
//     duel frames continuously. The arbiter grants that only for the stretch a PEERS
//     or LINK screen is open or a duel is in flight (Game::linkForegroundWanted),
//     where the exchange has to be immediate and the backlight already dominates the
//     draw.
//   * piggyback() — the audit SCAN owns a radio that is already hot and is idle
//     between sweeps. Announcing into that window costs the window, not a whole
//     power cycle. This is the ambient path, and it exists only while the scan runs
//     (Game::linkAmbientStarved is how the UI says so when it doesn't).
//
// Device-only; compiled only into the esp32 build behind LINK_ENABLED.
#pragma once

#include <cstdint>
#include <cstring>

#include "config.h"
#include "core/app/game.h"
#include "core/net/peer_link.h"
#include "core/net/pvp_link.h"

#if LINK_ENABLED
#  include <WiFi.h>
#  include <esp_mac.h>
#  include <esp_now.h>
#  include <esp_wifi.h>
#endif

namespace mal {

class NetLink {
public:
    // Wire the engine seam. Does NOT power the radio — the RadioArbiter decides when
    // this owner may run (service) vs must stand down (ensureDown).
    void begin(Game* game) {
        game_ = game;
#if LINK_ENABLED
        instance_ = this;
        // Hand the engine this device's own radio MAC. Duels need it to decide which
        // side hosts (pvpHostIsLocal, core/net/pvp_link.h), and esp_read_mac returns
        // the burned-in address without the radio being powered — so the identity is
        // known from boot rather than only once LINK first wins the arbiter. A build
        // without LINK never calls this, leaving the key 0 and duels inert.
        uint8_t mac[6] = {0};
        if (esp_read_mac(mac, ESP_MAC_WIFI_STA) == ESP_OK && game_)
            game_->setLocalRadioMac(mac);
#endif
    }

    // Whether this tier should OWN the radio right now. Only the foreground cases
    // (Game::linkForegroundWanted): the PEERS or LINK screen is open and wants
    // discovery inside a second, or a duel session is mid-handshake and cannot wait on
    // a duty cycle. Ambient LINK deliberately does not claim ownership — it rides the
    // scan's window instead, so leaving the toggle on never powers the radio up on its
    // own schedule.
    bool wants() const {
#if LINK_ENABLED
        return game_ && game_->linkForegroundWanted() && game_->linkWanted();
#else
        return false;
#endif
    }

    // Run one step ASSUMING the arbiter has granted this owner the radio: bring it
    // up in STA (never associating), park it on the rendezvous channel, and beacon
    // on PEER_BEACON_INTERVAL_MS. Receiving is interrupt-driven, so listening costs
    // nothing here beyond the radio already being on. Non-blocking.
    void service(uint32_t nowMs) {
#if LINK_ENABLED
        if (!game_) return;
        if (!radioOn_) {
            WiFi.mode(WIFI_STA);
            WiFi.disconnect(false, false);
            esp_wifi_set_channel(PEER_LINK_CHANNEL, WIFI_SECOND_CHAN_NONE);
            radioOn_ = true;
            if (!startEspNow()) { radioOn_ = false; return; }
            lastBeaconMs_ = nowMs - PEER_BEACON_INTERVAL_MS;   // announce immediately
            Serial.println("[link] radio up (STA, esp-now broadcast)");
        }
        if (nowMs - lastBeaconMs_ >= PEER_BEACON_INTERVAL_MS) {
            announce();
            lastBeaconMs_ = nowMs;
        }
        drainInbox();
        drainDuelFrames();
#else
        (void)nowMs;
#endif
    }

    // Announce into a radio window the SCAN owner already has hot. Call only while
    // the scan is idle between sweeps; this parks the channel and beacons, and does
    // not touch the radio's power state — the scan still owns that, and its next
    // sweep re-sweeps every channel regardless of where we left it parked.
    //
    // The window is bounded by PEER_BURST_MS so a burst can't crowd out the sweep
    // cadence the scan exists to run.
    void piggyback(uint32_t nowMs) {
#if LINK_ENABLED
        if (!game_ || !game_->linkWanted()) return;
        if (burstStartMs_ == 0 || nowMs - burstStartMs_ >= kBurstPeriodMs) {
            burstStartMs_ = nowMs;
            if (!espNowUp_) {
                esp_wifi_set_channel(PEER_LINK_CHANNEL, WIFI_SECOND_CHAN_NONE);
                if (!startEspNow()) return;
            }
            announce();
            drainInbox();
            drainDuelFrames();
            lastBeaconMs_ = nowMs;
            return;
        }
        // Inside the open window: keep announcing, so a peer whose own window
        // overlaps ours by only part of its length still hears us.
        if (nowMs - burstStartMs_ < PEER_BURST_MS &&
            nowMs - lastBeaconMs_ >= PEER_BEACON_INTERVAL_MS) {
            announce();
            drainInbox();
            drainDuelFrames();
            lastBeaconMs_ = nowMs;
        }
#else
        (void)nowMs;
#endif
    }

    // Fully release the radio (the arbiter handed it to another owner, or nobody
    // wants it). Idempotent; safe to call every loop.
    void ensureDown() {
#if LINK_ENABLED
        stopEspNow();
        if (radioOn_) {
            WiFi.mode(WIFI_OFF);
            radioOn_ = false;
        }
        burstStartMs_ = 0;
#endif
    }

    bool radioOn() const {
#if LINK_ENABLED
        return radioOn_;
#else
        return false;
#endif
    }

private:
#if LINK_ENABLED
    // How often a piggyback window opens: once per scan cycle, since that is how
    // often the scan's radio-up window comes around to borrow.
    static constexpr uint32_t kBurstPeriodMs = NET_SNIFF_SCAN_INTERVAL_MS;

    bool startEspNow() {
        if (espNowUp_) return true;
        if (esp_now_init() != ESP_OK) return false;
        esp_now_register_recv_cb(&NetLink::onRecv);
        // Sending needs a peer entry even for broadcast; RECEIVING does not, which
        // is what keeps the met-list off the 20-peer table entirely.
        esp_now_peer_info_t peer = {};
        std::memset(peer.peer_addr, 0xFF, ESP_NOW_ETH_ALEN);
        peer.channel = PEER_LINK_CHANNEL;
        peer.encrypt = false;
        peer.ifidx = WIFI_IF_STA;
        esp_now_add_peer(&peer);
        espNowUp_ = true;
        return true;
    }

    void stopEspNow() {
        if (!espNowUp_) return;
        esp_now_unregister_recv_cb();
        esp_now_deinit();
        espNowUp_ = false;
    }

    void announce() {
        if (!espNowUp_ || !game_) return;
        PeerHello hello;
        game_->buildPeerHello(&hello);
        uint8_t frame[kPeerHelloSize];
        if (encodePeerHello(hello, frame, sizeof(frame)) != kPeerHelloSize) return;
        static const uint8_t kBroadcast[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
        esp_now_send(kBroadcast, frame, kPeerHelloSize);
    }

    // Send every duel frame the engine has queued (Game::takePvpOut). UNICAST, which is
    // where the ESP-NOW peer table finally binds: broadcast needs no addressee but a
    // directed frame does.
    //
    // The addressee STAYS registered. esp_now_send() is asynchronous — it queues the
    // frame and returns long before the radio has actually transmitted it — so removing
    // the peer straight after the call can pull the entry out from under the pending
    // TX. A duel talks to exactly one operator, and stopEspNow()'s deinit drops the
    // whole table anyway, so holding one slot for the length of a session costs nothing
    // and is the only version that reliably transmits.
    void drainDuelFrames() {
        if (!espNowUp_ || !game_) return;
        uint8_t mac[6];
        uint8_t buf[kPvpFrameCap];
        size_t len = 0;
        while (game_->takePvpOut(mac, buf, sizeof(buf), &len)) {
            esp_now_peer_info_t peer = {};
            std::memcpy(peer.peer_addr, mac, ESP_NOW_ETH_ALEN);
            peer.channel = PEER_LINK_CHANNEL;
            peer.encrypt = false;
            peer.ifidx = WIFI_IF_STA;
            esp_now_add_peer(&peer);          // already-present is fine; the send is what matters
            const esp_err_t err = esp_now_send(mac, buf, len);
            Serial.printf("[pvp] tx type=%u len=%u -> %02X:%02X err=%d\n",
                          static_cast<unsigned>(buf[5]), static_cast<unsigned>(len),
                          mac[4], mac[5], static_cast<int>(err));
        }
    }

    // Hand the engine every duel frame the radio buffered since the last loop.
    //
    // THE DEFERRAL IS THE POINT. onRecv runs in the WiFi task, and the duel state
    // machine it feeds is not a small in-RAM note like a beacon sighting — it allocates
    // combatants, switches the nav state, and pushes replies into an outbox the main
    // loop is concurrently draining. Running that from another task races the main loop
    // for both the outbox and the game state. So the callback only copies bytes, and
    // every decision happens here, on the main loop, exactly like button input.
    void drainInbox() {
        if (!game_) return;
        while (inboxTail_ != inboxHead_) {
            const Rx& r = inbox_[inboxTail_ % kInboxCap];
            Serial.printf("[pvp] rx type=%u len=%u from %02X:%02X\n",
                          static_cast<unsigned>(r.len > 5 ? r.buf[5] : 0),
                          static_cast<unsigned>(r.len), r.mac[4], r.mac[5]);
            game_->onPvpFrame(r.mac, r.buf, r.len);
            ++inboxTail_;
        }
        // One line per transition, so a stalled handshake says which stage it died at
        // rather than going quiet.
        const uint8_t phase = static_cast<uint8_t>(game_->pvpPhase());
        if (phase != lastPhase_) {
            static const char* kNames[] = {"idle", "inviting", "invited",
                                           "arming", "fighting", "result"};
            Serial.printf("[pvp] phase %s -> %s (host=%d)\n", kNames[lastPhase_],
                          kNames[phase], game_->pvpLocalIsHost() ? 1 : 0);
            lastPhase_ = phase;
        }
    }

    // ESP-NOW hands frames to a plain C callback with no user pointer, so the single
    // live instance is routed through here. Runs in the WiFi task and does the least
    // possible: a beacon is a decode plus two small in-RAM structures (with an SD
    // write-through only on a genuinely new meeting), and a duel frame is copied
    // verbatim for the main loop to deal with (drainInbox).
    //
    // One callback, two formats, offered to both seams. Each decoder checks its own
    // magic first and drops anything that isn't its format, so a beacon never reaches
    // the duel state machine and a handshake frame never becomes a meeting — no length
    // guessing or format sniffing needed here. (registerPeer's return value means "a
    // NEW operator", not "this was a beacon", so it can't be used to short-circuit.)
    static void onRecv(const uint8_t* mac, const uint8_t* data, int len) {
        if (!instance_ || !instance_->game_ || len <= 0) return;
        Game* g = instance_->game_;
        const size_t n = static_cast<size_t>(len);
        g->registerPeer(mac, data, n);
        instance_->pushInbox(mac, data, n);
    }

    // Copy one frame in for the main loop. Single producer (the WiFi task) and single
    // consumer (drainInbox), so the two indices need no lock — only that each is
    // written by one side. A full inbox drops the oldest-unread rather than blocking
    // the radio; every duel frame is repeated on a timer, so a drop costs a retry.
    void pushInbox(const uint8_t* mac, const uint8_t* data, size_t len) {
        // Beacons vastly outnumber duel frames, so they are dropped on the magic here
        // rather than at decode — otherwise every HELLO would burn an inbox slot and a
        // trace line, burying the handshake it is supposed to make visible.
        if (len > kPvpFrameCap || !isPvpFrame(data, len)) return;
        Rx& r = inbox_[inboxHead_ % kInboxCap];
        std::memcpy(r.mac, mac, 6);
        std::memcpy(r.buf, data, len);
        r.len = len;
        ++inboxHead_;
        if (inboxHead_ - inboxTail_ > kInboxCap) inboxTail_ = inboxHead_ - kInboxCap;
    }

    // Deep enough for a whole handshake arriving inside one loop (a retrying peer can
    // land an ACCEPT and a START back to back).
    static constexpr uint32_t kInboxCap = 4;
    struct Rx {
        uint8_t mac[6] = {0};
        uint8_t buf[kPvpFrameCap] = {0};
        size_t len = 0;
    };
    Rx inbox_[kInboxCap];
    volatile uint32_t inboxHead_ = 0;   // written by the WiFi task
    volatile uint32_t inboxTail_ = 0;   // written by the main loop

    static NetLink* instance_;
    bool radioOn_ = false;
    bool espNowUp_ = false;
    uint8_t lastPhase_ = 0;
    uint32_t lastBeaconMs_ = 0;
    uint32_t burstStartMs_ = 0;   // 0 = no window opened yet this power-up
#endif
    Game* game_ = nullptr;
};

#if LINK_ENABLED
inline NetLink* NetLink::instance_ = nullptr;
#endif

}  // namespace mal
