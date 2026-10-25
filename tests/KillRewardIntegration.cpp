// Exercise production resolution/kill/morale callbacks with synthetic formations.
#include "../src/Game.cpp"
#include <stdio.h>

using namespace MoraleMatters;
static int Assertions = 0;
#define CHECK(expression) do { ++Assertions; if (!(expression)) { fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #expression); return 1; } } while (0)
static bool Near(float a, float b) { return fabsf(a - b) < 0.001f; }
alignas(16) static uint8_t ManagerMemory[0x100], GroupMemory[10 * 0x2E68], CorpsMemory[3 * 0xEF8];
static uint8_t DeadMembers[10][64];
struct Death { int group; bool leader; int member; int flags; };
static Death Deaths[64];
static int DeathCount, ResolveCalls;
static float NativeGain, NativeLossAfterGain;
static bool DirectGain, RefuseDeath, NestedResolve, ChangeWinnerSide;
static bool ResetDuringResolve, ShrinkDuringResolve;
static bool ConcurrentResolve;
static thread_local int WorkerIndex = 0;
namespace MoraleMatters { void Log(const char*, ...) {} }

static void FakeTick(void*, float) {}
static void FakeRaise(void* corps, float amount, int) { At<float>(corps, 0x28) = Clamp(At<float>(corps, 0x28) + amount, 1, 899); }
static void FakeLower(void* corps, float amount, int cause) { FakeRaise(corps, -amount, cause); }
static void FakeKill(void* group, uint8_t leader, int member, int)
{
    if (RefuseDeath || (leader ? At<uint8_t>(group, 0x110) != 0 : MemberDead(group, member)))
        return;
    if (leader)
        At<uint8_t>(group, 0x110) = 1;
    else
        DeadMembers[At<int>(group, 0x3C)][member] = 1;
    --At<uint8_t>(group, 0x2D61);
    Lower(CorpsObject(At<int>(group, 0x38)), 4, -1);
    void* source = Group(At<int>(group, 0x1D0));
    if (source && NativeGain > 0)
    {
        void* winner = CorpsObject(At<int>(source, 0x38));
        if (DirectGain)
            FakeRaise(winner, NativeGain, -1);
        else
            Raise(winner, NativeGain, -1);
        if (NativeLossAfterGain > 0)
            Lower(winner, NativeLossAfterGain, -1);
    }
}

static void FakeResolve(void* group)
{
    ++ResolveCalls;
    if (ConcurrentResolve)
    {
        for (int i = 0; i < 4; ++i)
            Kill(group, 0, WorkerIndex * 4 + i, 1);
        return;
    }
    const int root = At<int>(group, 0x3C);
    for (int i = 0; i < DeathCount; ++i)
    {
        const auto& death = Deaths[i];
        void* victim = Group(death.group);
        if (death.group == root || At<int>(victim, 0x48) == root)
            Kill(victim, death.leader ? 1 : 0, death.member, death.flags);
    }
    if (NestedResolve && root == 0)
        OffscreenCasualties(Group(2));
    if (ChangeWinnerSide)
        At<int>(CorpsObject(1), 0xC) = 0;
    if (ResetDuringResolve)
        ResetBattle(ManagerMemory);
    if (ShrinkDuringResolve)
    {
        At<int>(ManagerMemory, 0x70) = 1;
        RefreshStorage();
    }
    At<int>(group, 0x1D0) = -1; // The real native resolver clears this after processing.
}

