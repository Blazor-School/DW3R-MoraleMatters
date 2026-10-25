#pragma once

#include "Model.h"

namespace MoraleMatters
{
    struct BehaviorSettings
    {
        bool enabled = true;
        bool profiles = true;
        bool cohesion = true;
        bool regroup = true;
        bool panic = true;
        bool recovery = true;
        bool leadership = true;
        bool withdrawal = false;
        float withdrawalScale = 0.45f;
        float withdrawalDistance = 800;
        float lowProfileShift = -3;
        float highProfileShift = 2;
        float lowFormationScale = 1.35f;
        float highFormationScale = 0.80f;
        float regroupBelow = 0.15f * MoraleMaximum;
        float regroupRecover = 0.30f * MoraleMaximum;
        float regroupSeconds = 8;
        float regroupCooldown = 20;
        float panicRadius = 2000;
        float panicLoss = 15;
        float panicWindow = 10;
        float panicCap = 30;
        float shockSeconds = 5;
        float recoveryPerSecond = 1;
        float recoveryCeiling = 400;
        float recoveryDelay = 10;
        float commanderRadius = 2000;
        float commanderRecovery = 2;
        float commanderPanic = 0.5f;
    };

    inline float BehaviorCurve(float morale, float low, float normal, float high)
    {
        const float anchors[5] = {100, 200, 400, 600, 800};
        const float values[5] = {low, (low + normal) * 0.5f, normal, (normal + high) * 0.5f, high};
        return Interpolate(morale, anchors, values);
    }

    inline int MoraleProfile(int rank, float morale, bool officer, const Settings& settings, const BehaviorSettings& behavior)
    {
        if (rank < 0 || rank >= 36)
            return rank;
        const float strength = officer ? settings.namedOfficerScale : 1;
        const int shift = static_cast<int>(roundf(BehaviorCurve(morale, behavior.lowProfileShift, 0, behavior.highProfileShift) * strength));
        // Bodyguard profiles are separate four-entry families, not ordinary rank upgrades.
        const int minimum = rank < 20 ? 0 : 20 + ((rank - 20) / 4) * 4;
        const int maximum = rank < 20 ? 19 : minimum + 3;
        return static_cast<int>(Clamp(static_cast<float>(rank + shift), static_cast<float>(minimum), static_cast<float>(maximum)));
    }

    struct GroupBehaviorState
    {
        float regroupRemaining = 0;
        float cooldown = 0;
        float shockRemaining = 0;
        float regroupDuration = 0;
        bool commanderNear = false;

        void Advance(float seconds, float morale, bool eligible, const BehaviorSettings& settings)
        {
            if (!isfinite(seconds) || seconds <= 0)
                return;
            cooldown = fmaxf(0, cooldown - seconds);
            shockRemaining = fmaxf(0, shockRemaining - seconds);
            regroupRemaining = fmaxf(0, regroupRemaining - seconds);
            if (!eligible || !settings.enabled || !settings.regroup || morale >= settings.regroupRecover)
            {
                regroupRemaining = 0;
                return;
            }
            if (cooldown == 0 && (morale <= settings.regroupBelow || shockRemaining > 0))
            {
                regroupRemaining = settings.regroupSeconds;
                regroupDuration = settings.regroupSeconds;
                cooldown = settings.regroupSeconds + settings.regroupCooldown;
            }
        }
    };

    struct CorpsBehaviorState
    {
        float sampleTime = 0;
        float quietTime = 0;
        float panicRemaining = 0;
        float panicSpent = 0;

        float Panic(float amount, const BehaviorSettings& settings)
        {
            if (panicRemaining <= 0)
            {
                panicRemaining = settings.panicWindow;
                panicSpent = 0;
            }
            const float allowed = fminf(fmaxf(0, amount), fmaxf(0, settings.panicCap - panicSpent));
            panicSpent += allowed;
            return allowed;
        }

        float Advance(float seconds, bool combat, float morale, float nearFraction, const BehaviorSettings& settings)
        {
            panicRemaining = fmaxf(0, panicRemaining - seconds);
            quietTime = combat ? 0 : quietTime + seconds;
            if (!settings.enabled || !settings.recovery || quietTime < settings.recoveryDelay || morale >= settings.recoveryCeiling)
                return 0;
            const float leadership = settings.leadership ? 0.5f + Clamp(nearFraction, 0, 1) * (settings.commanderRecovery - 0.5f) : 1;
            const float confidence = BehaviorCurve(morale, 0.5f, 1, 1.25f);
            const float recoveringSeconds = fminf(seconds, fmaxf(0, quietTime - settings.recoveryDelay));
            return fminf(settings.recoveryCeiling - morale, recoveringSeconds * settings.recoveryPerSecond * leadership * confidence);
        }
    };
}
