using Microsoft.Extensions.Configuration;
using System.ComponentModel.DataAnnotations;

namespace MoraleMattersUI.Forms;

public class ModSettingForm : IValidatableObject
{
    // [General]
    public bool GeneralEnabled { get; set; } = true;

    public bool GeneralProfile { get; set; } = false;

    // [Offscreen]
    public bool OffscreenEnabled { get; set; } = true;

    [Range(0.0, 1.0)]
    public double OffscreenOfficerDamageShare { get; set; } = 0.25;

    // [OffscreenKillRewards]
    public bool OffscreenKillRewardsEnabled { get; set; } = true;

    [Range(0.0, 898.0)]
    public double OffscreenKillRewardsSoldier { get; set; } = 0.25;

    [Range(0.0, 898.0)]
    public double OffscreenKillRewardsElite { get; set; } = 0.5;

    [Range(0.0, 898.0)]
    public double OffscreenKillRewardsCaptain { get; set; } = 2;

    [Range(0.0, 898.0)]
    public double OffscreenKillRewardsSubordinateOfficer { get; set; } = 10;

    [Range(0.0, 898.0)]
    public double OffscreenKillRewardsWindowMoraleCap { get; set; } = 10;

    [Range(0.25, 120.0)]
    public double OffscreenKillRewardsWindowSeconds { get; set; } = 10;

    // [Reinforcements]
    public bool ReinforcementsEnabled { get; set; } = true;

    [Range(0.0, 898.0)]
    public double ReinforcementsMoralePerTenPercent { get; set; } = 10;

    [Range(0.0, 898.0)]
    public double ReinforcementsWaveMoraleCap { get; set; } = 50;

    [Range(0.25, 10.0)]
    public double ReinforcementsWaveSeconds { get; set; } = 3;

    // [SurpriseForces]
    public bool SurpriseForcesEnabled { get; set; } = true;

    // [Drops]
    public bool DropsEnabled { get; set; } = true;

    [Range(1.0, 999.0)]
    public int DropsComboHits { get; set; } = 12;

    [Range(0.0, 100.0)]
    public int DropsMeatBunPercent { get; set; } = 80;

    // [EnemyProximity]
    public bool EnemyProximityEnabled { get; set; } = true;

    [Range(1.0, 200.0)]
    public double EnemyProximityRadiusMetres { get; set; } = 30;

    [Range(0.0, 898.0)]
    public double EnemyProximityMoraleBonus { get; set; } = 100;

    // [Behavior]
    public bool BehaviorEnabled { get; set; } = true;

    public bool BehaviorAIProfiles { get; set; } = true;

    [Range(-19.0, 0.0)]
    public double BehaviorLowMoraleProfileShift { get; set; } = -6;

    [Range(0.0, 19.0)]
    public double BehaviorHighMoraleProfileShift { get; set; } = 6;

    public bool BehaviorFormationCohesion { get; set; } = true;

    [Range(1.0, 2.0)]
    public double BehaviorLowMoraleFormationScale { get; set; } = 1.70;

    [Range(0.5, 1.0)]
    public double BehaviorHighMoraleFormationScale { get; set; } = 0.65;

    public bool BehaviorLocalRegroup { get; set; } = true;

    [Range(0.0, 50.0)]
    public double BehaviorRegroupBelowPercent { get; set; } = 25;

    [Range(0.0, 80.0)]
    public double BehaviorRegroupRecoverPercent { get; set; } = 45;

    [Range(1.0, 30.0)]
    public double BehaviorRegroupSeconds { get; set; } = 10;

    [Range(5.0, 120.0)]
    public double BehaviorRegroupCooldownSeconds { get; set; } = 10;

    public bool BehaviorDefeatPanic { get; set; } = true;

    [Range(1.0, 100.0)]
    public double BehaviorPanicRadiusMetres { get; set; } = 20;

    [Range(0.0, 100.0)]
    public double BehaviorPanicMoraleLoss { get; set; } = 15;

