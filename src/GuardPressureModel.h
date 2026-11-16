#pragma once

#include "BehaviorModel.h"

namespace MoraleMatters
{
    struct GuardPressureSettings
    {
        bool enabled = true;
        float normalPressure = 1;
        float chargePressure = 3;
        float soldierThreshold = 6;
        float eliteThreshold = 9;
        float officerThreshold = 12;
        float recoveryDelay = 2;
        float recoveryPerSecond = 2;
        float openingSeconds = 0.9f;
        float breakCooldown = 3;
        bool disruption = true;
        float disruptionRadius = 400;
        float disruptionSeconds = 0.8f;
        float disruptionCooldown = 4;
        int disruptionTargets = 6;
        bool showPressureBar = true;
        bool showBreakIndicator = true;
        bool breakSound = true;
        float indicatorSeconds = 1.2f;
    };

    struct GuardPressureState
    {
        float pressure = 0;
        float quiet = 0;
        float opening = 0;
        float cooldown = 0;
        float hesitation = 0;
        float disruptionCooldown = 0;
        float hitFlash = 0;
        float breakFlash = 0;

        void Advance(float seconds, float morale, const GuardPressureSettings& settings)
        {
            if (!isfinite(seconds) || seconds <= 0)
                return;
            opening = fmaxf(0, opening - seconds);
            cooldown = fmaxf(0, cooldown - seconds);
            hesitation = fmaxf(0, hesitation - seconds);
            disruptionCooldown = fmaxf(0, disruptionCooldown - seconds);
            hitFlash = fmaxf(0, hitFlash - seconds);
            breakFlash = fmaxf(0, breakFlash - seconds);
            const float recovering = fmaxf(0, quiet + seconds - settings.recoveryDelay) - fmaxf(0, quiet - settings.recoveryDelay);
            quiet = fminf(settings.recoveryDelay, quiet + seconds);
            pressure = fmaxf(0, pressure - recovering * settings.recoveryPerSecond * BehaviorCurve(morale, 0.5f, 1, 1.5f));
        }

        bool Block(bool charge, float threshold, const GuardPressureSettings& settings)
        {
            if (!settings.enabled || opening > 0 || cooldown > 0)
                return false;
            quiet = 0;
            hitFlash = 0.15f;
            pressure += charge ? settings.chargePressure : settings.normalPressure;
            return pressure >= threshold;
        }

        void Break(const GuardPressureSettings& settings)
        {
            pressure = 0;
            quiet = 0;
            opening = settings.openingSeconds;
            cooldown = settings.openingSeconds + settings.breakCooldown;
            hitFlash = 0;
            breakFlash = settings.indicatorSeconds;
        }

        bool Disrupt(float morale, const GuardPressureSettings& settings)
        {
            if (!settings.disruption || disruptionCooldown > 0)
                return false;
            hesitation = settings.disruptionSeconds * BehaviorCurve(morale, 1.5f, 1, 0.65f);
            disruptionCooldown = hesitation + settings.disruptionCooldown;
            return hesitation > 0;
        }
    };
}
