#pragma once

#include <windows.h>
#include <stdint.h>
#include "Model.h"
#include "BehaviorModel.h"
#include "KillRewardModel.h"
#include "Experience.h"

namespace MoraleMatters
{
    extern Settings Config;
    extern BehaviorSettings Behavior;
    extern OffscreenKillSettings KillRewards;
    extern uint8_t* Image;
    extern CorpsState Corps[MaxCorps];
    extern EncounterState Encounters[MaxGroups];

    void Log(const char* format, ...);
    bool Readable(const void* address, size_t bytes);
    bool InitializeGameHooks();
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

    template <typename T> inline T& At(void* object, size_t offset)
    {
        return *reinterpret_cast<T*>(static_cast<uint8_t*>(object) + offset);
    }
}
