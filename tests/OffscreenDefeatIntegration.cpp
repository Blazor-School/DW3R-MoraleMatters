// Production extra-damage/reward callbacks with the actual supported executable's
// IsDead, group elimination and save/load instructions in an isolated process.
#include "../src/Game.cpp"
#include "../src/ExpectedBytes.h"
#include <stdio.h>
#include <stdlib.h>

using namespace MoraleMatters;
static int Assertions = 0;
#define CHECK(expression) do { ++Assertions; if (!(expression)) { fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #expression); return 1; } } while (0)
static bool Near(float a, float b) { return fabsf(a - b) < 0.002f; }
alignas(16) static uint8_t Formation[0x100], Groups[4 * 0x2E68], CorpsMemory[2 * 0xEF8];
static uint8_t DeadMembers[4][2];
static void* Members[4][2];
static int Children[3] = {2, 2, -1};
static float Damage, NativeGain, DefeatEntryHP;
static int DefeatCalls, LeaderCalls, MemberCalls;
static bool NativeDefeatedTarget, MismatchAttribution;
static GroupDefeatFn ActualDefeat;
namespace MoraleMatters { void Log(const char*, ...) {} }
static void FakeRaise(void* corps, float amount, int) { At<float>(corps, 0x28) = Clamp(At<float>(corps, 0x28) + amount, 1, 899); }
static void FakeLower(void* corps, float amount, int cause) { FakeRaise(corps, -amount, cause); }
static void FakeKill(void* group, uint8_t leader, int member, int)
{
    if (leader ? At<uint8_t>(group, 0x110) != 0 : MemberDead(group, member))
        return;
    if (leader)
    {
        ++LeaderCalls;
        At<uint8_t>(group, 0x110) = 1;
        At<float>(group, 0x2DEC) = 0;
    }
    else
    {
        ++MemberCalls;
        DeadMembers[At<int>(group, 0x3C)][member] = 1;
    }
    --At<uint8_t>(group, 0x2D61);
    Lower(CorpsObject(At<int>(group, 0x38)), 4, -1);
    if (NativeGain > 0 && leader)
        Raise(CorpsObject(1), NativeGain, -1);
}
static void TrackDefeat(void* group, int flags)
{
    ++DefeatCalls;
    DefeatEntryHP = At<float>(group, 0x2DEC);
    ActualDefeat(group, flags);
}
static void FakeAttack(void* attacker)
{
    void* target = Group(At<int>(attacker, 0x180));
    At<float>(target, 0x2DF4) -= Damage;
    At<int>(target, 0x1D0) = MismatchAttribution ? -1 : At<int>(attacker, 0x3C);
    if (NativeDefeatedTarget)
        At<uint8_t>(target, 0x110) = 1;
}
static void Setup(float hp = 10)
{
    Config = {};
    Behavior = {};
    Behavior.enabled = false;
    KillRewards = {};
    KillRewards.cap = 50;
    memset(Formation, 0, sizeof(Formation));
    memset(Groups, 0, sizeof(Groups));
    memset(CorpsMemory, 0, sizeof(CorpsMemory));
    memset(DeadMembers, 0, sizeof(DeadMembers));
    At<void*>(Formation, 0x68) = CorpsMemory;
    At<int>(Formation, 0x70) = 2;
    At<void*>(Formation, 0x78) = Groups;
    At<int>(Formation, 0x80) = 4;
    for (int id = 0; id < 2; ++id)
    {
        void* corps = CorpsMemory + id * 0xEF8;
        At<void*>(corps, 0) = Formation;
        At<int>(corps, 0xC) = id;
        At<int>(corps, 0x14) = id == 0 ? 3 : 0;
        At<float>(corps, 0x28) = 400;
    }
    for (int id = 0; id < 4; ++id)
    {
        void* group = Groups + id * 0x2E68;
        At<void*>(group, 0x28) = Formation;
        At<int>(group, 0x38) = id == 0 ? 1 : 0;
        At<int>(group, 0x3C) = id;
        At<int>(group, 0x40) = id == 0 ? 1 : 0;
        At<int>(group, 0x48) = id;
        At<int>(group, 0x1D0) = id == 0 ? 1 : 0;
        At<uint8_t>(group, 0x1E8) = 2;
        At<uint8_t>(group, 0x2D20) = 3;
        At<uint8_t>(group, 0x2D21) = 1;
        At<float>(group, 0x2DEC) = hp;
        At<float>(group, 0x2DF0) = At<float>(group, 0x2DF4) = 1000;
        At<int>(group, 0x2E10) = 100;
        At<uint8_t>(group, 0x2D61) = 3;
        At<int>(group, 0x70) = At<int>(group, 0x120) = 2;
        At<void*>(group, 0x118) = DeadMembers[id];
        Members[id][0] = Members[id][1] = group;
        At<void*>(group, 0x68) = Members[id];
    }
    At<int>(Groups, 0x180) = 1;
    OriginalAttack = FakeAttack;
    OriginalRaise = FakeRaise;
    OriginalLower = FakeLower;
    OriginalKill = FakeKill;
    NativeGroupDefeat = TrackDefeat;
    Damage = 40;
    NativeGain = DefeatEntryHP = 0;
    DefeatCalls = LeaderCalls = MemberCalls = 0;
    NativeDefeatedTarget = MismatchAttribution = false;
    ResetBattle(Formation);
}
static void Jump(uintptr_t rva, void* target)
{
    Image[rva] = 0x48; Image[rva + 1] = 0xB8;
    memcpy(Image + rva + 2, &target, 8);
    Image[rva + 10] = 0xFF; Image[rva + 11] = 0xE0;
}
static void Noop() {}
static void* EmptyDebug(void*) { static void* empty = nullptr; return &empty; }
static void* CopyPosition(void*, void* output, void* input) { memcpy(output, input, 24); return output; }

