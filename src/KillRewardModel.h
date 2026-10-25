#pragma once

#include "Model.h"

namespace MoraleMatters
{
    struct OffscreenKillSettings
    {
        bool enabled = true;
        float soldier = 0.25f;
        float elite = 0.5f;
        float captain = 2;
        float officer = 10;
        float cap = 10;
        float seconds = 10;
    };

    struct KillRewardBudget
    {
        float remaining = 0;
        float spent = 0;

        void Advance(float seconds)
        {
            if (isfinite(seconds) && seconds > 0)
                remaining = fmaxf(0, remaining - seconds);
        }

        float Allow(float target, float nativeCredit, const OffscreenKillSettings& settings)
        {
            const float wanted = fmaxf(0, target - nativeCredit);
            if (!settings.enabled || wanted <= 0 || settings.cap <= 0)
                return 0;
            if (remaining <= 0)
            {
                remaining = settings.seconds;
                spent = 0;
            }
            return fminf(wanted, fmaxf(0, settings.cap - spent));
        }
    };
}
