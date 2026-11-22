// Game-thread state. Only the native successful-guard branch can add pressure.
GuardPressureSettings GuardPressure;
struct GuardActor
{
    void* character;
    void* controller;
    int objectIndex;
    int group;
    int member;
    GuardPressureState state;
    uint64_t token;
    uint64_t touched;
    unsigned long breakSequence;
};
static GuardActor GuardActors[16384];
static float GuardPulseCooldown[MaxGroups];
static unsigned long GuardBreakEvents = 0;
static uint64_t GuardFeedbackSequence = 0;
static GuardFeedback GuardFeedbackRows[3];
static int GuardFeedbackCount = 0;
using GuardTagFn = bool (*)(void*, int);
using GuardAbilityFn = bool (*)(void*, uint8_t);
static GuardTagFn GuardTag = nullptr;
static GuardAbilityFn GuardAbility = nullptr;

static void ResetGuardPressure()
{
    memset(GuardActors, 0, sizeof(GuardActors));
    memset(GuardPulseCooldown, 0, sizeof(GuardPulseCooldown));
    GuardFeedbackCount = 0;
}

static void* GuardGroup(void* character)
{
    if (!GuardPressure.enabled || !Readable(character, 0x8B8) || At<uint8_t>(character, 0x74C) < 2 || At<uint8_t>(character, 0x787))
        return nullptr;
    void* group = Group(At<int>(character, 0x7A0));
    const int corps = group ? At<int>(group, 0x38) : -1;
    if (!group || !CorpsObject(corps) || !isfinite(GroupMorale(group)) || At<int>(group, 0x40) != 1 || At<int>(CorpsObject(corps), 0xC) != 1 || At<uint8_t>(group, 0x1E8) != 2 || At<uint8_t>(group, 0x2D20) < 2 || !GroupAlive(group) || At<uint8_t>(group, 0x191) || At<uint8_t>(group, 0x198) || At<uint8_t>(group, 0x2DFD) || At<uint8_t>(group, 0x2E3A) || At<uint8_t>(group, 0x2E58))
        return nullptr;
    return group;
}

static GuardActor* GuardActorFor(void* character, bool create)
{
    const size_t first = (reinterpret_cast<uintptr_t>(character) >> 4) & 16383;
    GuardActor* reusable = nullptr;
    for (size_t n = 0; n < 16; ++n)
    {
        auto& actor = GuardActors[(first + n) & 16383];
        if (actor.character == character)
        {
            if (actor.objectIndex != At<int>(character, 0xC) || actor.group != At<int>(character, 0x7A0) || actor.member != At<int>(character, 0x7A4) || actor.controller != At<void*>(character, 0x2E0))
                actor = {};
            else
                return &actor;
        }
        if (!reusable && (!actor.character || (actor.state.pressure == 0 && actor.state.opening == 0 && actor.state.cooldown == 0 && actor.state.hesitation == 0 && actor.state.disruptionCooldown == 0)))
            reusable = &actor;
    }
    if (!create || !reusable)
        return nullptr; // A full bucket leaves native behavior intact.
    *reusable = {character, At<void*>(character, 0x2E0), At<int>(character, 0xC), At<int>(character, 0x7A0), At<int>(character, 0x7A4), {}, ++GuardFeedbackSequence, 0, 0};
    return reusable;
}

static bool GuardPosition(void* character, double (&position)[3])
{
    void* root = At<void*>(character, 0x1C0);
    if (!Readable(root, 0x218))
        return false;
    for (int axis = 0; axis < 3; ++axis)
    {
        position[axis] = At<double>(root, 0x200 + axis * 8);
        if (!isfinite(position[axis]))
            return false;
    }
    return true;
}

static float GuardThreshold(void* character, void* group)
{
    const bool officer = At<int>(character, 0x7A4) == -1 && At<uint8_t>(group, 0x2D20) <= 5;
    return officer ? GuardPressure.officerThreshold : EliteUnits[At<uint16_t>(group, 0x2D0E)] ? GuardPressure.eliteThreshold : GuardPressure.soldierThreshold;
}

