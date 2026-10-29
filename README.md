# Morale Matters 1.7.2

A native ASI and UE4SS mod for DYNASTY WARRIORS 3 Complete Edition Remastered.

## Features and defaults

- Random drops from enemy infantry/captains now require 12 combo hits and roll 80% small meat bun / 20% arrows. Configure `[Drops] ComboHits` (1-999) and `MeatBunPercent` (0-100); the remainder is arrows. `Enabled=0` restores the native 16-hit, 50/50 rule. Native mode/protection checks, fixed drops, explicit item combo upgrades and easy-difficulty extra bun drops retain their existing rules. The actual combo counter is not changed.
- Every surviving subordinate group commander has an individual portrait and a three-pixel HP bar underneath it, for both sides. This includes named officers, Vice Generals and generic captains; they are no longer combined into count portraits. HP uses the native living actor getter when instantiated, otherwise simulated group-leader HP. The corps commander's header portrait remains separate.
- Two or more surviving leaders of different enemy corps within 30 metres give each participating corps +100 temporary morale for as long as they remain together. No neighbour stacking and no separation grace period apply. When a leader is defeated or leaves, a corps loses the bonus at the next proximity sample unless another qualifying enemy leader remains nearby. Presence uses only the actual corps leader's horizontal native position; subordinate groups do not represent the corps. Proximity is sampled once per battle second and preserves native morale changes, the 899 ceiling and independent allied support. Reserved, appearing, removing, defeated and transferred leaders do not qualify. Allied corps receive no proximity bonus. `[EnemyProximity]` configures Enabled, RadiusMetres and MoraleBonus. Yellow morale includes this bonus and the detail row identifies enemy cohesion.

- Battlefield distress signals identify a wounded allied officer in a corps with low and deteriorating morale. The notification includes the officer's portrait, distance and direction relative to the nearest player's facing. Global and per-corps cooldowns limit interruptions.
- Corps panels show a ten-second morale trend beside the commander's name: a green up arrow, red down arrow or muted gray bar for steady morale. These are drawn HUD icons, independent of font glyph support, and remain visible with MoraleDetails=0. The detail rows show a recorded recent cause, remaining temporary support time, and active infantry regroup phases. Unknown native changes are labelled `Battle event`; the mod does not guess their cause.
- Optional rescue objectives recognize supporting an endangered corps, helping clear nearby enemies, and stabilizing the officer and corps. A corps can earn one +10 permanent morale reward per battle, with +50 shared across all rescues.
- Ordinary infantry now have a bounded fall-back, reform and rejoin cycle inside their existing regroup timer. Their formation contracts and can shift up to eight metres toward a nearby living corps commander before returning to ordinary spacing and native orders. Named officers, group captains, scripted orders and strategic group positions are not relocated.
- F8 opens the contribution report during battle. The report also appears on the results screen and is written to `MoraleMatters-last-battle.txt` beside the ASI. It records rescued corps, enemy formations broken nearby, successful rallies, confirmed healing and the largest permanent morale recovery of a supported corps. Nearby breaks are observations, not personal kill attribution.

- Morale changes native AI decision profiles, including guarding and evasion where the unit supports them. High morale tightens formation spacing; low morale loosens it. Ordinary infantry at critically low morale can briefly regroup. Nearby subordinate leader defeats can cause capped additional panic. Corps recover confidence out of combat, helped by proximity to their living commander. See the behavior settings below.
- Confirmed off-screen kills grant permanent morale to the recorded attacking corps: ordinary soldier 0.25, elite 0.5, captain 2, subordinate officer 10. Native morale rewards count first; additional mod gains share a cap of 10 per corps per 10 battle seconds. Existing casualty losses continue independently.
- Reinforcements joining an existing corps grant permanent morale: 10 points per 10% increase in soldiers, capped at 50 per wave. Groups arriving within a 3-second quiet window share one original troop-count baseline and cap. Native positive morale changes just before/during the wave count toward the target; the mod adds only the shortfall and preserves larger native rewards. Entirely new corps receive no extra starting bonus.
- Surprise forces, including Zhang Liao's returning Hefei formation, set their corps to maximum permanent base morale (899 / 100%) when activation completes. This applies to both sides, including newly arriving corps and lone officers. It tops up only the missing morale; scripted gains cannot push the reward above maximum. Morale can fall normally afterward.

