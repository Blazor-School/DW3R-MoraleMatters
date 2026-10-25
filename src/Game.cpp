#include "Game.h"
#include "MinHook.h"
#include <string.h>

namespace MoraleMatters
{
    Settings Config;
    uint8_t* Image = nullptr;
    CorpsState Corps[MaxCorps];
    EncounterState Encounters[MaxGroups];
    static void* BattleManager = nullptr;
    static void* GroupStorage = nullptr;
    static void* CorpsStorage = nullptr;
    static int ValidGroups = 0;
    static int ValidCorps = 0;
    static int ObservedGroups = 0;
    static int ObservedCorps = 0;
    static bool EncounterLogged = false;
    static unsigned long DamageEvents = 0;
    static unsigned long CasualtyEvents = 0;
    static unsigned long WaitEvents = 0;
    static bool EliteUnits[65536] = {};
    static ReinforcementState Reinforcements[MaxCorps];
    struct ArrivalState { int corps; int side; bool seen; bool pending; int wave; bool surprise; };
    static ArrivalState Arrivals[MaxGroups] = {};
    struct ScenarioEvent { void* scenario; int cause; bool known; };
    static ScenarioEvent GroupEvents[MaxGroups] = {};
    struct EventCredit { void* scenario; int cause; int corps; float unused; };
    static EventCredit EventCredits[16384] = {};
    struct WaveRecord { float paid; };
    static WaveRecord WaveRecords[MaxGroups] = {};
    struct EventLink { int event; int wave; };
    static EventLink EventLinks[MaxGroups] = {};
    static int EventCount = 0;
    static int LinkCount = 0;
    static int WaveCount = 0;
    static int CurrentWaves[MaxCorps] = {};

    void ClassifyUnit(int unit, bool elite)
    {
        if (unit >= 0 && unit < 65536)
            EliteUnits[unit] = elite;
    }

    using TickFn = void (*)(void*, float);
    using AttackFn = void (*)(void*);
    using MoraleFn = void (*)(void*, float, int);
    using KillFn = void (*)(void*, uint8_t, int, int);
    using GroupDefeatFn = void (*)(void*, int);
    using SpecialAttributeFn = void (*)(void*, uint8_t);
    using ScenarioEffectFn = bool (*)(void*, int, int, uint8_t);
    static TickFn OriginalGroupTick = nullptr;
    static TickFn OriginalCorpsTick = nullptr;
    static TickFn OriginalAttackWait = nullptr;
    static TickFn OriginalSecondaryWait = nullptr;
    static AttackFn OriginalAttack = nullptr;
    static MoraleFn OriginalRaise = nullptr;
    static MoraleFn OriginalLower = nullptr;
    static KillFn OriginalKill = nullptr;
    static GroupDefeatFn NativeGroupDefeat = nullptr;
    static SpecialAttributeFn OriginalSpecialAttribute = nullptr;
    static ScenarioEffectFn OriginalScenarioEffect = nullptr;
    static void ResetBehavior();
    static void ResetKillRewards();
    static void ResetExperience();
    static void ChangeMorale(void* object, float amount, int cause, MoraleFn original);

    struct KillContext
    {
        void* corps = nullptr;
        float loss = 0;
        bool replaced = false;
        KillContext* previous = nullptr;
    };
    static thread_local KillContext* CurrentKill = nullptr;
    static thread_local int WaitDepth = 0;

    bool Readable(const void* address, size_t bytes)
    {
        if (!address || bytes == 0)
            return false;
        MEMORY_BASIC_INFORMATION region;
        if (!VirtualQuery(address, &region, sizeof(region)) || region.State != MEM_COMMIT || (region.Protect & (PAGE_NOACCESS | PAGE_GUARD)))
            return false;
        const uintptr_t end = reinterpret_cast<uintptr_t>(address) + bytes;
        return end >= reinterpret_cast<uintptr_t>(address) && end <= reinterpret_cast<uintptr_t>(region.BaseAddress) + region.RegionSize;
    }

    static void RefreshStorage()
    {
        GroupStorage = BattleManager ? At<void*>(BattleManager, 0x78) : nullptr;
        CorpsStorage = BattleManager ? At<void*>(BattleManager, 0x68) : nullptr;
        ObservedGroups = BattleManager ? At<int>(BattleManager, 0x80) : 0;
        ObservedCorps = BattleManager ? At<int>(BattleManager, 0x70) : 0;
        ValidGroups = ObservedGroups;
        ValidCorps = ObservedCorps;
        if (ValidGroups < 0 || ValidGroups > MaxGroups || !Readable(GroupStorage, static_cast<size_t>(ValidGroups) * 0x2E68))
            ValidGroups = 0;
        if (ValidCorps < 0 || ValidCorps > MaxCorps || !Readable(CorpsStorage, static_cast<size_t>(ValidCorps) * 0xEF8))
            ValidCorps = 0;
    }