static void Setup(float morale = 400)
{
    Config = {};
    Config.reinforcements = Config.surpriseForces = false;
    Config.enemyProximity = false; // This fixture isolates off-screen rewards without a native death getter.
    Behavior = {};
    Behavior.enabled = false;
    KillRewards = {};
    memset(ManagerMemory, 0, sizeof(ManagerMemory));
    memset(GroupMemory, 0, sizeof(GroupMemory));
    memset(CorpsMemory, 0, sizeof(CorpsMemory));
    memset(DeadMembers, 0, sizeof(DeadMembers));
    At<void*>(ManagerMemory, 0x68) = CorpsMemory;
    At<int>(ManagerMemory, 0x70) = 3;
    At<void*>(ManagerMemory, 0x78) = GroupMemory;
    At<int>(ManagerMemory, 0x80) = 10;
    for (int id = 0; id < 3; ++id)
    {
        void* corps = CorpsMemory + id * 0xEF8;
        At<void*>(corps, 0) = ManagerMemory;
        At<int>(corps, 0xC) = id == 1 ? 1 : 0;
        At<int>(corps, 0x14) = id == 0 ? 4 : id == 1 ? 7 : 9;
        At<float>(corps, 0x28) = morale;
    }
    for (int id = 0; id < 10; ++id)
    {
        void* group = GroupMemory + id * 0x2E68;
        const int corps = id < 5 ? 0 : id < 8 ? 1 : 2;
        At<void*>(group, 0x28) = ManagerMemory;
        At<int>(group, 0x3C) = id;
        At<int>(group, 0x38) = corps;
        At<int>(group, 0x40) = corps == 1 ? 1 : 0;
        At<int>(group, 0x48) = id;
        At<int>(group, 0x1D0) = corps == 1 ? 0 : 5;
        At<uint8_t>(group, 0x1E8) = 2;
        At<uint8_t>(group, 0x2D20) = 6;
        At<uint8_t>(group, 0x2D21) = 1;
        At<uint8_t>(group, 0x2D61) = 64;
        At<int>(group, 0x70) = 64;
        At<int>(group, 0x120) = 64;
        At<void*>(group, 0x118) = DeadMembers[id];
    }
    OriginalCorpsTick = OriginalGroupTick = &FakeTick;
    OriginalRaise = &FakeRaise;
    OriginalLower = &FakeLower;
    OriginalKill = &FakeKill;
    OriginalOffscreenCasualties = &FakeResolve;
    DeathCount = ResolveCalls = 0;
    NativeGain = NativeLossAfterGain = 0;
    DirectGain = RefuseDeath = NestedResolve = ChangeWinnerSide = false;
    ResetDuringResolve = ShrinkDuringResolve = false;
    ConcurrentResolve = false;
    EliteUnits[42] = false;
    ResetBattle(ManagerMemory);
}

static void Plan(int group, bool leader = false, int member = 0, int flags = 1) { Deaths[DeathCount++] = {group, leader, member, flags}; }
static void Resolve(int group = 0) { OffscreenCasualties(Group(group)); }
static float Morale(int corps) { return At<float>(CorpsObject(corps), 0x28); }
static DWORD WINAPI WorkerResolve(void* index)
{
    WorkerIndex = static_cast<int>(reinterpret_cast<intptr_t>(index));
    Resolve();
    return 0;
}

