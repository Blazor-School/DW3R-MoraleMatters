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

static void ResetBehavior()
{
    memset(GroupBehavior, 0, sizeof(GroupBehavior));
    memset(CorpsBehavior, 0, sizeof(CorpsBehavior));
}

static bool BehaviorGroup(void* group)
{
    if (!Behavior.enabled || !group)
        return false;
    const int id = At<int>(group, 0x3C);
    const int corps = At<int>(group, 0x38);
    const int side = At<int>(group, 0x40);
    return Group(id) == group && corps >= 0 && corps < CorpsCount() && side >= 0 && side < 2 && side == At<int>(CorpsObject(corps), 0xC) && At<uint8_t>(group, 0x1E8) == 2 && At<uint8_t>(group, 0x2D20) >= 2 && !At<uint8_t>(group, 0x110) && !At<uint8_t>(group, 0x186) && !At<uint8_t>(group, 0x191) && !At<uint8_t>(group, 0x198) && At<uint8_t>(group, 0x2DFD) == 0;
}

static bool CanReshape(void* group)
{
    const int attribute = At<uint8_t>(group, 0x2DFC);
    // Fixed positions, commanders, scripted targets and script-protected groups keep their orders.
    return At<uint8_t>(group, 0x2D21) == 1 && attribute != 0 && attribute != 4 && attribute != 8 && !At<uint8_t>(group, 0x2E3A) && !At<uint8_t>(group, 0x2E58) && At<int>(group, 0x178) <= 2;
}

static bool CanRegroup(void* group) { return CanReshape(group) && At<uint8_t>(group, 0x2D20) >= 6; }

static float GroupMorale(void* group) { return At<float>(CorpsObject(At<int>(group, 0x38)), 0x28); }

static double GroupDistanceSquared(void* a, void* b)
{
    const double x = At<double>(a, 0x2DA8) - At<double>(b, 0x2DA8);
    const double y = At<double>(a, 0x2DB0) - At<double>(b, 0x2DB0);
    return x * x + y * y;
}

static void AdvanceGroupBehavior(void* group, float dt)
{
    const int id = At<int>(group, 0x3C);
    if (id >= 0 && id < GroupCount())
    {
        const bool eligible = BehaviorGroup(group);
        const bool wasRegrouping = GroupBehavior[id].regroupRemaining > 0;
        GroupBehavior[id].Advance(dt, eligible ? GroupMorale(group) : 400, eligible && CanRegroup(group), Behavior);
        if (!wasRegrouping && GroupBehavior[id].regroupRemaining > 0)
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
        return nullptr;
    return character;
}

static thread_local int BehaviorDepth = 0;
static volatile LONG ProfileEvents = 0;

static void BehaviorTick(void* controller, float dt)
{
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
    const bool regroup = Behavior.regroup && CanRegroup(group) && At<uint8_t>(controller, 0x3D8) && formation.regroupRemaining > 0 && !returning && statusBefore != 2 && statusBefore != 3 && statusBefore != 4 && blackboard && !BehaviorStopped(controller, blackboard, 24);
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
    if (!regroup || !BehaviorGroup(group) || !CanRegroup(group))
        return;
    const int status = At<uint8_t>(controller, 0x3EC);
    if (status == 1 || status == 5 || status == 6)
        SetBehaviorStatus(controller, 0); // Native NORMAL follows the local formation; no evacuation/despawn.
}

static void* FormationOffset(void* group, double* result, int member)
{
    void* value = OriginalFormationOffset(group, result, member);
    if (member < 0 || !BehaviorGroup(group) || !CanReshape(group))
        return value;
    const float morale = GroupMorale(group);
    const auto& state = GroupBehavior[At<int>(group, 0x3C)];
    float scale = Behavior.cohesion ? BehaviorCurve(morale, Behavior.lowFormationScale, 1, Behavior.highFormationScale) : 1;
    if (state.commanderNear)
        scale = 1 + (scale - 1) * 0.75f;
    float blend = 0;
    if (Behavior.withdrawal && Behavior.regroup && CanRegroup(group) && !At<uint8_t>(group, 0x190) && state.regroupRemaining > 0 && state.regroupDuration > 0)
    {
        const float progress = 1 - state.regroupRemaining / state.regroupDuration;
        blend = progress < 0.45f ? progress / 0.45f : progress > 0.8f ? (1 - progress) / 0.2f : 1;
        blend = Clamp(blend, 0, 1);
        scale += (Behavior.withdrawalScale - scale) * blend;
    }
    result[0] *= scale;
    result[1] *= scale;
    // Pull ordinary infantry a bounded distance toward their living corps leader.
    // The group leader itself, strategic orders and group position remain native.
    void* commander = Group(At<int>(CorpsObject(At<int>(group, 0x38)), 0x14));
    if (blend > 0 && commander && commander != group && BehaviorGroup(commander) && GroupAlive(commander) && At<int>(commander, 0x38) == At<int>(group, 0x38) && At<int>(commander, 0x40) == At<int>(group, 0x40))
    {
        const double dx = At<double>(commander, 0x2DA8) - At<double>(group, 0x2DA8);
        const double dy = At<double>(commander, 0x2DB0) - At<double>(group, 0x2DB0);
        const double distance = sqrt(dx * dx + dy * dy);
        if (isfinite(distance) && distance > 100 && distance <= Behavior.commanderRadius * 2)
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
    const auto& state = GroupBehavior[id];
    if (state.regroupRemaining <= 0 || state.regroupDuration <= 0) return 0;
    if (!Behavior.withdrawal) return 2;
    const float progress = 1 - state.regroupRemaining / state.regroupDuration;
    return progress < 0.45f ? 1 : progress < 0.8f ? 2 : 3;
}
