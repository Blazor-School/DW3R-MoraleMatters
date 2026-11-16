// Production guard callbacks and the real supported executable guard branch,
// mapped into this isolated process. No running game or save is accessed.
#include "../src/Game.cpp"
#include "GameTestAddresses.h"
#include "../src/ExpectedBytes.h"
#include <stdio.h>
#include <stdlib.h>

using namespace MoraleMatters;
static uint32_t NativeGroupStride;
static int Assertions = 0, TickCalls = 0, ObservedMode = -1;
#define CHECK(expression) do { ++Assertions; if (!(expression)) { fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #expression); return 1; } } while (0)
static bool Near(float a, float b) { return fabsf(a - b) < 0.002f; }
alignas(16) static uint8_t Formation[0x100], Groups[4 * 0x2E70], CorpsMemory[2 * 0xEF8];
alignas(16) static uint8_t Characters[12][0xB10], Positions[12][0x220], Controllers[12][0x510], Blackboard[0x100], CharacterManager[0x100], Component[0x160], AttackData[0x80];
static void* Entries[12];
static bool Immune = false, Stopped = false, Charging = false, Reassign = false, Attacking = false, Heavy = false;
namespace MoraleMatters { void Log(const char*, ...) {} }
static bool Dead(void* group) { return At<uint8_t>(group, 0x110) || At<uint8_t>(group, 0x186); }
static bool Tag(void*, int tag) { return (tag == 0xCF && Charging) || (tag == 0xCE && Attacking) || (tag == 0xEA && Heavy); }
static bool Ability(void*, uint8_t ability) { return ability == 0x16 && Immune; }
static void* Owner(void* weak) { return At<void*>(weak, 0); }
static bool Stop(void*, void*, int) { return Stopped; }
static void Status(void* controller, uint8_t status) { At<uint8_t>(controller, 0x3EC) = status; }
static void Tick(void* controller, float)
{
    ++TickCalls;
    void* character = Owner(static_cast<uint8_t*>(controller) + 0x3DC);
    void* group = Group(At<int>(character, 0x7A0));
    ObservedMode = At<int>(group, 0x178);
    if (Reassign) At<int>(group, 0x178) = 3;
    At<uint8_t>(controller, 0x3EC) = 5;
}
static void GroupTickStub(void*, float) {}
static void Jump(uintptr_t rva, void* target)
{
    Image[rva] = 0x48; Image[rva + 1] = 0xB8;
    memcpy(Image + rva + 2, &target, 8);
    Image[rva + 10] = 0xFF; Image[rva + 11] = 0xE0;
}

extern "C" { void* GuardProbeEntry = nullptr; }
extern "C" int ProbeNativeGuard(void*, void*, uint32_t, float);
extern "C" void GuardProbeBlocked();
extern "C" void GuardProbeBroken();
extern "C" void GuardProbeOpen();
asm(
    ".text\n"
    "ProbeNativeGuard:\n"
    "pushq %rbx\n"
    "pushq %rdi\n"
    "pushq %r12\n"
    "pushq %r13\n"
    "pushq %r14\n"
    "pushq %r15\n"
    "subq $0xc8, %rsp\n"
    "movaps %xmm11, 0x90(%rsp)\n"
    "movq %rcx, %r15\n"
    "movq %rdx, %rbx\n"
    "movq %rdx, %r14\n"
    "movzbl %r8b, %r10d\n"
    "movl %r8d, %edi\n"
    "shrl $8, %edi\n"
    "movl %r8d, %eax\n"
    "shrl $16, %eax\n"
    "movb %al, 0x40(%rsp)\n"
    "movl %r8d, %r13d\n"
    "shrl $24, %r13d\n"
    "movaps %xmm3, %xmm11\n"
    "jmp *GuardProbeEntry(%rip)\n"
    "GuardProbeBlocked:\n"
    "movb $1, 0x141(%r15)\n"
    "xorl %eax, %eax\n"
    "jmp GuardProbeReturn\n"
    "GuardProbeBroken:\n"
    "movl $1, %eax\n"
    "cmpw $0x101, 0x141(%r15)\n"
    "jne GuardProbeInvalid\n"
    "cmpw $0x31, %r12w\n"
    "je GuardProbeReturn\n"
    "GuardProbeInvalid:\n"
    "movl $-1, %eax\n"
    "jmp GuardProbeReturn\n"
    "GuardProbeOpen:\n"
    "movl $2, %eax\n"
    "GuardProbeReturn:\n"
    "movaps 0x90(%rsp), %xmm11\n"
    "addq $0xc8, %rsp\n"
    "popq %r15\n"
    "popq %r14\n"
    "popq %r13\n"
    "popq %r12\n"
    "popq %rdi\n"
    "popq %rbx\n"
    "ret\n"
);

