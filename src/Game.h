#pragma once

#include <windows.h>
#include <stdint.h>
#include "GameBuild.h"
#include "Model.h"
#include "BehaviorModel.h"
#include "GuardPressureModel.h"
#include "KillRewardModel.h"
#include "Experience.h"

namespace MoraleMatters
{
    extern Settings Config;
    extern BehaviorSettings Behavior;
    extern GuardPressureSettings GuardPressure;
    extern OffscreenKillSettings KillRewards;
    extern uint8_t* Image;
    extern CorpsState Corps[MaxCorps];
    extern EncounterState Encounters[MaxGroups];

    void Log(const char* format, ...);
    bool Readable(const void* address, size_t bytes);
    bool InitializeGameHooks();
    int WeaponFusionMenu(void* manager, bool toggle, int* stock = nullptr);
    bool BodyguardFusionAvailable();
    bool AddBodyguardFusionRow(void* manager, void* row);
    void ResetBattle(void* manager);
    void* Manager();
    void* Group(int id);
    void* CorpsObject(int id);
    bool RefreshBattle();
    int UpdateCharacterRates(void* manager, int* supportIndices, int capacity);
    int GroupCount();
    int CorpsCount();
    bool GroupAlive(void* group);
    int GroupIdFromCharacter(void* character);
    int MemberIdFromCharacter(void* character);
    float FrequencyForGroup(int id, bool officer);
    bool ProbeOnscreenHit(void* attacker, void* defender, float motion, float& nativeDamage, float& scaledDamage, float& outgoing, float& incoming);
    float Encounter(int group, float distance, float hp, float maximum, bool& rallied);
    bool SetCharacterHealth(void* character, float value);
    void ClassifyUnit(int unit, bool elite);
    int RegroupPhase(int group);
    int InfantryBreakPhase(int group, int member);
    bool SetInfantryControl(void* character, int object, bool acquire, bool disarm);
    int CorpsRallyPhase(int corps);

    struct GuardFeedback
    {
        uint64_t token;
        int group;
        int member;
        double x, y, z;
        float pressure, threshold, opening, hitFlash, breakFlash;
        unsigned long breakSequence;
        uint64_t hitSequence;
        int objectIndex;
        bool refilled;
    };
    int CollectGuardFeedback();
    bool ReadGuardFeedback(int index, GuardFeedback& result);

    template <typename T> inline T& At(void* object, size_t offset)
    {
        return *reinterpret_cast<T*>(static_cast<uint8_t*>(object) + offset);
    }
}
