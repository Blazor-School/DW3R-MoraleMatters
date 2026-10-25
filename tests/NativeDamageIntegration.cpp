// Execute only the supported executable's isolated hit formula in this test process.
// Engine lookups are stubbed; no game process is opened and no health is applied.
#include "../src/Game.cpp"
#include "../src/ExpectedBytes.h"
#include <stdio.h>
#include <stdlib.h>

using namespace MoraleMatters;
static int Assertions = 0;
#define CHECK(expression) do { ++Assertions; if (!(expression)) { fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #expression); return 1; } } while (0)
static bool Near(float a, float b) { return fabsf(a - b) < 0.002f; }
alignas(16) static uint8_t Formation[0x100], Groups[2 * 0x2E68], CorpsMemory[2 * 0xEF8], Characters[2][0xB10], Rates[2][0x180], NativeConfig[0x800];
static void* Vtable[0xC00 / 8];
static bool NativeStatus = false;
static LONG WINAPI FormulaException(EXCEPTION_POINTERS* error)
{
    fprintf(stderr, "Native fixture exception %lx at image RVA %llx / test RVA %llx (RAX=%llx RCX=%llx RDX=%llx).\n", error->ExceptionRecord->ExceptionCode, static_cast<unsigned long long>(error->ContextRecord->Rip - reinterpret_cast<uintptr_t>(Image)), static_cast<unsigned long long>(error->ContextRecord->Rip - reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr))), static_cast<unsigned long long>(error->ContextRecord->Rax), static_cast<unsigned long long>(error->ContextRecord->Rcx), static_cast<unsigned long long>(error->ContextRecord->Rdx));
    ExitProcess(1);
}
namespace MoraleMatters { void Log(const char*, ...) {} }
static void* FakeConfig() { return NativeConfig; }
static void* FakeNull(void*) { return nullptr; }
static bool FakeAttribute(void*, int id) { return id == 0xCE && NativeStatus; }
static float FakePower(void*) { return 100; }
static float FakeHealth(void*) { return 100; }
static bool FakeDead(void*) { return false; }
static void* FakeGroup(void* character) { return Group(At<int>(character, 0x7A0)); }
static void Jump(uint8_t* image, uintptr_t rva, void* target)
{
    image[rva] = 0x48; image[rva + 1] = 0xB8;
    memcpy(image + rva + 2, &target, 8);
    image[rva + 10] = 0xFF; image[rva + 11] = 0xE0;
}