int CollectGuardFeedback()
{
    GuardFeedbackCount = 0;
    if (!GuardPressure.enabled)
        return 0;
    uint64_t priorities[3] = {};
    for (auto& actor : GuardActors)
    {
        // Keep the full shields visible for half a second after the opening.
        const bool refilled = actor.state.opening <= 0 && actor.state.cooldown > 0 && actor.state.cooldown > GuardPressure.breakCooldown - 0.5f;
        if (!actor.character || (actor.state.pressure <= 0 && actor.state.opening <= 0 && actor.state.breakFlash <= 0 && !refilled) || !Readable(actor.character, 0x8B8))
            continue;
        void* group = GuardGroup(actor.character);
        if (!group || GuardActorFor(actor.character, false) != &actor)
            continue;
        double position[3];
        if (!GuardPosition(actor.character, position))
            continue;
        int at = GuardFeedbackCount;
        while (at > 0 && actor.touched > priorities[at - 1])
            --at;
        if (at >= 3)
            continue;
        if (GuardFeedbackCount < 3)
            ++GuardFeedbackCount;
        for (int n = GuardFeedbackCount - 1; n > at; --n)
        {
            GuardFeedbackRows[n] = GuardFeedbackRows[n - 1];
            priorities[n] = priorities[n - 1];
        }
        priorities[at] = actor.touched;
        GuardFeedbackRows[at] = {actor.token, actor.group, actor.member, position[0], position[1], position[2], actor.state.pressure, GuardThreshold(actor.character, group), actor.state.opening, actor.state.hitFlash, actor.state.breakFlash, actor.breakSequence, actor.touched, actor.objectIndex, refilled};
    }
    return GuardFeedbackCount;
}

bool ReadGuardFeedback(int index, GuardFeedback& result)
{
    if (index < 0 || index >= GuardFeedbackCount)
        return false;
    result = GuardFeedbackRows[index];
    return true;
}

static void GuardDisruption(void* defender, void* group)
{
    const int groupId = At<int>(group, 0x3C);
    if (!GuardPressure.disruption || GuardPressure.disruptionTargets <= 0 || groupId < 0 || groupId >= GroupCount() || GuardPulseCooldown[groupId] > 0)
        return;
    double origin[3];
    if (!GuardPosition(defender, origin))
        return;
    void* manager = At<void*>(Image, GameRva<0xA9C50F0>());
    if (!Readable(manager, 0x60))
        return;
    const int count = At<int>(manager, 0x58);
    auto** characters = At<void**>(manager, 0x50);
    if (count <= 0 || count > 16384 || !Readable(characters, static_cast<size_t>(count) * sizeof(void*)))
        return;
    void* nearest[16] = {};
    double distances[16] = {};
    int selected = 0;
    const int limit = GuardPressure.disruptionTargets < 16 ? GuardPressure.disruptionTargets : 16;
    const double radius = static_cast<double>(GuardPressure.disruptionRadius) * GuardPressure.disruptionRadius;
    for (int index = 0; index < count; ++index)
    {
        void* candidate = characters[index];
        void* candidateGroup = candidate != defender ? GuardGroup(candidate) : nullptr;
        if (!candidateGroup || (At<int>(candidate, 0x7A4) == -1 && At<uint8_t>(candidateGroup, 0x2D20) <= 5) || At<uint8_t>(candidateGroup, 0x2D21) != 1 || At<int>(candidateGroup, 0x178) != 2 || !CanReshape(candidateGroup))
            continue;
        auto* existing = GuardActorFor(candidate, false);
        if (existing && existing->state.disruptionCooldown > 0)
            continue;
        double position[3];
        if (!GuardPosition(candidate, position))
            continue;
        double distance = 0;
        for (int axis = 0; axis < 3; ++axis)
            distance += (position[axis] - origin[axis]) * (position[axis] - origin[axis]);
        if (distance > radius)
            continue;
        int at = selected;
        while (at > 0 && distance < distances[at - 1])
            --at;
        if (at >= limit)
            continue;
        if (selected < limit)
            ++selected;
        for (int n = selected - 1; n > at; --n)
        {
            nearest[n] = nearest[n - 1];
            distances[n] = distances[n - 1];
        }
        nearest[at] = candidate;
        distances[at] = distance;
    }
    GuardPulseCooldown[groupId] = GuardPressure.disruptionCooldown;
    int disrupted = 0;
    for (int index = 0; index < selected; ++index)
        if (auto* actor = GuardActorFor(nearest[index], true))
        {
            actor->state.Disrupt(GroupMorale(Group(actor->group)), GuardPressure);
            ++disrupted;
        }
    static volatile LONG evidence = 0;
    if (disrupted > 0 && SampleLiveEvidence(evidence))
        Log("Live evidence: guard disruption group=%d targets=%d.", groupId, disrupted);
}

