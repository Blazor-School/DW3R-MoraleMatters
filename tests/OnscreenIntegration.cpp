// Production per-hit hook with synthetic game objects and a replacement native calculation.
#include "../src/Game.cpp"
#include <stdio.h>
#include <initializer_list>

using namespace MoraleMatters;
static int Assertions = 0, Calls = 0;
#define CHECK(expression) do { ++Assertions; if (!(expression)) { fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #expression); return 1; } } while (0)
static bool Near(float a, float b) { return fabsf(a - b) < 0.001f; }
alignas(16) static uint8_t Formation[0x100], Groups[2 * 0x2E68], CorpsMemory[2 * 0xEF8], Characters[2][0x900], AttributesMemory[2][0x180];
static float ObservedMotion, NativeRate = 1;
static uint64_t ObservedFlags;
static bool Protect = false, Block = false;
namespace MoraleMatters { void Log(const char*, ...) {} }
static bool FakeDead(void* group) { return At<uint8_t>(group, 0x110) != 0; }
static void FakeHit(void*, void*, float motion, uint64_t flags, float* damage, float* guard, bool* guarded)
{
    ++Calls;
    ObservedMotion = motion;
    ObservedFlags = flags;
    *guard = 7; // Native guard power is independent of the motion damage coefficient.
    *guarded = Block;
    *damage = Block ? 0 : fmaxf(1, 10 * motion * NativeRate);
    if (Protect) *damage = fminf(*damage, 9); // A native 1-HP floor from 10 HP.
}
static void Setup(float morale)
{
    Config = {};
    memset(Formation, 0, sizeof(Formation));
    memset(Groups, 0, sizeof(Groups));
    memset(CorpsMemory, 0, sizeof(CorpsMemory));
    memset(Characters, 0, sizeof(Characters));
    memset(AttributesMemory, 0, sizeof(AttributesMemory));
    At<void*>(Formation, 0x68) = CorpsMemory;
    At<int>(Formation, 0x70) = 2;
    At<void*>(Formation, 0x78) = Groups;
    At<int>(Formation, 0x80) = 2;
    for (int id = 0; id < 2; ++id)
    {
        void* group = Groups + id * 0x2E68;
        At<void*>(group, 0x28) = Formation;
        At<int>(group, 0x38) = id;
        At<int>(group, 0x3C) = id;
        At<uint8_t>(group, 0x2D20) = 6;
        At<float>(CorpsMemory + id * 0xEF8, 0x28) = morale;
        At<int>(Characters[id], 0x7A0) = id;
        At<int>(Characters[id], 0x7A4) = 0;
        At<uint8_t>(Characters[id], 0x74C) = 2;
        At<void*>(Characters[id], 0x698) = AttributesMemory[id];
        At<float>(AttributesMemory[id], 0xF8) = 1;
        At<float>(AttributesMemory[id], 0xFC) = 1;
        At<float>(AttributesMemory[id], 0x108) = 1;
        At<float>(AttributesMemory[id], 0x10C) = 1;
    }
    Image = reinterpret_cast<uint8_t*>(reinterpret_cast<uintptr_t>(&FakeDead) - 0x13AE060);
    ResetBattle(Formation);
    OriginalOnscreenHit = &FakeHit;
    Protect = Block = false;
    NativeRate = 1;
    Calls = 0;
}
int main()
{
    float damage, guard;
    bool guarded;
    Setup(100);
    At<uint8_t>(Characters[1], 0x74C) = 0;
    OnscreenHit(Characters[0], Characters[1], 2, 0x123456789abcdef0ULL, &damage, &guard, &guarded);
    CHECK(Calls == 1 && Near(damage, 12) && Near(guard, 7) && !guarded);
    CHECK(ObservedFlags == 0x123456789abcdef0ULL && Near(ObservedMotion, 1.2f));
    At<uint8_t>(Characters[0], 0x74C) = 0;
    At<uint8_t>(Characters[1], 0x74C) = 2;
    OnscreenHit(Characters[0], Characters[1], 2, 0, &damage, &guard, &guarded);
    CHECK(Near(damage, 26)); // Player attack power is unchanged; NPC vulnerability applies.
    At<uint8_t>(Characters[0], 0x74C) = 2;
    OnscreenHit(Characters[0], Characters[1], 2, 0, &damage, &guard, &guarded);
    CHECK(Near(damage, 15.6f)); // Both NPC sides contribute exactly once.
    Config.attributes = false;
    OnscreenHit(Characters[0], Characters[1], 2, 0, &damage, &guard, &guarded);
    CHECK(Near(damage, 20));
    Config.attributes = true;
    At<uint8_t>(Group(0), 0x2D20) = 3;
    At<int>(Characters[0], 0x7A4) = -1;
    CHECK(Near(CharacterCombatScale(Characters[0], Config.outgoing), 0.8f));
    Config.namedOfficerScale = 1;
    CHECK(Near(CharacterCombatScale(Characters[0], Config.outgoing), 0.6f));
    At<int>(Characters[0], 0x7A4) = 0;
    Config.namedOfficerScale = 0;
    CHECK(Near(CharacterCombatScale(Characters[0], Config.outgoing), 0.6f));
    for (int offset : {0x191, 0x198, 0x110})
    {
        At<uint8_t>(Group(0), offset) = 1;
        CHECK(Near(CharacterCombatScale(Characters[0], Config.outgoing), 1));
        At<uint8_t>(Group(0), offset) = 0;
    }
    At<int>(Characters[0], 0x7A0) = -1;
    CHECK(Near(CharacterCombatScale(Characters[0], Config.outgoing), 1));
    At<int>(Characters[0], 0x7A0) = 0;
    At<float>(CorpsObject(0), 0x28) = NAN;
    CHECK(Near(CharacterCombatScale(Characters[0], Config.outgoing), 1));
    CHECK(Near(CharacterCombatScale(nullptr, Config.outgoing), 1));

    Setup(800);
    NativeRate = 2; // A native power-up remains authoritative and is composed once.
    At<uint8_t>(Characters[1], 0x74C) = 0;
    OnscreenHit(Characters[0], Characters[1], 2, 0, &damage, &guard, &guarded);
    CHECK(Near(damage, 60));
    float nativeDamage, scaledDamage, outgoing, incoming;
    const int before = Calls;
    CHECK(ProbeOnscreenHit(Characters[0], Characters[1], 2, nativeDamage, scaledDamage, outgoing, incoming));
    CHECK(Calls == before + 2 && Near(nativeDamage, 40) && Near(scaledDamage, 60) && Near(outgoing, 1.5f) && Near(incoming, 1));
    CHECK(!ProbeOnscreenHit(nullptr, Characters[1], 2, nativeDamage, scaledDamage, outgoing, incoming));
    CHECK(!ProbeOnscreenHit(Characters[0], Characters[1], NAN, nativeDamage, scaledDamage, outgoing, incoming));
    CHECK(!ProbeOnscreenHit(Characters[0], Characters[1], 0, nativeDamage, scaledDamage, outgoing, incoming));
    Protect = true;
    OnscreenHit(Characters[0], Characters[1], 2, 0, &damage, &guard, &guarded);
    CHECK(Near(damage, 9));
    Protect = false;
    Block = true;
    OnscreenHit(Characters[0], Characters[1], 2, 0, &damage, &guard, &guarded);
    CHECK(damage == 0 && guarded && Near(guard, 7));
    Block = false;
    OnscreenHit(Characters[0], Characters[1], 0, 0, &damage, &guard, &guarded);
    CHECK(Near(damage, 1));
    OnscreenHit(Characters[0], Characters[1], INFINITY, 0, &damage, &guard, &guarded);
    CHECK(isinf(ObservedMotion));
    // Scanning for support no longer writes native GAS rates or compounds combat effects.
    uint8_t manager[0x60] = {};
    void* entries[] = {Characters[0], Characters[1]};
    At<void*>(manager, 0x50) = entries;
    At<int>(manager, 0x58) = 2;
    int support[4];
    for (int repeat = 0; repeat < 100; ++repeat)
    {
        UpdateCharacterRates(manager, support, 4);
        CHECK(Near(At<float>(AttributesMemory[0], 0xFC), 1) && Near(At<float>(AttributesMemory[1], 0x10C), 1));
    }
    // Exercise every native morale point against the unchanged interpolation contract.
    NativeRate = 1;
    for (int morale = 1; morale <= 899; ++morale)
    {
        At<float>(CorpsObject(0), 0x28) = static_cast<float>(morale);
        OnscreenHit(Characters[0], Characters[1], 2, 0, &damage, &guard, &guarded);
        CHECK(Near(damage, 20 * Interpolate(static_cast<float>(morale), Config.anchors, Config.outgoing)));
    }
    printf("PASS: %d assertions for per-hit morale, NPC/player combinations, officer strength, native guard/floor/power-up composition, invalid contexts and untouched GAS attributes.\n", Assertions);
    return 0;
}
