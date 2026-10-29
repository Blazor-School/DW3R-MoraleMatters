#include "Game.h"
#include "ExpectedBytes.h"
#include <bcrypt.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

namespace
{
    HMODULE Module = nullptr;
    HANDLE LogFile = INVALID_HANDLE_VALUE;
    SRWLOCK LogLock = SRWLOCK_INIT;
    volatile LONG Started = 0;
    volatile LONG Status = 0;
    wchar_t IniPath[32768] = {};
    constexpr char ExpectedHash[] = "b57a2d4cbd01c9c5abd651fa42f43a4b8fdae88da6403106cd5f5c1bf4dade70";

#include "HashExecutable.inc"

    bool Boolean(const wchar_t* section, const wchar_t* key, bool fallback)
    {
        return GetPrivateProfileIntW(section, key, fallback ? 1 : 0, IniPath) != 0;
    }

    float Number(const wchar_t* section, const wchar_t* key, float fallback, float minimum, float maximum)
    {
        wchar_t text[128];
        GetPrivateProfileStringW(section, key, L"", text, 128, IniPath);
        if (!text[0])
            return fallback;
        wchar_t* end = nullptr;
        const float value = static_cast<float>(wcstod(text, &end));
        return end == text || !isfinite(value) ? fallback : MoraleMatters::Clamp(value, minimum, maximum);
    }

