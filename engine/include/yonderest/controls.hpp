#pragma once

// The controls interface: the player's input into intent.

#include <string_view>

namespace yonder {

// A key as the display collected it. Codes follow the convention the display and the controls
// share; the engine carries them unread.
struct Key {
    int code;
    bool pressed;
    bool repeat;
};

// A named intent that is not one of the framed actions, such as opening the backpack. Modules
// listen for the names they offer; one heard earlier may turn a toggle into a show.
struct Intent {
    std::string_view name;
    bool show = false;
};

// Going back: closing the innermost thing that is open, then offering the menu.
struct Back {
    bool claimed = false;
    bool stopped() const { return claimed; }
};

class Controls {
public:
    virtual ~Controls() = default;
    virtual void key(const Key& key) = 0;
    // Once a frame, after time advanced: held keys that repeat.
    virtual void frame(double now) = 0;
};

}
