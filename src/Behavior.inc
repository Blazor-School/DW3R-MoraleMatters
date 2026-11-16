// Included inside the game namespace; all callbacks run on the game thread.
BehaviorSettings Behavior;
static GroupBehaviorState GroupBehavior[MaxGroups];
static CorpsBehaviorState CorpsBehavior[MaxCorps];
using ProfileTickFn = void (*)(void*, float);
using OffsetFn = void* (*)(void*, double*, int);
using StatusFn = void (*)(void*, uint8_t);
using OwnerFn = void* (*)(void*);
using BlackboardBoolFn = bool (*)(void*, void*, int);
static ProfileTickFn OriginalBehaviorTick = nullptr;
static OffsetFn OriginalFormationOffset = nullptr;
static StatusFn SetBehaviorStatus = nullptr;
static OwnerFn BehaviorOwner = nullptr;
static BlackboardBoolFn BehaviorStopped = nullptr;

struct InfantryControl
{
    void* character;
    int objectIndex;
    int group;
    int member;
    ULONGLONG flightExpires;
    bool disarmed;
};
static InfantryControl Infantry[1024];
using EndAbilityFn = void (*)(void*, const uint64_t*);
using ActivateAbilityFn = bool (*)(void*, int, const void*, void*, const void*, const void*);
static EndAbilityFn EndInfantryAbility = nullptr;
static ActivateAbilityFn OriginalActivateAbility = nullptr;
static const uint64_t* AttackAbilityTag = nullptr;
static const uint64_t* StanceAbilityTag = nullptr;

static InfantryControl* FindInfantryControl(void* character, ULONGLONG now, bool acquire)
{
    const size_t slot = (reinterpret_cast<uintptr_t>(character) >> 6) % _countof(Infantry);
    InfantryControl* available = nullptr;
    for (size_t offset = 0; offset < 8; ++offset)
    {
        auto& row = Infantry[(slot + offset) % _countof(Infantry)];
        if (row.character == character)
            return &row;
        if (acquire && !available && !row.disarmed && row.flightExpires <= now)
            available = &row;
    }
    return available;
}

static bool MatchesInfantry(const InfantryControl& row, void* character)
{
    return row.character == character && row.objectIndex == At<int>(character, 0xC) &&
        row.group == At<int>(character, 0x7A0) && row.member == At<int>(character, 0x7A4);
}

static const InfantryControl* InfantryControlFor(void* character)
{
    if (!character)
        return nullptr;
    const auto* row = FindInfantryControl(character, 0, false);
    return row && MatchesInfantry(*row, character) ? row : nullptr;
}

static void ResetBehavior()
{
    memset(GroupBehavior, 0, sizeof(GroupBehavior));
    memset(CorpsBehavior, 0, sizeof(CorpsBehavior));
    memset(Infantry, 0, sizeof(Infantry));
}

static bool BehaviorGroup(void* group, bool allowLeaderDefeat = false)
{
    if (!Behavior.enabled || !group)
        return false;
    const int id = At<int>(group, 0x3C);
    const int corps = At<int>(group, 0x38);
    const int side = At<int>(group, 0x40);
    return Group(id) == group && corps >= 0 && corps < CorpsCount() && side >= 0 && side < 2 && side == At<int>(CorpsObject(corps), 0xC) && At<uint8_t>(group, 0x1E8) == 2 && At<uint8_t>(group, 0x2D20) >= 2 && (allowLeaderDefeat || !At<uint8_t>(group, 0x110)) && !At<uint8_t>(group, 0x186) && !At<uint8_t>(group, 0x191) && !At<uint8_t>(group, 0x198) && At<uint8_t>(group, 0x2DFD) == 0;
}

static bool CanMoveFormation(void* group)
{
    const int attribute = At<uint8_t>(group, 0x2DFC);
    // Fixed positions, commanders, scripted targets and script-protected groups keep their orders.
    return attribute != 0 && attribute != 4 && attribute != 8 && !At<uint8_t>(group, 0x2E3A) && !At<uint8_t>(group, 0x2E58) && At<int>(group, 0x178) <= 2;
}

