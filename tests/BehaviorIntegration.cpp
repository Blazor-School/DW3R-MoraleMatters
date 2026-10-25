// Production callbacks with synthetic memory; no running game is accessed.
#include "../src/Game.cpp"
#include <stdio.h>
#include <stdarg.h>

using namespace MoraleMatters;
static int Assertions = 0;
#define CHECK(expression) do { ++Assertions; if (!(expression)) { fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #expression); return 1; } } while (0)
static bool Near(float a, float b) { return fabsf(a - b) < 0.001f; }
alignas(16) static uint8_t ManagerMemory[0x100], GroupMemory[6 * 0x2E68], CorpsMemory[2 * 0xEF8];
alignas(16) static uint8_t CharacterMemory[0x900], ControllerMemory[0x510], BlackboardMemory[0x100];
static int ObservedRank, ObservedMode, StatusChanges;
static bool Stopped, ReassignRank, ReassignMode, Reenter;

namespace MoraleMatters { void Log(const char*, ...) {} }
static bool FakeDead(void* group) { return At<uint8_t>(group, 0x110) || At<uint8_t>(group, 0x186) || At<float>(group, 0x2DEC) <= 0; }
static void FakeTick(void*, float) {}
static void FakeRaise(void* corps, float amount, int) { At<float>(corps, 0x28) = Clamp(At<float>(corps, 0x28) + amount, 1, 899); }
static void FakeLower(void* corps, float amount, int cause) { FakeRaise(corps, -amount, cause); }
static void FakeKill(void* group, uint8_t leader, int, int) { if (leader) At<uint8_t>(group, 0x110) = 1; }
static void* FakeOwner(void* weak) { return At<void*>(weak, 0); }
static bool FakeStopped(void*, void*, int) { return Stopped; }
static void FakeStatus(void* controller, uint8_t status) { ++StatusChanges; At<uint8_t>(controller, 0x3EC) = status; }
static void* FakeOffset(void*, double* result, int) { result[0] = 100; result[1] = 200; return result; }
static void FakeBehaviorTick(void* controller, float dt)
{
    ObservedRank = At<int>(controller, 0x484);
    void* group = Group(At<int>(CharacterMemory, 0x7A0));
    ObservedMode = At<int>(group, 0x178);
    if (Reenter && !At<int>(controller, 0x500))
    {
        At<int>(controller, 0x500) = 1;
        BehaviorTick(controller, dt);
    }
    if (ReassignRank) At<int>(controller, 0x484) = 4;
    if (ReassignMode) At<int>(group, 0x178) = 3;
}

static void Setup(float morale = 400)
{
    Config = {};
    Behavior = {};
    memset(ManagerMemory, 0, sizeof(ManagerMemory));
    memset(GroupMemory, 0, sizeof(GroupMemory));
    memset(CorpsMemory, 0, sizeof(CorpsMemory));
    memset(CharacterMemory, 0, sizeof(CharacterMemory));
    memset(ControllerMemory, 0, sizeof(ControllerMemory));
    At<void*>(ManagerMemory, 0x68) = CorpsMemory;
    At<int>(ManagerMemory, 0x70) = 2;
    At<void*>(ManagerMemory, 0x78) = GroupMemory;
    At<int>(ManagerMemory, 0x80) = 6;
    for (int id = 0; id < 2; ++id)
    {
        void* corps = CorpsMemory + id * 0xEF8;
        At<void*>(corps, 0) = ManagerMemory;
        At<int>(corps, 0xC) = id;
        At<int>(corps, 0x14) = id * 3;
        At<float>(corps, 0x28) = morale;
    }
    for (int id = 0; id < 6; ++id)
    {
        void* group = GroupMemory + id * 0x2E68;
        At<void*>(group, 0x28) = ManagerMemory;
        At<int>(group, 0x3C) = id;
        At<int>(group, 0x38) = id / 3;
        At<int>(group, 0x40) = id / 3;
        At<uint8_t>(group, 0x1E8) = 2;
        At<uint8_t>(group, 0x2D20) = id % 3 ? 6 : 3;
        At<uint8_t>(group, 0x2D21) = 1;
        At<uint8_t>(group, 0x2DFC) = 1;
        At<uint8_t>(group, 0x2D61) = 8;
        At<float>(group, 0x2DEC) = 100;
        At<double>(group, 0x2DA8) = id / 3 * 50000;
    }
    At<void*>(ControllerMemory, 0x3DC) = CharacterMemory;
    At<void*>(ControllerMemory, 0x3A0) = BlackboardMemory;
    At<uint8_t>(ControllerMemory, 0x3D8) = 1;
    At<int>(ControllerMemory, 0x484) = 10;
    At<uint8_t>(ControllerMemory, 0x3EC) = 5;
    At<uint8_t>(CharacterMemory, 0x74C) = 2;
    At<int>(CharacterMemory, 0x7A0) = 1;
    At<int>(CharacterMemory, 0x7A4) = 0;
    Image = reinterpret_cast<uint8_t*>(reinterpret_cast<uintptr_t>(&FakeDead) - 0x13AE060);
    OriginalCorpsTick = OriginalGroupTick = &FakeTick;
    OriginalRaise = &FakeRaise;
    OriginalLower = &FakeLower;
    OriginalKill = &FakeKill;
    OriginalBehaviorTick = &FakeBehaviorTick;
    OriginalFormationOffset = &FakeOffset;
    BehaviorOwner = &FakeOwner;
    BehaviorStopped = &FakeStopped;
    SetBehaviorStatus = &FakeStatus;
    Stopped = ReassignRank = ReassignMode = Reenter = false;
    StatusChanges = 0;
    ResetBattle(ManagerMemory);
}

