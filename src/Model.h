#pragma once

#include <math.h>
#include <stdint.h>

namespace MoraleMatters
{
    constexpr int MaxCorps = 512;
    constexpr int MaxGroups = 4096;
    constexpr float MoraleMinimum = 1.0f;
    constexpr float MoraleMaximum = 899.0f;

    inline float Clamp(float value, float minimum, float maximum)
    {
        return value < minimum ? minimum : value > maximum ? maximum : value;
    }

    struct TrackedRate
    {
        bool initialized = false;
        float baseline = 0;
        float written = 0;

        void Apply(float& current, float multiplier)
        {
            if (!isfinite(current) || !isfinite(multiplier))
                return;
            if (!initialized || fabsf(current - written) > 0.0001f)
                baseline = current;
            const float wanted = baseline * multiplier;
            if (fabsf(current - wanted) > 0.0001f)
                current = wanted;
            written = wanted;
            initialized = true;
        }
    };

    struct Settings
    {
        bool enabled = true;
        bool offscreen = true;
        bool support = true;
        bool casualties = true;
        bool attributes = true;
        bool reinforcements = true;
        bool surpriseForces = true;
        bool enemyProximity = true;
        bool meatBuns = true;
        int dropComboHits = 12;
        int meatBunPercent = 80;
        float enemyProximityRadius = 3000.0f;
        float enemyProximityBonus = 100.0f;
        float reinforcementGain = 10.0f;
        float reinforcementCap = 50.0f;
        float reinforcementWaveSeconds = 3.0f;
        float officerDamageShare = 0.25f;
        float encounterRadius = 1000.0f;
        float exitRadius = 1500.0f;
        float rallyBonus = 100.0f;
        float rallyDuration = 180.0f;
        float healFraction = 0.5f;
        float healCooldown = 180.0f;
        float soldierLoss = 1.0f;
        float eliteLoss = 2.0f;
        float captainLoss = 10.0f;
        float officerLoss = 50.0f;
        float namedOfficerScale = 0.5f;
        float anchors[5] = {100, 200, 400, 600, 800};
        float outgoing[5] = {0.60f, 0.80f, 1.0f, 1.25f, 1.50f};
        float incoming[5] = {1.30f, 1.15f, 1.0f, 0.85f, 0.70f};
        float frequency[5] = {0.65f, 0.80f, 1.0f, 1.25f, 1.50f};
    };

    struct CorpsState
    {
        bool initialized = false;
        int depth = 0;
        float base = 0;
        float bonus = 0;
        float remaining = 0;
        float lastWritten = 0;
        float nativeGains = 0;
        float proximityBonus = 0;
        float proximitySample = 1;

        void CaptureBase(float current)
        {
            const float next = Clamp(current, MoraleMinimum, MoraleMaximum);
            nativeGains += fmaxf(0, next - base);
            base = next;
        }

        float Effective() const { return Clamp(base + (remaining > 0 ? bonus : 0) + proximityBonus, MoraleMinimum, MoraleMaximum); }
        float VisibleBonus() const { return Effective() - Clamp(base, MoraleMinimum, MoraleMaximum); }

        void Observe(float current)
        {
            if (!initialized)
            {
                initialized = true;
                base = Clamp(current, MoraleMinimum, MoraleMaximum);
                lastWritten = base;
            }
            else if (depth == 0 && isfinite(current))
            {
                // Also reconcile direct scripted writes that bypass the game's morale functions.
                CaptureBase(base + current - lastWritten);
                lastWritten = current;
            }
        }

        void Begin(float& nativeMorale)
        {
            if (depth == 0)
            {
                Observe(nativeMorale);
                nativeMorale = base;
            }
            else
                CaptureBase(nativeMorale);
            ++depth;
        }

        void End(float& nativeMorale)
        {
            CaptureBase(nativeMorale);
            --depth;
            if (depth == 0)
            {
                lastWritten = Effective();
                nativeMorale = lastWritten;
            }
        }

        void Advance(float seconds, float& nativeMorale)
        {
            Observe(nativeMorale);
            remaining = seconds >= remaining ? 0 : remaining - seconds;
            if (remaining == 0)
                bonus = 0;
            lastWritten = Effective();
            nativeMorale = lastWritten;
        }

        bool Rally(float& nativeMorale, const Settings& settings)
        {
            Observe(nativeMorale);
            if (remaining > 0 || settings.rallyBonus <= 0 || settings.rallyDuration <= 0 || base >= MoraleMaximum)
                return false;
            bonus = settings.rallyBonus;
            remaining = settings.rallyDuration;
            lastWritten = Effective();
            nativeMorale = lastWritten;
            return true;
        }

        void SetProximity(float& nativeMorale, bool nearby, const Settings& settings)
        {
            Observe(nativeMorale);
            proximityBonus = nearby && settings.enemyProximity ? fmaxf(0, settings.enemyProximityBonus) : 0;
            lastWritten = Effective();
            nativeMorale = lastWritten;
        }

