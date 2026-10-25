// Exercise the actual native callbacks against synthetic formation memory. No game
// process or hooks are touched; only the original game functions are substituted.
#include "../src/Game.cpp"
#include <stdio.h>
#include <stdarg.h>

using namespace MoraleMatters;
static int Assertions = 0;
#define CHECK(expression) do { ++Assertions; if (!(expression)) { fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #expression); return 1; } } while (0)
static bool Near(float a, float b) { return fabsf(a - b) < 0.001f; }
alignas(16) static uint8_t ManagerMemory[0x400];
alignas(16) static uint8_t GroupMemory[8 * 0x2E68];
alignas(16) static uint8_t CorpsMemory[2 * 0xEF8];
alignas(16) static uint8_t EffectMemory[0x28];
static int EffectArguments[3];
static bool RecursiveSpecial = false;

namespace MoraleMatters
{
    void Log(const char* format, ...)
    {
        va_list arguments;
        va_start(arguments, format);
        vprintf(format, arguments);
        va_end(arguments);
        printf("\n");
    }
}

static bool FakeDead(void* group) { return At<uint8_t>(group, 0x110) || At<uint8_t>(group, 0x186) || At<float>(group, 0x2DEC) <= 0; }
static void FakeSpecial(void* group, uint8_t value)
{
    At<uint8_t>(group, 0x2DFD) = value;
    if (RecursiveSpecial && At<int>(group, 0x3C) == 2)
        SpecialAttribute(Group(3), value);
}
static void FakeTick(void*, float) {}
static void FakeRaise(void* corps, float amount, int) { At<float>(corps, 0x28) = Clamp(At<float>(corps, 0x28) + amount, 1, 899); }
static void FakeLower(void* corps, float amount, int) { FakeRaise(corps, -amount, 0); }
static bool FakeEffect(void* effect, int cause, int, uint8_t)
{
    const int type = At<uint8_t>(effect, 0x18);
    auto* args = At<int*>(effect, 0x20);
    if (type == 10)
        At<uint8_t>(Group(args[1]), 0x191) = 1;
    else if (type >= 6 && type <= 8)
    {
        void* corps = CorpsObject(0);
        if (type == 6)
            At<float>(corps, 0x28) = static_cast<float>(args[2]);
        else
            Raise(corps, static_cast<float>(args[2]), cause);
    }
    return true;
}

static void Effect(int type, int cause, int argument, int group = 2, int side = 0)
{
    At<void*>(EffectMemory, 0) = ManagerMemory;
    At<uint8_t>(EffectMemory, 0x18) = static_cast<uint8_t>(type);
    At<int*>(EffectMemory, 0x20) = EffectArguments;
    EffectArguments[0] = side;
    EffectArguments[1] = group;
    EffectArguments[2] = argument;
    ScenarioEffect(EffectMemory, cause, 0, 0);
}

static void Setup()
{
    Config = {};
    RecursiveSpecial = false;
    memset(ManagerMemory, 0, sizeof(ManagerMemory));
    memset(GroupMemory, 0, sizeof(GroupMemory));
    memset(CorpsMemory, 0, sizeof(CorpsMemory));
    At<void*>(ManagerMemory, 0x68) = CorpsMemory;
    At<int>(ManagerMemory, 0x70) = 2;
    At<void*>(ManagerMemory, 0x78) = GroupMemory;
    At<int>(ManagerMemory, 0x80) = 6;
    for (int id = 0; id < 2; ++id)
    {
        void* corps = CorpsMemory + id * 0xEF8;
        At<void*>(corps, 0) = ManagerMemory;
        At<int>(corps, 0x14) = id == 0 ? 0 : 5;
        At<float>(corps, 0x28) = 400;
    }
    for (int id = 0; id < 8; ++id)
    {
        void* group = GroupMemory + id * 0x2E68;
        At<void*>(group, 0x28) = ManagerMemory;
        At<int>(group, 0x38) = id == 5 ? 1 : 0;
        At<int>(group, 0x3C) = id;
        At<int>(group, 0x44) = id;
        At<int>(group, 0x48) = id == 3 ? 2 : id;
        At<uint8_t>(group, 0x2D20) = 6;
        At<uint8_t>(group, 0x2DFD) = id == 2 || id == 3 || id >= 5 ? 2 : 0;
        At<uint8_t>(group, 0x2D61) = id == 2 || id == 3 ? 6 : 11;
        At<int>(group, 0x2D18) = At<uint8_t>(group, 0x2D61);
        At<float>(group, 0x2DEC) = 100;
    }
    At<uint8_t>(GroupMemory + 4 * 0x2E68, 0x110) = 1;
    Image = reinterpret_cast<uint8_t*>(reinterpret_cast<uintptr_t>(&FakeDead) - 0x13AE060);
    OriginalSpecialAttribute = &FakeSpecial;
    OriginalCorpsTick = OriginalGroupTick = &FakeTick;
    OriginalRaise = &FakeRaise;
    OriginalLower = &FakeLower;
    OriginalScenarioEffect = &FakeEffect;
    ResetBattle(ManagerMemory);
}

