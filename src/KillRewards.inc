OffscreenKillSettings KillRewards;
static KillRewardBudget KillRewardBudgets[MaxCorps];
static AttackFn OriginalOffscreenCasualties = nullptr;
static unsigned long KillRewardEvents = 0;
static unsigned long KillRewardGeneration = 0;
static SRWLOCK KillRewardLock = SRWLOCK_INIT;
static thread_local int KillRewardLockDepth = 0;

struct KillRewardGuard
{
    bool active;
    explicit KillRewardGuard(bool enabled = true) : active(enabled)
    {
        if (active && KillRewardLockDepth++ == 0)
            AcquireSRWLockExclusive(&KillRewardLock);
    }
    ~KillRewardGuard()
    {
        if (active && --KillRewardLockDepth == 0)
            ReleaseSRWLockExclusive(&KillRewardLock);
    }
};

struct OffscreenRewardContext
{
    OffscreenRewardContext* previous = nullptr;
    void* manager = nullptr;
    void* groups = nullptr;
    void* corps = nullptr;
    int groupCount = 0;
    int corpsCount = 0;
    unsigned long generation = 0;
    int root = -1;
    int victim = -1;
    int winner = -1;
    int victimSide = -1;
    float nativeBefore = 0;
    float target = 0;
    int deaths = 0;
};
static thread_local OffscreenRewardContext* CurrentOffscreenReward = nullptr;

static bool RewardStorageMatches(const OffscreenRewardContext& context)
{
    return context.manager == BattleManager && context.groups == GroupStorage && context.corps == CorpsStorage && context.groupCount == GroupCount() && context.corpsCount == CorpsCount() && context.generation == KillRewardGeneration;
}

static bool RewardFormation(void* group)
{
    if (!group || Group(At<int>(group, 0x3C)) != group)
        return false;
    const int corps = At<int>(group, 0x38);
    const int side = At<int>(group, 0x40);
    return corps >= 0 && corps < CorpsCount() && side >= 0 && side < 2 && side == At<int>(CorpsObject(corps), 0xC) && At<uint8_t>(group, 0x1E8) == 2 && At<uint8_t>(group, 0x2D20) >= 2 && !At<uint8_t>(group, 0x186) && !At<uint8_t>(group, 0x191) && !At<uint8_t>(group, 0x198) && At<uint8_t>(group, 0x2DFD) == 0;
}

static bool RewardKillEligible(void* group, bool wasDead, bool commander, int flags)
{
    const auto* context = CurrentOffscreenReward;
    if (!KillRewards.enabled || !context || context->winner < 0 || wasDead || commander || flags != 1 || !RewardStorageMatches(*context) || !RewardFormation(group) || At<uint8_t>(group, 0x187))
        return false;
    // Native elimination also visits the root's belonging groups. Other groups
    // and nested scripted removals must not inherit the root's attacker credit.
    return At<int>(group, 0x38) == context->victim && At<int>(group, 0x40) == context->victimSide && (At<int>(group, 0x3C) == context->root || At<int>(group, 0x48) == context->root);
}

static void RecordOffscreenKill(void* group, bool leader)
{
    auto& context = *CurrentOffscreenReward;
    const int rank = At<uint8_t>(group, 0x2D20);
    const float gain = leader ? (rank <= 5 ? KillRewards.officer : KillRewards.captain) : (EliteUnits[At<uint16_t>(group, 0x2D0E)] ? KillRewards.elite : KillRewards.soldier);
    context.target += gain;
    ++context.deaths;
}

static void ResolveOffscreen(void* group, AttackFn resolve)
{
    EnsureManager(At<void*>(group, 0x28));
    OffscreenRewardContext context;
    context.previous = CurrentOffscreenReward;
    context.manager = BattleManager;
    context.groups = GroupStorage;
    context.corps = CorpsStorage;
    context.groupCount = GroupCount();
    context.corpsCount = CorpsCount();
    context.generation = KillRewardGeneration;
    if (KillRewards.enabled && RewardFormation(group) && !At<uint8_t>(group, 0x187))
    {
        void* attacker = Group(At<int>(group, 0x1D0));
        if (RewardFormation(attacker) && At<int>(attacker, 0x40) != At<int>(group, 0x40))
        {
            const int winner = At<int>(attacker, 0x38);
            context.root = At<int>(group, 0x3C);
            context.victim = At<int>(group, 0x38);
            context.victimSide = At<int>(group, 0x40);
            context.winner = winner;
        }
    }
    // Group updates can run on native worker tasks. Serialize attributed
    // resolutions only, so simultaneous kills cannot race the shared budget
    // or spend overlapping native reward credit. Nested calls reuse the lock.
    KillRewardGuard guard(context.winner >= 0);
    if (context.winner >= 0)
    {
        if (!RewardStorageMatches(context) || Corps[context.winner].depth != 0)
            context.winner = -1;
        else
        {
            Corps[context.winner].Observe(At<float>(CorpsObject(context.winner), 0x28));
            context.nativeBefore = Corps[context.winner].nativeGains;
        }
    }
    // Even an ineligible nested resolution masks the outer attribution scope.
    CurrentOffscreenReward = &context;
    resolve(group);
    CurrentOffscreenReward = context.previous;
    if (context.winner < 0 || !context.deaths || !RewardStorageMatches(context) || At<int>(CorpsObject(context.winner), 0xC) != 1 - context.victimSide || Corps[context.winner].depth != 0)
        return;
    auto& state = Corps[context.winner];
    auto& morale = At<float>(CorpsObject(context.winner), 0x28);
    state.Observe(morale); // Includes direct native writes, not only Raise calls.
    if (context.previous && context.previous->winner == context.winner && RewardStorageMatches(*context.previous))
    {
        context.previous->target += context.target;
        context.previous->deaths += context.deaths;
        return; // The outer scope credits these native gains exactly once.
    }
    const float nativeCredit = fmaxf(0, state.nativeGains - context.nativeBefore);
    auto& budget = KillRewardBudgets[context.winner];
    const float added = state.AddPermanent(morale, budget.Allow(context.target, nativeCredit, KillRewards));
    budget.spent += added; // Only actual mod top-ups consume the cap.
    RecordMorale(context.winner, MoraleReason::OffscreenVictory, added);
    if (added > 0 && (++KillRewardEvents <= 5 || KillRewardEvents % 200 == 0))
        Log("Off-screen kill reward: corps=%d deaths=%d target=%.2f nativeCredit=%.2f added=%.2f windowSpent=%.2f.", context.winner, context.deaths, context.target, nativeCredit, added, budget.spent);
}

static void OffscreenCasualties(void* group) { ResolveOffscreen(group, OriginalOffscreenCasualties); }
