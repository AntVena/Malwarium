// update_source.h — where this device looks for the published artifact list.
//
// One question, answered in one place: what URL does an update check fetch? A
// stored override wins if there is one, otherwise the compiled-in
// UPDATE_MANIFEST_URL (config.h). Opens NVS_UPDATE_NAMESPACE in
// NVS_SAVE_PARTITION per call, exactly like NetCredentials (net_credentials.h) —
// cheap, and no handle is left open across the long stretches between checks.
//
// WHY AN OVERRIDE AT ALL. The compiled default is baked into a firmware image
// that, by definition, only changes via the very mechanism it points at — so a
// device whose publish host moved could never be told where the new one is. The
// override is the way out of that circle, and it is ADDITIVE: it never replaces
// the default, it just takes precedence while it is set, and clear() falls back.
// The setup page the SoftAP serves (ap_server.h) is where an operator will
// eventually type one; nothing but this file needs to change for that.
//
// It is stored in NVS, not the save blob, for the same reasons the credentials
// are: the save is portable and src/core is platform-agnostic, and a Factory
// Reset must not strand a device on a dead publish host.
//
// NO DEFAULT IS A LEGITIMATE STATE. With UPDATE_MANIFEST_URL empty and nothing
// stored, url() reports false and a check has nowhere to look — which is the
// honest answer, and far better than fetching a placeholder and reporting a
// network error the operator can do nothing about.
#pragma once

#include <cstddef>
#include <cstring>

#include "config.h"

#if UPDATE_ENABLED
#  include <Preferences.h>
#endif

namespace mal {

class UpdateSource {
public:
    // Matches UpdateArtifact::url (core/net/update_manifest.h): the manifest's own
    // address and the addresses inside it come off the same publish host, so a URL
    // one of them can hold and the other can't would be a trap.
    static constexpr size_t kUrlCap = 160;

    // Fill `out` with the URL to fetch. False (and an empty `out`) when neither an
    // override nor a compiled default exists — the caller must not fetch.
    bool url(char* out, size_t cap) const {
        if (!out || cap == 0) return false;
        out[0] = '\0';
#if UPDATE_ENABLED
        if (loadOverride(out, cap) && out[0] != '\0') return true;
        out[0] = '\0';
        if (std::strlen(UPDATE_MANIFEST_URL) == 0 ||
            std::strlen(UPDATE_MANIFEST_URL) >= cap)
            return false;
        std::strcpy(out, UPDATE_MANIFEST_URL);
        return true;
#else
        return false;
#endif
    }

    // Whether a stored override is what url() would return. The UI's business, so
    // a device pointed somewhere unusual can say so rather than looking stock.
    bool overridden() const {
#if UPDATE_ENABLED
        char buf[kUrlCap];
        return loadOverride(buf, sizeof(buf)) && buf[0] != '\0';
#else
        return false;
#endif
    }

    // Point this device at a different publish host. Rejects anything that isn't
    // an absolute http(s) URL that fits, rather than writing it — a bad override
    // would otherwise silently shadow a working default until someone cleared it.
    //
    // Both schemes are accepted because neither is trusted: the fetch runs
    // setInsecure() and the manifest is unsigned either way, so https buys
    // encryption, not authenticity. What actually protects an install is the
    // per-artifact SHA-256 (core/net/update_manifest.h), and that is unaffected by
    // the scheme.
    bool store(const char* url) {
#if UPDATE_ENABLED
        if (!acceptable(url)) return false;
        Preferences p;
        if (!p.begin(NVS_UPDATE_NAMESPACE, /*readOnly=*/false, NVS_SAVE_PARTITION))
            return false;
        const bool ok = p.putString(NVS_UPDATE_URL_KEY, url) > 0;
        p.end();
        return ok;
#else
        (void)url;
        return false;
#endif
    }

    // Drop the override and fall back to the compiled default.
    void clear() {
#if UPDATE_ENABLED
        Preferences p;
        if (!p.begin(NVS_UPDATE_NAMESPACE, /*readOnly=*/false, NVS_SAVE_PARTITION))
            return;
        p.remove(NVS_UPDATE_URL_KEY);
        p.end();
#endif
    }

    // What store() will take. Exposed so a form can reject a typo where it was
    // typed, instead of reporting a mysterious failure afterwards.
    static bool acceptable(const char* url) {
        if (!url) return false;
        const size_t n = std::strlen(url);
        if (n == 0 || n >= kUrlCap) return false;
        return std::strncmp(url, "http://", 7) == 0 ||
               std::strncmp(url, "https://", 8) == 0;
    }

private:
#if UPDATE_ENABLED
    bool loadOverride(char* out, size_t cap) const {
        // Zero first: a missing key leaves the caller's buffer untouched, so an
        // absent override would otherwise read as whatever the stack held.
        out[0] = '\0';
        Preferences p;
        if (!p.begin(NVS_UPDATE_NAMESPACE, /*readOnly=*/true, NVS_SAVE_PARTITION))
            return false;
        const size_t n = p.getString(NVS_UPDATE_URL_KEY, out, cap);
        p.end();
        return n > 0;
    }
#endif
};

}  // namespace mal
