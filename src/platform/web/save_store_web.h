// save_store_web.h — browser ISaveStore: the save blob lives in localStorage, and
// an empty one falls back to the baked demo seed.
//
// The storage boundary is the only thing the web target changes about persistence:
// the engine serializes the same versioned blob it writes to NVS on the device
// (core/model/save.h), and this puts the bytes somewhere a browser keeps them. A
// returning visitor resumes their own pet; a first-time one gets the seeded start
// state from generated/demo_save.h (built by tools/gen_demo_save.cpp).
//
// Bytes are held as hex rather than raw: localStorage stores DOMStrings, so a blob
// with embedded NULs and non-UTF-8 bytes cannot go in verbatim. Hex doubles the
// length — a ~1.5KB save becomes ~3KB of string, against a multi-megabyte quota —
// and costs nothing to get right, which base64 does not.
#pragma once

#include <emscripten.h>

#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "generated/demo_save.h"
#include "platform/platform.h"

namespace mal {

// The localStorage key. Namespaced so a page on the same origin serving anything
// else cannot collide with it.
inline constexpr const char* kWebSaveKey = "malwarium.save.v1";

EM_JS(char*, malWebSaveRead, (const char* key), {
    const s = localStorage.getItem(UTF8ToString(key));
    return s === null ? 0 : stringToNewUTF8(s);
});

EM_JS(void, malWebSaveWrite, (const char* key, const char* hex), {
    try {
        localStorage.setItem(UTF8ToString(key), UTF8ToString(hex));
    } catch (e) {
        // A full or blocked store (private browsing, quota) must not take the game
        // down with it — the session simply stops surviving a reload.
        console.warn('malwarium: save failed', e);
    }
});

EM_JS(void, malWebSaveClear, (const char* key), {
    localStorage.removeItem(UTF8ToString(key));
});

class WebSaveStore : public ISaveStore {
public:
    std::vector<uint8_t> load() override {
        std::vector<uint8_t> out;
        char* hex = malWebSaveRead(kWebSaveKey);
        if (hex) {
            out = fromHex(hex);
            std::free(hex);
            if (!out.empty()) return out;
        }
        // No save of their own yet: hand back the baked demo seed. The engine reads
        // it exactly as it would read a resumed save, so the demo's start state and a
        // returning visitor's state travel the same code path.
        out.assign(kDemoSaveBlob, kDemoSaveBlob + kDemoSaveBlobLen);
        return out;
    }

    bool save(const std::vector<uint8_t>& data) override {
        malWebSaveWrite(kWebSaveKey, toHex(data).c_str());
        return true;
    }

    // Drops the visitor's progress so the next load() falls back to the seed again —
    // what the shell's START OVER control calls before reloading the page.
    void clear() override { malWebSaveClear(kWebSaveKey); }

private:
    static std::string toHex(const std::vector<uint8_t>& d) {
        static const char* kDigits = "0123456789abcdef";
        std::string s;
        s.reserve(d.size() * 2);
        for (uint8_t b : d) { s += kDigits[b >> 4]; s += kDigits[b & 0x0f]; }
        return s;
    }

    // Returns empty on anything malformed, which the engine already treats as "no
    // save" — a corrupted key falls back to the seed rather than to a crash.
    static std::vector<uint8_t> fromHex(const char* s) {
        const size_t n = std::strlen(s);
        std::vector<uint8_t> out;
        if (n == 0 || n % 2 != 0) return out;
        out.reserve(n / 2);
        for (size_t i = 0; i < n; i += 2) {
            const int hi = hexVal(s[i]), lo = hexVal(s[i + 1]);
            if (hi < 0 || lo < 0) { out.clear(); return out; }
            out.push_back(static_cast<uint8_t>((hi << 4) | lo));
        }
        return out;
    }

    static int hexVal(char c) {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return -1;
    }
};

} // namespace mal
