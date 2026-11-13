// inline_confirm.h: the two-choice confirm a screen opens over itself before an action it
// cannot take back (ARCH's store, deploy, release, sale and new egg; overwriting a MOD or
// a MOVE). It opens on the safe choice: A flips it, B answers, C backs out.
#pragma once

#include <cstdint>

#include "core/device/platform.h"

namespace mal {

class InlineConfirm {
public:
    // What a press did. Every answer but Pending closes the confirm. No and Back differ
    // only in the button: a screen whose B-on-the-safe-choice also leaves treats them
    // apart.
    enum class Answer : uint8_t { Pending, Yes, No, Back };

    void open() {
        open_ = true;
        yes_ = false;
    }
    void close() { open_ = false; }
    bool isOpen() const { return open_; }
    // The highlighted choice as the renderers take it: 0 the safe one, 1 the action.
    int choice() const { return yes_ ? 1 : 0; }

    Answer onButton(const ButtonEvent& ev) {
        switch (ev.button) {
            case Button::A: yes_ = !yes_; return Answer::Pending;
            case Button::B: open_ = false; return yes_ ? Answer::Yes : Answer::No;
            case Button::C: open_ = false; return Answer::Back;
        }
        return Answer::Pending;
    }

private:
    bool open_ = false;
    bool yes_ = false;
};

}  // namespace mal