static bool CanReshape(void* group) { return At<uint8_t>(group, 0x2D21) == 1 && CanMoveFormation(group); }
static bool CanRegroup(void* group) { return CanReshape(group) && At<uint8_t>(group, 0x2D20) >= 6; }

static bool CorpsBreakEnabled() { return Behavior.enabled && Behavior.regroup && Behavior.corpsBreak; }

static const CorpsBreakState* InfantryBreakState(void* group, int member)
{
    if (!CorpsBreakEnabled() || !group || member < -1 || At<uint8_t>(group, 0x186) ||
        At<uint8_t>(group, 0x191) || At<uint8_t>(group, 0x198) || At<uint8_t>(group, 0x2DFD))
        return nullptr;
    const int corps = At<int>(group, 0x38);
    void* owner = CorpsObject(corps);
    if (!owner || At<int>(group, 0x40) != At<int>(owner, 0xC))
        return nullptr;
    if (member == -1)
    {
        const int unit = At<uint16_t>(group, 0x2D0C);
        if (At<uint8_t>(group, 0x110) || At<int>(group, 0x3C) == At<int>(owner, 0x14) ||
            At<uint8_t>(group, 0x2D20) <= 5 || (unit >= 405 && unit <= 413))
            return nullptr;
    }
    return &CorpsBehavior[corps].formation;
}

static const CorpsBreakState* FormationBreakState(void* group)
{
    if (!CorpsBreakEnabled() || !BehaviorGroup(group, true) || !CanMoveFormation(group))
        return nullptr;
    const int type = At<uint8_t>(group, 0x2D21);
    return type == 1 || type == 2 ? &CorpsBehavior[At<int>(group, 0x38)].formation : nullptr;
}

static float GroupMorale(void* group) { return At<float>(CorpsObject(At<int>(group, 0x38)), 0x28); }

static double GroupDistanceSquared(void* a, void* b)
{
    const double x = At<double>(a, 0x2DA8) - At<double>(b, 0x2DA8);
    const double y = At<double>(a, 0x2DB0) - At<double>(b, 0x2DB0);
    return x * x + y * y;
}

static void AdvanceGroupBehavior(void* group, float dt)
{
    if (!isfinite(dt) || dt <= 0)
        return;
    const int id = At<int>(group, 0x3C);
    if (id >= 0 && id < GroupCount())
    {
        const bool eligible = BehaviorGroup(group);
        auto& member = GroupBehavior[id];
        const auto* formation = FormationBreakState(group);
        const bool breaking = formation && formation->Withdrawing(id);
        const bool wasRegrouping = member.regroupRemaining > 0 || member.corpsBroken;
        const bool corpsActive = formation && (formation->broken || formation->rallyRemaining > 0);
        member.Advance(dt, eligible ? GroupMorale(group) : 400, eligible && CanRegroup(group) && !corpsActive, Behavior, !formation);
        if (corpsActive)
            member.shockRemaining = 0;
        member.corpsBroken = breaking;
        if (!wasRegrouping && (member.regroupRemaining > 0 || breaking))
            RecordFormationBreak(id);
    }
}