    [Range(1.0, 60.0)]
    public double BehaviorPanicWindowSeconds { get; set; } = 10;

    [Range(0.0, 200.0)]
    public double BehaviorPanicWindowCap { get; set; } = 30;

    [Range(0.0, 30.0)]
    public double BehaviorPanicShockSeconds { get; set; } = 5;

    public bool BehaviorMoraleRecovery { get; set; } = true;

    [Range(0.0, 10.0)]
    public double BehaviorRecoveryPerSecond { get; set; } = 1;

    [Range(1.0, 899.0)]
    public double BehaviorRecoveryCeiling { get; set; } = 400;

    [Range(1.0, 120.0)]
    public double BehaviorRecoveryDelaySeconds { get; set; } = 10;

    public bool BehaviorCommanderLeadership { get; set; } = true;

    [Range(1.0, 100.0)]
    public double BehaviorCommanderRadiusMetres { get; set; } = 20;

    [Range(0.5, 4.0)]
    public double BehaviorCommanderRecoveryMultiplier { get; set; } = 2;

    [Range(0.0, 1.0)]
    public double BehaviorCommanderPanicMultiplier { get; set; } = 0.5;

    // [Support]
    public bool SupportEnabled { get; set; } = true;

    [Range(0.1, 1000.0)]
    public double SupportEncounterRadiusMetres { get; set; } = 10;

    [Range(0.1, 1000.0)]
    public double SupportExitRadiusMetres { get; set; } = 15;

    [Range(0.0, 898.0)]
    public double SupportMoraleBonus { get; set; } = 100;

    [Range(1.0, 86400.0)]
    public double SupportDurationSeconds { get; set; } = 180;

    [Range(0.0, 1.0)]
    public double SupportHealFraction { get; set; } = 0.50;

    [Range(1.0, 86400.0)]
    public double SupportHealCooldownSeconds { get; set; } = 180;

    [RegularExpression(@"[^\r\n]*", ErrorMessage = "Use a single line of text.")]
    public string SupportMoraleText { get; set; } = "Morale boosted";

    [RegularExpression(@"[^\r\n]*", ErrorMessage = "Use a single line of text.")]
    public string SupportHealText { get; set; } = "Health restored";

    [Range(0.5, 15.0)]
    public double SupportPopupSeconds { get; set; } = 2.5;

    // [Casualties]
    public bool CasualtiesEnabled { get; set; } = true;

    [Range(0.0, 898.0)]
    public double CasualtiesSoldier { get; set; } = 1;

    [Range(0.0, 898.0)]
    public double CasualtiesElite { get; set; } = 2;

    [Range(0.0, 898.0)]
    public double CasualtiesCaptain { get; set; } = 10;

    [Range(0.0, 898.0)]
    public double CasualtiesSubordinateOfficer { get; set; } = 50;

    // [Combat]
    public bool CombatEnabled { get; set; } = true;

    [Range(0.0, 1.0)]
    public double CombatNamedOfficerStrength { get; set; } = 1.0;

    [Range(1.0, 899.0)]
    public double CombatVeryLowMorale { get; set; } = 100;

    [Range(0.05, 10.0)]
    public double CombatVeryLowDamageDealt { get; set; } = 0.30;

    [Range(0.05, 10.0)]
    public double CombatVeryLowDamageTaken { get; set; } = 1.90;

    [Range(0.1, 5.0)]
    public double CombatVeryLowAttackFrequency { get; set; } = 0.25;

    [Range(1.0, 899.0)]
    public double CombatLowMorale { get; set; } = 200;

    [Range(0.05, 10.0)]
    public double CombatLowDamageDealt { get; set; } = 0.55;

    [Range(0.05, 10.0)]
    public double CombatLowDamageTaken { get; set; } = 1.45;

    [Range(0.1, 5.0)]
    public double CombatLowAttackFrequency { get; set; } = 0.50;

    [Range(1.0, 899.0)]
    public double CombatReferenceMorale { get; set; } = 400;

