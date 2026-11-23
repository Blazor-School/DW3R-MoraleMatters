#pragma once

namespace MoraleMatters
{
    struct HotkeyState
    {
        bool active = false;
        bool down = false;

        bool Update(bool focused, bool pressed)
        {
            const bool triggered = focused && active && pressed && !down;
            active = focused;
            down = pressed;
            return triggered;
        }
    };
}
