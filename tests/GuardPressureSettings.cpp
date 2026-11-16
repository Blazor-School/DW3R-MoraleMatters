#include "../src/Plugin.cpp"

static int Assertions = 0;
#define CHECK(expression) do { ++Assertions; if (!(expression)) { fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #expression); return 1; } } while (0)
static bool Near(float a, float b) { return fabsf(a - b) < 0.001f; }

int main()
{
    using namespace MoraleMatters;
    wchar_t folder[MAX_PATH];
    CHECK(GetTempPathW(MAX_PATH, folder));
    CHECK(GetTempFileNameW(folder, L"mmg", 0, IniPath));
    ReadSettings();
    CHECK(GuardPressure.enabled && GuardPressure.disruption);
    CHECK(Near(GuardPressure.normalPressure, 1) && Near(GuardPressure.chargePressure, 3));
    CHECK(Near(GuardPressure.soldierThreshold, 6) && Near(GuardPressure.eliteThreshold, 9) && Near(GuardPressure.officerThreshold, 12));
    CHECK(Near(GuardPressure.recoveryDelay, 2) && Near(GuardPressure.recoveryPerSecond, 2));
    CHECK(Near(GuardPressure.openingSeconds, 0.9f) && Near(GuardPressure.breakCooldown, 3));
    CHECK(Near(GuardPressure.disruptionRadius, 400) && Near(GuardPressure.disruptionSeconds, 0.8f) && Near(GuardPressure.disruptionCooldown, 4) && GuardPressure.disruptionTargets == 6);
    CHECK(GuardPressure.showPressureBar && GuardPressure.showBreakIndicator && GuardPressure.breakSound && Near(GuardPressure.indicatorSeconds, 1.2f));
    struct Setting { const wchar_t* key; float GuardPressureSettings::*field; float minimum; float maximum; float fallback; float unit; };
    const Setting cases[] = {
        {L"NormalPressure", &GuardPressureSettings::normalPressure, 0.1f, 20, 1, 1},
        {L"ChargePressure", &GuardPressureSettings::chargePressure, 0.1f, 40, 3, 1},
        {L"SoldierThreshold", &GuardPressureSettings::soldierThreshold, 1, 100, 6, 1},
        {L"EliteThreshold", &GuardPressureSettings::eliteThreshold, 1, 100, 9, 1},
        {L"OfficerThreshold", &GuardPressureSettings::officerThreshold, 1, 100, 12, 1},
        {L"RecoveryDelaySeconds", &GuardPressureSettings::recoveryDelay, 0, 10, 2, 1},
        {L"RecoveryPerSecond", &GuardPressureSettings::recoveryPerSecond, 0, 20, 2, 1},
        {L"OpeningSeconds", &GuardPressureSettings::openingSeconds, 0.1f, 3, 0.9f, 1},
        {L"BreakCooldownSeconds", &GuardPressureSettings::breakCooldown, 0.5f, 20, 3, 1},
        {L"DisruptionRadiusMetres", &GuardPressureSettings::disruptionRadius, 0.5f, 10, 4, 100},
        {L"DisruptionSeconds", &GuardPressureSettings::disruptionSeconds, 0.1f, 3, 0.8f, 1},
        {L"DisruptionCooldownSeconds", &GuardPressureSettings::disruptionCooldown, 1, 30, 4, 1},
        {L"IndicatorSeconds", &GuardPressureSettings::indicatorSeconds, 0.3f, 3, 1.2f, 1},
    };
    for (const auto& setting : cases)
    {
        CHECK(WritePrivateProfileStringW(L"GuardPressure", setting.key, L"-100", IniPath)); ReadSettings();
        CHECK(Near(GuardPressure.*setting.field, setting.minimum * setting.unit));
        CHECK(WritePrivateProfileStringW(L"GuardPressure", setting.key, L"1000", IniPath)); ReadSettings();
        CHECK(Near(GuardPressure.*setting.field, setting.maximum * setting.unit));
        CHECK(WritePrivateProfileStringW(L"GuardPressure", setting.key, L"nan", IniPath)); ReadSettings();
        CHECK(Near(GuardPressure.*setting.field, setting.fallback * setting.unit));
        CHECK(WritePrivateProfileStringW(L"GuardPressure", setting.key, nullptr, IniPath));
    }
    CHECK(WritePrivateProfileStringW(L"GuardPressure", L"DisruptionMaxTargets", L"100", IniPath)); ReadSettings();
    CHECK(GuardPressure.disruptionTargets == 16);
    CHECK(WritePrivateProfileStringW(L"GuardPressure", L"DisruptionMaxTargets", L"-1", IniPath)); ReadSettings();
    CHECK(GuardPressure.disruptionTargets == 0);
    CHECK(WritePrivateProfileStringW(L"GuardPressure", L"Enabled", L"0", IniPath));
    CHECK(WritePrivateProfileStringW(L"GuardPressure", L"LocalDisruption", L"0", IniPath)); ReadSettings();
    CHECK(!GuardPressure.enabled && !GuardPressure.disruption);
    CHECK(WritePrivateProfileStringW(L"GuardPressure", L"ShowPressureBar", L"0", IniPath));
    CHECK(WritePrivateProfileStringW(L"GuardPressure", L"ShowBreakIndicator", L"0", IniPath));
    CHECK(WritePrivateProfileStringW(L"GuardPressure", L"BreakSound", L"0", IniPath)); ReadSettings();
    CHECK(!GuardPressure.showPressureBar && !GuardPressure.showBreakIndicator && !GuardPressure.breakSound);
    CHECK(DeleteFileW(IniPath));
    printf("Guard pressure production INI parser: %d assertions passed.\n", Assertions);
    return 0;
}