    void ReadSettings()
    {
        using namespace MoraleMatters;
        Config.enabled = Boolean(L"General", L"Enabled", true);
        Config.offscreen = Boolean(L"Offscreen", L"Enabled", true);
        Config.support = Boolean(L"Support", L"Enabled", true);
        Config.casualties = Boolean(L"Casualties", L"Enabled", true);
        Config.attributes = Boolean(L"Combat", L"Enabled", true);
        Config.reinforcements = Boolean(L"Reinforcements", L"Enabled", true);
        Config.surpriseForces = Boolean(L"SurpriseForces", L"Enabled", true);
        Config.enemyProximity = Boolean(L"EnemyProximity", L"Enabled", true);
        Config.meatBuns = Boolean(L"Drops", L"Enabled", Boolean(L"Drops", L"DoubleMeatBunChance", true));
        Config.dropComboHits = static_cast<int>(Number(L"Drops", L"ComboHits", 12, 1, 999));
        Config.meatBunPercent = static_cast<int>(Number(L"Drops", L"MeatBunPercent", 80, 0, 100));
        Config.enemyProximityRadius = Number(L"EnemyProximity", L"RadiusMetres", 30, 1, 200) * 100;
        Config.enemyProximityBonus = Number(L"EnemyProximity", L"MoraleBonus", 100, 0, 898);
        KillRewards.enabled = Boolean(L"OffscreenKillRewards", L"Enabled", true);
        KillRewards.soldier = Number(L"OffscreenKillRewards", L"Soldier", 0.25f, 0, 898);
        KillRewards.elite = Number(L"OffscreenKillRewards", L"Elite", 0.5f, 0, 898);
        KillRewards.captain = Number(L"OffscreenKillRewards", L"Captain", 2, 0, 898);
        KillRewards.officer = Number(L"OffscreenKillRewards", L"SubordinateOfficer", 10, 0, 898);
        KillRewards.cap = Number(L"OffscreenKillRewards", L"WindowMoraleCap", 10, 0, 898);
        KillRewards.seconds = Number(L"OffscreenKillRewards", L"WindowSeconds", 10, 0.25f, 120);
        Config.reinforcementGain = Number(L"Reinforcements", L"MoralePerTenPercent", 10, 0, 898);
        Config.reinforcementCap = Number(L"Reinforcements", L"WaveMoraleCap", 50, 0, 898);
        Config.reinforcementWaveSeconds = Number(L"Reinforcements", L"WaveSeconds", 3, 0.25f, 10);
        Behavior.enabled = Boolean(L"Behavior", L"Enabled", true);
        Behavior.profiles = Boolean(L"Behavior", L"AIProfiles", true);
        Behavior.cohesion = Boolean(L"Behavior", L"FormationCohesion", true);
        Behavior.regroup = Boolean(L"Behavior", L"LocalRegroup", true);
        Behavior.panic = Boolean(L"Behavior", L"DefeatPanic", true);
        Behavior.recovery = Boolean(L"Behavior", L"MoraleRecovery", true);
        Behavior.leadership = Boolean(L"Behavior", L"CommanderLeadership", true);
        Behavior.withdrawal = Boolean(L"Battlefield", L"PhasedRegroup", true);
        Behavior.withdrawalScale = Number(L"Battlefield", L"ReformFormationScale", 0.45f, 0.25f, 1);
        Behavior.withdrawalDistance = Number(L"Battlefield", L"WithdrawalDistanceMetres", 8, 0, 15) * 100;
        Experience.rescue = Boolean(L"Rescue", L"Enabled", true);
        Experience.rescueReward = Number(L"Rescue", L"MoraleReward", 10, 0, 50);
        Experience.rescueBattleCap = Number(L"Rescue", L"BattleRewardCap", 50, 0, 200);
        Behavior.lowProfileShift = Number(L"Behavior", L"LowMoraleProfileShift", -3, -19, 0);
        Behavior.highProfileShift = Number(L"Behavior", L"HighMoraleProfileShift", 2, 0, 19);
        Behavior.lowFormationScale = Number(L"Behavior", L"LowMoraleFormationScale", 1.35f, 1, 2);
        Behavior.highFormationScale = Number(L"Behavior", L"HighMoraleFormationScale", 0.8f, 0.5f, 1);
        Behavior.regroupBelow = Number(L"Behavior", L"RegroupBelowPercent", 15, 0, 50) * MoraleMaximum / 100;
        Behavior.regroupRecover = Number(L"Behavior", L"RegroupRecoverPercent", 30, Behavior.regroupBelow * 100 / MoraleMaximum, 80) * MoraleMaximum / 100;
        Behavior.regroupSeconds = Number(L"Behavior", L"RegroupSeconds", 8, 1, 30);
        Behavior.regroupCooldown = Number(L"Behavior", L"RegroupCooldownSeconds", 20, 5, 120);
        Behavior.panicRadius = Number(L"Behavior", L"PanicRadiusMetres", 20, 1, 100) * 100;
        Behavior.panicLoss = Number(L"Behavior", L"PanicMoraleLoss", 15, 0, 100);
        Behavior.panicWindow = Number(L"Behavior", L"PanicWindowSeconds", 10, 1, 60);
        Behavior.panicCap = Number(L"Behavior", L"PanicWindowCap", 30, 0, 200);
        Behavior.shockSeconds = Number(L"Behavior", L"PanicShockSeconds", 5, 0, 30);
        Behavior.recoveryPerSecond = Number(L"Behavior", L"RecoveryPerSecond", 1, 0, 10);
        Behavior.recoveryCeiling = Number(L"Behavior", L"RecoveryCeiling", 400, 1, MoraleMaximum);
        Behavior.recoveryDelay = Number(L"Behavior", L"RecoveryDelaySeconds", 10, 1, 120);
        Behavior.commanderRadius = Number(L"Behavior", L"CommanderRadiusMetres", 20, 1, 100) * 100;
        Behavior.commanderRecovery = Number(L"Behavior", L"CommanderRecoveryMultiplier", 2, 0.5f, 4);
        Behavior.commanderPanic = Number(L"Behavior", L"CommanderPanicMultiplier", 0.5f, 0, 1);
        Config.officerDamageShare = Number(L"Offscreen", L"OfficerDamageShare", 0.25f, 0, 1);
        Config.encounterRadius = Number(L"Support", L"EncounterRadiusMetres", 10, 0.1f, 1000) * 100;
        Config.exitRadius = fmaxf(Config.encounterRadius, Number(L"Support", L"ExitRadiusMetres", 15, 0.1f, 1000) * 100);
        Config.rallyBonus = Number(L"Support", L"MoraleBonus", 100, 0, 898);
        Config.rallyDuration = Number(L"Support", L"DurationSeconds", 180, 1, 86400);
        Config.healFraction = Number(L"Support", L"HealFraction", 0.5f, 0, 1);
        Config.healCooldown = Number(L"Support", L"HealCooldownSeconds", 180, 1, 86400);
        Config.soldierLoss = Number(L"Casualties", L"Soldier", 1, 0, 898);
        Config.eliteLoss = Number(L"Casualties", L"Elite", 2, 0, 898);
        Config.captainLoss = Number(L"Casualties", L"Captain", 10, 0, 898);
        Config.officerLoss = Number(L"Casualties", L"SubordinateOfficer", 50, 0, 898);
        Config.namedOfficerScale = Number(L"Combat", L"NamedOfficerStrength", 0.5f, 0, 1);
        const wchar_t* levels[] = {L"VeryLow", L"Low", L"Reference", L"High", L"VeryHigh"};
        const Settings defaults;
        for (int index = 0; index < 5; ++index)
        {
            wchar_t key[64];
            swprintf(key, 64, L"%lsMorale", levels[index]);
            Config.anchors[index] = Number(L"Combat", key, defaults.anchors[index], 1, 899);
            swprintf(key, 64, L"%lsDamageDealt", levels[index]);
            Config.outgoing[index] = Number(L"Combat", key, defaults.outgoing[index], 0.05f, 10);
            swprintf(key, 64, L"%lsDamageTaken", levels[index]);
            Config.incoming[index] = Number(L"Combat", key, defaults.incoming[index], 0.05f, 10);
            swprintf(key, 64, L"%lsAttackFrequency", levels[index]);
            Config.frequency[index] = Number(L"Combat", key, defaults.frequency[index], 0.1f, 5);
        }
        for (int index = 1; index < 5; ++index)
        {
            if (Config.anchors[index] <= Config.anchors[index - 1])
            {
                memcpy(Config.anchors, defaults.anchors, sizeof(Config.anchors));
                Log("Invalid morale anchor order; restored the five default anchors.");
                break;
            }
        }
    }

