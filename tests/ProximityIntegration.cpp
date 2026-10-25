#include "../src/Game.cpp"
#include <stdio.h>
using namespace MoraleMatters;
namespace MoraleMatters { void Log(const char*, ...) {} }
static int Assertions=0;
#define CHECK(e) do { ++Assertions; if (!(e)) { fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#e); return 1; } } while(0)
static bool Near(float a,float b) { return fabsf(a-b)<0.001f; }
alignas(16) static uint8_t Formation[0x100], Groups[5*0x2E68], Objects[4*0xEF8];
static bool FakeDead(void* group) { return At<float>(group,0x2DEC)<=0; }
static void FakeTick(void*,float) {}
static float& Morale(int id) { return At<float>(CorpsObject(id),0x28); }
static void Tick(float dt) { for(int id=0;id<4;++id) CorpsTick(CorpsObject(id),dt); }
static void Setup()
{
    Config={}; Behavior={}; Behavior.enabled=false;
    memset(Formation,0,sizeof(Formation)); memset(Groups,0,sizeof(Groups)); memset(Objects,0,sizeof(Objects));
    At<void*>(Formation,0x68)=Objects; At<int>(Formation,0x70)=4;
    At<void*>(Formation,0x78)=Groups; At<int>(Formation,0x80)=4;
    for(int id=0;id<4;++id)
    {
        void* group=Groups+id*0x2E68;void* object=Objects+id*0xEF8;
        At<void*>(object,0)=Formation; At<int>(object,0x14)=id;At<int>(object,0xC)=id?1:0;At<float>(object,0x28)=400;
        At<int>(group,0x3C)=id;At<int>(group,0x38)=id;At<int>(group,0x40)=id?1:0;
        At<float>(group,0x2DEC)=100;At<uint8_t>(group,0x2D20)=3;
        At<double>(group,0x2DA8)=id==3?50000:0;
    }
    void* subordinate=Groups+4*0x2E68;
    At<int>(subordinate,0x3C)=4;At<int>(subordinate,0x38)=3;At<int>(subordinate,0x40)=1;
    At<float>(subordinate,0x2DEC)=100;At<uint8_t>(subordinate,0x2D20)=6;
    At<int>(Formation,0x80)=5;
    Image=reinterpret_cast<uint8_t*>(reinterpret_cast<uintptr_t>(&FakeDead)-0x13AE060);
    OriginalCorpsTick=&FakeTick;
    ResetBattle(Formation);
}
int main()
{
    Setup(); Tick(1);
    CHECK(Near(Morale(0),400)); CHECK(Near(Morale(1),500)); CHECK(Near(Morale(2),500)); CHECK(Near(Morale(3),400));
    CHECK(Near(Corps[1].base,400) && Near(Corps[1].nativeGains,0));
    At<double>(Group(3),0x2DA8)=100;Tick(1);
    CHECK(Near(Morale(1),500) && Near(Morale(2),500) && Near(Morale(3),500)); // Three corps do not stack.
    CHECK(ReadCorpsExperience(1).reason==MoraleReason::EnemyProximity);
    At<uint8_t>(Group(2),0x110)=1;Tick(1);
    CHECK(Near(Morale(1),500) && Near(Morale(3),500) && Near(Morale(2),400));
    At<uint8_t>(Group(2),0x110)=0;Tick(1);
    CHECK(Near(Morale(1),500) && Near(Morale(2),500) && Near(Morale(3),500));

    for(int i=0;i<20;++i) Tick(1);
    CHECK(Near(Morale(1),500) && Near(Corps[1].base,400));
    Corps[1].Begin(Morale(1)); Morale(1)-=50; Corps[1].End(Morale(1));
    CHECK(Near(Morale(1),450) && Near(Corps[1].base,350));
    At<double>(Group(2),0x2DA8)=10000;At<double>(Group(3),0x2DA8)=20000;
    Tick(1);CHECK(Near(Morale(1),350) && Near(Corps[1].base,350));
    CHECK(Near(Morale(2),400) && Near(Morale(3),400));
    CHECK(ReadCorpsExperience(1).reason==MoraleReason::EnemySeparated);
    const float bonus=Corps[1].proximityBonus;Tick(0);CHECK(Near(Corps[1].proximityBonus,bonus));
    At<double>(Group(2),0x2DA8)=3000;Tick(1);CHECK(Near(Morale(1),450)); // Inclusive 30m boundary.
    At<double>(Group(2),0x2DA8)=3000.1;Tick(1);
    CHECK(Near(Morale(1),350));
    At<double>(Group(2),0x2DA8)=0;At<uint8_t>(Group(2),0x2DFD)=1;Tick(1);CHECK(Near(Morale(1),350));
    At<uint8_t>(Group(2),0x2DFD)=2;Tick(1);CHECK(Near(Morale(1),350));
    At<uint8_t>(Group(2),0x2DFD)=0;At<uint8_t>(Group(2),0x191)=1;Tick(1);CHECK(Near(Morale(1),350));
    At<uint8_t>(Group(2),0x191)=0;At<uint8_t>(Group(2),0x110)=1;Tick(1);CHECK(Near(Morale(1),350));
    At<uint8_t>(Group(2),0x110)=0;At<float>(Group(2),0x2DEC)=0;Tick(1);CHECK(Near(Morale(1),350));
    At<float>(Group(2),0x2DEC)=100;At<int>(Group(2),0x38)=3;Tick(1);CHECK(Near(Morale(1),350));
    At<int>(Group(2),0x38)=2;Tick(1);CHECK(Near(Morale(1),450));
    At<uint8_t>(Group(2),0x110)=1;Tick(1);
    CHECK(Near(Morale(1),350) && Near(Morale(2),400)); // A defeated leader ends mutual support.
    At<uint8_t>(Group(2),0x110)=0;Tick(1);CHECK(Near(Morale(1),450));
    Config.enemyProximity=false;Tick(0.1f);CHECK(Near(Morale(1),350));
    Setup();Morale(1)=850;Tick(1);CHECK(Near(Morale(1),899) && Near(Corps[1].base,850));
    CHECK(Near(Corps[1].VisibleBonus(),49));
    Config.enemyProximity=false;Tick(1);CHECK(Near(Morale(1),850));
    Setup();Tick(1);CHECK(Corps[1].Rally(Morale(1),Config));CHECK(Near(Morale(1),600));
    Config.enemyProximity=false;Tick(1);CHECK(Near(Morale(1),500) && Corps[1].remaining>0);
    Setup();At<double>(Group(2),0x2DA8)=10000;Tick(1);
    CHECK(Near(Morale(1),400)); // A nearby surviving subordinate of corps 3 cannot represent its distant leader.
    At<double>(Group(1),0x2DA8)=100000;At<double>(Group(4),0x2DA8)=100000;Tick(1);
    CHECK(Near(Morale(1),400)); // Same-corps proximity never supplies its own bonus.
    Setup();Tick(1);ResetBattle(Formation);CHECK(Near(Corps[1].proximityBonus,0));
    printf("PASS: %d proximity assertions (enemy-only, pair/three corps, separation/pause, native loss, caps, support independence, inactive/dead/transferred groups, disable and reset).\n",Assertions);
    return 0;
}