    void ResetBattle(void* manager)
    {
        // Called on the game thread when a new battle HUD/formation is created.
        BattleManager = Readable(manager, 0x90) ? manager : nullptr;
        RefreshStorage();
        memset(Corps, 0, sizeof(Corps));
        memset(Encounters, 0, sizeof(Encounters));
        ResetBehavior();
        ResetKillRewards();
        ResetExperience();
        memset(Reinforcements, 0, sizeof(Reinforcements));
        memset(Arrivals, 0, sizeof(Arrivals));
        memset(GroupEvents, 0, sizeof(GroupEvents));
        memset(EventCredits, 0, sizeof(EventCredits));
        memset(WaveRecords, 0, sizeof(WaveRecords));
        memset(CurrentWaves, 0xFF, sizeof(CurrentWaves));
        EventCount = LinkCount = WaveCount = 0;
        for (int id = 0; id < GroupCount(); ++id)
        {
            void* group = Group(id);
            const int attribute = At<uint8_t>(group, 0x2DFD);
            Arrivals[id].seen = (attribute != 1 && attribute != 2) || At<uint8_t>(group, 0x110) || At<uint8_t>(group, 0x186);
        }
        EncounterLogged = false;
        Log("Battle state reset; groups=%d corps=%d.", GroupCount(), CorpsCount());
    }

    void* Manager() { return BattleManager; }

    // Formation storage is validated at binding/reallocation, on the game thread.
    // Native tick callbacks provide a live manager; avoid VirtualQuery per group per frame.
    int GroupCount() { return ValidGroups; }
    int CorpsCount() { return ValidCorps; }

    void* Group(int id)
    {
        return id >= 0 && id < ValidGroups ? static_cast<uint8_t*>(GroupStorage) + static_cast<size_t>(id) * 0x2E68 : nullptr;
    }

    void* CorpsObject(int id)
    {
        return id >= 0 && id < ValidCorps ? static_cast<uint8_t*>(CorpsStorage) + static_cast<size_t>(id) * 0xEF8 : nullptr;
    }

    static int CorpsId(void* object)
    {
        if (!object || !BattleManager || At<void*>(object, 0) != BattleManager)
            return -1;
        const intptr_t difference = reinterpret_cast<intptr_t>(object) - reinterpret_cast<intptr_t>(At<void*>(BattleManager, 0x68));
        const int id = static_cast<int>(difference / 0xEF8);
        return difference >= 0 && difference % 0xEF8 == 0 && id < CorpsCount() ? id : -1;
    }

    bool GroupAlive(void* group)
    {
        // 0x110 records leader removal. Native IsDead also handles player-owned groups.
        if (!group || At<uint8_t>(group, 0x110) || At<uint8_t>(group, 0x186))
            return false;
        using IsDeadFn = bool (*)(void*);
        return !reinterpret_cast<IsDeadFn>(Image + 0x13AE060)(group);
    }

    int GroupIdFromCharacter(void* character)
    {
        return Readable(character, 0x7A8) ? At<int>(character, 0x7A0) : -1;
    }

    int MemberIdFromCharacter(void* character)
    {
        return Readable(character, 0x7A8) ? At<int>(character, 0x7A4) : -2;
    }

    static void EnsureManager(void* manager)
    {
        if (!manager)
            return;
        if (manager != BattleManager)
            ResetBattle(manager);
        else if (At<void*>(manager, 0x78) != GroupStorage || At<void*>(manager, 0x68) != CorpsStorage || At<int>(manager, 0x80) != ObservedGroups || At<int>(manager, 0x70) != ObservedCorps)
            RefreshStorage(); // Reallocation must not reset bonuses or cooldowns.
    }

    bool RefreshBattle()
    {
        if (!Readable(BattleManager, 0x90))
            return false;
        EnsureManager(BattleManager);
        if (!ValidGroups || !ValidCorps)
            RefreshStorage();
        return ValidGroups > 0 && ValidCorps > 0;
    }

    #include "Experience.inc"
    #include "Behavior.inc"
    #include "OnscreenCombat.inc"
    #include "Supplies.inc"

    int UpdateCharacterRates(void* manager, int* supportIndices, int capacity)
    {
        if (!RefreshBattle() || !Readable(manager, 0x60))
            return 0;
        auto** characters = At<void**>(manager, 0x50);
        const int count = At<int>(manager, 0x58);
        if (count < 0 || count > 16384 || !Readable(characters, static_cast<size_t>(count) * sizeof(void*)))
            return 0;
        int selected = 0;
        for (int i = 0; i < count; ++i)
        {
            void* character = characters[i];
            if (!Readable(character, 0x7A8) || At<uint8_t>(character, 0x787))
                continue;
            void* group = Group(At<int>(character, 0x7A0));
            if (!group || !GroupAlive(group) || At<uint8_t>(group, 0x191) || At<uint8_t>(group, 0x198))
                continue;
            const int rank = At<uint8_t>(group, 0x2D20);
            const bool named = At<int>(character, 0x7A4) == -1 && rank >= 2 && rank <= 5;
            const bool player = At<uint8_t>(character, 0x74C) < 2;
            if ((player || (named && At<int>(group, 0x40) == 0)) && selected < capacity)
                supportIndices[selected++] = i + 1; // UE4SS TArray indexing is one-based.
        }
        return selected;
    }