static void UpdateCorpsBehavior(int corps, float dt, float& morale)
{
    if (!Behavior.enabled)
        return;
    auto& state = CorpsBehavior[corps];
    state.sampleTime += dt;
    if (state.sampleTime < 1)
        return;
    const float elapsed = state.sampleTime;
    state.sampleTime = 0;
    void* commander = Group(At<int>(CorpsObject(corps), 0x14));
    const bool leaderAlive = commander && At<int>(commander, 0x38) == corps && At<int>(commander, 0x40) == At<int>(CorpsObject(corps), 0xC) && GroupAlive(commander) && At<uint8_t>(commander, 0x2DFD) == 0 && !At<uint8_t>(commander, 0x191) && !At<uint8_t>(commander, 0x198);
    bool combat = false;
    int count = 0, nearby = 0;
    const double radius = static_cast<double>(Behavior.commanderRadius) * Behavior.commanderRadius;
    for (int id = 0; id < GroupCount(); ++id)
    {
        void* group = Group(id);
        if (At<int>(group, 0x38) != corps)
            continue;
        auto& member = GroupBehavior[id];
        member.commanderNear = false;
        if (At<int>(group, 0x40) == At<int>(CorpsObject(corps), 0xC) && !At<uint8_t>(group, 0x110) && !At<uint8_t>(group, 0x186) && At<uint8_t>(group, 0x2DFD) == 0 && At<int>(group, 0x178) == 2)
            combat = true;
        if (!BehaviorGroup(group))
            continue;
        ++count;
        member.commanderNear = Behavior.leadership && leaderAlive && GroupDistanceSquared(group, commander) <= radius;
        nearby += member.commanderNear ? 1 : 0;
        combat = combat || At<int>(group, 0x178) == 2;
    }
    const float fraction = count > 0 ? static_cast<float>(nearby) / count : 0;
    const float gain = state.Advance(elapsed, combat || count == 0, Corps[corps].base, fraction, Behavior);
    if (gain > 0)
        RecordMorale(corps, MoraleReason::Recovery, Corps[corps].AddPermanent(morale, gain));
}

static void UpdateFormationBreak(int corps, float dt, float morale)
{
    if (!CorpsBreakEnabled())
        return;
    auto& formation = CorpsBehavior[corps].formation;
    const bool wasBroken = formation.broken;
    formation.Advance(dt, morale, Config.anchors[0]);
    if (formation.broken != wasBroken)
        Log("Corps %d %s at morale %.1f.", corps, formation.broken ? "formation broke" : "began rallying", morale);
}

static void LeaderDefeatPanic(void* source)
{
    if (!Behavior.enabled || !Behavior.panic)
        return;
    const int corps = At<int>(source, 0x38);
    if (corps < 0 || corps >= CorpsCount())
        return;
    int affected = 0, nearCommander = 0;
    const float confidence = BehaviorCurve(At<float>(CorpsObject(corps), 0x28), 1.5f, 1, 0.5f);
    const double radius = static_cast<double>(Behavior.panicRadius) * Behavior.panicRadius;
    for (int id = 0; id < GroupCount(); ++id)
    {
        void* group = Group(id);
        if (group == source || At<int>(group, 0x38) != corps || !BehaviorGroup(group) || GroupDistanceSquared(source, group) > radius)
            continue;
        ++affected;
        auto& state = GroupBehavior[id];
        nearCommander += state.commanderNear ? 1 : 0;
        const float leadership = state.commanderNear ? Behavior.commanderPanic : 1;
        state.shockRemaining = fmaxf(state.shockRemaining, Behavior.shockSeconds * confidence * leadership);
    }
    if (!affected)
        return;
    CorpsBehavior[corps].quietTime = 0;
    const float leadership = 1 + (Behavior.commanderPanic - 1) * static_cast<float>(nearCommander) / affected;
    const float loss = CorpsBehavior[corps].Panic(Behavior.panicLoss * confidence * leadership, Behavior);
    if (loss > 0)
    {
        ReasonScope reason(MoraleReason::Panic);
        ChangeMorale(CorpsObject(corps), loss, -1, OriginalLower);
        Log("Leader defeat panic: corps=%d nearbyGroups=%d extraLoss=%.2f.", corps, affected, loss);
    }
}