int main(int argc, char** argv)
{
    CHECK(argc == 2);
    FILE* file = fopen(argv[1], "rb");
    CHECK(file != nullptr);
    fseek(file, 0, SEEK_END);
    const long length = ftell(file);
    CHECK(length > 0x1000);
    rewind(file);
    auto* bytes = static_cast<uint8_t*>(malloc(static_cast<size_t>(length)));
    CHECK(bytes && fread(bytes, 1, length, file) == static_cast<size_t>(length));
    fclose(file);
    const auto* dos = reinterpret_cast<IMAGE_DOS_HEADER*>(bytes);
    CHECK(dos->e_magic == IMAGE_DOS_SIGNATURE && dos->e_lfanew > 0 && dos->e_lfanew < length - 0x100);
    const auto* nt = reinterpret_cast<IMAGE_NT_HEADERS64*>(bytes + dos->e_lfanew);
    CHECK(nt->Signature == IMAGE_NT_SIGNATURE && nt->FileHeader.Machine == IMAGE_FILE_MACHINE_AMD64 && nt->OptionalHeader.SizeOfImage == 0xB39E000);
    Image = static_cast<uint8_t*>(VirtualAlloc(nullptr, nt->OptionalHeader.SizeOfImage, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE));
    CHECK(Image != nullptr);
    const auto* sections = IMAGE_FIRST_SECTION(nt);
    for (int i = 0; i < nt->FileHeader.NumberOfSections; ++i)
    {
        CHECK(static_cast<uint64_t>(sections[i].PointerToRawData) + sections[i].SizeOfRawData <= static_cast<uint64_t>(length));
        CHECK(static_cast<uint64_t>(sections[i].VirtualAddress) + sections[i].SizeOfRawData <= nt->OptionalHeader.SizeOfImage);
        memcpy(Image + sections[i].VirtualAddress, bytes + sections[i].PointerToRawData, sections[i].SizeOfRawData);
    }
    free(bytes);
    bool guarded = false;
    for (const auto& expected : ExpectedHooks)
        if (expected.rva == 0x13B11E0)
        {
            CHECK(memcmp(Image + expected.rva, expected.bytes, sizeof(expected.bytes)) == 0);
            guarded = true;
        }
    CHECK(guarded);
    ActualDefeat = reinterpret_cast<GroupDefeatFn>(Image + 0x13B11E0);
    Jump(0x13AEF80, reinterpret_cast<void*>(&Kill)); // Real native elimination enters production casualty/reward wrappers.
    // Save/load use real HP and flag serialization; unrelated helpers are substituted.
    // The writer relies on R8/RCX surviving its initialization helper.
    Image[0x1283830] = 0xC3;
    Jump(0x13B5490, reinterpret_cast<void*>(&CopyPosition));
    Jump(0x13B2C10, reinterpret_cast<void*>(&Noop));
    Jump(0x13B2D00, reinterpret_cast<void*>(&Noop));
    Jump(0x1A363B0, reinterpret_cast<void*>(&EmptyDebug));
    FlushInstructionCache(GetCurrentProcess(), Image, nt->OptionalHeader.SizeOfImage);

    Setup();
    Damage = 20;
    Attack(Group(0));
    CHECK(Near(At<float>(Group(1), 0x2DEC), 5) && Near(At<float>(Group(1), 0x2DF4), 980));
    CHECK(DefeatCalls == 0 && LeaderCalls == 0 && Near(At<float>(CorpsObject(1), 0x28), 400));

    for (float damage : {40.0f, 400.0f})
    {
        Setup();
        Damage = damage;
        Attack(Group(0));
        CHECK(DefeatCalls == 1 && LeaderCalls == 1 && MemberCalls == 2 && Near(DefeatEntryHP, 10));
        CHECK(At<uint8_t>(Group(1), 0x110) && At<uint8_t>(Group(1), 0x186) && At<uint8_t>(Group(1), 0x1D4));
        CHECK(Near(At<float>(Group(1), 0x2DEC), 0) && At<uint8_t>(Group(1), 0x2D61) == 0);
        CHECK(Near(At<float>(CorpsObject(0), 0x28), 348) && Near(At<float>(CorpsObject(1), 0x28), 410.5f));
        CHECK(Near(KillRewardBudgets[1].spent, 10.5f) && CurrentOffscreenReward == nullptr && At<int>(Group(1), 0x1D0) == -1);
        Attack(Group(0));
        CHECK(DefeatCalls == 1 && LeaderCalls == 1 && Near(KillRewardBudgets[1].spent, 10.5f));
    }

    Setup();
    NativeGain = 20;
    Attack(Group(0));
    CHECK(Near(At<float>(CorpsObject(1), 0x28), 420) && Near(KillRewardBudgets[1].spent, 0));
    Setup();
    NativeGain = 6;
    Attack(Group(0));
    CHECK(Near(At<float>(CorpsObject(1), 0x28), 410.5f) && Near(KillRewardBudgets[1].spent, 4.5f));
    Setup();
    KillRewards.enabled = false;
    Attack(Group(0));
    CHECK(LeaderCalls == 1 && Near(At<float>(CorpsObject(1), 0x28), 400));
    Setup();
    Config.casualties = false;
    Attack(Group(0));
    CHECK(LeaderCalls == 1 && Near(At<float>(CorpsObject(0), 0x28), 388));
    Setup();
    KillRewards.cap = 2;
    Attack(Group(0));
    CHECK(LeaderCalls == 1 && Near(KillRewardBudgets[1].spent, 2));

    Setup();
    At<uint8_t>(Group(1), 0x2E3A) = 1;
    Attack(Group(0));
    CHECK(Near(At<float>(Group(1), 0x2DEC), 1) && DefeatCalls == 0);
    Setup();
    At<int>(Group(0), 0x38) = At<int>(Group(0), 0x40) = 0;
    At<int>(Group(1), 0x38) = At<int>(Group(1), 0x40) = 1;
    At<uint8_t>(Group(1), 0x2D20) = 2;
    Attack(Group(0));
    CHECK(Near(At<float>(Group(1), 0x2DEC), 1) && DefeatCalls == 0);

    for (int exclusion = 0; exclusion < 15; ++exclusion)
    {
        Setup();
        if (exclusion == 0) At<uint8_t>(Group(1), 0x187) = 1;
        if (exclusion == 1) At<uint8_t>(Group(1), 0x191) = 1;
        if (exclusion == 2) At<uint8_t>(Group(1), 0x192) = 1;
        if (exclusion == 3) At<uint8_t>(Group(1), 0x198) = 1;
        if (exclusion == 4) At<uint8_t>(Group(1), 0x199) = 1;
        if (exclusion == 5) At<uint8_t>(Group(1), 0x186) = 1;
        if (exclusion == 6) At<uint8_t>(Group(1), 0x110) = 1;
        if (exclusion == 7) At<uint8_t>(Group(1), 0x2DFD) = 1;
        if (exclusion == 8) At<uint8_t>(Group(1), 0x1E8) = 0;
        if (exclusion == 9) At<uint8_t>(Group(1), 0x2D61) = 1;
        if (exclusion == 10) At<uint8_t>(Group(1), 0x2D20) = 6;
        if (exclusion == 11) Config.offscreen = false;
        if (exclusion == 12) Config.officerDamageShare = 0;
        if (exclusion == 13) Damage = 0;
        if (exclusion == 14) NativeDefeatedTarget = true;
        Attack(Group(0));
        CHECK(DefeatCalls == 0 && Near(At<float>(Group(1), 0x2DEC), 10));
    }
    Setup();
    MismatchAttribution = true;
    Attack(Group(0));
    CHECK(DefeatCalls == 0 && Near(At<float>(Group(1), 0x2DEC), 1));
    Setup();
    NativeGroupDefeat = nullptr;
    Attack(Group(0));
    CHECK(DefeatCalls == 0 && Near(At<float>(Group(1), 0x2DEC), 1));
    Setup();
    At<int>(Group(1), 0x58) = 1;
    At<void*>(Group(1), 0x50) = nullptr;
    Attack(Group(0));
    CHECK(DefeatCalls == 0 && Near(At<float>(Group(1), 0x2DEC), 1));
    Setup();
    At<int>(Group(1), 0x58) = 3;
    At<void*>(Group(1), 0x50) = Children;
    At<int>(Group(2), 0x48) = 1;
    Attack(Group(0));
    CHECK(DefeatCalls == 2 && LeaderCalls == 2 && MemberCalls == 4); // Duplicate/invalid child IDs cannot repeat a death.
    CHECK(!At<uint8_t>(Group(3), 0x110) && At<uint8_t>(Group(2), 0x110));
    CHECK(Near(KillRewardBudgets[1].spent, 21));

    // The original bug's state survives the real version-16 serializer. Fixed
    // lethal hits reach zero only after the actual native elimination callbacks.
    using SaveFn = void* (*)(void*, void*, void*);
    using LoadFn = void (*)(void*, void*, void*);
    uint8_t record[0xD8] = {};
    alignas(16) uint8_t restored[0x2E68] = {};
    Setup();
    Attack(Group(0));
    CHECK(At<uint8_t>(Group(1), 0x110) && LeaderCalls == 1 && MemberCalls == 2);
    reinterpret_cast<SaveFn>(Image + 0x13C8DB0)(Formation, record, Group(1));
    CHECK(At<int>(record, 0) == 16 && Near(At<float>(record, 0x4C), 0));
    At<uint8_t>(restored, 0x1E8) = 2;
    At<int>(restored, 0x48) = -1;
    reinterpret_cast<LoadFn>(Image + 0x13C90D0)(Formation, restored, record);
    CHECK(Near(At<float>(restored, 0x2DEC), 0) && !GroupAlive(restored));
    Setup();
    Damage = 20;
    Attack(Group(0));
    reinterpret_cast<SaveFn>(Image + 0x13C8DB0)(Formation, record, Group(1));
    memset(restored, 0, sizeof(restored));
    At<uint8_t>(restored, 0x1E8) = 2;
    At<int>(restored, 0x48) = -1;
    reinterpret_cast<LoadFn>(Image + 0x13C90D0)(Formation, restored, record);
    CHECK(Near(At<float>(restored, 0x2DEC), 5) && GroupAlive(restored) && LeaderCalls == 0);
    printf("PASS: %d assertions against production off-screen damage and native defeat/save/load instructions.\n", Assertions);
    return 0;
}
