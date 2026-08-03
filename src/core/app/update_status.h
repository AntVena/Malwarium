// update_status.h — portable shape of an update CHECK's outcome.
//
// Fetching the published artifact list is device-tier work
// (platform/esp32/net_update.h drives the HTTP GET), but what the answer LOOKS
// like lives here so the UPDATES screen and the native tests can read it
// without a radio — the same split as net_status.h. The device layer fills one of
// these and hands it to Game::setUpdateStatus().
//
// PER-ARTIFACT, never one verdict. The firmware and the SD 'Pedia bundle version
// independently, so "there is an update" is two questions with two answers; a
// single bool would force the prompt to lie about one of them.
//
// A CHECK NEVER INSTALLS. Looking and installing are two acts with two separate
// yeses: the check fills in an UpdateStatus, the operator reads it, and only then
// does an InstallStatus job start. The version strings exist so that second prompt
// can name what it is about to replace.
#pragma once

#include <cstdint>

namespace mal {

enum class UpdateState : unsigned char {
    Idle,      // nothing asked for
    Checking,  // associating, fetching, or parsing
    UpToDate,  // the list was read; nothing in it is newer
    Available, // the list was read; at least one artifact is newer
    Failed,    // see UpdateFail for which step gave up
};

// Why a check ended without an answer. Distinct reasons because they need
// distinct fixes: one is a setup problem, one is a network problem, and one means
// whatever answered was not a manifest.
enum class UpdateFail : unsigned char {
    None,
    NoSource,    // this device has no manifest URL — nowhere to look
    NoNetwork,   // never associated, or the host was unreachable
    NoManifest,  // the host answered, but not with a manifest (404, a portal page)
    Malformed,   // it parsed as far as "not the shape we publish"
};

struct UpdateStatus {
    UpdateState state = UpdateState::Idle;
    UpdateFail fail = UpdateFail::None;

    // Per artifact: is the published one strictly newer, and what does it call
    // itself. The version strings are DISPLAY ONLY — every decision is made on the
    // packed codes, device-side, before one of these is ever filled in.
    bool firmwareNewer = false;
    bool webNewer = false;
    char firmwareVersion[16] = {0};
    char webVersion[16] = {0};
};

// Which artifact a job is working on. `None` means the job is a CHECK — one job
// runs at a time, so this is also how the screen knows what it is watching.
enum class UpdateTarget : unsigned char { None, Firmware, Web };

enum class InstallState : unsigned char {
    Idle,
    Downloading,  // bytes arriving; `received`/`total` drive the bar
    Verifying,    // the whole artifact is here — hashing it before it counts
    Installing,   // writing it where it will actually be used
    Done,
    Failed,
};

// Why an install stopped. Each one is a different thing to tell the operator, and
// two of them are the reason the SHA-256 exists at all.
enum class InstallFail : unsigned char {
    None,
    NoNetwork,    // the association dropped, or the host stopped answering
    Truncated,    // fewer bytes arrived than the manifest said to expect
    Corrupt,      // every byte arrived and the digest still didn't match
    NoRoom,       // the artifact does not fit where it has to go
    WriteFailed,  // the flash slot or the card refused the write
};

struct InstallStatus {
    InstallState state = InstallState::Idle;
    InstallFail fail = InstallFail::None;
    UpdateTarget target = UpdateTarget::None;
    uint32_t received = 0;   // bytes written so far
    uint32_t total = 0;      // the manifest's declared size; 0 until known

    // Whether finishing this one needs the device to restart to take effect. Set
    // by the device tier, because only it knows what it just wrote.
    bool restartNeeded = false;
};

}  // namespace mal