static void* BehaviorCharacter(void* controller, void*& group)
{
    group = nullptr;
    // Native tick supplies a live AppAIController; the engine weak-owner getter
    // validates its owner. Avoid VirtualQuery for every NPC on every frame.
    if (!Behavior.enabled || !controller)
        return nullptr;
    void* character = BehaviorOwner(static_cast<uint8_t*>(controller) + 0x3DC);
    if (!character || At<uint8_t>(character, 0x74C) < 2 || At<uint8_t>(character, 0x787))
        return nullptr;
    group = Group(At<int>(character, 0x7A0));
    if (!BehaviorGroup(group))
    {
        const auto* formation = At<int>(character, 0x7A4) >= 0 ? FormationBreakState(group) : nullptr;
        if (!formation || (!formation->broken && !formation->Rejoining(At<int>(group, 0x3C))))
            return nullptr;
    }
    return character;
}

static thread_local int BehaviorDepth = 0;
static volatile LONG ProfileEvents = 0;

static bool CanChangeLocalBehavior(void* controller, int status, void* blackboard)
{
    return At<uint8_t>(controller, 0x3D8) && status != 2 && status != 3 && status != 4 && blackboard && !BehaviorStopped(controller, blackboard, 24);
}

static bool BreakSuppressesAttack(void* controller)
{
    if (!CorpsBreakEnabled() || !controller)
        return false;
    void* character = BehaviorOwner(static_cast<uint8_t*>(controller) + 0x3DC);
    if (!character || At<uint8_t>(character, 0x74C) < 2 || At<uint8_t>(character, 0x787))
        return false;
    if (const auto* row = InfantryControlFor(character); row && row->disarmed)
        return true;
    const int id = At<int>(character, 0x7A0);
    const auto* formation = InfantryBreakState(Group(id), At<int>(character, 0x7A4));
    return formation && (formation->broken || formation->Rejoining(id))
        && CanChangeLocalBehavior(controller, At<uint8_t>(controller, 0x3EC), At<void*>(controller, 0x3A0));
}

bool SetInfantryControl(void* character, int object, bool acquire, bool disarm)
{
    if (!character)
        return false;
    const auto now = GetTickCount64();
    auto* row = FindInfantryControl(character, now, acquire);
    if (!row)
        return false;
    if (!acquire)
    {
        if (row->objectIndex == object)
        {
            if (disarm) *row = {};
            else row->flightExpires = 0;
        }
        return true;
    }
    if (!Readable(character, 0x7A8) || At<int>(character, 0xC) != object ||
        At<uint8_t>(character, 0x74C) < 2 || At<uint8_t>(character, 0x787))
        return false;
    const int id = At<int>(character, 0x7A0);
    const int member = At<int>(character, 0x7A4);
    const auto* formation = InfantryBreakState(Group(id), member);
    if (!formation || !formation->broken)
        return false;
    if (!disarm)
    {
        if (!MatchesInfantry(*row, character) || !row->disarmed)
            return false;
        row->flightExpires = now + 300;
        return true;
    }
    void* abilities = At<void*>(character, 0x690);
    if (!abilities || At<void*>(abilities, 0x3E8) != character || !*AttackAbilityTag || !*StanceAbilityTag)
        return false;
    *row = {character, object, id, member, 0, true};
    // Cancel any active attack or stance before Lua hides the weapon. The activation hook
    // also covers follow-ups requested by an ability while it is ending.
    EndInfantryAbility(abilities, AttackAbilityTag);
    EndInfantryAbility(abilities, StanceAbilityTag);
    return true;
}

static bool IsInfantryAttackAbility(void* ability)
{
    // FGameplayTagContainer holds explicit tags and their cached parent tags.
    for (int offset = 0xA8; offset <= 0xB8; offset += 0x10)
    {
        const auto* tags = At<const uint64_t*>(ability, offset);
        const int count = At<int>(ability, offset + 8);
        for (int i = 0; i < count; ++i)
            if (tags[i] == *AttackAbilityTag || tags[i] == *StanceAbilityTag)
                return true;
    }
    return false;
}