int main(int argc, char** argv)
{
    AddVectoredExceptionHandler(1, &FormulaException);
    CHECK(argc == 2);
    FILE* file = fopen(argv[1], "rb");
    CHECK(file != nullptr);
    fseek(file, 0, SEEK_END);
    const long length = ftell(file);
    CHECK(length > 0x1000);
    rewind(file);
    auto* bytes = static_cast<uint8_t*>(malloc(static_cast<size_t>(length)));
    CHECK(bytes != nullptr && fread(bytes, 1, length, file) == static_cast<size_t>(length));
    fclose(file);
    const auto* dos = reinterpret_cast<IMAGE_DOS_HEADER*>(bytes);
    CHECK(dos->e_magic == IMAGE_DOS_SIGNATURE && dos->e_lfanew > 0 && dos->e_lfanew < length - 0x100);
    const auto* nt = reinterpret_cast<IMAGE_NT_HEADERS64*>(bytes + dos->e_lfanew);
    CHECK(nt->Signature == IMAGE_NT_SIGNATURE && nt->FileHeader.Machine == IMAGE_FILE_MACHINE_AMD64 && nt->OptionalHeader.SizeOfImage == 0xB39E000);
    const size_t imageSize = nt->OptionalHeader.SizeOfImage;
    Image = static_cast<uint8_t*>(VirtualAlloc(nullptr, imageSize, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE));
    CHECK(Image != nullptr);
    const auto* section = IMAGE_FIRST_SECTION(nt);
    for (int i = 0; i < nt->FileHeader.NumberOfSections; ++i)
    {
        CHECK(static_cast<uint64_t>(section[i].PointerToRawData) + section[i].SizeOfRawData <= static_cast<uint64_t>(length));
        CHECK(static_cast<uint64_t>(section[i].VirtualAddress) + section[i].SizeOfRawData <= nt->OptionalHeader.SizeOfImage);
        memcpy(Image + section[i].VirtualAddress, bytes + section[i].PointerToRawData, section[i].SizeOfRawData);
    }
    bool found = false;
    for (const auto& expected : ExpectedHooks)
        if (expected.rva == 0x1534B60)
        {
            CHECK(memcmp(Image + expected.rva, expected.bytes, sizeof(expected.bytes)) == 0);
            found = true;
        }
    CHECK(found);
    free(bytes);
    // These are the only engine lookups reached by the ordinary-hit fixture.
    Jump(Image, 0x1453710, reinterpret_cast<void*>(&FakeConfig));
    Jump(Image, 0x42A83E0, reinterpret_cast<void*>(&FakeNull));
    Jump(Image, 0x1746710, reinterpret_cast<void*>(&FakeAttribute));
    Jump(Image, 0x174F770, reinterpret_cast<void*>(&FakeGroup));
    Jump(Image, 0x13AE060, reinterpret_cast<void*>(&FakeDead));
    FlushInstructionCache(GetCurrentProcess(), Image, imageSize);
    OriginalOnscreenHit = reinterpret_cast<OnscreenHitFn>(Image + 0x1534B60);
    memset(Formation, 0, sizeof(Formation));
    memset(Groups, 0, sizeof(Groups));
    memset(CorpsMemory, 0, sizeof(CorpsMemory));
    At<void*>(Formation, 0x68) = CorpsMemory;
    At<int>(Formation, 0x70) = 2;
    At<void*>(Formation, 0x78) = Groups;
    At<int>(Formation, 0x80) = 2;
    Vtable[0xA58 / 8] = reinterpret_cast<void*>(&FakePower);
    Vtable[0xA70 / 8] = reinterpret_cast<void*>(&FakePower);
    Vtable[0xA18 / 8] = reinterpret_cast<void*>(&FakeHealth);
    for (int id = 0; id < 2; ++id)
    {
        At<void*>(Characters[id], 0) = Vtable;
        At<void*>(Characters[id], 0x698) = Rates[id];
        At<int>(Characters[id], 0x7A0) = id;
        At<int>(Characters[id], 0x7A4) = -1;
        At<uint8_t>(Characters[id], 0x74C) = 2;
        At<int>(Groups + id * 0x2E68, 0x38) = id;
        At<int>(Groups + id * 0x2E68, 0x3C) = id;
        At<uint8_t>(Groups + id * 0x2E68, 0x2D20) = 3;
        At<float>(Rates[id], 0xF8) = At<float>(Rates[id], 0xFC) = 1;
        At<float>(Rates[id], 0x108) = At<float>(Rates[id], 0x10C) = 1;
    }
    ResetBattle(Formation);
    CHECK(GroupCount() == 2 && CorpsCount() == 2);
    Config.namedOfficerScale = 1;
    const float outgoing[] = {0.30f, 0.55f, 1, 1.60f, 2.50f};
    const float incoming[] = {1.90f, 1.45f, 1, 0.75f, 0.45f};
    memcpy(Config.outgoing, outgoing, sizeof(outgoing));
    memcpy(Config.incoming, incoming, sizeof(incoming));
    float nativeDamage, damage, auxiliary;
    bool nativeFlag;
    OriginalOnscreenHit(Characters[0], Characters[1], 10, 0, &nativeDamage, &auxiliary, &nativeFlag);
    CHECK(nativeDamage > 0 && !nativeFlag);
    printf("Native formula baseline: %.3f damage.\n", nativeDamage);
    // Player attacks the same officer: only the officer's morale changes.
    At<uint8_t>(Characters[0], 0x74C) = 0;
    for (int index = 0; index < 5; ++index)
    {
        At<float>(CorpsObject(1), 0x28) = Config.anchors[index];
        OnscreenHit(Characters[0], Characters[1], 10, 0, &damage, &auxiliary, &nativeFlag);
        CHECK(Near(damage, nativeDamage * incoming[index]));
        printf("Player -> officer, morale %.0f: %.3f damage (%.2fx).\n", Config.anchors[index], damage, incoming[index]);
    }
    // Officer attacks the same player: player morale does not contribute.
    At<uint8_t>(Characters[0], 0x74C) = 2;
    At<uint8_t>(Characters[1], 0x74C) = 0;
    OriginalOnscreenHit(Characters[0], Characters[1], 10, 0, &nativeDamage, &auxiliary, &nativeFlag);
    printf("Native formula versus player baseline: %.3f damage.\n", nativeDamage);
    for (int index = 0; index < 5; ++index)
    {
        At<float>(CorpsObject(0), 0x28) = Config.anchors[index];
        OnscreenHit(Characters[0], Characters[1], 10, 0, &damage, &auxiliary, &nativeFlag);
        CHECK(Near(damage, nativeDamage * outgoing[index]));
        printf("Officer -> player, morale %.0f: %.3f damage (%.2fx).\n", Config.anchors[index], damage, outgoing[index]);
    }
    // Native minimum damage, status flag and protected group rules are still inside the scaled calculation.
    float smallNative;
    OriginalOnscreenHit(Characters[0], Characters[1], 0.00001f, 0, &smallNative, &auxiliary, &nativeFlag);
    OnscreenHit(Characters[0], Characters[1], 0.00001f, 0, &damage, &auxiliary, &nativeFlag);
    CHECK(smallNative > 0 && Near(damage, smallNative));
    NativeStatus = true;
    float statusNative;
    OriginalOnscreenHit(Characters[0], Characters[1], 10, 0, &statusNative, &auxiliary, &nativeFlag);
    OnscreenHit(Characters[0], Characters[1], 10, 0, &damage, &auxiliary, &nativeFlag);
    CHECK(nativeFlag && Near(damage, statusNative * 2.5f));
    NativeStatus = false;
    At<uint8_t>(Group(1), 0x2E3A) = 1;
    OnscreenHit(Characters[0], Characters[1], 100, 0, &damage, &auxiliary, &nativeFlag);
    CHECK(Near(damage, 99));
    At<uint8_t>(Group(1), 0x2E3A) = 0;
    At<float>(Rates[0], 0xFC) = 2; // Native power-up is kept, with base remaining 1.
    OnscreenHit(Characters[0], Characters[1], 10, 0, &damage, &auxiliary, &nativeFlag);
    CHECK(Near(damage, nativeDamage * 5));
    printf("PASS: %d assertions executing the actual supported native hit formula with controlled engine dependencies; no live battle or animations simulated.\n", Assertions);
    VirtualFree(Image, 0, MEM_RELEASE);
    return 0;
}
