#pragma once

#include "Model.h"

namespace MoraleMatters
{
    enum class MoraleReason : int { None, BattleEvent, Casualties, OfficerDefeated, Panic, Recovery, Reinforcements, Surprise, Support, SupportExpired, Rescue, OffscreenVictory, CaptainDefeated, EnemyProximity, EnemySeparated };

    struct ExperienceSettings
    {
        bool rescue = true;
        float rescueReward = 10;
        float rescueBattleCap = 50;
    };

    struct CorpsExperience
    {
        float seconds = 0;
        float reasonTime = -100;
        float amount = 0;
        MoraleReason reason = MoraleReason::None;
        bool rewarded = false;
    };

    struct GroupExperience
    {
        int casualties = 0;
        int breaks = 0;
    };

    extern ExperienceSettings Experience;
    CorpsExperience ReadCorpsExperience(int id);
    GroupExperience ReadGroupExperience(int id);
    float ExperienceTime();
    unsigned long ExperienceGeneration();
    float CompleteRescue(int group);
}
