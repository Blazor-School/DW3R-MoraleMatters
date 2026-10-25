#include "Game.h"
#include <string.h>

// No second Lua runtime is linked into the game. Resolve the installed UE4SS runtime's
// C API through its exported, validated LuaMadeSimple wrapper thunks.
namespace
{
    struct lua_State;
    using ToIntegerFn = int64_t (*)(lua_State*, int, int*);
    using ToNumberFn = double (*)(lua_State*, int, int*);
    using PushIntegerFn = void (*)(lua_State*, int64_t);
    using PushNumberFn = void (*)(lua_State*, double);
    int SupportIndices[MoraleMatters::MaxGroups];
    int SupportCount = 0;
    ToIntegerFn ToInteger = nullptr;
    ToNumberFn ToNumber = nullptr;
    PushIntegerFn PushInteger = nullptr;
    PushNumberFn PushNumber = nullptr;

    void* RelativeTarget(uint8_t* instruction)
    {
        int32_t offset;
        memcpy(&offset, instruction + 1, sizeof(offset));
        return instruction + 5 + offset;
    }

    bool BindLua()
    {
        if (ToInteger && ToNumber && PushInteger && PushNumber)
            return true;
        HMODULE module = GetModuleHandleW(L"UE4SS.dll");
        if (!module)
            return false;
        auto* integer = reinterpret_cast<uint8_t*>(GetProcAddress(module, "?get_integer@Lua@LuaMadeSimple@RC@@QEBA_JH@Z"));
        auto* number = reinterpret_cast<uint8_t*>(GetProcAddress(module, "?get_number@Lua@LuaMadeSimple@RC@@QEBANH@Z"));
        auto* pushInteger = reinterpret_cast<uint8_t*>(GetProcAddress(module, "?set_integer@Lua@LuaMadeSimple@RC@@QEBAX_J@Z"));
        auto* pushNumber = reinterpret_cast<uint8_t*>(GetProcAddress(module, "?set_number@Lua@LuaMadeSimple@RC@@QEBAXN@Z"));
        if (!integer || !number || !pushInteger || !pushNumber || integer[0x1A] != 0xE8 || number[0x1A] != 0xE8 || memcmp(pushInteger, "\x48\x8B\x09\xE9", 4) || memcmp(pushNumber, "\x48\x8B\x09\xE9", 4))
            return false;
        ToInteger = reinterpret_cast<ToIntegerFn>(RelativeTarget(integer + 0x1A));
        ToNumber = reinterpret_cast<ToNumberFn>(RelativeTarget(number + 0x1A));
        PushInteger = reinterpret_cast<PushIntegerFn>(RelativeTarget(pushInteger + 3));
        PushNumber = reinterpret_cast<PushNumberFn>(RelativeTarget(pushNumber + 3));
        return true;
    }

    void Number(lua_State* lua, double value) { PushNumber(lua, value); }
    int64_t Integer(lua_State* lua, int index) { return ToInteger(lua, index, nullptr); }
    double Numeric(lua_State* lua, int index) { return ToNumber(lua, index, nullptr); }
}

extern "C" __declspec(dllexport) LONG MoraleMatters_GetStatus();

