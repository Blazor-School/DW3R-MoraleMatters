// Exercise the production hooks against the existing synthetic battle fixture.
#define main BehaviorRegressionMain
#include "BehaviorIntegration.cpp"
#undef main

int main()
{
    Setup(100);
    Experience = {};
    const auto generation = ExperienceGeneration();
    CHECK(Near(ExperienceTime(),0));
    CorpsTick(CorpsObject(0),0);
    CorpsTick(CorpsObject(0),-1);
    CHECK(Near(ExperienceTime(),0));
    CorpsTick(CorpsObject(0),1);
    CorpsTick(CorpsObject(1),1);
    CHECK(Near(ExperienceTime(),1)); // Parallel corps clocks do not accelerate battle time.
    {
        ReasonScope reason(MoraleReason::Reinforcements);
        Raise(CorpsObject(0),20,0);
    }
    auto event = ReadCorpsExperience(0);
    CHECK(event.reason == MoraleReason::Reinforcements && Near(event.amount,20));
    Lower(CorpsObject(0),1,0);
    CHECK(ReadCorpsExperience(0).reason == MoraleReason::Reinforcements);
    for (int i=0;i<4;++i) CorpsTick(CorpsObject(0),1);
    Lower(CorpsObject(0),1,0);
    CHECK(ReadCorpsExperience(0).reason == MoraleReason::BattleEvent);
    CHECK(Near(ReadCorpsExperience(0).amount,-1));

    Setup(400);
    Corps[0].Begin(At<float>(CorpsObject(0),0x28));
    {
        ReasonScope reason(MoraleReason::Casualties);
        Lower(CorpsObject(0),3,0);
    }
    Corps[0].End(At<float>(CorpsObject(0),0x28));
    CHECK(ReadCorpsExperience(0).reason == MoraleReason::Casualties);
    CHECK(Near(ReadCorpsExperience(0).amount,-3)); // A known cause survives an outer native corps transaction.

    Setup(100);
    Behavior.panic=false;
    Kill(Group(1),1,-1,0);
    CHECK(ReadGroupExperience(1).casualties == 1);
    CHECK(ReadCorpsExperience(0).reason == MoraleReason::CaptainDefeated);
    Kill(Group(1),1,-1,0);
    CHECK(ReadGroupExperience(1).casualties == 1);
    Kill(Group(2),1,-1,0x10000);
    CHECK(ReadGroupExperience(2).casualties == 0);

    Setup(100);
    Behavior.withdrawal=true;
    Behavior.commanderRadius=2000;
    At<double>(Group(0),0x2DA8)=1000;
    At<int>(Group(1),0x178)=2;
    GroupTick(Group(1),0.1f);
    CHECK(ReadGroupExperience(1).breaks == 1 && RegroupPhase(1) == 1);
    GroupTick(Group(1),4);
    CHECK(RegroupPhase(1) == 2);
    double result[3]={0,0,77};
    FormationOffset(Group(1),result,0);
    CHECK(result[0]>800 && result[0]<900 && Near(static_cast<float>(result[1]),90) && result[2]==77);
    FormationOffset(Group(1),result,-1);
    CHECK(Near(static_cast<float>(result[0]),100));
    At<uint8_t>(Group(0),0x110)=1;
    FormationOffset(Group(1),result,0);
    CHECK(Near(static_cast<float>(result[0]),45));
    At<uint8_t>(Group(0),0x110)=0;
    At<int>(Group(0),0x40)=1;
    FormationOffset(Group(1),result,0);
    CHECK(Near(static_cast<float>(result[0]),45));
    At<int>(Group(0),0x40)=0;
    GroupTick(Group(1),3);
    CHECK(RegroupPhase(1) == 3);
    BehaviorTick(ControllerMemory,0.1f);
    CHECK(ObservedMode == 2); // Native strategic combat resumes during rejoining.
    GroupTick(Group(1),1);
    CHECK(RegroupPhase(1) == 0);
    GroupTick(Group(1),20);
    CHECK(ReadGroupExperience(1).breaks == 1); // No farming repeated regroup episodes.
    At<uint8_t>(Group(1),0x2E58)=1;
    FormationOffset(Group(1),result,0);
    CHECK(Near(static_cast<float>(result[0]),100));
    CHECK(Near(static_cast<float>(result[1]),200));

    Setup(100);
    Behavior.withdrawal=true;
    GroupTick(Group(1),0.1f);
    GroupTick(Group(1),4);
    At<uint8_t>(Group(1),0x190)=1;
    FormationOffset(Group(1),result,0);
    CHECK(Near(static_cast<float>(result[0]),135)); // Native overridden destinations do not receive withdrawal offsets.

    Setup(400);
    Experience = {};
    Experience.rescueReward=10;
    Experience.rescueBattleCap=15;
    CHECK(Near(CompleteRescue(0),10));
    CHECK(CompleteRescue(0)<0);
    CHECK(CompleteRescue(3)<0); // Enemy corps cannot earn player rescue rewards.
    CHECK(CompleteRescue(-1)<0);
    CHECK(ReadCorpsExperience(0).rewarded && ReadCorpsExperience(0).reason==MoraleReason::Rescue);
    At<int>(Group(3),0x40)=0;
    At<int>(CorpsObject(1),0xC)=0;
    CHECK(Near(CompleteRescue(3),5));
    CHECK(Near(RescueRewardsPaid,15));
    ResetBattle(ManagerMemory);
    CHECK(ExperienceGeneration()>generation && Near(ExperienceTime(),0));
    CHECK(!ReadCorpsExperience(0).rewarded && ReadGroupExperience(1).breaks==0);
    Experience.rescue=false;
    CHECK(CompleteRescue(0)<0);

    Setup(890);
    Experience = {};
    CHECK(Near(CompleteRescue(0),9));
    CHECK(Near(RescueRewardsPaid,9)); // Only actually paid morale consumes the cap.
    printf("Passed %d battlefield experience integration assertions.\n",Assertions);
    return 0;
}