static bool ActivateAbility(void* component, int handle, const void* predictionKey, void* instance, const void* ended, const void* trigger)
{
    if (CorpsBreakEnabled())
    {
        void* character = At<void*>(component, 0x3E8); // AbilitySystemComponent.AvatarActor
        const auto* row = InfantryControlFor(character);
        if (row && row->disarmed && At<void*>(character, 0x690) == component)
        {
            // Match the native spec lookup so both direct and queued activations are covered.
            auto* spec = At<uint8_t*>(component, 0x508);
            const int count = At<int>(component, 0x510);
            for (int i = 0; i < count; ++i, spec += 0xF0)
            {
                if (At<int>(spec, 0xC) != handle)
                    continue;
                void* ability = At<void*>(spec, 0x10);
                if (ability && IsInfantryAttackAbility(ability))
                    return false;
                break;
            }
        }
    }
    return OriginalActivateAbility(component, handle, predictionKey, instance, ended, trigger);
}

static bool BreakWithdraws(void* controller, void* character, void* group);

static void BehaviorTick(void* controller, float dt)
{
    if (!BehaviorDepth && controller && CorpsBreakEnabled())
    {
        void* owner = BehaviorOwner(static_cast<uint8_t*>(controller) + 0x3DC);
        if (const auto* row = InfantryControlFor(owner); row && row->disarmed)
        {
            if (At<int>(controller, 0x3F0) || At<int>(controller, 0x3F8))
                OriginalAttackWait(controller, 0);
            if (row->flightExpires && row->flightExpires > GetTickCount64())
                return;
        }
    }
    if (!BehaviorDepth && TickGuardPressure(controller, dt))
        return;
    void* group = nullptr;
    void* character = !BehaviorDepth && isfinite(dt) && dt > 0 ? BehaviorCharacter(controller, group) : nullptr;
    if (!character)
    {
        OriginalBehaviorTick(controller, dt);
        return;
    }
    int& rank = At<int>(controller, 0x484);
    const int original = rank;
    const bool officer = At<int>(character, 0x7A4) == -1 && At<uint8_t>(group, 0x2D20) <= 5;
    const int adjusted = Behavior.profiles ? MoraleProfile(original, GroupMorale(group), officer, Config, Behavior) : original;
    if (adjusted != original && InterlockedIncrement(&ProfileEvents) <= 8)
        Log("AI decision profile: group=%d morale=%.1f named=%d profile=%d -> %d.", At<int>(group, 0x3C), GroupMorale(group), officer, original, adjusted);
    void* blackboard = At<void*>(controller, 0x3A0);
    const int statusBefore = At<uint8_t>(controller, 0x3EC);
    const auto& formation = GroupBehavior[At<int>(group, 0x3C)];
    const bool returning = Behavior.withdrawal && formation.regroupRemaining <= formation.regroupDuration * 0.2f;
    const bool regroup = ((Behavior.regroup && CanRegroup(group) && formation.regroupRemaining > 0 && !returning) ||
        BreakWithdraws(controller, character, group)) && CanChangeLocalBehavior(controller, statusBefore, blackboard);
    const int groupMode = At<int>(group, 0x178);
    if (regroup)
    {
        if (statusBefore != 0)
            SetBehaviorStatus(controller, 0);
        At<int>(group, 0x178) = 0; // Local decision context only; restore strategic orders immediately afterward.
    }
    rank = adjusted;
    ++BehaviorDepth;
    OriginalBehaviorTick(controller, dt);
    --BehaviorDepth;
    if (rank == adjusted)
        rank = original; // Never overwrite a genuine native reassignment.
    if (regroup && At<int>(group, 0x178) == 0)
        At<int>(group, 0x178) = groupMode;
    group = Group(At<int>(character, 0x7A0));
    if (!regroup || (!FormationBreakState(group) && (!BehaviorGroup(group) || !CanRegroup(group))))
        return;
    const int status = At<uint8_t>(controller, 0x3EC);
    if (status == 1 || status == 5 || status == 6)
        SetBehaviorStatus(controller, 0); // Native NORMAL follows the local formation; no evacuation/despawn.
}

