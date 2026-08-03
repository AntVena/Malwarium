// save_store_host.h — desktop ISaveStore: the save blob is a file in the cwd.
// Lets the host preview survive a relaunch exactly as the device survives a
// power-cycle (the engine is identical; only the byte sink differs).
#pragma once

#include <cstdio>
#include <vector>

#include "platform/platform.h"

namespace mal {

class FileSaveStore : public ISaveStore {
public:
    explicit FileSaveStore(const char* path = "malwarium_save.bin") : path_(path) {}

    std::vector<uint8_t> load() override {
        std::vector<uint8_t> out;
        FILE* f = std::fopen(path_, "rb");
        if (!f) return out;
        std::fseek(f, 0, SEEK_END);
        long n = std::ftell(f);
        std::fseek(f, 0, SEEK_SET);
        if (n > 0) {
            out.resize(static_cast<size_t>(n));
            if (std::fread(out.data(), 1, out.size(), f) != out.size()) out.clear();
        }
        std::fclose(f);
        return out;
    }

    bool save(const std::vector<uint8_t>& data) override {
        FILE* f = std::fopen(path_, "wb");
        if (!f) return false;
        const bool ok = data.empty() ||
                        std::fwrite(data.data(), 1, data.size(), f) == data.size();
        std::fclose(f);
        return ok;
    }

    void clear() override { std::remove(path_); }

private:
    const char* path_;
};

} // namespace mal