static void Advance(int seconds) { for (int i = 0; i < seconds; ++i) CorpsTick(CorpsObject(0), 1); }

int main()
{
    Setup();
    for (int rank = 0; rank < 36; ++rank)
        for (int morale = 1; morale <= 899; morale += 10)
        {
            const int adjusted = MoraleProfile(rank, static_cast<float>(morale), false, Config, Behavior);
            CHECK(adjusted >= 0 && adjusted < 36);
            CHECK(rank < 20 ? adjusted < 20 : (adjusted >= 20 && (rank - 20) / 4 == (adjusted - 20) / 4));
        }
    CHECK(MoraleProfile(-1, 899, false, Config, Behavior) == -1);
    CHECK(MoraleProfile(36, 899, false, Config, Behavior) == 36);
    CHECK(MoraleProfile(10, 100, false, Config, Behavior) == 7);
    CHECK(MoraleProfile(10, 400, false, Config, Behavior) == 10);
    CHECK(MoraleProfile(10, 800, false, Config, Behavior) == 12);
    CHECK(MoraleProfile(10, 100, true, Config, Behavior) == 8);
    CHECK(MoraleProfile(10, 800, true, Config, Behavior) == 11);

    Setup(800);
    BehaviorTick(ControllerMemory, 0.1f);
    CHECK(ObservedRank == 12 && At<int>(ControllerMemory, 0x484) == 10);
    Reenter = true;
    BehaviorTick(ControllerMemory, 0.1f);
    CHECK(ObservedRank == 12 && At<int>(ControllerMemory, 0x484) == 10);
    Reenter = false;
    ReassignRank = true;
    BehaviorTick(ControllerMemory, 0.1f);
    CHECK(At<int>(ControllerMemory, 0x484) == 4);

    Setup(100);
    BehaviorTick(ControllerMemory, 0.1f);
    CHECK(ObservedRank == 7 && At<int>(ControllerMemory, 0x484) == 10);
    At<uint8_t>(CharacterMemory, 0x74C) = 0;
    BehaviorTick(ControllerMemory, 0.1f);
    CHECK(ObservedRank == 10);
    At<uint8_t>(CharacterMemory, 0x74C) = 2;
    BehaviorTick(ControllerMemory, 0);
    CHECK(ObservedRank == 10);
    Behavior.enabled = false;
    BehaviorTick(ControllerMemory, 0.1f);
    CHECK(ObservedRank == 10);
    Behavior.enabled = true;
    At<uint8_t>(Group(1), 0x2DFD) = 1;
    BehaviorTick(ControllerMemory, 0.1f);
    CHECK(ObservedRank == 10);

    Setup(100);
    double result[3] = {0, 0, 777};
    CHECK(FormationOffset(Group(1), result, 0) == result);
    CHECK(Near(static_cast<float>(result[0]), 135) && Near(static_cast<float>(result[1]), 270) && result[2] == 777);
    At<float>(CorpsObject(0), 0x28) = 800;
    FormationOffset(Group(1), result, 0);
    CHECK(Near(static_cast<float>(result[0]), 80));
    GroupBehavior[1].commanderNear = true;
    FormationOffset(Group(1), result, 0);
    CHECK(Near(static_cast<float>(result[0]), 85));
    FormationOffset(Group(1), result, -1);
    CHECK(Near(static_cast<float>(result[0]), 100));
    At<uint8_t>(Group(1), 0x2DFC) = 4;
    FormationOffset(Group(1), result, 0);
    CHECK(Near(static_cast<float>(result[0]), 100));
    At<uint8_t>(Group(1), 0x2DFC) = 1;
    At<uint8_t>(Group(1), 0x2E58) = 1;
    FormationOffset(Group(1), result, 0);
    CHECK(Near(static_cast<float>(result[0]), 100));

    Setup(100);
    At<int>(Group(1), 0x178) = 2;
    GroupTick(Group(1), 0.1f);
    CHECK(Near(GroupBehavior[1].regroupRemaining, 8));
    BehaviorTick(ControllerMemory, 0.1f);
    CHECK(ObservedMode == 0 && At<int>(Group(1), 0x178) == 2);
    CHECK(At<uint8_t>(ControllerMemory, 0x3EC) == 0 && StatusChanges == 1);
    GroupTick(Group(1), 8);
    CHECK(Near(GroupBehavior[1].regroupRemaining, 0) && Near(GroupBehavior[1].cooldown, 20));
    GroupTick(Group(1), 19);
    CHECK(Near(GroupBehavior[1].regroupRemaining, 0));
    GroupTick(Group(1), 1);
    CHECK(Near(GroupBehavior[1].regroupRemaining, 8));
    At<float>(CorpsObject(0), 0x28) = 300;
    GroupTick(Group(1), 0.1f);
    CHECK(Near(GroupBehavior[1].regroupRemaining, 0));
    CHECK(!CanRegroup(Group(0))); // Named commander keeps strategic orders.

    Setup(100);
    At<int>(Group(1), 0x178) = 2;
    GroupTick(Group(1), 0.1f);
    Stopped = true;
    BehaviorTick(ControllerMemory, 0.1f);
    CHECK(ObservedMode == 2 && StatusChanges == 0);
    Stopped = false;
    At<uint8_t>(ControllerMemory, 0x3EC) = 3;
    BehaviorTick(ControllerMemory, 0.1f);
    CHECK(ObservedMode == 2 && StatusChanges == 0);
    At<uint8_t>(ControllerMemory, 0x3EC) = 5;
    ReassignMode = true;
    BehaviorTick(ControllerMemory, 0.1f);
    CHECK(At<int>(Group(1), 0x178) == 3); // Preserve a genuine native order change.

    Setup(100);
    Config.casualties = false;
    Behavior.leadership = false;
    Kill(Group(1), 1, -1, 0);
    CHECK(Near(Corps[0].base, 77.5f));
    CHECK(Near(GroupBehavior[2].shockRemaining, 7.5f));
    CHECK(Near(At<float>(CorpsObject(1), 0x28), 100));
    Kill(Group(1), 1, -1, 0);
    CHECK(Near(Corps[0].base, 77.5f));
    Kill(Group(2), 1, -1, 0);
    CHECK(Near(Corps[0].base, 70) && Near(CorpsBehavior[0].panicSpent, 30));
    Advance(10);
    CHECK(Near(CorpsBehavior[0].panicRemaining, 0));

    Setup(800);
    Config.casualties = false;
    Behavior.leadership = false;
    Kill(Group(1), 1, -1, 0);
    CHECK(Near(Corps[0].base, 792.5f));

    Setup(100);
    Config.casualties = false;
    Advance(1); // Commander aura sample.
    CHECK(GroupBehavior[1].commanderNear && GroupBehavior[2].commanderNear);
    Kill(Group(1), 1, -1, 0);
    CHECK(Near(Corps[0].base, 88.75f));
    CHECK(Near(GroupBehavior[2].shockRemaining, 3.75f));

    Setup(100);
    Config.casualties = false;
    At<double>(Group(0), 0x2DA8) = 5000;
    At<double>(Group(2), 0x2DA8) = 5000;
    Kill(Group(1), 1, -1, 0);
    CHECK(Near(At<float>(CorpsObject(0), 0x28), 100));
    CHECK(Near(GroupBehavior[2].shockRemaining, 0));

    Setup(100);
    Config.casualties = false;
    Kill(Group(1), 1, -1, 0x10000);
    CHECK(Near(At<float>(CorpsObject(0), 0x28), 100));
    Kill(Group(0), 1, -1, 0);
    CHECK(Near(At<float>(CorpsObject(0), 0x28), 100));

    Setup(100);
    CHECK(Corps[0].Rally(At<float>(CorpsObject(0), 0x28), Config));
    Advance(10);
    CHECK(Near(Corps[0].base, 100));
    Advance(1);
    CHECK(Near(Corps[0].base, 101) && Near(At<float>(CorpsObject(0), 0x28), 201));
    CHECK(Near(Corps[0].nativeGains, 0) && Near(Corps[0].remaining, 169));
    At<int>(Group(1), 0x178) = 2;
    Advance(20);
    CHECK(Near(Corps[0].base, 101) && Near(CorpsBehavior[0].quietTime, 0));
    At<int>(Group(1), 0x178) = 0;
    Advance(10);
    CHECK(Near(Corps[0].base, 101));

    Setup(100);
    At<uint8_t>(Group(0), 0x110) = 1;
    Advance(11);
    CHECK(Near(Corps[0].base, 100.25f) && !GroupBehavior[1].commanderNear);

    Setup(399.9f);
    Advance(11);
    CHECK(Near(Corps[0].base, 400));
    Advance(20);
    CHECK(Near(Corps[0].base, 400));
    Behavior.enabled = false;
    At<float>(CorpsObject(0), 0x28) = 100;
    Advance(20);
    CHECK(Near(Corps[0].base, 100));
    ResetBattle(ManagerMemory);
    CHECK(Near(GroupBehavior[1].regroupRemaining, 0) && Near(CorpsBehavior[0].quietTime, 0));

    printf("PASS: %d assertions for native AI/formation callbacks, regroup, panic, recovery, leadership and profile families.\n", Assertions);
    return 0;
}