    static int SoldierCount(void* group)
    {
        // Native 0x2D61 includes the leader; 0x192 uses the full formation while
        // soldier actors are being created. Neither actor visibility nor HP is a gain.
        const int members = At<uint8_t>(group, 0x192) ? At<int>(group, 0x2D18) : At<uint8_t>(group, 0x2D61);
        return members > 1 ? members - 1 : 0;
    }

    static int CorpsSoldiers(int corps)
    {
        int soldiers = 0;
        for (int id = 0; id < GroupCount(); ++id)
        {
            void* group = Group(id);
            const int attribute = At<uint8_t>(group, 0x2DFD);
            // Same inactive attributes excluded by native corps logic: surprise,
            // reinforcement, treason and replacement (bit mask 0x56).
            if (At<int>(group, 0x38) == corps && At<int>(group, 0x40) == At<int>(CorpsObject(corps), 0xC) && GroupAlive(group) && !At<uint8_t>(group, 0x198) && !(attribute <= 6 && (0x56 & (1 << attribute))))
                soldiers += SoldierCount(group);
        }
        return soldiers;
    }

    static int FindEvent(const ScenarioEvent& key, int corps)
    {
        for (int i = 0; i < EventCount; ++i)
            if (EventCredits[i].scenario == key.scenario && EventCredits[i].cause == key.cause && EventCredits[i].corps == corps)
                return i;
        if (!key.known || EventCount >= 16384)
            return -1;
        const int index = EventCount++;
        EventCredits[index] = {key.scenario, key.cause, corps, 0};
        return index;
    }

    static void LinkEvent(const ScenarioEvent& key, int corps, int wave)
    {
        if (!key.known || wave < 0)
            return;
        const int event = FindEvent(key, corps);
        if (event < 0)
            return;
        for (int i = 0; i < LinkCount; ++i)
            if (EventLinks[i].event == event && EventLinks[i].wave == wave)
                return;
        if (LinkCount < MaxGroups)
            EventLinks[LinkCount++] = {event, wave};
    }

    static void CreditWaveEvents(int wave, float& credit, float target)
    {
        float linked = 0;
        for (int i = 0; i < LinkCount; ++i)
            if (EventLinks[i].wave == wave)
                linked += EventCredits[EventLinks[i].event].unused;
        // Window credit may contain the same event gains: never add it twice.
        credit = fmaxf(credit, linked);
        float remaining = target;
        for (int i = 0; i < LinkCount && remaining > 0; ++i)
            if (EventLinks[i].wave == wave)
            {
                auto& unused = EventCredits[EventLinks[i].event].unused;
                const float used = fminf(unused, remaining);
                unused -= used;
                remaining -= used;
            }
    }

