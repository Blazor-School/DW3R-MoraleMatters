// Production archer callbacks plus the supported executable's real range and
// target-assignment routines. Engine services are controlled fixtures; no game is opened.
#include "../src/Game.cpp"
#include "../src/ExpectedBytes.h"
#include <stdio.h>
#include <stdlib.h>
#include <initializer_list>

using namespace MoraleMatters;
static int Assertions = 0, PriorityCalls = 0, ResolveCalls = 0, AssignCalls = 0, HitCalls = 0;
#define CHECK(expression) do { ++Assertions; if (!(expression)) { fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #expression); return 1; } } while (0)
static bool Near(float a, float b) { return fabsf(a - b) < 0.001f; }
alignas(16) static uint8_t Formation[0x100], Groups[8 * 0x2E68], CorpsMemory[2 * 0xEF8];
alignas(16) static uint8_t Characters[8][0xB10], Positions[8][0x220], Controller[0x510], CharacterManager[0x500], Blackboard[0x100], Parameters[0x800], Class[0x40], HitComponent[0x100];
static uint64_t Identities[8];
static void* Entries[8], *ClassSlots[1], *Vtable[0xC00 / 8], *Current;
alignas(16) static uint8_t Rates[8][0x180];
static ArcherRangeFn NativeRange;
static int RangeCalls;
static bool CountedRange(void* controller, void* candidate) { ++RangeCalls; return NativeRange(controller, candidate); }
static bool Stop = false, Guard = false;
static float ObservedDamage, ObservedAuxiliary;
static uint8_t ObservedStatus;
static bool ObservedFlag;
extern "C" { void* ArcherProbeCall = nullptr; float ArcherCallerDamage = 0; float ArcherStoredDamage = 0; }
extern "C" bool ArcherDamageProbe(void*, void*, float, float, uint8_t, bool, bool*);
asm(
    ".text\n"
    ".seh_proc ArcherDamageProbe\n"
    "ArcherDamageProbe:\n"
    "subq $0x70, %rsp\n"
    ".seh_stackalloc 0x70\n"
    "movups %xmm6, 0x50(%rsp)\n"
    ".seh_savexmm %xmm6, 0x50\n"
    ".seh_endprologue\n"
    "movq 0x98(%rsp), %rax\n"
    "movq %rax, 0x18(%rsp)\n"
    "movq 0xa0(%rsp), %rax\n"
    "movq %rax, 0x20(%rsp)\n"
    "movq 0xa8(%rsp), %rax\n"
    "movq %rax, 0x28(%rsp)\n"
    "movss %xmm2, 0x40(%rsp)\n"
    "movaps %xmm2, %xmm6\n"
    "call *ArcherProbeCall(%rip)\n"
    "movss %xmm6, ArcherCallerDamage(%rip)\n"
    "movss 0x40(%rsp), %xmm0\n"
    "movss %xmm0, ArcherStoredDamage(%rip)\n"
    "movups 0x50(%rsp), %xmm6\n"
    "addq $0x70, %rsp\n"
    "ret\n"
    ".seh_endproc\n"
);