- Off-screen combat allocates 25% of incoming group damage to a named officer while troops remain. This is part of the group's existing damage budget. Native protection that keeps an officer at 1 HP is respected.
- Entering an allied officer's 10-metre radius adds 100 temporary corps morale for 180 battle seconds. Leaving beyond 15 metres rearms the encounter. Nearby officers in the same corps cannot stack or refresh an active bonus. Expiry preserves ordinary morale changes.
- The encounter also restores 50% of the officer's maximum HP, capped at full health. Each officer has a separate 180-second healing cooldown, consumed only when healing succeeds. Staying nearby does not repeatedly trigger either benefit.
- Morale changes soldiers' damage dealt, damage taken, and time between AI attack attempts. The shipped strong preset gives named officers the full effect. Player characters do not receive their own morale multipliers; enemy damage and vulnerability still affect fights against the player. Maximum HP is unchanged. Ordinary on-screen weapon hits are scaled inside the native hit calculation, without periodically rewriting GAS damage-rate attributes.
- Casualties reduce corps morale: ordinary soldier 1, elite soldier 2, group captain 10, subordinate named officer 50. These replace the corresponding native loss instead of adding a second penalty. Corps commander defeat retains the game's normal outcome.
- Corps panels show the commander's name, total morale percentage, and portraits/icons of surviving group leaders. Allied names use the game's blue and enemies its red. Permanent morale is green for allies and red for enemies; temporary morale is yellow. The commander portrait sits beside the name and darkens to 25% brightness when defeated; it is not duplicated in the lower row. Subordinate portraits and their HP bars disappear as their group leaders are defeated. Commander defeat alone does not hide a surviving corps. Compact panels default to 260 pixels wide with 20-pixel portraits and a 16-point commander name. CommanderPortraitSize independently controls the header portrait (24 pixels by default, range 16-64). The header grows for larger portraits, moving the morale bar and lower portrait row down. Long commander names use an ellipsis within their reserved space. The name and morale percentage share the header; a full-width bar and portrait row sit below.

| Native morale | Damage dealt | Damage taken | Native retry-wait frequency factor |
| --- | --- | --- | --- |
| 100 | 0.30x | 1.90x | 0.25x |
| 200 | 0.55x | 1.45x | 0.50x |
| 400 | 1.00x | 1.00x | 1.00x |
| 600 | 1.60x | 0.75x | 1.75x |
| 800 | 2.50x | 0.45x | 2.75x |

The packaged INI and installed user preset use the values above. High versus very low morale gives 8.33 times the outgoing damage coefficient and approximately 4.22 times the resistance to the same ordinary hit. Native guarding, minimum damage and protected-officer HP floors still apply; scripted fixed damage and other paths that bypass ordinary weapon-hit calculation keep their native rules.

The frequency factor divides native attack-slot release/retry waits. It is not an attacks-per-second or animation-speed multiplier: attack tickets, distance, native decision probabilities, abilities and animations still govern attacks. A zero native wait remains zero. Stronger profile shifts supply the additional aggression/defense contrast.

Values interpolate between rows and clamp outside them. The HUD maps the game's native range, 1Ã¢â‚¬â€œ899, to a percentage. A 100-point bonus therefore contributes approximately 11 percentage points when not capped.

## Installation

Close the game. Extract the ZIP into the game root, preserving its folders. Requires the existing ASI loader and UE4SS installation; this package does not replace or redistribute either loader.

Installed files:

```
DW3CE_RE/Binaries/Win64/MoraleMatters.asi
DW3CE_RE/Binaries/Win64/MoraleMatters.ini
DW3CE_RE/Binaries/Win64/ue4ss/Mods/MoraleMatters/enabled.txt
DW3CE_RE/Binaries/Win64/ue4ss/Mods/MoraleMatters/Scripts/main.lua
DW3CE_RE/Binaries/Win64/ue4ss/Mods/MoraleMatters/Scripts/Experience.lua
```

The native core checks the executable SHA-256 and hook bytes before applying changes. Supported executable:

```
b57a2d4cbd01c9c5abd651fa42f43a4b8fdae88da6403106cd5f5c1bf4dade70
```

Other executable versions are refused. Check `MoraleMatters.log` beside the ASI for native status and `ue4ss/UE4SS.log` for Lua errors. Avoid other mods that change the same morale, casualty, off-screen attack, AI controller tick, formation offset, AI wait, on-screen hit calculation, special-attribute, item-drop request, or scenario-effect functions. Other rate-changing mods can also interfere with damage scaling.