    static bool ScenarioEffect(void* effect, int cause, int parameter, uint8_t flag)
    {
        const int type = At<uint8_t>(effect, 0x18);
        if ((!Config.reinforcements && !Config.surpriseForces) || cause < 0 || type < 6 || type > 10 || !RefreshBattle())
            return OriginalScenarioEffect(effect, cause, parameter, flag);
        const ScenarioEvent key = {At<void*>(effect, 0), cause, true};
        if (type == 9 || type == 10)
        {
            auto* args = At<int*>(effect, 0x20);
            if (Readable(args, 12))
            {
                int selected = -1;
                for (int id = 0; id < GroupCount(); ++id)
                {
                    void* group = Group(id);
                    if (type == 9 && !Arrivals[id].seen && (At<uint8_t>(group, 0x2DFD) == 1 || At<uint8_t>(group, 0x2DFD) == 2) && At<int>(group, 0x40) == args[0] && At<uint8_t>(group, 0x2D20) != 0 && At<uint8_t>(group, 0x2DFD) == static_cast<uint8_t>(args[2]) && (At<int>(group, 0x44) == args[1] || At<int>(group, 0x2D1C) == args[1]))
                        GroupEvents[id] = key;
                    else if (type == 10 && !(At<int>(group, 0x40) == (args[0] == 0 ? 1 : 0) && At<uint8_t>(group, 0x2DFD) != 5) && ((args[1] == 255 && At<uint8_t>(group, 0x1E8) != 2) || (args[1] != 255 && At<int>(group, 0x44) == args[1])) && selected < 0)
                        selected = id;
                }
                if (selected >= 0)
                    for (int id = 0; id < GroupCount(); ++id)
                        if (!Arrivals[id].seen && (At<uint8_t>(Group(id), 0x2DFD) == 1 || At<uint8_t>(Group(id), 0x2DFD) == 2) && (id == selected || At<int>(Group(id), 0x48) == selected))
                            GroupEvents[id] = key;
            }
            return OriginalScenarioEffect(effect, cause, parameter, flag);
        }
        float before[MaxCorps];
        const int count = CorpsCount();
        for (int id = 0; id < count; ++id)
        {
            Corps[id].Observe(At<float>(CorpsObject(id), 0x28));
            before[id] = Corps[id].nativeGains;
        }
        const bool result = OriginalScenarioEffect(effect, cause, parameter, flag);
        for (int id = 0; id < count; ++id)
        {
            auto& morale = At<float>(CorpsObject(id), 0x28);
            auto& state = Corps[id];
            state.Observe(morale);
            float gain = state.nativeGains - before[id];
            if (gain <= 0)
                continue;
            const int event = FindEvent(key, id);
            if (event < 0)
                continue;
            EventCredits[event].unused += gain;
            auto& wave = Reinforcements[id];
            wave.CaptureNative(state.nativeGains, Config);
            for (int i = 0; i < LinkCount && gain > 0; ++i)
                if (EventLinks[i].event == event)
                {
                    auto& paid = WaveRecords[EventLinks[i].wave].paid;
                    const float refund = state.RemovePermanent(morale, fminf(paid, gain));
                    paid -= refund;
                    gain -= refund;
                    EventCredits[event].unused -= refund;
                    if (wave.open)
                        wave.nativeCredit = fmaxf(0, wave.nativeCredit - refund);
                    else
                        wave.recentNative = fmaxf(0, wave.recentNative - refund);
                    if (refund > 0)
                        Log("Delayed arrival morale reconciled: corps=%d event=%d replacedModGain=%.2f.", id, cause, refund);
                }
        }
        return result;
    }

    static void SpecialAttribute(void* group, uint8_t value)
    {
        EnsureManager(At<void*>(group, 0x28));
        const int id = At<int>(group, 0x3C);
        const int corps = At<int>(group, 0x38);
        const int side = At<int>(group, 0x40);
        const int attribute = At<uint8_t>(group, 0x2DFD);
        const bool surprise = attribute == 1;
        const bool enabled = surprise ? Config.surpriseForces : attribute == 2 && Config.reinforcements;
        const bool candidate = enabled && id >= 0 && id < GroupCount() && Group(id) == group && corps >= 0 && corps < CorpsCount() && side >= 0 && side < 2 && side == At<int>(CorpsObject(corps), 0xC) && !Arrivals[id].seen && value == 0 && !At<uint8_t>(group, 0x110) && !At<uint8_t>(group, 0x186);
        if (candidate)
        {
            Arrivals[id].seen = true; // Before the recursive native setter visits children.
            auto& state = Corps[corps];
            state.Observe(At<float>(CorpsObject(corps), 0x28));
            Reinforcements[corps].CaptureNative(state.nativeGains, Config);
            if (!Reinforcements[corps].open)
            {
                Reinforcements[corps].Begin(CorpsSoldiers(corps), Config);
                CurrentWaves[corps] = WaveCount < MaxGroups ? WaveCount++ : -1;
            }
        }
        OriginalSpecialAttribute(group, value);
        if (candidate && At<uint8_t>(group, 0x2DFD) == 0)
            Arrivals[id] = {corps, side, true, true, CurrentWaves[corps], surprise};
    }

    static void UpdateReinforcements(int corps, float dt, float& morale)
    {
        auto& wave = Reinforcements[corps];
        wave.CaptureNative(Corps[corps].nativeGains, Config);
        if (!wave.open)
        {
            float ignored;
            wave.Advance(dt, false, Config, ignored);
            return;
        }
        bool pending = false;
        for (int id = 0; id < GroupCount(); ++id)
        {
            auto& arrival = Arrivals[id];
            if (!arrival.pending || arrival.corps != corps)
                continue;
            void* group = Group(id);
            if (At<int>(group, 0x38) != corps || At<int>(group, 0x40) != arrival.side || At<int>(CorpsObject(corps), 0xC) != arrival.side || At<uint8_t>(group, 0x110) || At<uint8_t>(group, 0x186) || At<uint8_t>(group, 0x198) || At<uint8_t>(group, 0x2DFD) != 0)
                arrival.pending = false;
            else if (!At<uint8_t>(group, 0x191) && GroupAlive(group))
            {
                wave.Arrive(SoldierCount(group), Config, arrival.surprise);
                if (!GroupEvents[id].known)
                {
                    const int parent = At<int>(group, 0x48);
                    if (parent >= 0 && parent < GroupCount())
                        GroupEvents[id] = GroupEvents[parent];
                }
                LinkEvent(GroupEvents[id], corps, arrival.wave);
                arrival.pending = false;
            }
            else
                pending = true;
        }
        float gain;
        if (wave.Advance(dt, pending, Config, gain))
        {
            const int record = CurrentWaves[corps];
            if (wave.surprise && !pending)
            {
                // Earlier scripted gains are already in base; do not subtract them twice.
                // Link the paid remainder so late same-event gains also replace this top-up.
                gain = fmaxf(0, MoraleMaximum - Corps[corps].base);
                CreditWaveEvents(record, wave.nativeCredit, gain);
            }
            else
            {
                CreditWaveEvents(record, wave.nativeCredit, wave.target);
                gain = fmaxf(0, wave.target - wave.nativeCredit);
            }
            const float added = Corps[corps].AddPermanent(morale, gain);
            RecordMorale(corps, wave.surprise ? MoraleReason::Surprise : MoraleReason::Reinforcements, added);
            if (record >= 0)
                WaveRecords[record].paid = added;
            if (wave.surprise && !pending)
                Log("Surprise arrival: corps=%d morale=%.2f added=%.2f (maximum base morale; scripted gains reconciled).", corps, morale, added);
            else
                Log("Reinforcement wave: corps=%d before=%d arrived=%d target=%.2f nativeCredit=%.2f added=%.2f%s.", corps, wave.beforeSoldiers, wave.arrivedSoldiers, wave.target, wave.nativeCredit, added, pending ? " activation timed out" : "");
            if (pending)
                for (int id = 0; id < GroupCount(); ++id)
                    if (Arrivals[id].corps == corps)
                        Arrivals[id].pending = false;
        }
    }