    DWORD WINAPI Initialize(void*)
    {
        using namespace MoraleMatters;
        wchar_t path[32768];
        if (!GetModuleFileNameW(Module, path, 32768))
            return 0;
        wchar_t* slash = wcsrchr(path, L'\\');
        if (!slash)
            return 0;
        *slash = 0;
        swprintf(IniPath, 32768, L"%ls\\MoraleMatters.ini", path);
        wchar_t logPath[32768];
        swprintf(logPath, 32768, L"%ls\\MoraleMatters.log", path);
        LogFile = CreateFileW(logPath, GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        ReadSettings();
        Log("Morale Matters 1.7.3. Officer share=%.2f rally=%.0f/%.0fs healing=%.0f%% cooldown=%.0fs.", Config.officerDamageShare, Config.rallyBonus, Config.rallyDuration, Config.healFraction * 100, Config.healCooldown);
        Log("Battlefield: phasedRegroup=%d withdrawal=%.0fm reformScale=%.2f rescue=%d reward=%.0f battleCap=%.0f.", Behavior.withdrawal, Behavior.withdrawalDistance / 100, Behavior.withdrawalScale, Experience.rescue, Experience.rescueReward, Experience.rescueBattleCap);
        Log("Supplies/cohesion: drops=%d comboHits=%d meatBunPercent=%d enemyProximity=%d radius=%.0fm bonus=%.0f (mutual living leaders; no separation grace).", Config.meatBuns, Config.dropComboHits, Config.meatBunPercent, Config.enemyProximity, Config.enemyProximityRadius / 100, Config.enemyProximityBonus);
        Log("Off-screen kill rewards: enabled=%d soldier=%.2f elite=%.2f captain=%.2f officer=%.2f cap=%.2f window=%.2fs (native gains credited).", KillRewards.enabled, KillRewards.soldier, KillRewards.elite, KillRewards.captain, KillRewards.officer, KillRewards.cap, KillRewards.seconds);
        Log("Reinforcements: enabled=%d gain=%.2f per 10%% cap=%.2f wave=%.2fs (native morale gains credited).", Config.reinforcements, Config.reinforcementGain, Config.reinforcementCap, Config.reinforcementWaveSeconds);
        Log("Surprise forces: enabled=%d maximum base morale=%.0f on confirmed arrival.", Config.surpriseForces, MoraleMaximum);
        Log("Behavior: enabled=%d profiles=%d cohesion=%d regroup=%d panic=%d recovery=%d leadership=%d.", Behavior.enabled, Behavior.profiles, Behavior.cohesion, Behavior.regroup, Behavior.panic, Behavior.recovery, Behavior.leadership);
        Log("On-screen combat: enabled=%d namedOfficerStrength=%.2f; per-hit motion scaling, native guards/floors retained.", Config.attributes, Config.namedOfficerScale);
        for (int index = 0; index < 5; ++index)
            Log("Combat anchor: morale=%.0f dealt=%.2fx taken=%.2fx waitFrequency=%.2fx.", Config.anchors[index], Config.outgoing[index], Config.incoming[index], Config.frequency[index]);
        if (!Config.enabled)
        {
            Log("DISABLED in MoraleMatters.ini.");
            InterlockedExchange(&Status, 2);
            return 0;
        }
        wchar_t executable[32768];
        if (!GetModuleFileNameW(nullptr, executable, 32768))
            return 0;
        const wchar_t* name = wcsrchr(executable, L'\\');
        char hash[65];
        if (!name || _wcsicmp(name + 1, L"DW3CE_RE-Win64-Shipping.exe") != 0 || !HashExecutable(executable, hash) || strcmp(hash, ExpectedHash))
        {
            Log("NOT APPLIED: unsupported host or executable SHA-256. No game hooks installed.");
            InterlockedExchange(&Status, 3);
            return 0;
        }
        Image = reinterpret_cast<uint8_t*>(GetModuleHandleW(nullptr));
        const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(Image);
        const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(Image + dos->e_lfanew);
        if (nt->OptionalHeader.SizeOfImage != 0x0B39E000)
        {
            Log("NOT APPLIED: in-memory executable layout differs.");
            InterlockedExchange(&Status, 3);
            return 0;
        }
        for (const auto& expected : ExpectedHooks)
        {
            if (memcmp(Image + expected.rva, expected.bytes, sizeof(expected.bytes)))
            {
                Log("NOT APPLIED: code differs at RVA %llx; possible conflicting mod.", static_cast<unsigned long long>(expected.rva));
                InterlockedExchange(&Status, 3);
                return 0;
            }
        }
        HMODULE pinned;
        if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN, reinterpret_cast<LPCWSTR>(&Initialize), &pinned) || !InitializeGameHooks())
        {
            Log("FAILED: native hook initialization.");
            InterlockedExchange(&Status, 4);
            return 0;
        }
        InterlockedExchange(&Status, 1);
        return 0;
    }

    void Start()
    {
        if (InterlockedCompareExchange(&Started, 1, 0))
            return;
        HANDLE thread = CreateThread(nullptr, 0, Initialize, nullptr, 0, nullptr);
        if (thread)
            CloseHandle(thread);
        else
            InterlockedExchange(&Status, 4);
    }
}

namespace MoraleMatters
{
    void Log(const char* format, ...)
    {
        if (LogFile == INVALID_HANDLE_VALUE)
            return;
        char body[1024];
        va_list args;
        va_start(args, format);
        vsnprintf(body, sizeof(body), format, args);
        va_end(args);
        SYSTEMTIME now;
        GetLocalTime(&now);
        char line[1152];
        const int length = snprintf(line, sizeof(line), "[%02u:%02u:%02u] [MoraleMatters] %s\r\n", now.wHour, now.wMinute, now.wSecond, body);
        if (length <= 0)
            return;
        DWORD written;
        AcquireSRWLockExclusive(&LogLock);
        WriteFile(LogFile, line, static_cast<DWORD>(length < static_cast<int>(sizeof(line)) ? length : sizeof(line) - 1), &written, nullptr);
        ReleaseSRWLockExclusive(&LogLock);
    }
}

extern "C" __declspec(dllexport) void InitializeASI() { Start(); }
extern "C" __declspec(dllexport) LONG MoraleMatters_GetStatus() { return InterlockedCompareExchange(&Status, 0, 0); }

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH)
    {
        Module = instance;
        DisableThreadLibraryCalls(instance);
        Start();
    }
    return TRUE;
}