namespace MoraleMatters { void Log(const char*, ...) {} }
static void* Owner(void* weak) { return At<void*>(weak, 0); }
static bool WeakValid(void* weak) { return Owner(weak) != nullptr; }
static bool Stopped(void*, void*, int) { return Stop; }
static bool Dead(void* group) { return At<uint8_t>(group, 0x110) || At<uint8_t>(group, 0x186); }
static void* GetTarget(void*) { return Current; }
static void* GetClass() { return Class; }
static void* NativeConfig() { return Parameters; }
static void* NativeNull(void*) { return nullptr; }
static bool NativeAttribute(void*, int) { return false; }
static float NativePower(void*) { return 100; }
static void* NativeGroup(void* character) { return Group(At<int>(character, 0x7A0)); }
static bool NotMounted(void*) { return false; }
static void ClearAttack(void*) {}
static void ReleaseTicket(void*, void*) {}
static void SetTargetKey(void*, uint16_t, void* target) { Current = target; }
static void Priority(void*, void*) { ++PriorityCalls; }
static bool Resolve(void*, int) { ++ResolveCalls; return false; }
static int Assign(void*, int, void*) { ++AssignCalls; return 4; }
static bool Hit(void*, void*, float damage, float auxiliary, uint8_t status, bool flag, bool* result)
{
    ++HitCalls;
    ObservedDamage = damage;
    ObservedAuxiliary = auxiliary;
    ObservedStatus = status;
    ObservedFlag = flag;
    *result = Guard;
    return !Guard;
}
static void Jump(uintptr_t rva, void* function)
{
    Image[rva] = 0x48; Image[rva + 1] = 0xB8;
    memcpy(Image + rva + 2, &function, 8);
    Image[rva + 10] = 0xFF; Image[rva + 11] = 0xE0;
}
static void Setup()
{
    Config = {};
    Behavior = {};
    Current = nullptr;
    Stop = Guard = false;
    RangeCalls = 0;
    PriorityCalls = ResolveCalls = AssignCalls = HitCalls = 0;
    memset(Formation, 0, sizeof(Formation));
    memset(Groups, 0, sizeof(Groups));
    memset(CorpsMemory, 0, sizeof(CorpsMemory));
    memset(Characters, 0, sizeof(Characters));
    memset(Positions, 0, sizeof(Positions));
    memset(Controller, 0, sizeof(Controller));
    memset(CharacterManager, 0, sizeof(CharacterManager));
    memset(Parameters, 0, sizeof(Parameters));
    memset(Rates, 0, sizeof(Rates));
    At<float>(CorpsMemory, 0x28) = At<float>(CorpsMemory + 0xEF8, 0x28) = 400;
    At<void*>(Formation, 0x68) = CorpsMemory;
    At<int>(Formation, 0x70) = 2;
    At<void*>(Formation, 0x78) = Groups;
    At<int>(Formation, 0x80) = 8;
    At<void*>(CharacterManager, 0x50) = Entries;
    At<int>(CharacterManager, 0x58) = 8;
    At<void*>(Image, 0xA9C50F0) = CharacterManager;
    At<void*>(Image, 0xA9C6418) = Parameters;
    At<float>(Parameters, 0x324) = 1500;
    for (int id = 0; id < 8; ++id)
    {
        void* group = Groups + id * 0x2E68;
        At<void*>(group, 0x28) = Formation;
        At<int>(group, 0x38) = id ? 1 : 0;
        At<int>(group, 0x3C) = id;
        At<int>(group, 0x40) = id ? 1 : 0;
        At<int>(group, 0x178) = 2;
        At<uint8_t>(group, 0x2D20) = 6;
        At<uint8_t>(group, 0x2D21) = 1;
        At<uint8_t>(group, 0x2DFC) = 1;
        At<void*>(Characters[id], 0) = Vtable;
        At<void*>(Characters[id], 0x10) = Class;
        At<void*>(Characters[id], 0x1C0) = Positions[id];
        At<void*>(Characters[id], 0x6C8) = &Identities[id];
        At<void*>(Characters[id], 0x698) = Rates[id];
        At<float>(Rates[id], 0xF8) = At<float>(Rates[id], 0xFC) = At<float>(Rates[id], 0x108) = At<float>(Rates[id], 0x10C) = 1;
        Identities[id] = static_cast<uint64_t>(id + 1);
        At<uint8_t>(Characters[id], 0x74C) = 2;
        At<int>(Characters[id], 0x754) = 1;
        At<int>(Characters[id], 0x7A0) = id;
        At<int>(Characters[id], 0x7A4) = 0;
        At<double>(Positions[id], 0x200) = 500 + id * 10;
        Entries[id] = Characters[id];
    }
    At<uint8_t>(Groups, 0x2D21) = 2;
    At<int>(Characters[0], 0x754) = 2;
    At<uint8_t>(Characters[1], 0x74C) = 0; // Player is an equal candidate.
    At<uint8_t>(Characters[2], 0x74C) = 1;
    At<double>(Positions[0], 0x200) = 0;
    At<void*>(Controller, 0) = Vtable;
    At<void*>(Controller, 0x3DC) = Characters[0];
    At<void*>(Controller, 0x3A0) = Blackboard;
    At<uint8_t>(Controller, 0x3EC) = 5;
    At<float>(Controller, 0x414) = 12345;
    ResetBattle(Formation);
    BehaviorOwner = &Owner;
    BehaviorStopped = &Stopped;
}

