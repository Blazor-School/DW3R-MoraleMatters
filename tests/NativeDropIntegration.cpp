// Execute the supported game's actual drop-selection and queue code in isolation.
#include "../src/Game.cpp"
#include "../src/ExpectedBytes.h"
#include <stdio.h>
#include <stdlib.h>
using namespace MoraleMatters;
static int Assertions = 0;
#define CHECK(e) do { ++Assertions; if (!(e)) { fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#e); return 1; } } while(0)
namespace MoraleMatters { void Log(const char*, ...) {} }
alignas(16) static uint8_t Formation[0x100], Groups[0x2E68], CorpsMemory[0xEF8], Character[0xB10], Items[0x100], Requests[16*0x38], Mode[0x100], Attributes[0x200], OtherCharacter[0x700];
static void* Vtable[0xC00/8];
static int Combo = 16, Roll = 0;
static bool Protected = false;
static float FakeAttributeCombo(void*) { return static_cast<float>(Combo); }
static int FakeRand() { return Roll; }
static bool FakeEligible(void*) { return true; }
static bool FakeProtected(void*) { return Protected; }
static void Jump(uintptr_t rva, void* target) { Image[rva]=0x48; Image[rva+1]=0xB8; memcpy(Image+rva+2,&target,8); Image[rva+10]=0xFF; Image[rva+11]=0xE0; }
static DWORD WINAPI ReadComboOnWorker(void*)
{
    return reinterpret_cast<DropComboFn>(Image+0x1749490)(Character)==12 ? 0 : 1;
}
static uint8_t LastItem() { return Requests[(At<int>(Items,0x68)-1)*0x38+0x30]; }
int main(int argc,char** argv)
{
    CHECK(argc==2);
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
    int found = 0;
    for (const auto& expected : ExpectedHooks)
        if (expected.rva == 0x1377DF0 || expected.rva == 0x1749490)
        {
            CHECK(memcmp(Image + expected.rva, expected.bytes, sizeof(expected.bytes)) == 0);
            ++found;
        }
    CHECK(found==2);
    free(bytes);

    Jump(0x2B68910,reinterpret_cast<void*>(&FakeAttributeCombo));
    Jump(0x17519D0,reinterpret_cast<void*>(&FakeProtected));
    At<void*>(Image,0xA9C50C8)=Mode;
    At<void*>(Image,0x7E1C290)=reinterpret_cast<void*>(&FakeRand);
    At<void*>(Image,0x7E1AB18)=reinterpret_cast<void*>(&EnterCriticalSection);
    At<void*>(Image,0x7E1AB20)=reinterpret_cast<void*>(&LeaveCriticalSection);
    FlushInstructionCache(GetCurrentProcess(),Image,imageSize);
    memset(Groups,0,sizeof(Groups)); memset(CorpsMemory,0,sizeof(CorpsMemory)); memset(Character,0,sizeof(Character));
    At<void*>(Formation,0x68)=CorpsMemory; At<int>(Formation,0x70)=1;
    At<void*>(Formation,0x78)=Groups; At<int>(Formation,0x80)=1;
    At<int>(Groups,0x38)=0; At<int>(Groups,0x40)=1; At<uint8_t>(Groups,0x2D20)=6;
    At<void*>(Character,0x698)=Attributes; At<void*>(OtherCharacter,0x698)=Attributes;
    At<void*>(Character,0)=Vtable; Vtable[0xAB0/8]=reinterpret_cast<void*>(&FakeEligible);
    At<uint8_t>(Character,0x74C)=2; At<uint8_t>(Character,0x750)=1;
    At<int>(Character,0x7A0)=0; At<int>(Character,0x7A4)=0;
    At<void*>(Items,0x60)=Requests; At<int>(Items,0x6C)=16;
    auto* lock=reinterpret_cast<CRITICAL_SECTION*>(Items+0x70); InitializeCriticalSection(lock);
    At<uint8_t>(Groups,0x2DFD)=1;
    ResetBattle(Formation); At<uint8_t>(Groups,0x2DFD)=0;
    OriginalRequestDrop=reinterpret_cast<RequestDropFn>(Image+0x1377DF0);
    CHECK(MH_Initialize()==MH_OK);
    CHECK(MH_CreateHook(Image+0x1377DF0,reinterpret_cast<void*>(&RequestDrop),reinterpret_cast<void**>(&OriginalRequestDrop))==MH_OK);
    CHECK(MH_CreateHook(Image+0x1749490,reinterpret_cast<void*>(&DropCombo),reinterpret_cast<void**>(&OriginalDropCombo))==MH_OK);
    CHECK(MH_EnableHook(MH_ALL_HOOKS)==MH_OK);
    const double position[3]={100,200,300};
    auto drop = [&](int hits,int roll,int item=255) {
        Combo=hits; Roll=roll; At<int>(Items,0x68)=0;
        return reinterpret_cast<RequestDropFn>(Image+0x1377DF0)(Items,item,position,Character);
    };
    Config.meatBuns=false;
    CHECK(drop(16,0) && LastItem()==0);
    CHECK(drop(16,32767) && LastItem()==4);
    CHECK(!drop(12,0) && At<int>(Items,0x68)==0);
    Config.meatBuns=true;
    CHECK(Config.dropComboHits==12 && Config.meatBunPercent==80);
    CHECK(drop(12,0) && LastItem()==0);
    CHECK(Requests[0x31]==1);
    CHECK(At<double>(Requests,0)==100 && At<double>(Requests,8)==200);
    CHECK(drop(12,32767) && LastItem()==4);
    CHECK(drop(15,0) && LastItem()==0);
    CHECK(!drop(11,0) && At<int>(Items,0x68)==0);
    CHECK(!drop(0,0) && At<int>(Items,0x68)==0);
    Protected=true; CHECK(!drop(12,0) && At<int>(Items,0x68)==0); Protected=false;
    CHECK(drop(16,0,4) && LastItem()==5); // Explicit arrow/stat upgrades stay native.
    CHECK(drop(16,0,8) && LastItem()==9);
    At<int>(Groups,0x40)=0;
    CHECK(drop(16,32767) && LastItem()==4);
    CHECK(!drop(12,0));
    At<int>(Groups,0x40)=1; At<uint8_t>(Groups,0x2D20)=3;
    CHECK(drop(16,32767) && LastItem()==4);
    CHECK(!drop(12,0));
    At<uint8_t>(Groups,0x2D20)=6;
    Config.dropComboHits=20;
    CHECK(!drop(19,0)); CHECK(drop(20,0) && LastItem()==0);
    Config.dropComboHits=1;
    CHECK(!drop(0,0)); CHECK(drop(1,0) && LastItem()==0);
    Config.dropComboHits=12;
    int vanillaBuns=0,modifiedBuns=0,modifiedArrows=0;
    for(int sample=0;sample<100;++sample)
    {
        const int roll=(sample*32768+16384)/100;
        Config.meatBuns=false; CHECK(drop(16,roll)); vanillaBuns+=LastItem()==0;
        Config.meatBuns=true; CHECK(drop(12,roll)); modifiedBuns+=LastItem()==0; modifiedArrows+=LastItem()==4;
        CHECK(Combo==12 && DropComboCharacter==nullptr);
    }
    CHECK(vanillaBuns==50 && modifiedBuns==80 && modifiedArrows==20);
    Config.meatBunPercent=0; CHECK(drop(12,0) && LastItem()==4);
    Config.meatBunPercent=100; CHECK(drop(12,32767) && LastItem()==0);
    Config.meatBunPercent=80;
    Combo=12;
    CHECK(reinterpret_cast<DropComboFn>(Image+0x1749490)(Character)==12);
    {
        DropComboScope scoped(Character);
        CHECK(reinterpret_cast<DropComboFn>(Image+0x1749490)(Character)==16);
        CHECK(reinterpret_cast<DropComboFn>(Image+0x1749490)(OtherCharacter)==12);
        HANDLE worker=CreateThread(nullptr,0,&ReadComboOnWorker,nullptr,0,nullptr);
        CHECK(worker!=nullptr);
        CHECK(WaitForSingleObject(worker,5000)==WAIT_OBJECT_0);
        DWORD exitCode=1;CHECK(GetExitCodeThread(worker,&exitCode) && exitCode==0);
        CloseHandle(worker);

    }
    CHECK(reinterpret_cast<DropComboFn>(Image+0x1749490)(Character)==12);
    CHECK(MH_DisableHook(MH_ALL_HOOKS)==MH_OK);
    CHECK(reinterpret_cast<DropComboFn>(Image+0x1749490)(Character)==12);
    CHECK(MH_Uninitialize()==MH_OK);
    DeleteCriticalSection(lock);
    VirtualFree(Image,0,MEM_RELEASE);
    printf("PASS: %d actual-native drop assertions (12-hit boundary/configuration, 80/20 rolls, native gates, fixed drops, scoped combo reader and both live detours).\n",Assertions);
    return 0;
}