    [Range(0.05, 10.0)]
    public double CombatReferenceDamageDealt { get; set; } = 1.0;

    [Range(0.05, 10.0)]
    public double CombatReferenceDamageTaken { get; set; } = 1.0;

    [Range(0.1, 5.0)]
    public double CombatReferenceAttackFrequency { get; set; } = 1.0;

    [Range(1.0, 899.0)]
    public double CombatHighMorale { get; set; } = 600;

    [Range(0.05, 10.0)]
    public double CombatHighDamageDealt { get; set; } = 1.60;

    [Range(0.05, 10.0)]
    public double CombatHighDamageTaken { get; set; } = 0.75;

    [Range(0.1, 5.0)]
    public double CombatHighAttackFrequency { get; set; } = 1.75;

    [Range(1.0, 899.0)]
    public double CombatVeryHighMorale { get; set; } = 800;

    [Range(0.05, 10.0)]
    public double CombatVeryHighDamageDealt { get; set; } = 2.50;

    [Range(0.05, 10.0)]
    public double CombatVeryHighDamageTaken { get; set; } = 0.45;

    [Range(0.1, 5.0)]
    public double CombatVeryHighAttackFrequency { get; set; } = 2.75;

    // [HUD]
    public bool HUDEnabled { get; set; } = true;

    [Range(100.0, 2000.0)]
    public double HUDUpdateMilliseconds { get; set; } = 500;

    [Range(220.0, 900.0)]
    public double HUDPanelWidth { get; set; } = 260;

    [Range(16.0, 64.0)]
    public double HUDPortraitSize { get; set; } = 20;

    [Range(16.0, 64.0)]
    public double HUDCommanderPortraitSize { get; set; } = 24;

    [Range(12.0, 18.0)]
    public double HUDTitleFontSize { get; set; } = 16;

    [Range(0.0, 3840.0)]
    public double HUDLeft { get; set; } = 36;

    [Range(0.0, 2160.0)]
    public double HUDTop { get; set; } = 170;

    [Required]
    [RegularExpression(@"[^\r\n]*", ErrorMessage = "Use a single line of text.")]
    public string HUDAllCorpsKey { get; set; } = "F7";

    [Range(0.0, 1.0)]
    public double HUDAllyMoraleRed { get; set; } = 0.10;

    [Range(0.0, 1.0)]
    public double HUDAllyMoraleGreen { get; set; } = 0.72;

    [Range(0.0, 1.0)]
    public double HUDAllyMoraleBlue { get; set; } = 0.30;

    [Range(0.0, 1.0)]
    public double HUDTemporaryRed { get; set; } = 1.0;

    [Range(0.0, 1.0)]
    public double HUDTemporaryGreen { get; set; } = 0.78;

    [Range(0.0, 1.0)]
    public double HUDTemporaryBlue { get; set; } = 0.08;

    // [Battlefield]
    public bool BattlefieldDistressSignals { get; set; } = true;

    public bool BattlefieldMoraleDetails { get; set; } = true;

    [Range(3.0, 30.0)]
    public double BattlefieldTrendSeconds { get; set; } = 10;

    [Range(2.0, 15.0)]
    public double BattlefieldReasonSeconds { get; set; } = 6;

    [Range(5.0, 60.0)]
    public double BattlefieldDistressMoralePercent { get; set; } = 35;

    [Range(5.0, 80.0)]
    public double BattlefieldDistressHealthPercent { get; set; } = 40;

    [Range(0.5, 20.0)]
    public double BattlefieldDistressDropPercent { get; set; } = 3;

    [Range(1.0, 30.0)]
    public double BattlefieldDistressCasualties { get; set; } = 3;

    [Range(2.0, 15.0)]
    public double BattlefieldAlertSeconds { get; set; } = 6;

    [Range(10.0, 180.0)]
    public double BattlefieldAlertCooldownSeconds { get; set; } = 20;