static int Hit(uint64_t flags = 0, uint32_t context = 1, float damage = 10)
{
    At<uint64_t>(AttackData, 8) = flags;
    At<uint16_t>(Component, 0x141) = 0;
    return ProbeNativeGuard(Component, Characters[0], context, damage);
}

static void Setup()
{
    Config = {};
    Behavior = {};
    GuardPressure = {};
    GuardPressure.disruption = false;
    Immune = Stopped = Charging = Reassign = Attacking = Heavy = false;
    memset(Formation, 0, sizeof(Formation));
    memset(Groups, 0, sizeof(Groups));
    memset(CorpsMemory, 0, sizeof(CorpsMemory));
    memset(Characters, 0, sizeof(Characters));
    memset(Controllers, 0, sizeof(Controllers));
    memset(Positions, 0, sizeof(Positions));
    memset(Component, 0, sizeof(Component));
    memset(AttackData, 0, sizeof(AttackData));
    memset(EliteUnits, 0, sizeof(EliteUnits));
    At<void*>(Formation, 0x68) = CorpsMemory;
    At<int>(Formation, 0x70) = 2;
    At<void*>(Formation, 0x78) = Groups;
    At<int>(Formation, 0x80) = 4;
    for (int id = 0; id < 2; ++id)
    {
        void* corps = CorpsMemory + id * 0xEF8;
        At<void*>(corps, 0) = Formation;
        At<int>(corps, 0xC) = id;
        At<float>(corps, 0x28) = 400;
        At<int>(corps, 0x14) = id ? 2 : 0;
    }
    for (int id = 0; id < 4; ++id)
    {
        void* group = Groups + id * NativeGroupStride;
        At<void*>(group, 0x28) = Formation;
        At<int>(group, 0x3C) = id;
        At<int>(group, 0x38) = id ? 1 : 0;
        At<int>(group, 0x40) = id ? 1 : 0;
        At<int>(group, 0x178) = 2;
        At<uint8_t>(group, 0x1E8) = 2;
        At<uint8_t>(group, 0x2D20) = id == 2 ? 3 : 6;
        At<uint8_t>(group, 0x2D21) = id == 3 ? 2 : 1;
        At<uint8_t>(group, 0x2DFC) = 1;
        At<uint16_t>(group, 0x2D0E) = 7;
        At<float>(group, 0x2DEC) = 100;
    }
    for (int id = 0; id < 12; ++id)
    {
        void* character = Characters[id];
        Entries[id] = character;
        At<int>(character, 0xC) = id + 100;
        At<void*>(character, 0x1C0) = Positions[id];
        At<void*>(character, 0x2E0) = Controllers[id];
        At<uint8_t>(character, 0x74C) = id ? 2 : 0;
        At<int>(character, 0x7A0) = id == 0 || id == 11 ? 0 : id == 10 ? 2 : id == 9 ? 3 : 1;
        At<int>(character, 0x7A4) = id == 10 ? -1 : id;
        At<double>(Positions[id], 0x200) = id > 1 ? (id - 1) * 100 : 0;
        At<void*>(Controllers[id], 0x3DC) = character;
        At<void*>(Controllers[id], 0x3A0) = Blackboard;
        At<uint8_t>(Controllers[id], 0x3D8) = 1;
        At<uint8_t>(Controllers[id], 0x3EC) = 5;
        At<int>(Controllers[id], 0x484) = 10;
    }
    At<double>(Positions[9], 0x200) = 10;
    At<double>(Positions[10], 0x200) = 20;
    At<double>(Positions[11], 0x200) = 30;
    At<void*>(CharacterManager, 0x50) = Entries;
    At<int>(CharacterManager, 0x58) = 12;
    At<void*>(Image, TestRva<0xA9C50F0>()) = CharacterManager;
    At<void*>(Component, 0xB8) = Characters[1];
    At<void*>(Component, 0xC8) = AttackData;
    BehaviorOwner = &Owner;
    BehaviorStopped = &Stop;
    SetBehaviorStatus = &Status;
    OriginalBehaviorTick = &Tick;
    OriginalGroupTick = &GroupTickStub;
    GuardTag = &Tag;
    GuardAbility = &Ability;
    ResetBattle(Formation);
    TickCalls = 0;
    ObservedMode = -1;
}