// Mid-function ABI: candidate/native-break/mounted/special occupy successive bytes.
// Return 1 requests the existing BreakGuard branch; 2 skips guarding during an
// earned opening. No HP, attack-data flags, GAS attributes or animation rates change.
extern "C" int MoraleMattersGuardDecision(void* component, void* attacker, uint32_t context, float damage)
{
    if (!GuardPressure.enabled || !(context & 0xFF) || (context & 0xFFFF0000) || !GuardTag || !GuardAbility || !Readable(component, 0x143) || !Readable(attacker, 0x8B8) || At<uint8_t>(attacker, 0x74C) >= 2 || At<uint8_t>(attacker, 0x787) || !isfinite(damage) || damage <= 0)
        return 0;
    void* defender = At<void*>(component, 0xB8);
    void* group = GuardGroup(defender);
    void* data = At<void*>(component, 0xC8);
    if (!group || !Readable(data, 0x10))
        return 0;
    // Active attacks can enter the later native weapon-clash veto.
    // Leave that path, including glowing charge-interrupt attacks, entirely native.
    if (GuardTag(defender, 0xCE) || GuardTag(defender, 0xCF) || GuardTag(defender, 0xEA))
        return 0;
    const uint64_t flags = At<uint64_t>(data, 8);
    constexpr uint64_t excluded = 1024 | 524288 | 0x10000000 | 0x20000000 | 0x40000000 | 0x80000000ULL;
    if (flags & excluded)
        return 0; // Mounted attacks, throws, fixed damage, projectiles and unblockables stay native.
    auto* actor = GuardActorFor(defender, true);
    if (!actor)
        return 0;
    if (actor->state.opening > 0)
        return 2;
    // Native guard-break immunity and already guaranteed breaks remain authoritative.
    if (GuardAbility(defender, 0x16))
        return 0;
    const bool naturalBreak = (context & 0xFF00) || damage >= At<float>(Image, GameRva<0x9D3316C>()) || (GuardTag(attacker, 0xEA) && GuardAbility(attacker, 0x26));
    const float threshold = GuardThreshold(defender, group);
    if (actor->state.cooldown <= 0)
        actor->touched = ++GuardFeedbackSequence;
    if (!naturalBreak)
    {
        const float before = actor->state.pressure;
        const bool broke = actor->state.Block((flags & 1) != 0, threshold, GuardPressure);
        static volatile LONG evidence = 0;
        if (actor->state.pressure != before && SampleLiveEvidence(evidence))
            Log("Live evidence: guarded hit group=%d member=%d pressure=%.2f -> %.2f threshold=%.2f.", actor->group, actor->member, before, actor->state.pressure, threshold);
        if (!broke)
            return 0;
    }
    if (actor->state.cooldown > 0)
        return 0;
    actor->state.Break(GuardPressure);
    actor->breakSequence = ++GuardBreakEvents;
    GuardDisruption(defender, group);
    if (Config.liveEvidence || GuardBreakEvents <= 12 || GuardBreakEvents % 100 == 0)
        Log("Guard break: group=%d member=%d morale=%.0f charge=%d native=%d opening=%.2fs.", actor->group, actor->member, GroupMorale(group), (flags & 1) != 0, naturalBreak, GuardPressure.openingSeconds);
    return naturalBreak ? 0 : 1;
}