static void* FormationOffset(void* group, double* result, int member)
{
    void* value = OriginalFormationOffset(group, result, member);
    if (member < 0)
        return value;
    const auto* formation = FormationBreakState(group);
    const float breakBlend = formation ? formation->WithdrawalBlend(At<int>(group, 0x3C)) : 0;
    if (breakBlend <= 0 && (!BehaviorGroup(group) || !CanReshape(group)))
        return value;
    const float morale = GroupMorale(group);
    const auto& state = GroupBehavior[At<int>(group, 0x3C)];
    float scale = Behavior.cohesion ? BehaviorCurve(morale, Behavior.lowFormationScale, 1, Behavior.highFormationScale) : 1;
    if (state.commanderNear)
        scale = 1 + (scale - 1) * 0.75f;
    float blend = breakBlend;
    if (Behavior.withdrawal && Behavior.regroup && CanRegroup(group) && !At<uint8_t>(group, 0x190) && state.regroupRemaining > 0 && state.regroupDuration > 0)
    {
        const float progress = 1 - state.regroupRemaining / state.regroupDuration;
        blend = progress < 0.45f ? progress / 0.45f : progress > 0.8f ? (1 - progress) / 0.2f : 1;
        blend = Clamp(blend, 0, 1);
    }
    scale += (Behavior.withdrawalScale - scale) * blend;
    result[0] *= scale;
    result[1] *= scale;
    // Pull ordinary infantry a bounded distance toward their living corps leader.
    // The group leader itself, strategic orders and group position remain native.
    void* commander = Group(At<int>(CorpsObject(At<int>(group, 0x38)), 0x14));
    if (blend > 0)
    {
        const bool towardCommander = commander && commander != group && BehaviorGroup(commander) && GroupAlive(commander) && At<int>(commander, 0x38) == At<int>(group, 0x38) && GroupDistanceSquared(group, commander) <= 4.0 * Behavior.commanderRadius * Behavior.commanderRadius;
        void* target = towardCommander ? commander : breakBlend > 0 ? Group(At<int>(group, 0x180)) : nullptr;
        if (!target || (!towardCommander && (!GroupAlive(target) || At<int>(target, 0x40) == At<int>(group, 0x40))))
            return value;
        const double direction = towardCommander ? 1 : -1;
        const double dx = (At<double>(target, 0x2DA8) - At<double>(group, 0x2DA8)) * direction;
        const double dy = (At<double>(target, 0x2DB0) - At<double>(group, 0x2DB0)) * direction;
        const double distance = sqrt(dx * dx + dy * dy);
        if (isfinite(distance) && distance > 100)
        {
            const double amount = fmin(distance - 100, static_cast<double>(Behavior.withdrawalDistance)) * blend / distance;
            result[0] += dx * amount;
            result[1] += dy * amount;
        }
    }
    // The caller projects this adjusted offset onto navigation after adding the group position.
    return value;
}

int RegroupPhase(int id)
{
    if (id < 0 || id >= GroupCount() || !Behavior.enabled || !Behavior.regroup) return 0;
    if (const auto* formation = FormationBreakState(Group(id)))
    {
        if (formation->Withdrawing(id)) return 4;
        if (formation->Rejoining(id)) return 5;
    }
    const auto& state = GroupBehavior[id];
    if (state.regroupRemaining <= 0 || state.regroupDuration <= 0) return 0;
    if (!Behavior.withdrawal) return 2;
    const float progress = 1 - state.regroupRemaining / state.regroupDuration;
    return progress < 0.45f ? 1 : progress < 0.8f ? 2 : 3;
}

int CorpsRallyPhase(int corps)
{
    if (!CorpsBreakEnabled() || corps < 0 || corps >= CorpsCount())
        return 0;
    const auto& formation = CorpsBehavior[corps].formation;
    return formation.broken ? 4 : formation.rallyRemaining > 0 ? 5 : 0;
}

int InfantryBreakPhase(int group, int member)
{
    const auto* formation = InfantryBreakState(Group(group), member);
    return formation ? formation->broken ? 4 : formation->rallyRemaining > 0 ? 5 : 0 : -1;
}