## Battlefield, rescue and report settings

The new `[Battlefield]`, `[Rescue]` and `[Report]` sections can be configured independently. Existing combat and morale presets are retained.

Distress requires morale at or below 35%, a living named officer at or below 40% health, and either a drop of at least three percentage points or three confirmed casualties within ten battle seconds. Alerts last six battle seconds, with a 20-second global cooldown and 90-second per-corps cooldown. Direction is relative to the closest player's facing, not the camera or the minimap. Reserved surprise/reinforcement groups are excluded from active HUD rows.

A rescue opportunity lasts 120 battle seconds. After a successful existing support rally or heal, the player must be within 30 metres of confirmed enemy casualties near the endangered officer: five casualties or one group-leader defeat qualifies. These are nearby contributions, not proof of the final blow. The target must survive, reach 50% health and its corps reach 40% effective morale, remain free of fresh casualties for five seconds, then sustain those conditions for another five seconds. Failed, expired or transferred targets do not grant rewards. Each corps can complete once per battle; the default +10 base morale reward shares a +50 battle cap. Native morale ceilings apply and only actual additions consume the cap. Ordinary support healing/rallies retain their existing cooldowns.

Phased regrouping uses the existing `Behavior.RegroupSeconds`: the first 45% falls back, the next 35% reforms, and the last 20% rejoins. The contraction and bounded pull toward the living commander fade back to native formation spacing. The existing member-offset hook supplies the destination before native navigation projection. Strategic group orders and positions are not rewritten, no unit is despawned, and no new retreat/evacuation command is issued. Protected/fixed/scripted formations and retreat/evacuation modes remain excluded. The native scenario may still prevent movement; this is local infantry behavior, not a new army routing system.

`MoraleDetails=0` restores the earlier compact panel height. `PhasedRegroup=0` retains the previous local-regroup behavior. `DistressSignals=0` hides distress notifications. `[Rescue] Enabled=0` disables the objective/reward. `[Report] Enabled=0` disables the report overlay, while `SaveToFile` separately controls the text report. F8 is configurable with `Report.Key`; `ShowOnResults` controls automatic opening. On results screens, the report sits below the header at the upper-right, anchored 36 canvas units from the right edge; during battle it remains below the corps panels. All gameplay timers use battle time. Reports aggregate both local players' observed contributions; they are not separate per-player scores. The report and objective ledger reset each battle and are not serialized into suspend saves.

## Off-screen kill reward settings

```ini
[OffscreenKillRewards]
Enabled=1
Soldier=0.25
Elite=0.5
Captain=2
SubordinateOfficer=10
WindowMoraleCap=10
WindowSeconds=10
```

Values are native morale points, not HUD percentages. A native off-screen resolution records confirmed new deaths in the attacked group and its accompanying groups, then the mod totals their reward target. Actual native positive morale changes for the attacking corps during that resolution count toward this target. A target of 10 with a native gain of 6 adds 4; a native gain of 50 is preserved and adds nothing. Native credit is measured as positive gains, so a later loss within the same resolution does not earn the reward again. Nested resolutions for the same winning corps share credit rather than spending it twice. Unrelated later scenario rewards retain their normal behavior.

Only the missing amount is subject to WindowMoraleCap. The window begins with the first positive top-up request and expires after WindowSeconds of battle time; all groups in that corps share it. Only morale actually added consumes the cap. Kills beyond the available cap are not queued for later payment, and native rewards are never reduced by this cap. Gain/cap accept 0-898; the window accepts 0.25-120 seconds. Set Enabled=0 to disable these rewards independently of casualty losses and off-screen officer damage.

Attribution uses the native last-damaging-group record while the native off-screen casualty resolver runs. Unknown/invalid or same-side attackers, player-owned attack groups, visible casualties, reserved/removing groups, scripted removals, duplicate deaths and commander defeat do not receive additional rewards. Normal losing-corps casualty penalties remain in force. If native attribution or the morale transaction is unsafe, the mod skips the additional reward. It does not estimate deaths from HP damage or add a second on-screen kill reward. Both sides use the same rules, and the reward is permanent base morale rather than a temporary yellow bonus. Budgets reset for each battle and are not serialized into suspend saves.

## Morale behavior settings

All settings are in `[Behavior]` in the INI. Both sides receive the same rules. `Enabled=0` disables this entire section; each mechanic also has its own toggle. These changes supplement the existing combat multipliers and native off-screen battle formula.

