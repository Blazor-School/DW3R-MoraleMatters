// Run the actual fusion selection callback and tile renderer in an isolated
// mapping of the supported game. Engine UI/save services are controlled fixtures.
#include "../src/Game.cpp"
#include "../src/ExpectedBytes.h"
#include <stdio.h>
#include <stdlib.h>
#include <initializer_list>
using namespace MoraleMatters;
static int Assertions = 0;
#define CHECK(e) do { ++Assertions; if (!(e)) { fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #e); return 1; } } while (0)
namespace MoraleMatters { void Log(const char*, ...) {} }
alignas(16) static uint8_t Widget[0x4A8], Material[0x3A0], Result[0x3A0], Tiles[2][10][0x4D0], Singleton[0x100];
static void* Vtable[0x400 / 8], *MaterialRows[10], *ResultRows[10];
static WeaponFusionTile Source[9], Destination[9], OriginalDestination[9], InventorySource[9], Saved[9];
static int SavedCount, SavedId, ConsumedId, CommitCalls, ConsumeCalls, MapCalls;
static void Noop() {}
static bool Yes(void*) { return true; }
static int Focus(void* widget) { return At<int>(widget, 0x394); }
static void* Null(void*) { return nullptr; }
static void Animation(void*, void* output) { memset(output, 0, 16); }
static void MapAdd(void*, void*, void*, int) { ++MapCalls; }
static int Grow(int, void* array, int* capacity)
{
    const int index = At<int>(array, 8);
    At<void*>(array, 0) = realloc(At<void*>(array, 0), 32 * sizeof(WeaponFusionTile));
    *capacity = 32;
    return index;
}
static void Resize(int size, int, int count, int capacity, void* array, int, int* maximum)
{
    if (count > capacity) At<void*>(array, 0) = realloc(At<void*>(array, 0), static_cast<size_t>(count) * size);
    *maximum = count > capacity ? count : capacity;
}
static void Reserve(int size, int, int count, void* array, int, int* maximum)
{
    At<void*>(array, 0) = realloc(At<void*>(array, 0), static_cast<size_t>(count) * size);
    *maximum = count;
}
static void Commit(void*, int id, void* weapon)
{
    SavedId = id; SavedCount = At<int>(weapon, 0x10); ++CommitCalls;
    memcpy(Saved, At<void*>(weapon, 8), SavedCount * sizeof(WeaponFusionTile));
}
static void Consume(void*, int id) { ConsumedId = id; ++ConsumeCalls; }
static void Copy(void* destination, const void* source, size_t bytes) { memcpy(destination, source, bytes); }
static void Jump(uintptr_t rva, void* target) { Image[rva] = 0x48; Image[rva + 1] = 0xB8; memcpy(Image + rva + 2, &target, 8); Image[rva + 10] = 0xFF; Image[rva + 11] = 0xE0; }
static void Setup(int destinationValue, int sourceValue, uint8_t destinationType = 4, uint8_t sourceType = 4, int slot = 1)
{
    memset(Widget, 0, sizeof(Widget)); memset(Material, 0, sizeof(Material)); memset(Result, 0, sizeof(Result)); memset(Tiles, 0, sizeof(Tiles));
    for (int i = 0; i < 9; ++i) Source[i] = Destination[i] = {100, 10, 0, 0};
    Source[slot - 1] = {sourceType, 10, 0, sourceValue}; Destination[slot - 1] = {destinationType, 10, 0, destinationValue};
    memcpy(InventorySource, Source, sizeof(Source)); memcpy(OriginalDestination, Destination, sizeof(Destination));
    for (int i = 0; i < 10; ++i) { MaterialRows[i] = Tiles[0][i]; ResultRows[i] = Tiles[1][i]; At<void*>(Tiles[0][i], 0) = At<void*>(Tiles[1][i], 0) = Vtable; }
    At<void*>(Widget, 0) = Vtable;
    At<void*>(Widget, 0x350) = Material; At<void*>(Widget, 0x358) = Result;
    At<void*>(Material, 0x380) = MaterialRows; At<int>(Material, 0x388) = 10; At<int>(Material, 0x38C) = 10;
    At<void*>(Result, 0x380) = ResultRows; At<int>(Result, 0x388) = 10; At<int>(Result, 0x38C) = 10; At<int>(Result, 0x394) = slot;
    At<void*>(Widget, 0x3A8) = Source; At<int>(Widget, 0x3B0) = 9; At<int>(Widget, 0x3B4) = 9;
    At<void*>(Widget, 0x3D8) = Destination; At<int>(Widget, 0x3E0) = 9; At<int>(Widget, 0x3E4) = 9;
    At<void*>(Widget, 0x480) = OriginalDestination; At<int>(Widget, 0x488) = 9; At<int>(Widget, 0x48C) = 9;
    At<int>(Widget, 0x400) = slot; At<int>(Widget, 0x404) = 1;
    At<int>(Widget, 0x390) = 123; At<int>(Widget, 0x3C0) = 456;
    MapCalls = 0;
}
static LONG WINAPI Failure(PEXCEPTION_POINTERS error)
{
    fprintf(stderr, "Native exception %lx at RVA %llx; rcx=%llx rdx=%llx r8=%llx r9=%llx\n", error->ExceptionRecord->ExceptionCode, error->ContextRecord->Rip - reinterpret_cast<uintptr_t>(Image), error->ContextRecord->Rcx, error->ContextRecord->Rdx, error->ContextRecord->R8, error->ContextRecord->R9);
    fprintf(stderr, "Image=%p exe=%p rip=%llx assertions=%d rsp=%llx\n", Image, GetModuleHandleW(nullptr), error->ContextRecord->Rip, Assertions, error->ContextRecord->Rsp);
    auto* stack = reinterpret_cast<uint64_t*>(error->ContextRecord->Rsp);
    for (int i = 0; i < 16; ++i) fprintf(stderr, "%llx ", stack[i]);
    fprintf(stderr, "\n");
    ExitProcess(1);
}
#include "GuardWeaponFusionCases.inc"
int main(int argc, char** argv)
{
    AddVectoredExceptionHandler(1, Failure);
    CHECK(argc == 2);
    FILE* file = fopen(argv[1], "rb"); CHECK(file);
    fseek(file, 0, SEEK_END); const long length = ftell(file); rewind(file);
    auto* bytes = static_cast<uint8_t*>(malloc(length)); CHECK(bytes && fread(bytes, 1, length, file) == static_cast<size_t>(length)); fclose(file);
    const auto* nt = reinterpret_cast<IMAGE_NT_HEADERS64*>(bytes + reinterpret_cast<IMAGE_DOS_HEADER*>(bytes)->e_lfanew);
    CHECK(nt->Signature == IMAGE_NT_SIGNATURE && nt->OptionalHeader.SizeOfImage == 0xB39E000);
    Image = static_cast<uint8_t*>(VirtualAlloc(nullptr, nt->OptionalHeader.SizeOfImage, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE)); CHECK(Image);
    const auto* section = IMAGE_FIRST_SECTION(nt);
    for (int i = 0; i < nt->FileHeader.NumberOfSections; ++i) memcpy(Image + section[i].VirtualAddress, bytes + section[i].PointerToRawData, section[i].SizeOfRawData);
    int signatures = 0;
    for (const auto& expected : ExpectedHooks) { CHECK(memcmp(Image + expected.rva, expected.bytes, sizeof(expected.bytes)) == 0); if (expected.rva == 0x1699750) ++signatures; }
    CHECK(signatures == 1); free(bytes);
    memcpy(GuardNativeCommit, Image + 0x1517360, sizeof(GuardNativeCommit));
    memcpy(GuardNativeConsume, Image + 0x15172D0, sizeof(GuardNativeConsume));
    Vtable[0x180 / 8] = reinterpret_cast<void*>(&Null);
    Jump(0x4424AE0, reinterpret_cast<void*>(&Null)); Jump(0x16EF470, reinterpret_cast<void*>(&Noop));
    Jump(0x3E86730, reinterpret_cast<void*>(&Animation)); Jump(0x1463840, reinterpret_cast<void*>(&Null));
    Jump(0x3E8D5B0, reinterpret_cast<void*>(&Noop)); Jump(0x3E8D510, reinterpret_cast<void*>(&Noop));
    Jump(0x3E8EB00, reinterpret_cast<void*>(&Noop)); Jump(0x3E8D420, reinterpret_cast<void*>(&Noop));
    Jump(0x169B500, reinterpret_cast<void*>(&Noop));
    Jump(0x169B710, reinterpret_cast<void*>(&Noop)); Jump(0x1270980, reinterpret_cast<void*>(&MapAdd));
    Jump(0x1536A60, reinterpret_cast<void*>(&Null)); Jump(0x14BEEB0, reinterpret_cast<void*>(&Null));
    Jump(0x11F69B0, reinterpret_cast<void*>(&Reserve));
    Jump(0x1215C60, reinterpret_cast<void*>(&Grow)); Jump(0x1216160, reinterpret_cast<void*>(&Resize));
    Jump(0x7CB8202, reinterpret_cast<void*>(&Copy)); Jump(0x1A9D0D0, reinterpret_cast<void*>(&free));
    Jump(0x1517360, reinterpret_cast<void*>(&Commit)); Jump(0x15172D0, reinterpret_cast<void*>(&Consume)); Jump(0x153FE50, reinterpret_cast<void*>(&Null));
    // Voice/system singletons stay null. Only confirmation uses GameInstance.
    At<void*>(Image, 0xA9C50E0) = nullptr; At<void*>(Image, 0xA9C50F8) = nullptr; At<void*>(Image, 0xA9C50F0) = nullptr;
    Image[0xA6DA768] = 0;
    CHECK(MH_Initialize() == MH_OK);
    CHECK(MH_CreateHook(Image + 0x1699750, reinterpret_cast<void*>(&WeaponFusionSelect), reinterpret_cast<void**>(&OriginalWeaponFusionSelect)) == MH_OK);
    CHECK(MH_EnableHook(MH_ALL_HOOKS) == MH_OK);
    auto select = reinterpret_cast<WeaponFusionSelectFn>(Image + 0x1699750);
    for (int type = 0; type <= 12; ++type)
        for (int slot : {1, 8, 9})
            for (int increment : {1, 2, 7, 1000})
                for (int sourceValue : {0, 10, 20, 21, 50})
                {
                    Config = Settings(); Config.weaponFusionIncrement = increment;
                    Setup(20, sourceValue, type, type, slot); select(Widget);
                    const int expected = sourceValue > 20 ? sourceValue : 20 + increment;
                    CHECK(Destination[slot - 1].value == expected && Destination[slot - 1].item == type);
                    CHECK(At<int>(Tiles[1][slot], 0x4C4) == expected && At<uint8_t>(Tiles[1][slot], 0x4B8) == type);
                    CHECK(memcmp(Source, InventorySource, sizeof(Source)) == 0 && At<void*>(Widget, 0x3A8) == Source);
                    CHECK(At<int>(Widget, 0x404) == 0 && At<uint8_t>(Widget, 0x408) == 1 && MapCalls == 1);
                    CHECK(CommitCalls == 0 && ConsumeCalls == 0);
                }
    for (int destinationType : {0, 4, 12, 13, 100})
        for (int sourceType : {0, 4, 12, 13, 100})
        {
            Config = Settings(); Setup(20, 10, destinationType, sourceType); select(Widget);
            CHECK(Destination[0].item == sourceType && Destination[0].value == (sourceType <= 12 && sourceType == destinationType ? 22 : 10));
        }
    for (int disabled = 0; disabled < 2; ++disabled)
    {
        Config = Settings(); if (disabled) Config.enabled = false; else Config.weaponFusion = false;
        Setup(20, 10); select(Widget); CHECK(Destination[0].value == 10);
    }
    Config = Settings(); Setup(INT32_MAX - 1, 10); select(Widget); CHECK(Destination[0].value == INT32_MAX);
    Setup(20, -1); select(Widget); CHECK(Destination[0].value == -1);
    Setup(-1, 0); select(Widget); CHECK(Destination[0].value == 0);
    Setup(20, 10); At<int>(Widget, 0x400) = 0; At<int>(Result, 0x394) = 0; select(Widget); CHECK(Destination[0].value == 20); // Element row, no numeric fusion.
    Setup(20, 10); At<int>(Widget, 0x404) = 0; select(Widget); CHECK(Destination[0].value == 20); // No pending selection.
    // Replacing/revisiting a pending selection must not farm increments without
    // consuming another material. Match against the weapon's original tile.
    Setup(20, 10); select(Widget);
    At<int>(Widget, 0x400) = 1; At<int>(Widget, 0x404) = 1; select(Widget);
    CHECK(Destination[0].value == 22 && Source[0].value == 10);
    Setup(20, 10, 4, 5); select(Widget);
    Source[0].item = 4; At<int>(Widget, 0x400) = 1; At<int>(Widget, 0x404) = 1; select(Widget);
    CHECK(Destination[0].item == 4 && Destination[0].value == 22);
    // Native undo restores the original result and its displayed value.
    Setup(20, 10); WeaponFusionTile before[9]; memcpy(before, Destination, sizeof(before)); select(Widget);
    int mapping[4] = {1, 1, -1, 0};
    At<void*>(Widget, 0x418) = mapping; At<int>(Widget, 0x420) = 1;
    At<int>(Widget, 0x428) = 1; At<int>(Widget, 0x440) = 1;
    At<void*>(Widget, 0x480) = before; At<int>(Widget, 0x488) = 9;
    Jump(0x169B940, reinterpret_cast<void*>(&Yes)); Jump(0x169B9C0, reinterpret_cast<void*>(&Focus)); Jump(0x169C7C0, reinterpret_cast<void*>(&Noop));
    CHECK(reinterpret_cast<bool (*)(void*)>(Image + 0x169A2D0)(Widget));
    CHECK(Destination[0].value == 20 && At<int>(Tiles[1][1], 0x4C4) == 20);
    CHECK(memcmp(Source, InventorySource, sizeof(Source)) == 0 && CommitCalls == 0 && ConsumeCalls == 0);
    // Actual native confirmation normalizes the preview, passes it to the save
    // service, consumes the material, and clears its UI-local weapon copies.
    Setup(20, 10); select(Widget); At<void*>(Image, 0xA9C50E0) = Singleton;
    reinterpret_cast<WeaponFusionSelectFn>(Image + 0x16990F0)(Widget);
    CHECK(CommitCalls == 1 && ConsumeCalls == 1 && SavedId == 123 && ConsumedId == 456);
    CHECK(SavedCount == 9 && Saved[0].item == 4 && Saved[0].value == 22);
    CHECK(At<int>(Widget, 0x390) == -1 && At<int>(Widget, 0x3C0) == -1);
    CHECK(GuardFusionCases() == 0);
    CHECK(MH_DisableHook(MH_ALL_HOOKS) == MH_OK && MH_Uninitialize() == MH_OK);
    VirtualFree(Image, 0, MEM_RELEASE);
    printf("Weapon fusion native integration: %d assertions passed.\n", Assertions);
    return 0;
}

