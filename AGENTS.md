# Morale Matters project context

Morale Matters is the development baseline for this project. It is a Windows x64 mod for **DYNASTY WARRIORS 3 Complete Edition Remastered**. **Morale Matters UI** is its companion .NET MAUI Blazor Hybrid desktop app. Both live in this repository. Use the current working tree to understand in-progress development. The shipped archive, deployed files and verification records can reflect different revisions.

## Gameplay

- Corps morale uses the game's 1–899 scale. Five configurable anchors interpolate damage dealt, damage taken and AI attack retry frequency for both armies. Named officers have adjustable effect strength. Player characters are exempt from their own morale multipliers.
- Morale affects aggression, guarding, evasion and formation spacing. Low-morale infantry can fall back and reform. In the 1.9.0 corps break and rally rules, ordinary troops stop requesting attack tickets at Very Low morale. Some slump and others flee and surrender, then rally in waves after morale recovers. Named officers, including Vice Generals, and corps commanders retain their combat behavior.
- Casualties and lost leaders lower morale and can spread local panic. Commander proximity helps stabilize troops and improves out-of-combat recovery. Approaching allied officers can heal them and temporarily raise their corps morale. Nearby enemy leaders can support one another's corps.
- Off-screen battles share damage with officers and award morale for confirmed kills. Reinforcements and surprise arrivals can raise base morale. Reward logic accounts for native awards. The mod also adjusts random item drops and ordinary archers' targeting and on-screen damage taken.
- Blocked player melee hits build guard pressure. A guard break creates an opening and can briefly disrupt nearby ordinary infantry. Native floating HP gauges host the guard shields and break indicator, with optional blocked-hit and break sounds. Guard pressure and its feedback can run independently of the morale combat settings and corps HUD.
- The in-game HUD shows corps morale, officer portraits and health, morale changes, support, and regroup or rally phases. Distress and rescue objectives reward stabilizing endangered allied corps. Battle reports summarize support and can be written to text. F7 toggles the all-corps panels and F8 toggles the report by default.
- Officer and bodyguard weapon fusion allow matching numeric skills to increase through repeated fusion. A higher donor value replaces the destination value, while a lower or equal donor increments it by a configurable amount. The bodyguard path uses the native fusion screens from Camp Equipment.

## Runtime and source map

- `src/Plugin.cpp` reads the INI and checks the game executable against its supported SHA-256 hashes and image sizes, then validates the hook bytes listed in `src/ExpectedBytes.h`. `src/Game.cpp` and its included `.inc` files implement combat, morale, AI, guard pressure and fusion using MinHook. The model headers contain the corresponding gameplay rules and battle state.
- `Scripts/main.lua` is the UE4SS entry point. It calls `MoraleMatters_Query` in `src/LuaBridge.cpp` to access native state and operations. `Experience.lua` covers distress, rescue and reports, `GuardFeedback.lua` renders guard feedback, and `InfantryPose.lua` and `InfantrySlump.lua` handle infantry poses. `WeaponFusion.lua` integrates the Camp fusion menu. `Scripts/Assets/` holds guard feedback assets. `AnimationAssets/` contains the infantry animation packs and authoring sources.
- Lua uses retained UE4SS game-thread timers and foreground game-thread hotkey polling. `VERIFICATION.md` records a Lua stability regression involving async callbacks. The HUD path checks for `LoopInGameThreadWithDelay` and `ExecuteInGameThreadWithDelay` at startup.
- The deployed `MoraleMatters.asi` and `MoraleMatters.ini` sit in `../DW3CE_RE/Binaries/Win64/`. The installed Lua is in `ue4ss/Mods/MoraleMatters/Scripts/` under that folder. Animation packs go in `../DW3CE_RE/Content/Paks/`. Runtime diagnostics include `MoraleMatters.log`, `ue4ss/UE4SS.log`, and the optional `MoraleMatters-last-battle.txt` report.

## Morale Matters UI

- The solution is `MoraleMattersUI/MoraleMatterApp/MoraleMatterApp.slnx`. The project is `MoraleMattersUI/MoraleMatterApp/MoraleMattersUI/MoraleMattersUI.csproj`, targeting .NET 10 on Windows. `MainPage.xaml` hosts the Blazor WebView and `Components/Pages/Home.razor` contains the main installer and settings page, using RoarUI components.
- `Forms/GameFolderForm.cs` validates the chosen `Win64` folder using the game's shipping `.exe` and `.tnk` files. `Services/ModService.cs` maps known ASI hashes to mod versions, fetches the `mod` release feed through Velopack, installs the release ZIP and writes settings. Unknown ASI builds may appear as uninstalled. `Services/AppUpdateService.cs` handles desktop app updates, which have their own version and release channel.
- `Forms/ModSettingForm.cs` defines, validates and loads the settings. `ModService.cs` maps property-name prefixes to INI sections when saving. When working on a setting, check the shipped `MoraleMatters.ini`, its native or Lua reader, the form's defaults and loader, the Razor controls, and the INI writer. The installed INI can contain custom values. Installing a release overwrites it with the release defaults. Saving settings requires a game restart for them to take effect.

## Build and verification

- `build.ps1 -ZigPath <path-to-zig.exe>` builds and tests the native plugin and creates the mod ZIP. It runs native model and integration tests plus Python-driven Lua and asset checks. `-Install` copies the package into the live game, including the default INI.
- `tests/GuardPressureSettingsCheck.csproj` exercises the desktop settings and installed-version detection code. The UI uses the .NET 10 SDK and Windows MAUI workload. `pack.bat` in the UI project publishes the Windows x64 app and creates its Velopack installer.
- `README.md` is the player-facing feature and setup guide. `VERIFICATION.md` records implementation history, automated checks and live observations. Check the current source and the relevant build or runtime evidence before relying on an older verification entry.