    #include "NativeEntry.inc"
    #include "KillRewards.inc"
    #include "OffscreenDamage.inc"

    static void ResetKillRewards()
    {
        KillRewardGuard guard;
        memset(KillRewardBudgets, 0, sizeof(KillRewardBudgets));
        KillRewardEvents = 0;
        ++KillRewardGeneration;
    }

    static void* ProximityCommander(int corps)
    {
        void* object = CorpsObject(corps);
        if (!object || At<int>(object, 0xC) != 1)
            return nullptr;
        void* group = Group(At<int>(object, 0x14));
        return group && At<int>(group, 0x38) == corps && At<int>(group, 0x40) == 1 && At<uint8_t>(group, 0x2DFD) == 0 && !At<uint8_t>(group, 0x191) && !At<uint8_t>(group, 0x198) && GroupAlive(group) ? group : nullptr;
    }

    static void UpdateEnemyProximity(int id, float dt, float& morale)
    {
        auto& state = Corps[id];
        if (!Config.enemyProximity || !ProximityCommander(id))
        {
            const float before = morale;
            Settings disabled = Config;
            disabled.enemyProximity = false;
            state.SetProximity(morale, false, disabled);
            RecordMorale(id, MoraleReason::EnemySeparated, morale - before);
            return;
        }
        state.proximitySample += dt;
        if (state.proximitySample < 1)
            return;
        state.proximitySample = 0;
        void* commander = ProximityCommander(id);
        const double radius = static_cast<double>(Config.enemyProximityRadius) * Config.enemyProximityRadius;
        bool nearby = false;
        for (int other = 0; other < CorpsCount() && !nearby; ++other)
        {
            void* candidate = other != id ? ProximityCommander(other) : nullptr;
            if (candidate)
                nearby = GroupDistanceSquared(commander, candidate) <= radius;
        }
        const float before = morale;
        state.SetProximity(morale, nearby, Config);
        RecordMorale(id, nearby ? MoraleReason::EnemyProximity : MoraleReason::EnemySeparated, morale - before);
    }

    static void CorpsTick(void* object, float dt)
    {
        EnsureManager(At<void*>(object, 0));
        const int id = CorpsId(object);
        if (id < 0)
        {
            OriginalCorpsTick(object, dt);
            return;
        }
        auto& state = Corps[id];
        auto& morale = At<float>(object, 0x28);
        state.Begin(morale);
        const float beforeNativeTick = morale;
        OriginalCorpsTick(object, dt);
        state.End(morale);
        RecordMorale(id, MoraleReason::BattleEvent, state.base - beforeNativeTick);
        if (isfinite(dt) && dt > 0)
        {
            AdvanceExperience(id, dt);
            const float beforeExpiry = morale;
            state.Advance(dt, morale);
            RecordMorale(id, MoraleReason::SupportExpired, morale - beforeExpiry);
            UpdateEnemyProximity(id, dt, morale);
            {
                KillRewardGuard guard;
                KillRewardBudgets[id].Advance(dt);
            }
            if (Config.reinforcements || Config.surpriseForces)
                UpdateReinforcements(id, dt, morale);
            UpdateCorpsBehavior(id, dt, morale);
        }
    }