static void Advance(int corps, int seconds)
{
    for (int i = 0; i < seconds; ++i)
        CorpsTick(CorpsObject(corps), 1);
}

static void SetupSurprise()
{
    Setup();
    At<uint8_t>(Group(2), 0x2DFD) = 1;
    At<uint8_t>(Group(3), 0x2DFD) = 1;
    ResetBattle(ManagerMemory);
}

#include "NativeEntryProbe.inc"

int main()
{
    Setup();
    OriginalSpecialAttribute = &ClobberSpecialRegisters;
    CHECK(ProbeSpecialEntry(Group(2), 0) == 1);
    CHECK(At<uint8_t>(Group(2), 0x2DFD) == 0);
    CHECK(Arrivals[2].pending);
    Setup();
    void* corps = CorpsObject(0);
    CHECK(CorpsSoldiers(0) == 20);
    CHECK(Corps[0].Rally(At<float>(corps, 0x28), Config));
    Raise(corps, 30, 7); // A separate native morale effect preceding the appearance.
    SpecialAttribute(Group(2), 0);
    SpecialAttribute(Group(3), 0);
    CHECK(Reinforcements[0].beforeSoldiers == 20);
    Advance(0, 3);
    CHECK(Reinforcements[0].arrivedSoldiers == 10);
    CHECK(Near(Reinforcements[0].target, 50));
    CHECK(Near(Reinforcements[0].nativeCredit, 30));
    CHECK(Near(Corps[0].base, 450) && Near(At<float>(corps, 0x28), 550));
    CHECK(Near(Corps[0].nativeGains, 30));
    // Repeated activation, actor recreation and healing must not earn more morale.
    SpecialAttribute(Group(2), 0);
    At<uint8_t>(Group(2), 0x2DFD) = 2;
    SpecialAttribute(Group(2), 0);
    At<float>(Group(2), 0x2DEC) = 200;
    Advance(0, 4);
    CHECK(!Reinforcements[0].open && Near(Corps[0].base, 450));
    CorpsTick(corps, 173);
    CHECK(Near(At<float>(corps, 0x28), 450)); // Meeting bonus expires, reinforcement gain remains.

    Setup();
    SpecialAttribute(Group(5), 0);
    Advance(1, 3);
    CHECK(Reinforcements[1].beforeSoldiers == 0);
    CHECK(Near(Corps[1].base, 400)); // Entirely new corps.

    Setup();
    SpecialAttribute(Group(2), 0);
    At<uint8_t>(Group(2), 0x191) = 1;
    Advance(0, 5);
    CHECK(Reinforcements[0].open && Near(Corps[0].base, 400));
    At<uint8_t>(Group(2), 0x191) = 0;
    At<uint8_t>(Group(2), 0x192) = 1;
    At<uint8_t>(Group(2), 0x2D61) = 1;
    CorpsTick(CorpsObject(0), 0); // Pause does not accept/expire arrivals.
    CHECK(Reinforcements[0].arrivedSoldiers == 0);
    Advance(0, 3);
    CHECK(Reinforcements[0].arrivedSoldiers == 5);
    CHECK(Near(Corps[0].base, 425)); // Logical soldiers while actors stream in.

    Setup();
    SpecialAttribute(Group(2), 0);
    At<int>(Group(2), 0x38) = 1; // Transferred before arrival confirmation.
    Advance(0, 3);
    CHECK(Near(Corps[0].base, 400) && Near(At<float>(CorpsObject(1), 0x28), 400));
    CHECK(!Arrivals[2].pending);
    SpecialAttribute(Group(4), 0);
    CHECK(!Arrivals[4].pending); // Dead groups cannot be re-rewarded.

    Setup();
    SpecialAttribute(Group(2), 0);
    At<uint8_t>(Group(2), 0x110) = 1;
    Advance(0, 3);
    CHECK(Near(Corps[0].base, 400)); // Failed/dead arrival.

    Setup();
    SpecialAttribute(Group(2), 0);
    At<uint8_t>(Group(2), 0x191) = 1;
    Advance(0, 30);
    CHECK(!Reinforcements[0].open && !Arrivals[2].pending && Near(Corps[0].base, 400));

    Setup();
    SpecialAttribute(Group(2), 0);
    Raise(CorpsObject(0), 70, 7);
    Advance(0, 3);
    CHECK(Near(Corps[0].base, 470)); // Never replace or lower a larger native award.

    Setup();
    SpecialAttribute(Group(2), 0);
    At<float>(CorpsObject(0), 0x28) = 430; // Scripted MoraleSet bypasses Raise.
    Advance(0, 3);
    CHECK(Near(Reinforcements[0].nativeCredit, 30) && Near(Corps[0].base, 430));

    Setup();
    Raise(CorpsObject(0), 10, 7);
    Advance(0, 4); // An old event is outside the credit window.
    SpecialAttribute(Group(2), 0);
    Advance(0, 3);
    CHECK(Near(Reinforcements[0].nativeCredit, 0) && Near(Corps[0].base, 435));

    Setup();
    At<int>(CorpsObject(0), 0xC) = 1;
    for (int id = 0; id < GroupCount(); ++id)
        if (At<int>(Group(id), 0x38) == 0)
            At<int>(Group(id), 0x40) = 1;
    SpecialAttribute(Group(2), 0);
    Advance(0, 3);
    CHECK(Near(Corps[0].base, 425)); // Same rule for the enemy side.

    Setup();
    SpecialAttribute(Group(2), 0);
    At<int>(Group(2), 0x40) = 1;
    Advance(0, 3);
    CHECK(Near(Corps[0].base, 400)); // Side changes are transfers, not arrivals.

    Setup();
    Effect(10, 42, 0);
    CHECK(GroupEvents[2].known && GroupEvents[3].known && GroupEvents[2].cause == 42);
    SpecialAttribute(Group(2), 0);
    SpecialAttribute(Group(3), 0);
    At<uint8_t>(Group(2), 0x191) = 0;
    Advance(0, 3);
    CHECK(Near(Corps[0].base, 450));
    Advance(0, 10);
    Effect(8, 42, 30); // Same scenario event, after the quiet window closed.
    CHECK(Near(Corps[0].base, 450)); // Native +30 replaces 30 of the earlier mod +50.
    Effect(7, 42, 40);
    CHECK(Near(Corps[0].base, 470)); // Native total +70 is preserved; no mod grant remains.
    CHECK(Near(WaveRecords[0].paid, 0));

    Setup();
    Effect(8, 42, 30); // Same-event native award well before the appearance.
    Advance(0, 10);
    Effect(10, 42, 0);
    SpecialAttribute(Group(2), 0);
    At<uint8_t>(Group(2), 0x191) = 0;
    Advance(0, 3);
    CHECK(Near(Corps[0].base, 430));
    CHECK(Near(Reinforcements[0].nativeCredit, 30));

    Setup();
    Effect(10, 42, 0);
    SpecialAttribute(Group(2), 0);
    At<uint8_t>(Group(2), 0x191) = 0;
    Advance(0, 3);
    Effect(8, 99, 30); // Another event is an ordinary later native award.
    CHECK(Near(Corps[0].base, 455));

    Setup();
    CHECK(Corps[0].Rally(At<float>(CorpsObject(0), 0x28), Config));
    Effect(10, 42, 0);
    SpecialAttribute(Group(2), 0);
    At<uint8_t>(Group(2), 0x191) = 0;
    Advance(0, 3);
    CHECK(Near(Corps[0].base, 425));
    Effect(6, 42, 540); // Direct native set while the meeting bonus is visible.
    CHECK(Near(Corps[0].base, 425) && Near(At<float>(CorpsObject(0), 0x28), 525));
    CHECK(Near(WaveRecords[0].paid, 10));

    Setup();
    Effect(8, 42, 10);
    Effect(10, 42, 0);
    SpecialAttribute(Group(2), 0);
    At<uint8_t>(Group(2), 0x191) = 0;
    Advance(0, 3);
    CHECK(Near(Reinforcements[0].nativeCredit, 10) && Near(Corps[0].base, 425)); // Window/event credit is not doubled.

    Setup();
    Effect(9, 42, 2);
    CHECK(GroupEvents[2].known);
    SpecialAttribute(Group(2), 0);
    SpecialAttribute(Group(3), 0);
    Advance(0, 3);
    Effect(8, 42, 30);
    CHECK(Near(Corps[0].base, 450)); // Belong-group appearance and child association.

    Setup();
    Effect(10, 42, 0);
    SpecialAttribute(Group(2), 0);
    At<uint8_t>(Group(2), 0x191) = 0;
    Advance(0, 3);
    SpecialAttribute(Group(3), 0);
    Advance(0, 3);
    CHECK(Near(Corps[0].base, 445)); // Two waves from the same scenario paragraph.
    Effect(8, 42, 30);
    CHECK(Near(Corps[0].base, 445) && Near(WaveRecords[0].paid, 0) && Near(WaveRecords[1].paid, 15));
    Effect(8, 42, 30);
    CHECK(Near(Corps[0].base, 460) && Near(WaveRecords[1].paid, 0));

    Setup();
    Effect(10, 42, 0);
    Effect(10, 43, 0, 3);
    SpecialAttribute(Group(2), 0);
    SpecialAttribute(Group(3), 0);
    At<uint8_t>(Group(2), 0x191) = At<uint8_t>(Group(3), 0x191) = 0;
    Advance(0, 3);
    CHECK(Near(Corps[0].base, 450));
    Effect(8, 43, 30);
    Effect(8, 42, 30);
    CHECK(Near(Corps[0].base, 460)); // Both event keys share one wave's paid budget.

    Setup();
    At<float>(CorpsObject(0), 0x28) = 890;
    SpecialAttribute(Group(2), 0);
    Advance(0, 3);
    CHECK(Near(Corps[0].base, 899));

    Setup();
    Config.reinforcements = false;
    SpecialAttribute(Group(2), 0);
    Advance(0, 3);
    CHECK(!Reinforcements[0].open && Near(Corps[0].base, 400));

    Setup();
    SpecialAttribute(Group(2), 0);
    At<int>(ManagerMemory, 0x80) = 7; // Storage refresh must preserve the pending wave.
    CHECK(RefreshBattle() && Reinforcements[0].open);
    Advance(0, 3);
    CHECK(Near(Corps[0].base, 425));
    SpecialAttribute(Group(6), 0);
    Advance(0, 3);
    CHECK(Near(Corps[0].base, 465)); // Second wave: 10/25 * 100 = 40.
    ResetBattle(ManagerMemory);
    CHECK(!Reinforcements[0].open && Arrivals[2].seen && Arrivals[6].seen);

    SetupSurprise();
    CHECK(!Arrivals[2].seen && !Arrivals[3].seen);
    GroupTick(Group(2), 1); // Reserved surprise groups stay eligible while hidden.
    CHECK(!Arrivals[2].seen && !Reinforcements[0].open);
    Effect(8, 42, 100);
    Effect(10, 42, 0);
    RecursiveSpecial = true;
    SpecialAttribute(Group(2), 0); // Native recursion activates the accompanying groups.
    CHECK(Arrivals[2].pending && Arrivals[3].pending && WaveCount == 1);
    CHECK(Arrivals[2].surprise && Arrivals[3].surprise);
    CorpsTick(CorpsObject(0), 0);
    CHECK(Near(Corps[0].base, 500)); // Paused/unconfirmed arrivals do not grant morale.
    Advance(0, 1);
    CHECK(Reinforcements[0].open && Near(Corps[0].base, 500));
    At<uint8_t>(Group(2), 0x191) = 0;
    CorpsTick(CorpsObject(0), 0.01f);
    CHECK(!Reinforcements[0].open && Near(Corps[0].base, MoraleMaximum));
    CHECK(Near(Corps[0].nativeGains, 100) && Near(WaveRecords[0].paid, 399));
    CHECK(Near(At<float>(CorpsObject(1), 0x28), 400));
    Effect(8, 42, 100); // Scripted gains after arrival already meet the ceiling.
    CHECK(Near(Corps[0].base, MoraleMaximum));
    Lower(CorpsObject(0), 100, 0);
    CHECK(Near(Corps[0].base, 799));
    Effect(8, 42, 30); // Delayed same-event gain replaces the top-up after combat losses.
    CHECK(Near(Corps[0].base, 799) && Near(WaveRecords[0].paid, 369));
    Effect(8, 99, 30); // A genuinely different event still grants its native reward.
    CHECK(Near(Corps[0].base, 829));
    Effect(10, 42, 0);
    At<uint8_t>(Group(2), 0x2DFD) = 1;
    SpecialAttribute(Group(2), 0);
    At<uint8_t>(Group(2), 0x191) = 0;
    Advance(0, 4);
    CHECK(WaveCount == 1 && Near(Corps[0].base, 829)); // Never regrant the same group.

    SetupSurprise();
    CHECK(Corps[0].Rally(At<float>(CorpsObject(0), 0x28), Config));
    Effect(6, 42, 600); // A direct script set before activation is included.
    SpecialAttribute(Group(2), 0);
    CorpsTick(CorpsObject(0), 0.01f);
    CHECK(Near(Corps[0].base, MoraleMaximum) && Near(Corps[0].VisibleBonus(), 0));
    CHECK(Near(Corps[0].remaining, 179.99f));
    CorpsTick(CorpsObject(0), 180);
    CHECK(Near(Corps[0].base, MoraleMaximum) && Near(At<float>(CorpsObject(0), 0x28), MoraleMaximum));
    Effect(6, 42, 700); // Later scripted reductions still apply; no permanent lock.
    CHECK(Near(Corps[0].base, 700));

    SetupSurprise();
    Effect(8, 42, 499); // Native script already maximized the corps.
    SpecialAttribute(Group(2), 0);
    CorpsTick(CorpsObject(0), 0.01f);
    CHECK(Near(Corps[0].base, MoraleMaximum) && Near(Corps[0].nativeGains, 499));
    CHECK(Near(WaveRecords[0].paid, 0));

    SetupSurprise();
    Config.reinforcements = false;
    At<int>(CorpsObject(0), 0xC) = 1;
    for (int id = 0; id < GroupCount(); ++id)
        if (At<int>(Group(id), 0x38) == 0)
            At<int>(Group(id), 0x40) = 1;
    SpecialAttribute(Group(2), 0);
    CorpsTick(CorpsObject(0), 0.01f);
    CHECK(Near(Corps[0].base, MoraleMaximum)); // Enemy side; independent feature toggle.

    SetupSurprise();
    Config.surpriseForces = false;
    SpecialAttribute(Group(2), 0);
    Advance(0, 4);
    CHECK(!Reinforcements[0].open && Near(Corps[0].base, 400));
    At<uint8_t>(Group(3), 0x2DFD) = 2;
    SpecialAttribute(Group(3), 0);
    Advance(0, 3);
    CHECK(Near(Corps[0].base, 420)); // Ordinary reinforcement behavior is unchanged.

    Setup();
    At<uint8_t>(Group(5), 0x2DFD) = 1;
    At<uint8_t>(Group(5), 0x2D61) = 1; // An officer arriving without soldiers.
    ResetBattle(ManagerMemory);
    SpecialAttribute(Group(5), 0);
    CorpsTick(CorpsObject(1), 0.01f);
    CHECK(Near(Corps[1].base, MoraleMaximum) && Reinforcements[1].beforeSoldiers == 0);

    SetupSurprise();
    SpecialAttribute(Group(2), 0);
    At<int>(Group(2), 0x38) = 1;
    CorpsTick(CorpsObject(0), 0.01f);
    CHECK(Near(Corps[0].base, 400) && Near(At<float>(CorpsObject(1), 0x28), 400));
    CHECK(!Arrivals[2].pending);

    SetupSurprise();
    SpecialAttribute(Group(2), 0);
    At<int>(Group(2), 0x40) = 1;
    CorpsTick(CorpsObject(0), 0.01f);
    CHECK(Near(Corps[0].base, 400) && !Arrivals[2].pending);

    SetupSurprise();
    SpecialAttribute(Group(2), 0);
    At<uint8_t>(Group(2), 0x110) = 1;
    CorpsTick(CorpsObject(0), 0.01f);
    CHECK(Near(Corps[0].base, 400));

    SetupSurprise();
    SpecialAttribute(Group(2), 0);
    At<uint8_t>(Group(2), 0x191) = 1;
    Advance(0, 30);
    CHECK(!Reinforcements[0].open && !Arrivals[2].pending && Near(Corps[0].base, 400));

    SetupSurprise();
    SpecialAttribute(Group(2), 0);
    SpecialAttribute(Group(5), 0); // Concurrent ordinary arrival in another corps.
    Advance(0, 3);
    CHECK(Near(Corps[0].base, MoraleMaximum) && Near(At<float>(CorpsObject(1), 0x28), 400));
    At<uint8_t>(GroupMemory + 6 * 0x2E68, 0x2DFD) = 1;
    Lower(CorpsObject(0), 100, 0);
    At<int>(ManagerMemory, 0x80) = 7;
    CHECK(RefreshBattle());
    SpecialAttribute(Group(6), 0); // A genuinely distinct later surprise group.
    CorpsTick(CorpsObject(0), 0.01f);
    CHECK(Near(Corps[0].base, MoraleMaximum));
    ResetBattle(ManagerMemory);
    CHECK(Arrivals[2].seen && Arrivals[6].seen && !Arrivals[3].seen);

    SetupSurprise();
    At<uint8_t>(Group(3), 0x2DFD) = 2;
    ResetBattle(ManagerMemory);
    Effect(10, 42, 0, 3);
    SpecialAttribute(Group(3), 0);
    SpecialAttribute(Group(2), 0); // A mixed wave uses one maximum, not two rewards.
    At<uint8_t>(Group(3), 0x191) = 0;
    CorpsTick(CorpsObject(0), 0.01f);
    CHECK(Near(Corps[0].base, MoraleMaximum) && WaveCount == 1);
    CHECK(Near(WaveRecords[0].paid, 499));
    Effect(8, 42, 30);
    CHECK(Near(Corps[0].base, MoraleMaximum));
    Lower(CorpsObject(0), 100, 0);
    Effect(8, 42, 30);
    CHECK(Near(Corps[0].base, 799) && Near(WaveRecords[0].paid, 469));

    SetupSurprise();
    Config.reinforcements = false;
    At<int>(CorpsObject(0), 0xC) = 1;
    for (int id = 0; id < GroupCount(); ++id)
        if (At<int>(Group(id), 0x38) == 0)
            At<int>(Group(id), 0x40) = 1;
    At<int>(Group(2), 0x44) = 106;
    At<int>(Group(3), 0x2D1C) = 106;
    Effect(8, 5, 100);
    Advance(0, 10); // Early scripted reward outlasts the quiet window.
    Effect(9, 5, 1, 106, 1); // Hefei's actual side/selector/special-attribute arguments.
    CHECK(GroupEvents[2].known && GroupEvents[3].known && GroupEvents[2].cause == 5);
    SpecialAttribute(Group(2), 0);
    SpecialAttribute(Group(3), 0);
    CorpsTick(CorpsObject(0), 0.01f);
    CHECK(Near(Corps[0].base, MoraleMaximum) && Near(WaveRecords[0].paid, 399));
    Lower(CorpsObject(0), 100, 0);
    Advance(0, 10);
    Effect(8, 5, 50); // Late same-paragraph award must not restore the loss again.
    CHECK(Near(Corps[0].base, 799) && Near(WaveRecords[0].paid, 349));
    Effect(6, 5, 849); // A direct MoraleSet also replaces the earlier mod contribution.
    CHECK(Near(Corps[0].base, 799) && Near(WaveRecords[0].paid, 299));
    Effect(8, 99, 20);
    CHECK(Near(Corps[0].base, 819));

    printf("PASS: %d assertions against native reinforcement callbacks and formation memory.\n", Assertions);
    return 0;
}
