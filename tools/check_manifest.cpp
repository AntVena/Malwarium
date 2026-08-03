// check_manifest.cpp — read a manifest with the SAME parser the device uses.
//
//   ./check_manifest <manifest.json> [artifact-dir]
//
// A published manifest that the device rejects is indistinguishable, from the
// operator's side, from a dead network — so the generator that writes one
// (`make manifest`) is checked against core/net/update_manifest.h rather than
// against a human reading JSON. Exits non-zero on anything the device would
// refuse, which is what makes it usable as a publish gate.
//
// Given an artifact directory it also re-derives each row's size on disk, because
// the other half of a broken publish is a manifest that parses perfectly and
// describes a file that has since been rebuilt. (The SHA-256 is not re-derived
// here — the generator computes it from the same file in the same run, and adding
// a second implementation of it to disagree with is how that check rots.)
#include <cstdio>
#include <cstring>
#include <string>

#include "core/net/update_manifest.h"

using namespace mal;

static const char* parseName(ManifestParse r) {
    switch (r) {
        case ManifestParse::Ok: return "ok";
        case ManifestParse::Malformed: return "MALFORMED — the device would reject this";
        case ManifestParse::NoArtifacts: return "empty — nothing is published";
        case ManifestParse::TooMany: return "ok (more rows than the device can hold)";
    }
    return "?";
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: check_manifest <manifest.json> [artifact-dir]\n");
        return 2;
    }
    FILE* f = std::fopen(argv[1], "rb");
    if (!f) {
        std::fprintf(stderr, "check_manifest: cannot open %s\n", argv[1]);
        return 2;
    }
    std::string body;
    char buf[1024];
    size_t n;
    while ((n = std::fread(buf, 1, sizeof(buf), f)) > 0) body.append(buf, n);
    std::fclose(f);

    UpdateManifest m;
    const ManifestParse r = m.parse(body.data(), body.size());
    std::printf("%s: %s (%d rows, %zu bytes)\n", argv[1], parseName(r), m.count(),
                body.size());
    if (r == ManifestParse::Malformed || r == ManifestParse::NoArtifacts) return 1;

    bool ok = true;
    for (int i = 0; i < m.count(); ++i) {
        const UpdateArtifact* a = m.artifact(i);
        std::printf("  %-10s %-8s code=%-6u %9u B  %s\n", a->id, a->version, a->code,
                    a->size, a->url);

        if (argc < 3) continue;
        // The file the row claims to describe, by the URL's last component.
        const char* slash = std::strrchr(a->url, '/');
        const std::string path = std::string(argv[2]) + "/" + (slash ? slash + 1 : a->url);
        FILE* af = std::fopen(path.c_str(), "rb");
        if (!af) {
            std::printf("      MISSING: %s\n", path.c_str());
            ok = false;
            continue;
        }
        std::fseek(af, 0, SEEK_END);
        const long size = std::ftell(af);
        std::fclose(af);
        if (static_cast<long>(a->size) != size) {
            std::printf("      SIZE MISMATCH: manifest says %u, file is %ld\n", a->size,
                        size);
            ok = false;
        }
    }

    // Neither of the two rows the device looks up is required to exist — a publish
    // may legitimately carry only one — but saying which are present beats letting
    // a typo'd id look like nothing was published.
    std::printf("  firmware row: %s\n", m.byId("firmware") ? "present" : "absent");
    std::printf("  web row:      %s\n", m.byId("web") ? "present" : "absent");
    return ok ? 0 : 1;
}