extern "C" __declspec(dllexport) int MoraleMatters_Query(lua_State* lua)
{
    using namespace MoraleMatters;
    if (!BindLua())
        return 0;
    const int operation = static_cast<int>(Integer(lua, 1));
    if (operation == 0)
    {
        PushInteger(lua, MoraleMatters_GetStatus());
        return 1;
    }
    if (MoraleMatters_GetStatus() != 1)
        return 0;
    if (operation == 1)
    {
        ResetBattle(reinterpret_cast<void*>(Integer(lua, 2)));
        return 0;
    }
    if (operation == 2)
    {
        void* group = Group(static_cast<int>(Integer(lua, 2)));
        if (!group)
            return 0;
        Number(lua, At<int>(group, 0x38));
        Number(lua, At<uint16_t>(group, 0x2D0C));
        Number(lua, At<uint8_t>(group, 0x2D20));
        Number(lua, At<int>(group, 0x40));
        const int attribute = At<uint8_t>(group, 0x2DFD);
        Number(lua, GroupAlive(group) && !At<uint8_t>(group, 0x191) && !At<uint8_t>(group, 0x198) && !(attribute <= 6 && (0x56 & (1 << attribute))) ? 1 : 0);
        float health = At<float>(group, 0x2DEC);
        void* character = At<void*>(group, 0x60);
        if (Readable(character, 0x8B2) && GroupIdFromCharacter(character) == static_cast<int>(Integer(lua, 2)) && MemberIdFromCharacter(character) == -1 && Readable(At<void*>(character, 0x698), 0x50))
        {
            using HealthFn = float (*)(void*);
            const float live = reinterpret_cast<HealthFn>(Image + 0x17481D0)(character);
            if (isfinite(live))
                health = live;
        }
        Number(lua, health);
        Number(lua, At<int>(group, 0x2E10));
        Number(lua, At<double>(group, 0x2DA8));
        Number(lua, At<double>(group, 0x2DB0));
        Number(lua, At<double>(group, 0x2DB8));
        Number(lua, At<uint8_t>(group, 0x2D61));
        const auto experience = ReadGroupExperience(static_cast<int>(Integer(lua, 2)));
        Number(lua, experience.casualties);
        Number(lua, experience.breaks);
        Number(lua, RegroupPhase(static_cast<int>(Integer(lua, 2))));
        return 14;
    }
    if (operation == 3)
    {
        const int id = static_cast<int>(Integer(lua, 2));
        void* object = CorpsObject(id);
        if (!object)
            return 0;
        Corps[id].Observe(At<float>(object, 0x28));
        Number(lua, At<int>(object, 0x14));
        Number(lua, At<int>(object, 0xC));
        Number(lua, Corps[id].base);
        Number(lua, At<float>(object, 0x28));
        Number(lua, Corps[id].VisibleBonus());
        Number(lua, Corps[id].remaining);
        const auto experience = ReadCorpsExperience(id);
        Number(lua, static_cast<int>(experience.reason));
        Number(lua, experience.amount);
        Number(lua, experience.seconds - experience.reasonTime);
        Number(lua, experience.seconds);
        Number(lua, Corps[id].proximityBonus > 0 ? 1 : 0);
        return 11;
    }
    if (operation == 4)
    {
        const int id = static_cast<int>(Integer(lua, 2));
        bool rallied = false;
        float healed = Encounter(id, static_cast<float>(Numeric(lua, 3)), static_cast<float>(Numeric(lua, 4)), static_cast<float>(Numeric(lua, 5)), rallied);
        if (healed > 0)
        {
            void* character = reinterpret_cast<void*>(Integer(lua, 6));
            const float previous = static_cast<float>(Numeric(lua, 4));
            if (!SetCharacterHealth(character, previous + healed))
            {
                Encounters[id].cooldown = 0;
                healed = 0;
                Log("Native NPC health setter did not confirm healing; cooldown was not consumed.");
            }
        }
        Number(lua, rallied ? 1 : 0);
        Number(lua, healed);
        return 2;
    }
    if (operation == 5)
    {
        void* character = reinterpret_cast<void*>(Integer(lua, 2));
        const int id = GroupIdFromCharacter(character);
        void* group = Group(id);
        if (!group)
            return 0;
        const int member = At<int>(character, 0x7A4);
        void* object = CorpsObject(At<int>(group, 0x38));
        const int rank = At<uint8_t>(group, 0x2D20);
        const bool named = member == -1 && rank >= 2 && rank <= 5;
        const bool player = At<uint8_t>(character, 0x74C) < 2;
        const float morale = object ? At<float>(object, 0x28) : 400;
        Number(lua, id);
        Number(lua, member);
        Number(lua, At<int>(group, 0x38));
        Number(lua, At<int>(group, 0x40));
        Number(lua, Config.attributes && !player ? ScaleForOfficer(Interpolate(morale, Config.anchors, Config.outgoing), named, Config) : 1);
        Number(lua, Config.attributes && !player ? ScaleForOfficer(Interpolate(morale, Config.anchors, Config.incoming), named, Config) : 1);
        Number(lua, FrequencyForGroup(id, named));
        Number(lua, named ? 1 : 0);
        return 8;
    }
    if (operation == 6)
    {
        ClassifyUnit(static_cast<int>(Integer(lua, 2)), Integer(lua, 3) != 0);
        return 0;
    }
    if (operation == 7)
    {
        if (!RefreshBattle())
            return 0;
        Number(lua, GroupCount());
        Number(lua, CorpsCount());
        return 2;
    }
    if (operation == 8)
    {
        SupportCount = UpdateCharacterRates(reinterpret_cast<void*>(Integer(lua, 2)), SupportIndices, MaxGroups);
        // Lua guarantees only LUA_MINSTACK free slots on C entry. Return one index per
        // call instead of pushing a variable-sized array without lua_checkstack.
        Number(lua, SupportCount);
        return 1;
    }
    if (operation == 9)
    {
        const int index = static_cast<int>(Integer(lua, 2));
        if (index >= 1 && index <= SupportCount)
        {
            Number(lua, SupportIndices[index - 1]);
            return 1;
        }
    }
    if (operation == 10)
    {
        LARGE_INTEGER counter, frequency;
        QueryPerformanceCounter(&counter);
        QueryPerformanceFrequency(&frequency);
        Number(lua, static_cast<double>(counter.QuadPart) * 1000.0 / static_cast<double>(frequency.QuadPart));
        return 1;
    }
    if (operation == 11)
    {
        float nativeDamage = 0, scaledDamage = 0, outgoing = 1, incoming = 1;
        if (!ProbeOnscreenHit(reinterpret_cast<void*>(Integer(lua, 2)), reinterpret_cast<void*>(Integer(lua, 3)), static_cast<float>(Numeric(lua, 4)), nativeDamage, scaledDamage, outgoing, incoming))
            return 0;
        Number(lua, nativeDamage);
        Number(lua, scaledDamage);
        Number(lua, outgoing);
        Number(lua, incoming);
        return 4;
    }
    if (operation == 12)
    {
        Number(lua, ExperienceTime());
        Number(lua, ExperienceGeneration());
        return 2;
    }
    if (operation == 13)
    {
        Number(lua, CompleteRescue(static_cast<int>(Integer(lua, 2))));
        return 1;
    }
    return 0;
}