static LONG WINAPI NativeException(EXCEPTION_POINTERS* error)
{
    fprintf(stderr, "Native archer fixture exception %lx at RVA %llx (RCX=%llx RDX=%llx RAX=%llx).\n", error->ExceptionRecord->ExceptionCode, static_cast<unsigned long long>(error->ContextRecord->Rip - reinterpret_cast<uintptr_t>(Image)), static_cast<unsigned long long>(error->ContextRecord->Rcx), static_cast<unsigned long long>(error->ContextRecord->Rdx), static_cast<unsigned long long>(error->ContextRecord->Rax));
    ExitProcess(1);
}

int main(int argc, char** argv)
{
    AddVectoredExceptionHandler(1, &NativeException);
    CHECK(argc == 2);
    FILE* file = fopen(argv[1], "rb");
    CHECK(file != nullptr);
    fseek(file, 0, SEEK_END);
    const long length = ftell(file);
    rewind(file);
    auto* bytes = static_cast<uint8_t*>(malloc(static_cast<size_t>(length)));
    CHECK(bytes && fread(bytes, 1, length, file) == static_cast<size_t>(length));
    fclose(file);
    const auto* dos = reinterpret_cast<IMAGE_DOS_HEADER*>(bytes);
    CHECK(dos->e_magic == IMAGE_DOS_SIGNATURE);
    const auto* nt = reinterpret_cast<IMAGE_NT_HEADERS64*>(bytes + dos->e_lfanew);
    CHECK(nt->Signature == IMAGE_NT_SIGNATURE && nt->OptionalHeader.SizeOfImage == 0xB39E000);
    const size_t imageSize = nt->OptionalHeader.SizeOfImage;
    Image = static_cast<uint8_t*>(VirtualAlloc(nullptr, imageSize, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE));
    CHECK(Image != nullptr);
    const auto* sections = IMAGE_FIRST_SECTION(nt);
    for (int i = 0; i < nt->FileHeader.NumberOfSections; ++i)
    {
        CHECK(static_cast<uint64_t>(sections[i].PointerToRawData) + sections[i].SizeOfRawData <= static_cast<uint64_t>(length));
        memcpy(Image + sections[i].VirtualAddress, bytes + sections[i].PointerToRawData, sections[i].SizeOfRawData);
    }
    free(bytes);
    for (const auto& expected : ExpectedHooks)
        CHECK(memcmp(Image + expected.rva, expected.bytes, sizeof(expected.bytes)) == 0);
    Jump(0x13AE060, reinterpret_cast<void*>(&Dead));
    Jump(0x1E291F0, reinterpret_cast<void*>(&Owner));
    Jump(0x1E2BCF0, reinterpret_cast<void*>(&WeakValid));
    Jump(0x13DD1B0, reinterpret_cast<void*>(&GetTarget));
    Jump(0x1290CA0, reinterpret_cast<void*>(&GetClass));
    Jump(0x174E580, reinterpret_cast<void*>(&NotMounted));
    Jump(0x13D0120, reinterpret_cast<void*>(&ReleaseTicket));
    Jump(0x13D0480, reinterpret_cast<void*>(&ReleaseTicket));
    Jump(0x13DE6B0, reinterpret_cast<void*>(&SetTargetKey));
    Jump(0x1A5DBE0, reinterpret_cast<void*>(static_cast<double (*)(double, double)>(&atan2)));
    Jump(0x1453710, reinterpret_cast<void*>(&NativeConfig));
    Jump(0x42A83E0, reinterpret_cast<void*>(&NativeNull));
    Jump(0x1746710, reinterpret_cast<void*>(&NativeAttribute));
    Jump(0x174F770, reinterpret_cast<void*>(&NativeGroup));
    Vtable[0xA58 / 8] = Vtable[0xA70 / 8] = Vtable[0xA18 / 8] = reinterpret_cast<void*>(&NativePower);
    Vtable[0x840 / 8] = reinterpret_cast<void*>(&ClearAttack);
    ClassSlots[0] = Class + 0x30;
    At<void*>(Class, 0x30) = ClassSlots;
    At<int>(Class, 0x38) = 0;
    ArcherTarget = &GetTarget;
    NativeRange = reinterpret_cast<ArcherRangeFn>(Image + 0x13D6B80);
    ArcherInRange = &CountedRange;
    ArcherSetTarget = reinterpret_cast<ArcherSetFn>(Image + 0x13CE640);
    CHECK(MH_Initialize() == MH_OK);
    CHECK(MH_CreateHook(Image + 0x13D43E0, reinterpret_cast<void*>(&ArcherPriority), reinterpret_cast<void**>(&OriginalArcherPriority)) == MH_OK);
    CHECK(MH_CreateHook(Image + 0x13D6E60, reinterpret_cast<void*>(&ArcherResolve), reinterpret_cast<void**>(&OriginalArcherResolve)) == MH_OK);
    CHECK(MH_CreateHook(Image + 0x13D77D0, reinterpret_cast<void*>(&ArcherAssign), reinterpret_cast<void**>(&OriginalArcherAssign)) == MH_OK);
    CHECK(MH_CreateHook(Image + 0x13978F0, reinterpret_cast<void*>(&ArcherHit), reinterpret_cast<void**>(&OriginalArcherHit)) == MH_OK);
    CHECK(MH_EnableHook(MH_ALL_HOOKS) == MH_OK);
    // The detours are installed on the real entry bytes. Only fallback bodies and
    // the large hit-reaction engine are stand-ins; range/assignment execute natively.
    OriginalArcherPriority = &Priority;
    OriginalArcherResolve = &Resolve;
    OriginalArcherAssign = &Assign;
    OriginalArcherHit = &Hit;
    FlushInstructionCache(GetCurrentProcess(), Image, imageSize);
    const auto priorityEntry = reinterpret_cast<ArcherPriorityFn>(Image + 0x13D43E0);
    const auto resolveEntry = reinterpret_cast<ArcherResolveFn>(Image + 0x13D6E60);
    const auto assignEntry = reinterpret_cast<ArcherAssignFn>(Image + 0x13D77D0);
    const auto hitEntry = reinterpret_cast<ArcherHitFn>(Image + 0x13978F0);
    // Execute the real CALL instruction at the shared damage calculation site.
    // Return immediately afterwards to inspect its retained HP-damage register.
    CHECK(Image[0x1534998] == 0xE8);
    Image[0x153499D] = 0xC3;
    ArcherProbeCall = Image + 0x1534998;
    FlushInstructionCache(GetCurrentProcess(), Image, imageSize);

    Setup();
    CHECK(RandomArcherOwner(Controller) == Characters[0]);
    Config.archerRandomTargets = false;
    priorityEntry(Controller, Group(0));
    CHECK(PriorityCalls == 1 && Current == nullptr);
    CHECK(!resolveEntry(Controller, 1) && ResolveCalls == 1);
    CHECK(assignEntry(Controller, 1, Group(0)) == 4 && AssignCalls == 1);
    Config.archerRandomTargets = true;
    // Seeded repeated acquisition: every equal candidate, including both players,
    // must be selected without a player-slot or member-array preference.
    int selections[8] = {};
    for (int trial = 0; trial < 14000; ++trial)
    {
        ArcherSetTarget(Controller, nullptr);
        ResetArchers();
        priorityEntry(Controller, Group(0));
        CHECK(Current != nullptr && Current != Characters[0]);
        const int id = At<int>(Current, 0x7A0);
        ++selections[id];
        CHECK(At<int>(Controller, 0x408) == id);
        const double x = At<double>(Positions[id], 0x200);
        CHECK(Near(At<float>(Controller, 0x414), static_cast<float>(x * x)));
        void* held = Current;
        CHECK(resolveEntry(Controller, 7) && Current == held);
        CHECK(assignEntry(Controller, 7, Group(0)) == 2 && Current == held);
    }
    for (int id = 1; id < 8; ++id)
        CHECK(selections[id] > 1750 && selections[id] < 2250);
    CHECK(PriorityCalls == 1); // Native player preference was never called while enabled.
    const int stableRangeCalls = RangeCalls;
    void* heldTarget = Current;
    for (int frame = 0; frame < 10000; ++frame) priorityEntry(Controller, Group(0));
    CHECK(Current == heldTarget && RangeCalls == stableRangeCalls); // No stable-frame range/population scan.

    Setup();
    At<uint8_t>(Characters[1], 0x787) = 1; // Dead player.
    At<int>(Group(2), 0x40) = 0; // Ally.
    At<double>(Positions[3], 0x200) = 2000; // Outside native archer range (+100 margin).
    At<double>(Positions[4], 0x210) = 2000; // More than 50 degrees above the archer.
    At<uint8_t>(Group(5), 0x198) = 1; // Being removed.
    At<uint32_t>(Characters[6], 0x860) = 1; // Native assignment capacity rejects this actor.
    At<uint32_t>(Characters[6], 0x85C) = 1;
    CHECK(SelectRandomArcherTarget(Controller, Characters[0]) && Current == Characters[7]);
    At<uint8_t>(Characters[7], 0x787) = 1;
    ResetArchers();
    CHECK(!SelectRandomArcherTarget(Controller, Characters[0]) && Current == nullptr);
    const int emptyRangeCalls = RangeCalls;
    CHECK(!SelectRandomArcherTarget(Controller, Characters[0]) && RangeCalls == emptyRangeCalls); // Retry does not rescan.
    At<uint8_t>(Characters[1], 0x787) = 0;
    ResetBattle(Formation);
    CHECK(SelectRandomArcherTarget(Controller, Characters[0]) && Current == Characters[1]);

    Setup();
    for (int special : {1, 2, 4, 6})
    {
        At<uint8_t>(Group(1), 0x2DFD) = static_cast<uint8_t>(special);
        CHECK(!ArcherCandidate(Controller, Characters[0], Characters[1]));
    }
    At<uint8_t>(Group(1), 0x2DFD) = 0;
    At<uint8_t>(Group(1), 0x2E56) = 1;
    CHECK(!ArcherCandidate(Controller, Characters[0], Characters[1]));
    At<uint8_t>(Group(1), 0x2E56) = 0;
    At<uint32_t>(Characters[1], 8) = 0x40000000;
    CHECK(!ArcherCandidate(Controller, Characters[0], Characters[1]));
    At<uint32_t>(Characters[1], 8) = 0;
    Identities[1] = 0;
    CHECK(!ArcherCandidate(Controller, Characters[0], Characters[1]));

    Setup();
    At<int>(Characters[0], 0x7A4) = -1;
    for (int id = 1; id < 8; ++id)
    {
        At<uint32_t>(Characters[id], 0x860) = 1;
        At<uint32_t>(Characters[id], 0x85C) = 1;
    }
    CHECK(SelectRandomArcherTarget(Controller, Characters[0])); // Native group-leader bypass is retained.
    CHECK(At<uint32_t>(Current, 0x85C) == 2);
    ArcherSetTarget(Controller, nullptr);
    At<int>(Characters[0], 0x7A4) = 0;
    At<uint8_t>(Group(0), 0x2D21) = 3;
    CHECK(!ArcherTroop(Characters[0]) && !RandomArcherOwner(Controller));
    At<uint8_t>(Group(0), 0x2D21) = 2;
    At<uint8_t>(Group(0), 0x2D20) = 3;
    CHECK(!ArcherTroop(Characters[0]));
    At<uint8_t>(Group(0), 0x2D20) = 6;
    Stop = true;
    CHECK(!RandomArcherOwner(Controller));
    Stop = false;
    for (int offset : {0x2E3A, 0x2E58})
    {
        At<uint8_t>(Group(0), offset) = 1;
        CHECK(!RandomArcherOwner(Controller));
        At<uint8_t>(Group(0), offset) = 0;
    }
    for (int status = 0; status < 5; ++status)
    {
        At<uint8_t>(Controller, 0x3EC) = static_cast<uint8_t>(status);
        CHECK(!RandomArcherOwner(Controller));
    }

    Setup();
    Config.archerRandomTargets = false;
    Config.attributes = false;
    Behavior.enabled = false;
    At<void*>(HitComponent, 0xB8) = Characters[0];
    bool result;
    for (float scale : {0.0f, 0.5f, 1.0f, 1.5f, 2.0f, 10.0f})
    {
        Config.archerDamageTaken = scale;
        const int before = HitCalls;
        CHECK(ArcherDamageProbe(HitComponent, nullptr, 20, 7, 0xA5, true, &result));
        CHECK(HitCalls == before + 1 && Near(ObservedDamage, 20 * scale) && Near(ArcherCallerDamage, 20 * scale) && Near(ArcherStoredDamage, 20 * scale));
        CHECK(Near(ObservedAuxiliary, 7) && ObservedStatus == 0xA5 && ObservedFlag && !result);
    }
    Config.archerDamageTaken = 0;
    for (int side = 0; side < 2; ++side)
    {
        At<int>(Group(0), 0x40) = side;
        for (bool blocked : {false, true})
        {
            Guard = blocked;
            const int before = HitCalls;
            CHECK(ArcherDamageProbe(HitComponent, nullptr, 20, 7, 0xA5, true, &result));
            CHECK(HitCalls == before + 1 && ObservedDamage == 0 && ArcherCallerDamage == 0 && ArcherStoredDamage == 0);
            CHECK(result == blocked && ObservedAuxiliary == 7 && ObservedStatus == 0xA5 && ObservedFlag);
        }
    }
    Guard = false;
    for (int protection : {0x2E3A, 0x2E58})
    {
        At<uint8_t>(Group(0), protection) = 1;
        CHECK(ArcherDamageProbe(HitComponent, nullptr, 20, 7, 0, false, &result));
        CHECK(ObservedDamage == 0 && ArcherCallerDamage == 0 && ArcherStoredDamage == 0);
        At<uint8_t>(Group(0), protection) = 0;
    }
    Config.archerDamageTaken = 2;
    At<int>(Group(0), 0x40) = 1;
    CHECK(ArcherDamageProbe(HitComponent, nullptr, 20, 7, 0, false, &result) && Near(ObservedDamage, 40));
    Guard = true;
    CHECK(!ArcherDamageProbe(HitComponent, nullptr, 20, 7, 0, false, &result) && result);
    Guard = false;
    for (float value : {0.0f, -5.0f, INFINITY, NAN})
    {
        ArcherDamageProbe(HitComponent, nullptr, value, 0, 0, false, &result);
        CHECK((isnan(value) && isnan(ObservedDamage)) || ObservedDamage == value);
    }
    At<uint8_t>(Group(0), 0x2D21) = 3;
    ArcherDamageProbe(HitComponent, nullptr, 20, 7, 0, false, &result);
    CHECK(Near(ObservedDamage, 20));
    At<uint8_t>(Group(0), 0x2D21) = 2;
    At<uint8_t>(Group(0), 0x2D20) = 3;
    ArcherDamageProbe(HitComponent, nullptr, 20, 7, 0, false, &result);
    CHECK(Near(ObservedDamage, 20));
    At<uint8_t>(Group(0), 0x2D20) = 6;
    At<uint8_t>(Characters[0], 0x74C) = 0;
    ArcherDamageProbe(HitComponent, nullptr, 20, 7, 0, false, &result);
    CHECK(Near(ObservedDamage, 20));
    Setup();
    OriginalOnscreenHit = reinterpret_cast<OnscreenHitFn>(Image + 0x1534B60);
    At<float>(CorpsObject(0), 0x28) = 100;
    float baseline, damage, auxiliary;
    bool nativeFlag;
    OriginalOnscreenHit(Characters[1], Characters[0], 10, 0, &baseline, &auxiliary, &nativeFlag);
    OnscreenHit(Characters[1], Characters[0], 10, 0, &damage, &auxiliary, &nativeFlag);
    CHECK(baseline > 0 && Near(damage, baseline * 1.3f));
    At<void*>(HitComponent, 0xB8) = Characters[0];
    ArcherDamageProbe(HitComponent, nullptr, damage, auxiliary, 0, nativeFlag, &result);
    CHECK(Near(ArcherCallerDamage, baseline * 1.3f * 1.5f));
    At<uint8_t>(Group(0), 0x2E3A) = 1;
    OnscreenHit(Characters[1], Characters[0], 1000, 0, &damage, &auxiliary, &nativeFlag);
    CHECK(damage > 0);
    ArcherDamageProbe(HitComponent, nullptr, damage, auxiliary, 0, nativeFlag, &result);
    CHECK(Near(ObservedDamage, damage) && Near(ArcherCallerDamage, damage));
    At<uint8_t>(Group(0), 0x2E3A) = 0;
    At<uint8_t>(Group(0), 0x2E58) = 1;
    ArcherDamageProbe(HitComponent, nullptr, 20, 0, 0, false, &result);
    CHECK(Near(ArcherCallerDamage, 20));
    At<uint8_t>(Group(0), 0x2E58) = 0;
    At<uint8_t>(Characters[0], 0x74C) = 2;
    hitEntry(HitComponent, nullptr, 20, 7, 0, false, &result);
    CHECK(Near(ObservedDamage, 20)); // Unrelated reaction callers are unchanged.
    printf("PASS: %d archer assertions, including actual native range/pitch/capacity rules, unbiased acquisition, target retention, exclusions, independent damage scaling and native argument preservation.\n", Assertions);
    CHECK(MH_DisableHook(MH_ALL_HOOKS) == MH_OK);
    CHECK(MH_Uninitialize() == MH_OK);
    VirtualFree(Image, 0, MEM_RELEASE);
    return 0;
}