    static void ChangeMorale(void* object, float amount, int cause, MoraleFn original)
    {
        EnsureManager(At<void*>(object, 0));
        const int id = CorpsId(object);
        if (id < 0)
        {
            original(object, amount, cause);
            return;
        }
        auto& state = Corps[id];
        auto& morale = At<float>(object, 0x28);
        state.Begin(morale);
        const float before = morale;
        original(object, amount, cause);
        state.End(morale);
        if (state.depth == 0 || CurrentMoraleReason != MoraleReason::None)
            RecordMorale(id, CurrentMoraleReason == MoraleReason::None ? MoraleReason::BattleEvent : CurrentMoraleReason, state.base - before);
    }

    static void Raise(void* object, float amount, int cause) { ChangeMorale(object, amount, cause, OriginalRaise); }

    static void Lower(void* object, float amount, int cause)
    {
        if (Config.casualties && CurrentKill && CurrentKill->corps == object)
        {
            if (CurrentKill->replaced)
                return;
            CurrentKill->replaced = true;
            amount = CurrentKill->loss;
        }
        ChangeMorale(object, amount, cause, OriginalLower);
    }

    static void GroupTick(void* object, float dt)
    {
        EnsureManager(At<void*>(object, 0x28));
        const int id = At<int>(object, 0x3C);
        if (id >= 0 && id < MaxGroups && isfinite(dt) && dt > 0)
        {
            Encounters[id].Advance(dt);
            const int attribute = At<uint8_t>(object, 0x2DFD);
            if (!Arrivals[id].pending && attribute != 1 && attribute != 2)
                Arrivals[id].seen = true;
        }
        OriginalGroupTick(object, dt);
        AdvanceGroupBehavior(object, dt);
    }

    static void Attack(void* attacker)
    {
        EnsureManager(At<void*>(attacker, 0x28));
        void* target = Group(At<int>(attacker, 0x180));
        const float before = target ? At<float>(target, 0x2DF4) : 0;
        OriginalAttack(attacker);
        if (!Config.offscreen || !target)
            return;
        KillRewardGuard guard; // Extra HP damage and lethal transitions share one transaction.
        if (!RewardFormation(target) || !RewardFormation(attacker) || At<int>(attacker, 0x40) == At<int>(target, 0x40) || At<uint8_t>(target, 0x187) || At<uint8_t>(target, 0x192) || At<uint8_t>(target, 0x199) || !GroupAlive(target))
            return;
        const int rank = At<uint8_t>(target, 0x2D20);
        if (rank < 2 || rank > 5 || At<uint8_t>(target, 0x2D61) <= 1)
            return; // A lone leader already receives the native proportional damage.
        const float damage = before - At<float>(target, 0x2DF4);
        if (damage <= 0 || !isfinite(damage))
            return;
        float& hp = At<float>(target, 0x2DEC);
        if (!isfinite(hp) || hp <= 0)
            return;
        const bool protectedOfficer = At<uint8_t>(target, 0x2E3A) || (At<int>(target, 0x40) == 1 && rank == 2);
        const float leak = OfficerLeak(damage, hp, protectedOfficer, Config);
        if (leak > 0 && leak >= hp)
        {
            // IsDead observes HP, not the defeat flag. Keep HP positive until the
            // native transaction has invoked the leader's normal casualty callback.
            if (NativeGroupDefeat && At<int>(target, 0x1D0) == At<int>(attacker, 0x3C) && ValidDefeatChildren(target))
                ResolveOffscreen(target, DefeatOffscreenFormation);
            else
                hp = fminf(hp, 1.0f); // Unsafe attribution/storage must never create an unreported death.
        }
        else
            hp -= leak;
        // Group HP already includes leader HP. Lowering both the pool by D and the leader by L
        // leaves D-L for native troop attrition, without charging L again as soldier casualties.
        if (leak > 0 && (++DamageEvents <= 5 || DamageEvents % 200 == 0))
            Log("Off-screen officer damage: group=%d damage=%.2f officer=%.2f remaining=%.2f.", At<int>(target, 0x3C), damage, leak, hp);
    }

    static bool MemberDead(void* group, int member)
    {
        if (member < 0 || member >= At<int>(group, 0x70))
            return true;
        if (member < At<int>(group, 0x120))
        {
            auto* flags = At<uint8_t*>(group, 0x118);
            if (Readable(flags, static_cast<size_t>(member) + 1) && flags[member])
                return true;
        }
        return false;
    }