int main(int argc, char** argv)
{
    CHECK(argc == 2);
    FILE* file = fopen(argv[1], "rb");
    CHECK(file != nullptr);
    fseek(file, 0, SEEK_END);
    const long length = ftell(file);
    rewind(file);
    auto* bytes = static_cast<uint8_t*>(malloc(static_cast<size_t>(length)));
    CHECK(bytes && fread(bytes, 1, length, file) == static_cast<size_t>(length));
    fclose(file);
    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(bytes);
    CHECK(dos->e_magic == IMAGE_DOS_SIGNATURE);
    const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(bytes + dos->e_lfanew);
    CHECK(nt->Signature == IMAGE_NT_SIGNATURE && SelectTestGameBuild(nt->OptionalHeader.SizeOfImage));
    Image = static_cast<uint8_t*>(VirtualAlloc(nullptr, nt->OptionalHeader.SizeOfImage, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE));
    CHECK(Image);
    const auto* section = IMAGE_FIRST_SECTION(nt);
    for (int n = 0; n < nt->FileHeader.NumberOfSections; ++n)
        memcpy(Image + section[n].VirtualAddress, bytes + section[n].PointerToRawData, section[n].SizeOfRawData);
    free(bytes);
    NativeGroupStride = NativeTestGroupStride(Image);
    CHECK(NativeGroupStride == BattleGroupStride());
    for (const auto& expected : GameExpectedHooks())
        CHECK(memcmp(Image + GameRva(expected.rva), expected.bytes, sizeof(expected.bytes)) == 0);
    Jump(TestRva<0x13AE060>(), reinterpret_cast<void*>(&Dead));
    Jump(TestRva<0x1746710>(), reinterpret_cast<void*>(&Tag));
    Jump(TestRva<0x175AFC0>(), reinterpret_cast<void*>(&Ability));
    Jump(TestRva<0x1397E81>(), reinterpret_cast<void*>(&GuardProbeBlocked));
    // Adjacent native branch destinations need short relative jumps, not
    // overlapping 12-byte absolute stubs.
    Image[TestRva<0x1398043>()] = 0xE9;
    *reinterpret_cast<int32_t*>(Image + TestRva<0x1398044>()) = TestRva<0x1399600>() - TestRva<0x1398048>();
    Jump(TestRva<0x1399600>(), reinterpret_cast<void*>(&GuardProbeBroken));
    Jump(TestRva<0x1398049>(), reinterpret_cast<void*>(&GuardProbeOpen));
    CHECK(MH_Initialize() == MH_OK);
    CHECK(MH_CreateHook(Image + TestRva<0x1397DF6>(), reinterpret_cast<void*>(&GuardDecisionEntry), &OriginalGuardDecision) == MH_OK);
    CHECK(MH_EnableHook(MH_ALL_HOOKS) == MH_OK);
    GuardProbeEntry = Image + TestRva<0x1397DF6>();

    Setup();
    CHECK(Hit() == 0);
    CHECK(CollectGuardFeedback() == 1);
    GuardFeedback feedback;
    CHECK(ReadGuardFeedback(0, feedback));
    CHECK(feedback.token > 0 && feedback.group == 1 && feedback.member == 1 && feedback.objectIndex == At<int>(Characters[1], 0xC));
    CHECK(Near(feedback.pressure, 1) && Near(feedback.threshold, 6) && feedback.hitFlash > 0 && feedback.breakFlash == 0);
    const uint64_t feedbackToken = feedback.token;
    const uint64_t hitSequence = feedback.hitSequence;
    CHECK(hitSequence > 0);
    CHECK(!ReadGuardFeedback(-1, feedback) && !ReadGuardFeedback(3, feedback));
    CHECK(Hit() == 0);
    CHECK(CollectGuardFeedback() == 1 && ReadGuardFeedback(0, feedback) && feedback.hitSequence > hitSequence);
    CHECK(Hit() == 0);
    CHECK(Hit(1) == 1); // Three setup hits and a charge reach six pressure.
    CHECK(At<uint16_t>(Component, 0x141) == 0x101);
    CHECK(CollectGuardFeedback() == 1 && ReadGuardFeedback(0, feedback));
    CHECK(feedback.token == feedbackToken && feedback.pressure == 0 && Near(feedback.breakFlash, 1.2f) && feedback.breakSequence > 0 && !feedback.refilled);
    auto* actor = GuardActorFor(Characters[1], false);
    CHECK(actor && Near(actor->state.opening, 0.9f) && actor->state.pressure == 0);
    CHECK(Hit() == 2); // Real native unguarded path during the earned opening.
    CHECK(At<uint16_t>(Component, 0x141) == 0);
    CHECK(!TickGuardPressure(Controllers[1], GuardPressure.openingSeconds));
    CHECK(CollectGuardFeedback() == 1 && ReadGuardFeedback(0, feedback));
    CHECK(feedback.token == feedbackToken && feedback.opening == 0 && feedback.pressure == 0 && feedback.refilled && feedback.breakFlash > 0);
    for (int n = 0; n < 20; ++n) CHECK(Hit(1) == 0);
    CHECK(!TickGuardPressure(Controllers[1], 3));
    CHECK(CollectGuardFeedback() == 0);
    CHECK(Hit(1) == 0);
    CHECK(Hit(1) == 1);

    Setup();
    GuardPressure.indicatorSeconds = 0.3f;
    CHECK(Hit(1) == 0 && Hit(1) == 1);
    CHECK(!TickGuardPressure(Controllers[1], 0.5f));
    CHECK(CollectGuardFeedback() == 1 && ReadGuardFeedback(0, feedback));
    CHECK(feedback.opening > 0 && feedback.breakFlash == 0 && !feedback.refilled);
    CHECK(!TickGuardPressure(Controllers[1], 0.5f));
    CHECK(CollectGuardFeedback() == 1 && ReadGuardFeedback(0, feedback));
    CHECK(feedback.opening == 0 && feedback.breakFlash == 0 && feedback.refilled);
    CHECK(!TickGuardPressure(Controllers[1], 0.5f));
    CHECK(CollectGuardFeedback() == 0);

    Setup();
    constexpr uint64_t musou = 4, trueMusou = 8, musouFinisher = 256, ignoreGuard = 0x80000000ULL;
    CHECK(Hit(musou, 0) == 2);
    CHECK(Hit(musou | ignoreGuard) == 0);
    CHECK(GuardActorFor(Characters[1], false) == nullptr);
    CHECK(Hit(musou) == 0 && Hit(trueMusou) == 0 && Hit(musouFinisher) == 0);
    CHECK(CollectGuardFeedback() == 1 && ReadGuardFeedback(0, feedback) && Near(feedback.pressure, 3));
    CHECK(Hit(1) == 1); // A charge can finish breaking a guard pressured by Musou.
    CHECK(Hit(musou) == 2);
    CHECK(!TickGuardPressure(Controllers[1], 4));
    CHECK(Hit(musou | 1) == 0 && Hit(musou | 1) == 1); // A charge-flagged Musou uses charge pressure.

    Setup();
    for (int n = 0; n < 5; ++n) CHECK(Hit(musou) == 0);
    CHECK(Hit(musouFinisher) == 1);

    Setup();
    EliteUnits[7] = true;
    CHECK(Hit(1) == 0);
    CHECK(Hit(1) == 0);
    CHECK(Hit(1) == 1);
    Setup();
    At<int>(Characters[1], 0x7A0) = 2;
    At<int>(Characters[1], 0x7A4) = -1;
    for (int n = 0; n < 3; ++n) CHECK(Hit(1) == 0);
    CHECK(Hit(1) == 1);

    Setup();
    CHECK(Hit(0, 1, 40) == 1); // A native damage-triggered break still works.
    CHECK(GuardActorFor(Characters[1], false)->state.opening > 0);
    Setup();
    GuardPressure.enabled = false;
    for (int n = 0; n < 8; ++n) CHECK(Hit(1) == 0);
    CHECK(GuardActorFor(Characters[1], false) == nullptr);
    CHECK(Hit(0, 1, 40) == 1);
    Setup();
    Attacking = true;
    for (int n = 0; n < 8; ++n) CHECK(Hit(1) == 0);
    CHECK(GuardActorFor(Characters[1], false) == nullptr);
    Attacking = false;
    Heavy = true;
    for (int n = 0; n < 8; ++n) CHECK(Hit(1) == 0);
    CHECK(GuardActorFor(Characters[1], false) == nullptr);
    Setup();
    Immune = true;
    for (int n = 0; n < 8; ++n) CHECK(Hit(1) == 0);
    CHECK(GuardActorFor(Characters[1], false)->state.pressure == 0);
    Setup();
    for (uint64_t flag = 4; flag <= 0x80000000ULL; flag <<= 1)
    {
        if (!(flag & (1024 | 524288 | 0x10000000 | 0x20000000 | 0x40000000 | 0x80000000ULL))) continue;
        for (int n = 0; n < 8; ++n) CHECK(Hit(flag) == 0);
        CHECK(GuardActorFor(Characters[1], false) == nullptr);
    }
    CHECK(Hit(0, 0) == 2); // An unguarded hit cannot accumulate pressure.
    CHECK(GuardActorFor(Characters[1], false) == nullptr);
    At<uint8_t>(Characters[0], 0x74C) = 2;
    for (int n = 0; n < 8; ++n) CHECK(Hit(1) == 0);
    CHECK(GuardActorFor(Characters[1], false) == nullptr);
    Setup();
    At<uint8_t>(Group(1), 0x2E3A) = 1;
    for (int n = 0; n < 8; ++n) CHECK(Hit(1) == 0);
    CHECK(GuardActorFor(Characters[1], false) == nullptr);
    Setup();
    CHECK(Hit() == 0);
    ++At<int>(Characters[1], 0xC);
    CHECK(GuardActorFor(Characters[1], false) == nullptr);
    CHECK(Hit() == 0);
    CHECK(Near(GuardActorFor(Characters[1], false)->state.pressure, 1));

    // Exact recovery-delay boundaries, frame partitioning, all morale values,
    // and shorter disruption at high morale without larger break thresholds.
    for (int morale = 1; morale <= 899; ++morale)
    {
        GuardPressureSettings settings;
        GuardPressureState state, partitioned;
        state.pressure = partitioned.pressure = 6;
        state.Advance(2, static_cast<float>(morale), settings);
        CHECK(Near(state.pressure, 6));
        state.Advance(0.5f, static_cast<float>(morale), settings);
        for (int n = 0; n < 25; ++n) partitioned.Advance(0.1f, static_cast<float>(morale), settings);
        CHECK(Near(state.pressure, partitioned.pressure));
        CHECK(state.Disrupt(static_cast<float>(morale), settings));
        CHECK(!state.Disrupt(static_cast<float>(morale), settings));
        CHECK(state.hesitation >= 0.52f && state.hesitation <= 1.2f);
    }
    GuardPressureState low, high;
    GuardPressureSettings modelSettings;
    low.pressure = high.pressure = 6;
    low.Advance(3, 100, modelSettings);
    high.Advance(3, 800, modelSettings);
    CHECK(low.pressure > high.pressure);
    low.Disrupt(100, modelSettings);
    high.Disrupt(800, modelSettings);
    CHECK(low.hesitation > high.hesitation);

    Setup();
    GuardPressure.disruption = true;
    GuardPressure.disruptionTargets = 3;
    CHECK(Hit(1) == 0);
    CHECK(Hit(1) == 1);
    for (int id = 2; id <= 4; ++id)
    {
        actor = GuardActorFor(Characters[id], false);
        CHECK(actor && Near(actor->state.hesitation, 0.8f));
    }
    CHECK(GuardActorFor(Characters[5], false) == nullptr); // Count limit.
    CHECK(GuardActorFor(Characters[9], false) == nullptr); // Archer.
    CHECK(GuardActorFor(Characters[10], false) == nullptr); // Named officer.
    CHECK(GuardActorFor(Characters[11], false) == nullptr); // Allied troop.
    CHECK(TickGuardPressure(Controllers[2], 0.1f));
    CHECK(TickCalls == 1 && ObservedMode == 0);
    CHECK(At<int>(Group(1), 0x178) == 2 && At<uint8_t>(Controllers[2], 0x3EC) == 0);
    Charging = true;
    CHECK(!TickGuardPressure(Controllers[3], 0.1f)); // Charge/counter abilities continue.
    Charging = false;
    Stopped = true;
    CHECK(!TickGuardPressure(Controllers[3], 0.1f));
    Stopped = false;
    Reassign = true;
    CHECK(TickGuardPressure(Controllers[3], 0.1f));
    CHECK(At<int>(Group(1), 0x178) == 3); // Native reassignment wins.
    At<int>(Group(1), 0x178) = 2;
    Reassign = false;
    CHECK(!TickGuardPressure(Controllers[2], 1));
    CHECK(GuardActorFor(Characters[2], false)->state.hesitation == 0);
    GuardDisruption(Characters[1], Group(1));
    CHECK(GuardActorFor(Characters[5], false) == nullptr); // Source cooldown blocks a new pulse.
    GroupTick(Group(1), 4.1f);
    CHECK(GuardPulseCooldown[1] == 0);
    ResetBattle(Formation);
    CHECK(GuardActorFor(Characters[1], false) == nullptr);
    CHECK(GuardActorFor(Characters[3], false) == nullptr);
    CHECK(CollectGuardFeedback() == 0 && !ReadGuardFeedback(0, feedback));

    Setup();
    GuardPressure.disruption = true;
    GuardPressure.disruptionTargets = 16;
    for (int id = 0; id < 12; ++id) Entries[id] = Characters[11 - id];
    CHECK(Hit(1) == 0 && Hit(1) == 1);
    CHECK(GuardActorFor(Characters[5], false)); // Exactly four metres.
    CHECK(GuardActorFor(Characters[6], false) == nullptr); // Beyond four metres.
    Setup();
    GuardPressure.disruption = true;
    At<double>(Positions[2], 0x200) = 0;
    At<double>(Positions[2], 0x210) = 500;
    CHECK(Hit(1) == 0 && Hit(1) == 1);
    CHECK(GuardActorFor(Characters[2], false) == nullptr); // Vertical distance counts.
    CHECK(GuardActorFor(Characters[3], false));
    Setup();
    GuardPressure.disruption = true;
    GuardPressure.disruptionTargets = 3;
    for (int id = 0; id < 12; ++id) Entries[id] = Characters[11 - id];
    CHECK(Hit(1) == 0 && Hit(1) == 1);
    CHECK(GuardActorFor(Characters[2], false) && GuardActorFor(Characters[4], false));
    CHECK(GuardActorFor(Characters[5], false) == nullptr); // Nearest, independent of actor-list order.
    for (int id = 2; id <= 4; ++id) GuardActorFor(Characters[id], false)->state.hesitation = 0;
    GuardPulseCooldown[1] = 0;
    GuardDisruption(Characters[1], Group(1));
    for (int id = 2; id <= 4; ++id) CHECK(GuardActorFor(Characters[id], false)->state.hesitation == 0);
    CHECK(GuardActorFor(Characters[5], false)->state.hesitation > 0); // Previously affected targets cannot be refreshed.
    Behavior.enabled = false;
    CHECK(TickGuardPressure(Controllers[5], 0.1f));
    CHECK(ObservedMode == 0 && At<int>(Group(1), 0x178) == 2);
    Setup();
    CHECK(MoraleMattersGuardDecision(Component, Characters[0], 0x10001, 10) == 0); // Mounted.
    CHECK(MoraleMattersGuardDecision(Component, Characters[0], 0x1000001, 10) == 0); // Special guard.
    CHECK(GuardActorFor(Characters[1], false) == nullptr);
    CHECK(Hit(1ULL << 32, 0x101) == 1); // Explicit native BreakGuard remains authoritative.
    Setup();
    CHECK(Hit() == 0 && Hit() == 0);
    CHECK(!TickGuardPressure(Controllers[1], 1.9f));
    CHECK(Near(GuardActorFor(Characters[1], false)->state.pressure, 2));
    CHECK(Hit() == 0);
    CHECK(!TickGuardPressure(Controllers[1], 0.2f));
    CHECK(Near(GuardActorFor(Characters[1], false)->state.pressure, 3)); // Fresh block restarts delay.
    CHECK(MH_Uninitialize() == MH_OK);
    printf("Guard pressure/native branch integration: %d assertions passed.\n", Assertions);
    return 0;
}