        float AddPermanent(float& nativeMorale, float amount)
        {
            Observe(nativeMorale);
            const float before = base;
            base = Clamp(base + fmaxf(0, amount), MoraleMinimum, MoraleMaximum);
            lastWritten = Effective();
            nativeMorale = lastWritten;
            return base - before; // Mod grants never enter the native-credit ledger.
        }

        float RemovePermanent(float& nativeMorale, float amount)
        {
            Observe(nativeMorale);
            const float before = base;
            base = Clamp(base - fmaxf(0, amount), MoraleMinimum, MoraleMaximum);
            lastWritten = Effective();
            nativeMorale = lastWritten;
            return before - base;
        }
    };

    struct ReinforcementState
    {
        bool open = false;
        bool surprise = false;
        int beforeSoldiers = 0;
        int arrivedSoldiers = 0;
        float remaining = 0;
        float age = 0;
        float observedNative = 0;
        float recentNative = 0;
        float recentRemaining = 0;
        float nativeCredit = 0;
        float target = 0;

        void CaptureNative(float total, const Settings& settings)
        {
            const float gain = fmaxf(0, total - observedNative);
            observedNative = total;
            if (gain <= 0)
                return;
            if (open)
                nativeCredit += gain;
            else
            {
                if (recentRemaining <= 0)
                    recentNative = 0;
                recentNative += gain;
                recentRemaining = settings.reinforcementWaveSeconds;
            }
        }

        void Begin(int soldiers, const Settings& settings)
        {
            if (open)
                return;
            open = true;
            surprise = false;
            beforeSoldiers = soldiers;
            arrivedSoldiers = 0;
            remaining = settings.reinforcementWaveSeconds;
            age = 0;
            nativeCredit = recentRemaining > 0 ? recentNative : 0;
            recentNative = recentRemaining = target = 0;
        }

        void Arrive(int soldiers, const Settings& settings, bool isSurprise = false)
        {
            arrivedSoldiers += soldiers;
            surprise = surprise || isSurprise;
            remaining = settings.reinforcementWaveSeconds;
        }

        bool Advance(float seconds, bool pending, const Settings& settings, float& gain)
        {
            gain = 0;
            if (!isfinite(seconds) || seconds <= 0)
                return false;
            if (!open)
            {
                recentRemaining = fmaxf(0, recentRemaining - seconds);
                return false;
            }
            age += seconds;
            if (pending)
            {
                remaining = settings.reinforcementWaveSeconds;
                if (age < 30)
                    return false;
                open = false; // An unsuccessful activation must never earn a reward.
                return true;
            }
            remaining = surprise ? 0 : fmaxf(0, remaining - seconds);
            if (remaining > 0)
                return false;
            open = false;
            if (!surprise && settings.reinforcements && beforeSoldiers > 0 && arrivedSoldiers > 0)
                target = fminf(settings.reinforcementCap, settings.reinforcementGain * 10 * static_cast<float>(arrivedSoldiers) / beforeSoldiers);
            gain = fmaxf(0, target - nativeCredit);
            return true;
        }
    };

    struct EncounterState
    {
        bool inside = false;
        float cooldown = 0;

        bool Enter(float distance, const Settings& settings)
        {
            if (inside)
            {
                if (distance > settings.exitRadius)
                    inside = false;
                return false;
            }
            if (distance > settings.encounterRadius)
                return false;
            inside = true;
            return true;
        }

        void Advance(float seconds) { cooldown = seconds >= cooldown ? 0 : cooldown - seconds; }

        float Heal(float hp, float maximum, const Settings& settings)
        {
            if (cooldown > 0 || hp <= 0 || maximum <= hp || settings.healFraction <= 0)
                return 0;
            const float amount = fminf(maximum - hp, maximum * settings.healFraction);
            if (amount > 0)
                cooldown = settings.healCooldown;
            return amount;
        }
    };

    inline float Interpolate(float morale, const float (&anchors)[5], const float (&values)[5])
    {
        if (morale <= anchors[0])
            return values[0];
        for (int index = 1; index < 5; ++index)
        {
            if (morale <= anchors[index])
            {
                const float fraction = (morale - anchors[index - 1]) / (anchors[index] - anchors[index - 1]);
                return values[index - 1] + fraction * (values[index] - values[index - 1]);
            }
        }
        return values[4];
    }

    inline float ScaleForOfficer(float multiplier, bool namedOfficer, const Settings& settings)
    {
        return 1 + (multiplier - 1) * (namedOfficer ? settings.namedOfficerScale : 1);
    }

    inline float OfficerLeak(float damage, float currentHp, bool protectedByScript, const Settings& settings)
    {
        const float floor = protectedByScript ? 1.0f : 0.0f;
        return Clamp(damage * settings.officerDamageShare, 0, fmaxf(0, currentHp - floor));
    }
}