    static void Kill(void* group, uint8_t leader, int member, int flags)
    {
        EnsureManager(At<void*>(group, 0x28));
        void* corps = CorpsObject(At<int>(group, 0x38));
        const bool wasDead = leader ? At<uint8_t>(group, 0x110) != 0 : MemberDead(group, member);
        const bool commander = corps && leader && At<int>(corps, 0x14) == At<int>(group, 0x3C);
        const bool panicEligible = leader && corps && !wasDead && !commander && (flags & 0x10000) == 0 && BehaviorGroup(group);
        const bool rewardEligible = RewardKillEligible(group, wasDead, commander, flags);
        KillContext context;
        const int casualtyRank = At<uint8_t>(group, 0x2D20);
        ReasonScope casualtyReason(leader ? (casualtyRank >= 2 && casualtyRank <= 5 ? MoraleReason::OfficerDefeated : MoraleReason::CaptainDefeated) : MoraleReason::Casualties);
        context.previous = CurrentKill;
        const bool eligible = Config.casualties && corps && !wasDead && !commander && (flags & 0x10000) == 0;
        if (eligible)
        {
            context.corps = corps;
            const int rank = At<uint8_t>(group, 0x2D20);
            context.loss = leader ? (rank >= 2 && rank <= 5 ? Config.officerLoss : Config.captainLoss) : Config.soldierLoss;
            // Filled from the game's UnitTypeData attack/defense power-up flags.
            if (!leader && EliteUnits[At<uint16_t>(group, 0x2D0E)])
                context.loss = Config.eliteLoss;
            CurrentKill = &context;
        }
        OriginalKill(group, leader, member, flags);
        if (eligible)
        {
            const bool dead = leader ? At<uint8_t>(group, 0x110) != 0 : MemberDead(group, member);
            if (dead && !context.replaced)
                ChangeMorale(corps, context.loss, -1, OriginalLower);
            if (dead && (++CasualtyEvents <= 5 || CasualtyEvents % 500 == 0))
                Log("Casualty morale: group=%d leader=%u loss=%.1f.", At<int>(group, 0x3C), leader, context.loss);
            CurrentKill = context.previous;
        }
        if (panicEligible && At<uint8_t>(group, 0x110))
            LeaderDefeatPanic(group);
        if (rewardEligible && (leader ? At<uint8_t>(group, 0x110) != 0 : MemberDead(group, member)))
            RecordOffscreenKill(group, leader != 0);
        if (!wasDead && (flags & 0x10000) == 0 && (leader ? At<uint8_t>(group, 0x110) != 0 : MemberDead(group, member)))
            RecordCasualty(At<int>(group, 0x3C));
    }

    float FrequencyForGroup(int id, bool officer)
    {
        void* group = Group(id);
        void* corps = group ? CorpsObject(At<int>(group, 0x38)) : nullptr;
        return !Config.attributes || !corps ? 1 : ScaleForOfficer(Interpolate(At<float>(corps, 0x28), Config.anchors, Config.frequency), officer, Config);
    }

    static float ControllerFrequency(void* controller)
    {
        // Match GetOwnerCharacter: resolve its weak owner, not GetTargetActor's target.
        using OwnerFn = void* (*)(void*);
        void* owner = reinterpret_cast<OwnerFn>(Image + 0x1E291F0)(static_cast<uint8_t*>(controller) + 0x3DC);
        if (!Readable(owner, 0x7A8) || At<uint8_t>(owner, 0x74C) < 2)
            return 1;
        const int id = At<int>(owner, 0x7A0);
        void* group = Group(id);
        const int rank = group ? At<uint8_t>(group, 0x2D20) : 6;
        return FrequencyForGroup(id, rank >= 2 && rank <= 5 && At<int>(owner, 0x7A4) == -1);
    }

    static float AdjustWait(void* controller, float seconds)
    {
        if (seconds <= 0)
            return seconds;
        const float frequency = ControllerFrequency(controller);
        const float adjusted = seconds / frequency;
        if (fabsf(frequency - 1) > 0.001f && ++WaitEvents <= 5)
            Log("AI attack wait: %.3fs -> %.3fs (frequency %.3fx).", seconds, adjusted, frequency);
        return adjusted;
    }

    static void AttackWait(void* controller, float seconds)
    {
        const float adjusted = WaitDepth ? seconds : AdjustWait(controller, seconds);
        ++WaitDepth;
        OriginalAttackWait(controller, adjusted); // Tail-calls the secondary setter in the native game.
        --WaitDepth;
    }

    static void SecondaryWait(void* controller, float seconds) { OriginalSecondaryWait(controller, WaitDepth ? seconds : AdjustWait(controller, seconds)); }

    float Encounter(int id, float distance, float hp, float maximum, bool& rallied)
    {
        rallied = false;
        void* group = Group(id);
        if (!Config.support || !group || id >= MaxGroups || At<int>(group, 0x40) != 0 || At<uint8_t>(group, 0x110))
            return 0;
        const int rank = At<uint8_t>(group, 0x2D20);
        if (rank < 2 || rank > 5 || !Encounters[id].Enter(distance, Config))
            return 0;
        const int corpsId = At<int>(group, 0x38);
        void* corps = CorpsObject(corpsId);
        if (corps)
        {
            const float before = At<float>(corps, 0x28);
            rallied = Corps[corpsId].Rally(At<float>(corps, 0x28), Config);
            RecordMorale(corpsId, MoraleReason::Support, At<float>(corps, 0x28) - before);
        }
        const float heal = Encounters[id].Heal(hp, maximum, Config);
        if (rallied || heal > 0)
        {
            Log("Encounter: group=%d corps=%d rally=%d heal=%.1f.", id, corpsId, rallied, heal);
            EncounterLogged = true;
        }
        return heal;
    }