int main()
{
    Setup();
    Plan(0);
    Resolve();
    CHECK(ResolveCalls == 1 && Near(Morale(0), 399) && Near(Morale(1), 400.25f));
    CHECK(Near(Corps[1].base, 400.25f) && Near(Corps[1].nativeGains, 0));
    CHECK(Near(KillRewardBudgets[1].spent, 0.25f) && Near(KillRewardBudgets[1].remaining, 10));
    At<int>(Group(0), 0x1D0) = 5;
    Resolve();
    CHECK(Near(Morale(1), 400.25f)); // Same death cannot pay twice.

    Setup();
    At<uint16_t>(Group(0), 0x2D0E) = 42;
    EliteUnits[42] = true;
    Plan(0);
    Resolve();
    CHECK(Near(Morale(0), 398) && Near(Morale(1), 400.5f));

    Setup();
    Plan(0, true);
    Resolve();
    CHECK(Near(Morale(0), 390) && Near(Morale(1), 402));
    Setup();
    At<uint8_t>(Group(0), 0x2D20) = 3;
    Plan(0, true);
    Resolve();
    CHECK(Near(Morale(0), 350) && Near(Morale(1), 410));

    Setup();
    Config.casualties = false;
    Plan(0);
    Resolve();
    CHECK(Near(Morale(0), 396) && Near(Morale(1), 400.25f));
    Setup();
    KillRewards.enabled = false;
    Plan(0);
    Resolve();
    CHECK(Near(Morale(0), 399) && Near(Morale(1), 400));

    for (int exclusion = 0; exclusion < 14; ++exclusion)
    {
        Setup();
        Plan(0);
        if (exclusion == 0) At<int>(Group(0), 0x1D0) = -1;
        if (exclusion == 1) At<int>(Group(0), 0x1D0) = 4096;
        if (exclusion == 2) At<int>(Group(0), 0x1D0) = 1; // Same-side attacker.
        if (exclusion == 3) At<uint8_t>(Group(0), 0x187) = 1;
        if (exclusion == 4) At<uint8_t>(Group(0), 0x2DFD) = 1;
        if (exclusion == 5) At<uint8_t>(Group(5), 0x186) = 1;
        if (exclusion == 6) At<uint8_t>(Group(5), 0x1E8) = 0;
        if (exclusion == 7) Deaths[0].flags = 0x10000;
        if (exclusion == 8) Deaths[0].flags = 0;
        if (exclusion == 9) RefuseDeath = true;
        if (exclusion == 10) { Deaths[0].leader = true; At<int>(CorpsObject(0), 0x14) = 0; }
        if (exclusion == 11) At<uint8_t>(Group(0), 0x191) = 1;
        if (exclusion == 12) ChangeWinnerSide = true;
        if (exclusion == 13) Corps[1].Begin(At<float>(CorpsObject(1), 0x28));
        Resolve();
        CHECK(Near(Morale(1), 400) && Near(KillRewardBudgets[1].spent, 0));
        CHECK(CurrentOffscreenReward == nullptr);
    }
    Setup();
    Kill(Group(0), 0, 0, 1); // Native/synthetic kill outside off-screen resolution.
    CHECK(Near(Morale(1), 400));

    const float credits[] = {0.1f, 0.25f, 2.0f};
    for (float credit : credits)
    {
        Setup();
        NativeGain = credit;
        Plan(0);
        Resolve();
        CHECK(Near(Morale(1), 400 + fmaxf(credit, 0.25f)));
        CHECK(Near(KillRewardBudgets[1].spent, fmaxf(0, 0.25f - credit)));
    }
    Setup();
    At<uint8_t>(Group(0), 0x2D20) = 3;
    NativeGain = 50;
    Plan(0, true);
    Resolve();
    CHECK(Near(Morale(1), 450) && Near(KillRewardBudgets[1].spent, 0));
    Setup();
    DirectGain = true;
    NativeGain = 0.1f;
    Plan(0);
    Resolve();
    CHECK(Near(Morale(1), 400.25f));
    Setup();
    NativeGain = 1;
    NativeLossAfterGain = 1;
    Plan(0);
    Resolve();
    CHECK(Near(Morale(1), 400) && Near(KillRewardBudgets[1].spent, 0));

    Setup();
    for (int member = 0; member < 50; ++member) Plan(0, false, member);
    Resolve();
    CHECK(Near(Morale(1), 410) && Near(KillRewardBudgets[1].spent, 10));
    CorpsTick(CorpsObject(1), 0);
    CHECK(Near(KillRewardBudgets[1].remaining, 10));
    CorpsTick(CorpsObject(1), 9.5f);
    DeathCount = 0;
    Plan(0, false, 50);
    At<int>(Group(0), 0x1D0) = 5;
    Resolve();
    CHECK(Near(Morale(1), 410));
    CorpsTick(CorpsObject(1), 0.5f);
    DeathCount = 0;
    Plan(0, false, 51);
    At<int>(Group(0), 0x1D0) = 5;
    Resolve();
    CHECK(Near(Morale(1), 410.25f) && Near(KillRewardBudgets[1].spent, 0.25f));

    Setup();
    KillRewards.soldier = 3;
    KillRewards.cap = 5;
    KillRewards.seconds = 2;
    Plan(0, false, 0);
    Plan(0, false, 1);
    Resolve();
    CHECK(Near(Morale(1), 405) && Near(KillRewardBudgets[1].remaining, 2));
    Setup();
    KillRewards.cap = 0;
    Plan(0);
    Resolve();
    CHECK(Near(Morale(1), 400));

    Setup();
    CHECK(Corps[1].Rally(At<float>(CorpsObject(1), 0x28), Config));
    Plan(0);
    Resolve();
    CHECK(Near(Morale(1), 500.25f) && Near(Corps[1].base, 400.25f) && Near(Corps[1].nativeGains, 0));
    CorpsTick(CorpsObject(1), 180);
    CHECK(Near(Morale(1), 400.25f) && Near(Corps[1].bonus, 0));
    Setup(898.9f);
    Plan(0);
    Resolve();
    CHECK(Near(Morale(1), 899) && Near(KillRewardBudgets[1].spent, 0.1f));
    Setup(899);
    Plan(0);
    Resolve();
    CHECK(Near(Morale(1), 899) && Near(KillRewardBudgets[1].spent, 0));

    Setup();
    Plan(5);
    Resolve(5);
    CHECK(Near(Morale(0), 400.25f) && Near(Morale(1), 399)); // Other side wins.
    Setup();
    At<int>(Group(5), 0x1D0) = 8;
    Plan(5);
    Resolve(5);
    CHECK(Near(Morale(2), 400.25f) && Near(KillRewardBudgets[0].spent, 0));

    Setup();
    At<int>(Group(1), 0x48) = 0;
    Plan(0, true);
    Plan(1);
    Resolve();
    CHECK(Near(Morale(1), 402.25f)); // Native elimination of belonging groups.
    Setup();
    NestedResolve = true;
    Plan(0);
    Plan(2);
    NativeGain = 0.1f;
    Resolve();
    CHECK(ResolveCalls == 2 && Near(Morale(1), 400.5f));
    CHECK(Near(KillRewardBudgets[1].spent, 0.3f) && CurrentOffscreenReward == nullptr);
    Setup();
    NestedResolve = true;
    Plan(0);
    Plan(2);
    At<int>(Group(2), 0x1D0) = -1;
    Resolve();
    CHECK(Near(Morale(1), 400.25f)); // Ineligible nested scope masks outer credit.

    Setup();
    At<int>(Group(1), 0x48) = 0;
    At<uint8_t>(Group(1), 0x187) = 1;
    Plan(0);
    Plan(1);
    Resolve();
    CHECK(Near(Morale(1), 400.25f)); // Visible accompanying casualties are excluded.
    Setup();
    KillRewards.soldier = 0;
    Plan(0);
    Resolve();
    CHECK(Near(Morale(1), 400) && Near(KillRewardBudgets[1].remaining, 0));
    Setup();
    ResetDuringResolve = true;
    Plan(0);
    Resolve();
    CHECK(Near(Morale(1), 400) && CurrentOffscreenReward == nullptr);
    Setup();
    ShrinkDuringResolve = true;
    Plan(0);
    Resolve();
    CHECK(Near(At<float>(CorpsMemory + 0xEF8, 0x28), 400) && CurrentOffscreenReward == nullptr);

    Setup();
    ConcurrentResolve = true;
    HANDLE workers[16];
    for (int i = 0; i < 16; ++i)
    {
        workers[i] = CreateThread(nullptr, 0, WorkerResolve, reinterpret_cast<void*>(static_cast<intptr_t>(i)), 0, nullptr);
        CHECK(workers[i] != nullptr);
    }
    CHECK(WaitForMultipleObjects(16, workers, TRUE, 10000) == WAIT_OBJECT_0);
    for (auto worker : workers) CloseHandle(worker);
    CHECK(ResolveCalls == 16 && Near(Morale(1), 410) && Near(KillRewardBudgets[1].spent, 10));
    CHECK(Near(Morale(0), 336));
    ResetBattle(ManagerMemory);
    CHECK(Near(KillRewardBudgets[1].spent, 0) && Near(KillRewardBudgets[1].remaining, 0));
    printf("PASS: %d assertions against production off-screen kill reward callbacks.\n", Assertions);
    return 0;
}