static bool TickGuardPressure(void* controller, float seconds)
{
    if (!GuardPressure.enabled || !controller || !BehaviorOwner || !isfinite(seconds) || seconds <= 0)
        return false;
    void* character = BehaviorOwner(static_cast<uint8_t*>(controller) + 0x3DC);
    if (!Readable(character, 0x8B8))
        return false;
    auto* actor = GuardActorFor(character, false);
    if (!actor)
        return false;
    void* group = GuardGroup(character);
    if (!group)
    {
        *actor = {};
        return false;
    }
    actor->state.Advance(seconds, GroupMorale(group), GuardPressure);
    const int status = At<uint8_t>(controller, 0x3EC);
    void* blackboard = At<void*>(controller, 0x3A0);
    if (actor->state.hesitation <= 0 || (At<int>(character, 0x7A4) == -1 && At<uint8_t>(group, 0x2D20) <= 5) || !CanReshape(group) || At<int>(group, 0x178) != 2 || !At<uint8_t>(controller, 0x3D8) || status == 2 || status == 3 || status == 4 || !blackboard || !BehaviorStopped || BehaviorStopped(controller, blackboard, 24) || !GuardTag || GuardTag(character, 0xCE) || GuardTag(character, 0xCF) || GuardTag(character, 0xEA))
        return false;
    // Native attack tickets wait for the opening; current abilities retain their
    // animation/reaction timing. Strategic orders are restored after this tick.
    const int mode = At<int>(group, 0x178);
    if (status != 0)
        SetBehaviorStatus(controller, 0);
    At<int>(group, 0x178) = 0;
    ++BehaviorDepth;
    OriginalBehaviorTick(controller, seconds);
    --BehaviorDepth;
    if (At<int>(group, 0x178) == 0)
        At<int>(group, 0x178) = mode;
    const int after = At<uint8_t>(controller, 0x3EC);
    if (after == 1 || after == 5 || after == 6)
        SetBehaviorStatus(controller, 0);
    return true;
}

extern "C" { void* OriginalGuardDecision = nullptr; }
extern "C" void GuardDecisionEntry();
// The native frame is already 16-byte aligned at this interior branch. Preserve
// every volatile register and RFLAGS, then resume its relocated instructions.
asm(
    ".text\n"
    ".p2align 4\n"
    "GuardDecisionEntry:\n"
    "pushfq\n"
    "subq $0xd8, %rsp\n"
    "movq %rax, 0x20(%rsp)\n"
    "movq %rcx, 0x28(%rsp)\n"
    "movq %rdx, 0x30(%rsp)\n"
    "movq %r8, 0x38(%rsp)\n"
    "movq %r9, 0x40(%rsp)\n"
    "movq %r10, 0x48(%rsp)\n"
    "movq %r11, 0x50(%rsp)\n"
    "movq 0xd8(%rsp), %rax\n"
    "movq %rax, 0x58(%rsp)\n"
    "movaps %xmm0, 0x60(%rsp)\n"
    "movaps %xmm1, 0x70(%rsp)\n"
    "movaps %xmm2, 0x80(%rsp)\n"
    "movaps %xmm3, 0x90(%rsp)\n"
    "movaps %xmm4, 0xa0(%rsp)\n"
    "movaps %xmm5, 0xb0(%rsp)\n"
    "movq %r15, %rcx\n"
    "movq %rbx, %rdx\n"
    "movzbl %r10b, %r8d\n"
    "movzbl %dil, %eax\n"
    "shll $8, %eax\n"
    "orl %eax, %r8d\n"
    "movzbl 0x120(%rsp), %eax\n"
    "shll $16, %eax\n"
    "orl %eax, %r8d\n"
    "movzbl %r13b, %eax\n"
    "shll $24, %eax\n"
    "orl %eax, %r8d\n"
    "movaps %xmm11, %xmm3\n"
    "call MoraleMattersGuardDecision\n"
    "cmpl $1, %eax\n"
    "jne 1f\n"
    "movb $1, %dil\n"
    "1: cmpl $2, %eax\n"
    "jne 2f\n"
    "movb $0, 0x48(%rsp)\n"
    "2: movaps 0x60(%rsp), %xmm0\n"
    "movaps 0x70(%rsp), %xmm1\n"
    "movaps 0x80(%rsp), %xmm2\n"
    "movaps 0x90(%rsp), %xmm3\n"
    "movaps 0xa0(%rsp), %xmm4\n"
    "movaps 0xb0(%rsp), %xmm5\n"
    "movq 0x20(%rsp), %rax\n"
    "movq 0x28(%rsp), %rcx\n"
    "movq 0x30(%rsp), %rdx\n"
    "movq 0x38(%rsp), %r8\n"
    "movq 0x40(%rsp), %r9\n"
    "movq 0x48(%rsp), %r10\n"
    "movq 0x50(%rsp), %r11\n"
    "pushq 0x58(%rsp)\n"
    "popfq\n"
    "leaq 0xe0(%rsp), %rsp\n"
    "jmp *OriginalGuardDecision(%rip)\n"
);