    bool SetCharacterHealth(void* character, float value)
    {
        if (!Readable(character, 0x8B2) || !isfinite(value))
            return false;
        if (!Readable(At<void*>(character, 0x698), 0x50))
            return false;
        // NPCs use cached HP while simulated and GAS HP while instantiated.
        // The game's own setter updates the appropriate store and the cached value.
        using SetFn = void (*)(void*, float);
        using GetFn = float (*)(void*);
        reinterpret_cast<SetFn>(Image + 0x1748200)(character, value);
        if (fabsf(reinterpret_cast<GetFn>(Image + 0x17481D0)(character) - value) > 0.1f)
            return false;
        void* group = Group(GroupIdFromCharacter(character));
        if (group && MemberIdFromCharacter(character) == -1)
        {
            const float delta = value - At<float>(group, 0x2DEC);
            At<float>(group, 0x2DEC) = value;
            At<float>(group, 0x2DF0) += delta;
            At<float>(group, 0x2DF4) += delta;
        }
        return true;
    }

    struct HookSpec { uintptr_t rva; void* replacement; void** original; };

    bool InitializeGameHooks()
    {
        NativeGroupDefeat = reinterpret_cast<GroupDefeatFn>(Image + 0x13B11E0);
        SetBehaviorStatus = reinterpret_cast<StatusFn>(Image + 0x13CFE30);
        BehaviorOwner = reinterpret_cast<OwnerFn>(Image + 0x1E291F0);
        BehaviorStopped = reinterpret_cast<BlackboardBoolFn>(Image + 0x13DD280);
        HookSpec hooks[] = {
            {0x13A1C80, reinterpret_cast<void*>(&CorpsTick), reinterpret_cast<void**>(&OriginalCorpsTick)},
            {0x13A5570, reinterpret_cast<void*>(&GroupTick), reinterpret_cast<void**>(&OriginalGroupTick)},
            {0x13AB0C0, reinterpret_cast<void*>(&Attack), reinterpret_cast<void**>(&OriginalAttack)},
            {0x13A27E0, reinterpret_cast<void*>(&Raise), reinterpret_cast<void**>(&OriginalRaise)},
            {0x13A2B80, reinterpret_cast<void*>(&Lower), reinterpret_cast<void**>(&OriginalLower)},
            {0x13AEF80, reinterpret_cast<void*>(&Kill), reinterpret_cast<void**>(&OriginalKill)},
            {0x13D0120, reinterpret_cast<void*>(&AttackWait), reinterpret_cast<void**>(&OriginalAttackWait)},
            {0x13D0200, reinterpret_cast<void*>(&SecondaryWait), reinterpret_cast<void**>(&OriginalSecondaryWait)},
            {0x13B2D80, reinterpret_cast<void*>(&MoraleMattersSpecialEntry), reinterpret_cast<void**>(&OriginalSpecialAttribute)},
            {0x1562680, reinterpret_cast<void*>(&ScenarioEffect), reinterpret_cast<void**>(&OriginalScenarioEffect)},
            {0x13DBC20, reinterpret_cast<void*>(&BehaviorTick), reinterpret_cast<void**>(&OriginalBehaviorTick)},
            {0x13B1A30, reinterpret_cast<void*>(&FormationOffset), reinterpret_cast<void**>(&OriginalFormationOffset)},
            {0x13AB470, reinterpret_cast<void*>(&OffscreenCasualties), reinterpret_cast<void**>(&OriginalOffscreenCasualties)},
            {0x1534B60, reinterpret_cast<void*>(&OnscreenHit), reinterpret_cast<void**>(&OriginalOnscreenHit)},
            {0x1377DF0, reinterpret_cast<void*>(&RequestDrop), reinterpret_cast<void**>(&OriginalRequestDrop)},
            {0x1749490, reinterpret_cast<void*>(&DropCombo), reinterpret_cast<void**>(&OriginalDropCombo)}
        };
        if (MH_Initialize() != MH_OK)
            return false;
        for (auto& hook : hooks)
        {
            const auto status = MH_CreateHook(Image + hook.rva, hook.replacement, hook.original);
            if (status != MH_OK)
            {
                Log("Hook creation failed at RVA %llx: %s.", static_cast<unsigned long long>(hook.rva), MH_StatusToString(status));
                MH_Uninitialize();
                return false;
            }
        }
        if (MH_EnableHook(MH_ALL_HOOKS) != MH_OK)
        {
            MH_Uninitialize();
            return false;
        }
        Log("ACTIVE: sixteen native hooks installed, including configurable combo drops, per-hit on-screen damage, corps, groups, morale, casualties, attack waits, arrivals, AI decisions, formation cohesion and off-screen kill rewards.");
        return true;
    }
}