| Mechanic | Default behavior | Main settings |
| --- | --- | --- |
| Blocking and evasion | Temporarily shift the unit's native decision profile by -6 at morale 100 and +6 at 800, with no shift at 400. Named officers receive full strength in the shipped preset. Native ability checks still decide whether a guard, counter or evasive action is possible. | AIProfiles, LowMoraleProfileShift, HighMoraleProfileShift |
| Formation cohesion | Infantry formation offsets scale to 170% at morale 100 and 65% at 800; neutral at 400. Destinations still pass through native navigation projection. | FormationCohesion, LowMoraleFormationScale, HighMoraleFormationScale |
| Willingness to engage | Eligible ordinary infantry at or below 25% morale use local formation-following behavior for 10 battle seconds, followed by a 10-second cooldown. Reaching 45% ends regrouping early. | LocalRegroup, RegroupBelowPercent, RegroupRecoverPercent, RegroupSeconds, RegroupCooldownSeconds |
| Nearby defeats | A subordinate leader's genuine defeat can shock surviving same-corps groups within 20 metres. Additional corps loss is 15 morale at reference morale, scaled to 22.5 at very low morale and 7.5 at very high morale. Additional losses share a cap of 30 per 10-second window. | DefeatPanic, PanicRadiusMetres, PanicMoraleLoss, PanicWindowSeconds, PanicWindowCap, PanicShockSeconds |
| Recovery | After 10 battle seconds with no corps group in native combat mode, base morale recovers toward 400 at 1 point per second before confidence and leadership scaling. Low morale recovers more slowly. | MoraleRecovery, RecoveryDelaySeconds, RecoveryPerSecond, RecoveryCeiling |
| Commander leadership | Eligible groups within 20 metres of their living commander receive half panic loss/shock and more stable formation spacing. Recovery scales from 0.5x when no groups are nearby to 2x when all eligible groups are nearby. | CommanderLeadership, CommanderRadiusMetres, CommanderRecoveryMultiplier, CommanderPanicMultiplier |

Behavior curves interpolate at native morale 100/200/400/600/800. Regroup thresholds alone use the HUD percentage scale (899 is 100%). LowMoraleProfileShift accepts -19 to 0 and HighMoraleProfileShift accepts 0 to 19; the shipped settings remain -6/+6. Profile shifts are rounded and clamped within the unit's native profile family. They influence several native decisions together, rather than setting a universal block or dodge percentage. Units without an evasive ability do not acquire one.

Formation reshaping and local regrouping exclude fixed positions, player groups, commander attributes, explicit scenario targets, script-protected groups, and retreat/evacuation modes. Local regrouping additionally excludes named-officer groups. It temporarily changes only the individual controller's decision context and restores the group's strategic mode immediately; it does not issue a retreat or evacuation order. Five-second defeat shock can trigger regrouping below the 45% recovery threshold, subject to the same cooldown. High morale resists this shock; commander proximity halves it.

Morale belongs to the corps: nearby defeats determine whether panic applies, then its capped extra morale penalty affects that whole corps. The ordinary casualty penalty still applies independently. Scripted removals and commander defeat do not trigger this extra subordinate-defeat penalty. Commander proximity and recovery are sampled once per battle second from native group storage. Recovery writes permanent base morale while preserving temporary support, expiry, and reinforcement reward accounting. All timers pause with battle time and reset with the mod's battle ledger.

## Reinforcement settings

```ini
[Reinforcements]
Enabled=1
MoralePerTenPercent=10
WaveMoraleCap=50
WaveSeconds=3
```

The gain is `min(WaveMoraleCap, MoralePerTenPercent * 10 * arrivingSoldiers / soldiersBeforeWave)`. For 500 existing soldiers, 50 arrivals target +10 morale, 100 target +20, and 250 or more target +50. Fractional points are retained. Officers/captains are excluded from the soldier count. The bonus is permanent base morale and does not add a yellow segment.

Only the native Reinforce-to-Normal activation of a previously reserved group qualifies, once per group per battle. Actor visibility, healing, ordinary transfers, initial active groups and dead-group reactivation do not grant rewards. Rewards wait until the appearance completes and the quiet window passes; a stuck activation times out after 30 battle seconds without a grant. No additional native formation scan runs while no wave is open.