    [Range(30.0, 300.0)]
    public double BattlefieldCorpsAlertCooldownSeconds { get; set; } = 90;

    public bool BattlefieldPhasedRegroup { get; set; } = true;

    [Range(0.25, 1.0)]
    public double BattlefieldReformFormationScale { get; set; } = 0.45;

    [Range(0.0, 15.0)]
    public double BattlefieldWithdrawalDistanceMetres { get; set; } = 8;

    // [Rescue]
    public bool RescueEnabled { get; set; } = true;

    [Range(30.0, 300.0)]
    public double RescueObjectiveSeconds { get; set; } = 120;

    [Range(10.0, 60.0)]
    public double RescueActionRadiusMetres { get; set; } = 30;

    [Range(35.0, 70.0)]
    public double RescueRecoverMoralePercent { get; set; } = 40;

    [Range(40.0, 90.0)]
    public double RescueRecoverHealthPercent { get; set; } = 50;

    [Range(1.0, 30.0)]
    public double RescueEnemyCasualties { get; set; } = 5;

    [Range(2.0, 20.0)]
    public double RescueStableSeconds { get; set; } = 5;

    [Range(0.0, 50.0)]
    public double RescueMoraleReward { get; set; } = 10;

    [Range(0.0, 200.0)]
    public double RescueBattleRewardCap { get; set; } = 50;

    // [Report]
    public bool ReportEnabled { get; set; } = true;

    [Required]
    [RegularExpression(@"[^\r\n]*", ErrorMessage = "Use a single line of text.")]
    public string ReportKey { get; set; } = "F8";

    public bool ReportShowOnResults { get; set; } = true;

    public bool ReportSaveToFile { get; set; } = true;

    public IEnumerable<ValidationResult> Validate(ValidationContext validationContext)
    {
        if (SupportExitRadiusMetres < SupportEncounterRadiusMetres)
        {
            yield return new ValidationResult("Support exit radius must be at least the encounter radius.", [nameof(SupportExitRadiusMetres), nameof(SupportEncounterRadiusMetres)]);
        }

        if (BehaviorRegroupRecoverPercent < BehaviorRegroupBelowPercent)
        {
            yield return new ValidationResult("Regroup recovery percent must be at least the regroup threshold.", [nameof(BehaviorRegroupRecoverPercent), nameof(BehaviorRegroupBelowPercent)]);
        }

        if (CombatVeryLowMorale >= CombatLowMorale || CombatLowMorale >= CombatReferenceMorale || CombatReferenceMorale >= CombatHighMorale || CombatHighMorale >= CombatVeryHighMorale)
        {
            yield return new ValidationResult("Combat morale thresholds must increase from very low through very high.", [nameof(CombatVeryLowMorale), nameof(CombatLowMorale), nameof(CombatReferenceMorale), nameof(CombatHighMorale), nameof(CombatVeryHighMorale)]);
        }
    }

    public void LoadModSettings(string gamePath)
    {
        string iniFilePath = Path.Combine(gamePath, "MoraleMatters.ini");
        using var stream = File.OpenRead(iniFilePath);
        using var configuration = (ConfigurationRoot)new ConfigurationBuilder().AddIniStream(stream).Build();
        var properties = typeof(ModSettingForm).GetProperties().ToDictionary(property => property.Name, StringComparer.OrdinalIgnoreCase);

        foreach (var entry in configuration.AsEnumerable())
        {
            if (entry.Value is null)
            {
                continue;
            }

            string propertyName = entry.Key.Replace(":", "");

            if (!properties.TryGetValue(propertyName, out var property))
            {
                continue;
            }

            object? value;

            if (property.PropertyType == typeof(bool))
            {
                value = entry.Value switch
                {
                    "1" => true,
                    "0" => false,
                    _ => throw new FormatException($"{entry.Key} must be 0 or 1.")
                };
            }
            else
            {
                value = configuration.GetValue(property.PropertyType, entry.Key);
            }

            property.SetValue(this, value);
        }
    }
}
