// Small snapshots shared with the game-thread HUD. Native group callbacks can use workers.
ExperienceSettings Experience;
static CorpsExperience CorpsExperiences[MaxCorps];
static GroupExperience GroupExperiences[MaxGroups];
static SRWLOCK ExperienceLock = SRWLOCK_INIT;
static float BattleExperienceTime = 0;
static float RescueRewardsPaid = 0;
static unsigned long BattleExperienceGeneration = 0;
static thread_local MoraleReason CurrentMoraleReason = MoraleReason::None;

struct ExperienceGuard
{
    ExperienceGuard() { AcquireSRWLockExclusive(&ExperienceLock); }
    ~ExperienceGuard() { ReleaseSRWLockExclusive(&ExperienceLock); }
};

struct ReasonScope
{
    MoraleReason previous;
    explicit ReasonScope(MoraleReason reason) : previous(CurrentMoraleReason) { CurrentMoraleReason = reason; }
    ~ReasonScope() { CurrentMoraleReason = previous; }
};

static void ResetExperience()
{
    ExperienceGuard guard;
    for (auto& state : CorpsExperiences) state = {};
    for (auto& state : GroupExperiences) state = {};
    BattleExperienceTime = RescueRewardsPaid = 0;
    ++BattleExperienceGeneration;
}

static void AdvanceExperience(int corps, float seconds)
{
    ExperienceGuard guard;
    CorpsExperiences[corps].seconds += seconds;
    BattleExperienceTime = fmaxf(BattleExperienceTime, CorpsExperiences[corps].seconds);
}

static int ReasonPriority(MoraleReason reason)
{
    if (reason == MoraleReason::None || reason == MoraleReason::BattleEvent) return 0;
    if (reason == MoraleReason::Rescue || reason == MoraleReason::Surprise) return 4;
    if (reason == MoraleReason::OfficerDefeated || reason == MoraleReason::CaptainDefeated || reason == MoraleReason::Panic || reason == MoraleReason::Reinforcements || reason == MoraleReason::Support) return 3;
    if (reason == MoraleReason::Casualties || reason == MoraleReason::SupportExpired) return 2;
    return 1;
}

static void RecordMorale(int corps, MoraleReason reason, float amount)
{
    if (corps < 0 || corps >= CorpsCount() || !isfinite(amount) || fabsf(amount) < 0.001f)
        return;
    ExperienceGuard guard;
    auto& state = CorpsExperiences[corps];
    if (state.seconds - state.reasonTime < 3 && ReasonPriority(reason) < ReasonPriority(state.reason))
        return;
    state.amount = reason == state.reason && state.seconds - state.reasonTime < 1 ? state.amount + amount : amount;
    state.reason = reason;
    state.reasonTime = state.seconds;
}

CorpsExperience ReadCorpsExperience(int id)
{
    ExperienceGuard guard;
    return id >= 0 && id < CorpsCount() ? CorpsExperiences[id] : CorpsExperience{};
}

GroupExperience ReadGroupExperience(int id)
{
    ExperienceGuard guard;
    return id >= 0 && id < GroupCount() ? GroupExperiences[id] : GroupExperience{};
}

float ExperienceTime() { ExperienceGuard guard; return BattleExperienceTime; }
unsigned long ExperienceGeneration() { ExperienceGuard guard; return BattleExperienceGeneration; }

static void RecordCasualty(int id)
{
    if (id < 0 || id >= GroupCount()) return;
    ExperienceGuard guard;
    ++GroupExperiences[id].casualties;
}

static void RecordFormationBreak(int id)
{
    ExperienceGuard guard;
    // A repeatedly regrouping formation counts once per battle, not once per cooldown.
    GroupExperiences[id].breaks = 1;
}

float CompleteRescue(int id)
{
    void* group = Group(id);
    if (!Experience.rescue || !group || At<int>(group, 0x40) != 0 || !GroupAlive(group) || At<uint8_t>(group, 0x191) || At<uint8_t>(group, 0x198) || At<uint8_t>(group, 0x2DFD) != 0 || At<uint8_t>(group, 0x2D20) < 2 || At<uint8_t>(group, 0x2D20) > 5)
        return -1;
    const int corps = At<int>(group, 0x38);
    void* object = CorpsObject(corps);
    if (!object || At<int>(object, 0xC) != 0)
        return -1;
    float reward;
    {
        ExperienceGuard guard;
        if (CorpsExperiences[corps].rewarded) return -1;
        CorpsExperiences[corps].rewarded = true;
        reward = fminf(Experience.rescueReward, fmaxf(0, Experience.rescueBattleCap - RescueRewardsPaid));
        RescueRewardsPaid += reward;
    }
    auto& morale = At<float>(object, 0x28);
    const float added = Corps[corps].AddPermanent(morale, reward);
    {
        ExperienceGuard guard;
        RescueRewardsPaid -= reward - added;
    }
    RecordMorale(corps, MoraleReason::Rescue, added);
    Log("Rescue completed: corps=%d officer=%d permanentReward=%.2f.", corps, id, added);
    return added;
}