The native appearance effect does not itself call the morale-raise function, but scenarios can issue separate MoraleSet/MoraleAdd effects. To avoid stacking rewards, native positive morale changes within the recent arrival window and throughout an open wave count toward its target. A +20 target with +15 native credit adds only +5; a +70 native award is kept and adds zero. Attribution is conservative: unrelated positive native changes in that window can also reduce the top-up. Appearance and morale effects are also linked by scenario event and corps. An award from that same event is credited even when it precedes the arrival by more than the quiet window. If it arrives after a mod top-up was paid, it replaces that top-up up to the amount previously paid, preserving the larger native reward. Unrelated later events retain their ordinary game behavior. All timers use battle time and pause with the battle.

## Surprise force settings

```ini
[SurpriseForces]
Enabled=1
```

This toggle is independent of Reinforcements.Enabled. A previously reserved native Surprise-to-Normal activation qualifies once per group per battle. When all pending groups in its arrival wave finish activation, the corps reaches maximum base morale on the next battle tick, without waiting for WaveSeconds. A mixed wave containing both ordinary reinforcements and surprise forces receives the maximum-morale top-up instead of an additional percentage reward. Morale is shared by the entire corps, including existing troops.

Earlier scripted gains are already included in the current base, so only the difference to 899 is added. Later gains use the same native ceiling. Appearance and morale effects are linked by scenario event and corps, as with ordinary reinforcements. If the same event grants morale after the corps has already taken losses, its actual gain replaces the earlier mod contribution up to the amount paid; it does not restore those losses a second time. Unrelated events and scripted reductions retain their ordinary effect, and the mod does not hold morale at maximum. Existing temporary support bonuses keep their original expiry, with no yellow segment visible while base morale is at maximum. Repeat activation, actor recreation, dead groups, corps/side transfers and failed appearance requests do not grant another top-up. The same 30-battle-second activation timeout applies. A distinct, previously reserved surprise group can trigger a later arrival.

## Controls and settings

Normally the HUD shows the nearest allied corps and the targeted/nearby enemy corps. F7 toggles an expanded view with six corps per page; Page Up and Page Down change pages. Subordinate named officers retain individual portraits. Vice Generals in each corps are combined into one native portrait with a `VG x count` label and are excluded from the generic captain count. The representative portrait belongs to the first surviving Vice General in that corps. All nine Vice General portrait assignments are shared with other game units; no exclusive Vice General portrait was found among the assigned assets. Generic troop captains are combined into one portrait with a `Ãƒâ€” count`; the count falls as their groups are defeated. Reinforcements are counted after entering the battle.

Edit `MoraleMatters.ini` with the game closed, then restart. Support duration, cooldown, distance, healing fraction, morale bonus, casualty losses, all combat multipliers, panel position, size, title font size (12-18 points), and colors are configurable. Individual gameplay sections and the HUD can be disabled. The update interval defaults to 500 ms. Set `General.Profile=1` to log average and peak update time for diagnosis.

On-screen damage scaling runs when a native weapon hit is calculated. The periodic character pass selects only the players and allied named officers needed for reflected Lua support queries; it does not modify damage attributes. HUD text, textures, and layout are updated only when their displayed values change; hidden expanded panels are created on first use.

Temporary bonuses, cooldowns, and reinforcement/event tracking are battle-local and are not serialized into suspend saves. Loading a suspend starts a fresh mod ledger; earlier event-credit history is not reconstructed. Permanent reinforcement gains are written to native corps morale.

## Uninstall

With the game closed, remove `MoraleMatters.asi`, `MoraleMatters.ini`, and the `ue4ss/Mods/MoraleMatters` directory. No original executable or game assets are patched on disk.

## Build from source

Requires Zig 0.15.2 and Python with `lupa` on Windows. The Lua regression runner also recognizes the existing development libraries under `build/LuaValidationLibs`. Pass `-PythonPath` if Python is not on PATH. From the game root:

```powershell
.\MoraleMatters\build.ps1 -ZigPath 'C:\path\to\zig.exe'
```

Add `-Install` to copy the built mod into this game installation while the game is closed; this replaces the installed INI with package defaults. The build runs the model and native callback assertions (including the per-hit combat suite) and unsupported-host guard check, then writes `MoraleMatters-v1.7.2.zip`. Source is in `src/` and `Scripts/`; the MinHook dependency and its license are in `vendor/minhook/`.

See `VERIFICATION.md` for the distinction between automated checks and in-game validation.
