param(
    [Parameter(Mandatory = $true)]
    [string]$ZigPath,
    [string]$PythonPath = 'python',
    [switch]$Install
)

$ErrorActionPreference = 'Stop'
$projectRoot = $PSScriptRoot
$compiler = (Resolve-Path -LiteralPath $ZigPath).Path
$buildRoot = Join-Path $projectRoot 'build'
$packageRoot = Join-Path $projectRoot 'package'
$packageBin = Join-Path $packageRoot 'DW3CE_RE\Binaries\Win64'
$packageMod = Join-Path $packageBin 'ue4ss\Mods\MoraleMatters'
$packagePaks = Join-Path $packageRoot 'DW3CE_RE\Content\Paks'
$sourceEncoding = [Text.UTF8Encoding]::new($false, $true)
foreach ($scriptFile in Get-ChildItem -LiteralPath (Join-Path $projectRoot 'Scripts') -Filter '*.lua' -File -Recurse) {
    [void]$sourceEncoding.GetString([IO.File]::ReadAllBytes($scriptFile.FullName))
}
New-Item -ItemType Directory -Path $buildRoot, $packageBin, $packageMod, $packagePaks -Force | Out-Null
$animationPacks = @('pak', 'ucas', 'utoc') | ForEach-Object { Join-Path $projectRoot "AnimationAssets\MoraleStationary_P.$_" }

function Invoke-Compiler([string[]]$CompilerArguments) {
    & $compiler @CompilerArguments
    if ($LASTEXITCODE -ne 0) { throw "Compiler exited with code $LASTEXITCODE" }
}

$flags = @('-target', 'x86_64-windows-gnu', '-O2', '-DNDEBUG', '-DWIN32_LEAN_AND_MEAN', '-DNOMINMAX', '-I', (Join-Path $projectRoot 'vendor\minhook\include'), '-I', (Join-Path $projectRoot 'src'))
$objects = @()
foreach ($source in @('buffer.c', 'hook.c', 'trampoline.c', 'hde\hde64.c')) {
    $object = Join-Path $buildRoot (([IO.Path]::GetFileNameWithoutExtension($source)) + '.o')
    Invoke-Compiler (@('cc') + $flags + @('-c', (Join-Path $projectRoot "vendor\minhook\src\$source"), '-o', $object))
    $objects += $object
}
$hookObjects = @($objects)
foreach ($source in @('Plugin', 'Game', 'LuaBridge')) {
    $object = Join-Path $buildRoot "$source.o"
    Invoke-Compiler (@('c++') + $flags + @('-std=c++17', '-fno-exceptions', '-fno-rtti', '-Wall', '-Wextra', '-Werror', '-c', (Join-Path $projectRoot "src\$source.cpp"), '-o', $object))
    $objects += $object
}
$dll = Join-Path $buildRoot 'MoraleMatters.dll'
Invoke-Compiler (@('cc') + $flags + @('-shared', '-s', '-o', $dll) + $objects + @('-lbcrypt', '-luser32', '-lwinmm'))
Copy-Item -LiteralPath $dll -Destination (Join-Path $packageBin 'MoraleMatters.asi') -Force

$guardSettingsTests = Join-Path $buildRoot 'GuardPressureSettings.exe'
Invoke-Compiler (@('c++') + $flags + @('-std=c++17', '-Wall', '-Wextra', '-Werror', '-static', '-o', $guardSettingsTests, (Join-Path $projectRoot 'tests\GuardPressureSettings.cpp'), (Join-Path $buildRoot 'Game.o'), (Join-Path $buildRoot 'LuaBridge.o')) + $hookObjects + @('-lbcrypt', '-luser32', '-lwinmm'))
& $guardSettingsTests
if ($LASTEXITCODE -ne 0) { throw 'Guard pressure INI parser tests failed' }

$tests = Join-Path $buildRoot 'ModelTests.exe'
Invoke-Compiler (@('c++') + $flags + @('-std=c++17', '-Wall', '-Wextra', '-Werror', '-static', '-o', $tests, (Join-Path $projectRoot 'tests\ModelTests.cpp')))
& $tests
if ($LASTEXITCODE -ne 0) { throw 'Model tests failed' }
$reinforcementTests = Join-Path $buildRoot 'ReinforcementIntegration.exe'
Invoke-Compiler (@('c++') + $flags + @('-std=c++17', '-Wall', '-Wextra', '-Werror', '-static', '-o', $reinforcementTests, (Join-Path $projectRoot 'tests\ReinforcementIntegration.cpp')) + $hookObjects)
& $reinforcementTests
if ($LASTEXITCODE -ne 0) { throw 'Reinforcement integration tests failed' }
& $reinforcementTests patch110
if ($LASTEXITCODE -ne 0) { throw 'Reinforcement integration tests failed on the patch 1.1.0 layout' }
$behaviorTests = Join-Path $buildRoot 'BehaviorIntegration.exe'
Invoke-Compiler (@('c++') + $flags + @('-std=c++17', '-Wall', '-Wextra', '-Werror', '-static', '-o', $behaviorTests, (Join-Path $projectRoot 'tests\BehaviorIntegration.cpp')) + $hookObjects)
& $behaviorTests
if ($LASTEXITCODE -ne 0) { throw 'Behavior integration tests failed' }
& $behaviorTests patch110
if ($LASTEXITCODE -ne 0) { throw 'Behavior integration tests failed on the patch 1.1.0 layout' }
$experienceTests = Join-Path $buildRoot 'ExperienceIntegration.exe'
Invoke-Compiler (@('c++') + $flags + @('-std=c++17', '-Wall', '-Wextra', '-Werror', '-static', '-o', $experienceTests, (Join-Path $projectRoot 'tests\ExperienceIntegration.cpp')) + $hookObjects)
& $experienceTests
if ($LASTEXITCODE -ne 0) { throw 'Battlefield experience integration tests failed' }
& $experienceTests patch110
if ($LASTEXITCODE -ne 0) { throw 'Battlefield experience integration tests failed on the patch 1.1.0 layout' }
$killRewardTests = Join-Path $buildRoot 'KillRewardIntegration.exe'
Invoke-Compiler (@('c++') + $flags + @('-std=c++17', '-Wall', '-Wextra', '-Werror', '-static', '-o', $killRewardTests, (Join-Path $projectRoot 'tests\KillRewardIntegration.cpp')) + $hookObjects)
& $killRewardTests
if ($LASTEXITCODE -ne 0) { throw 'Off-screen kill reward integration tests failed' }
& $killRewardTests patch110
if ($LASTEXITCODE -ne 0) { throw 'Off-screen kill reward integration tests failed on the patch 1.1.0 layout' }
$proximityTests = Join-Path $buildRoot 'ProximityIntegration.exe'
Invoke-Compiler (@('c++') + $flags + @('-std=c++17', '-Wall', '-Wextra', '-Werror', '-static', '-o', $proximityTests, (Join-Path $projectRoot 'tests\ProximityIntegration.cpp')) + $hookObjects)
& $proximityTests
if ($LASTEXITCODE -ne 0) { throw 'Enemy proximity integration tests failed' }
& $proximityTests patch110
if ($LASTEXITCODE -ne 0) { throw 'Enemy proximity integration tests failed on the patch 1.1.0 layout' }
$probe = Join-Path $buildRoot 'HostProbe.exe'
$onscreenTests = Join-Path $buildRoot 'OnscreenIntegration.exe'
Invoke-Compiler (@('c++') + $flags + @('-std=c++17', '-Wall', '-Wextra', '-Werror', '-static', '-o', $onscreenTests, (Join-Path $projectRoot 'tests\OnscreenIntegration.cpp')) + $hookObjects)
& $onscreenTests
if ($LASTEXITCODE -ne 0) { throw 'On-screen combat integration tests failed' }
& $onscreenTests patch110
if ($LASTEXITCODE -ne 0) { throw 'On-screen combat integration tests failed on the patch 1.1.0 layout' }
$nativeDamageTests = Join-Path $buildRoot 'NativeDamageIntegration.exe'
$nativeExecutable = Join-Path (Split-Path -Parent $projectRoot) 'DW3CE_RE\Binaries\Win64\DW3CE_RE-Win64-Shipping.exe'
if (Test-Path -LiteralPath $nativeExecutable) {
    Invoke-Compiler (@('c++') + $flags + @('-std=c++17', '-Wall', '-Wextra', '-Werror', '-static', '-o', $nativeDamageTests, (Join-Path $projectRoot 'tests\NativeDamageIntegration.cpp')) + $hookObjects)
    & $nativeDamageTests $nativeExecutable
    if ($LASTEXITCODE -ne 0) { throw 'Native executable damage formula tests failed' }
    $offscreenTests = Join-Path $buildRoot 'OffscreenDefeatIntegration.exe'
    Invoke-Compiler (@('c++') + $flags + @('-std=c++17', '-Wall', '-Wextra', '-Werror', '-static', '-o', $offscreenTests, (Join-Path $projectRoot 'tests\OffscreenDefeatIntegration.cpp')) + $hookObjects)
    & $offscreenTests $nativeExecutable
    if ($LASTEXITCODE -ne 0) { throw 'Native off-screen defeat and save/load tests failed' }
    $nativeDropTests = Join-Path $buildRoot 'NativeDropIntegration.exe'
    Invoke-Compiler (@('c++') + $flags + @('-std=c++17', '-Wall', '-Wextra', '-Werror', '-static', '-o', $nativeDropTests, (Join-Path $projectRoot 'tests\NativeDropIntegration.cpp')) + $hookObjects)
    & $nativeDropTests $nativeExecutable
    if ($LASTEXITCODE -ne 0) { throw 'Native executable drop selection tests failed' }
    $weaponFusionTests = Join-Path $buildRoot 'WeaponFusionIntegration.exe'
    Invoke-Compiler (@('c++') + $flags + @('-std=c++17', '-Wall', '-Wextra', '-Werror', '-static', '-o', $weaponFusionTests, (Join-Path $projectRoot 'tests\WeaponFusionIntegration.cpp')) + $hookObjects)
    & $weaponFusionTests $nativeExecutable
    if ($LASTEXITCODE -ne 0) { throw 'Native weapon fusion integration tests failed' }
    $archerTests = Join-Path $buildRoot 'ArcherIntegration.exe'
    Invoke-Compiler (@('c++') + $flags + @('-std=c++17', '-Wall', '-Wextra', '-Werror', '-static', '-o', $archerTests, (Join-Path $projectRoot 'tests\ArcherIntegration.cpp')) + $hookObjects)
    & $archerTests $nativeExecutable
    if ($LASTEXITCODE -ne 0) { throw 'Native archer targeting and damage integration tests failed' }
    $guardTests = Join-Path $buildRoot 'GuardPressureIntegration.exe'
    Invoke-Compiler (@('c++') + $flags + @('-std=c++17', '-Wall', '-Wextra', '-Werror', '-static', '-o', $guardTests, (Join-Path $projectRoot 'tests\GuardPressureIntegration.cpp')) + $hookObjects)
    & $guardTests $nativeExecutable
    if ($LASTEXITCODE -ne 0) { throw 'Native guard pressure integration tests failed' }

}
& $PythonPath (Join-Path $projectRoot 'tests\LuaExperienceTests.py')
if ($LASTEXITCODE -ne 0) { throw 'Lua battlefield/HUD integration tests failed (requires Python and lupa)' }
& $PythonPath (Join-Path $projectRoot 'tests\LuaGuardFeedbackTests.py')
if ($LASTEXITCODE -ne 0) { throw 'Lua guard pressure presentation tests failed' }
& $PythonPath (Join-Path $projectRoot 'tests\LuaInfantryTests.py')
if ($LASTEXITCODE -ne 0) { throw 'Lua infantry break and recovery tests failed' }
& $PythonPath (Join-Path $projectRoot 'tests\GuardFeedbackAssets.py')
if ($LASTEXITCODE -ne 0) { throw 'Guard shield/audio assets are missing or invalid' }
Invoke-Compiler (@('c++') + $flags + @('-std=c++17', '-static', '-municode', '-o', $probe, (Join-Path $projectRoot 'tests\HostProbe.cpp')))
& $probe $dll
if ($LASTEXITCODE -ne 0) { throw 'Host guard test failed' }

Copy-Item -LiteralPath (Join-Path $projectRoot 'MoraleMatters.ini') -Destination $packageBin -Force
Copy-Item -LiteralPath (Join-Path $projectRoot 'Scripts') -Destination $packageMod -Recurse -Force
Copy-Item -LiteralPath $animationPacks -Destination $packagePaks -Force
Set-Content -LiteralPath (Join-Path $packageMod 'enabled.txt') -Value '' -NoNewline
foreach ($document in @('README.md', 'VERIFICATION.md', 'LICENSE.txt')) {
    if (Test-Path -LiteralPath (Join-Path $projectRoot $document)) { Copy-Item -LiteralPath (Join-Path $projectRoot $document) -Destination $packageRoot -Force }
}
Copy-Item -LiteralPath (Join-Path $projectRoot 'vendor\minhook\LICENSE.txt') -Destination (Join-Path $packageRoot 'MinHook-LICENSE.txt') -Force
$archive = Join-Path $projectRoot 'MoraleMatters-v1.9.0.zip'
Compress-Archive -Path (Join-Path $packageRoot '*') -DestinationPath $archive -Force
if ($Install) {
    if (Get-Process -Name 'DW3CE_RE-Win64-Shipping' -ErrorAction SilentlyContinue) { throw 'Close the game before installing.' }
    $gameBin = Join-Path (Split-Path -Parent $projectRoot) 'DW3CE_RE\Binaries\Win64'
    Copy-Item -LiteralPath (Join-Path $packageBin 'MoraleMatters.asi'), (Join-Path $packageBin 'MoraleMatters.ini') -Destination $gameBin -Force
    Copy-Item -LiteralPath (Join-Path $packageMod 'Scripts') -Destination (Join-Path $gameBin 'ue4ss\Mods\MoraleMatters') -Recurse -Force
    $gamePaks = Join-Path $gameBin '..\..\Content\Paks'
    Copy-Item -LiteralPath $animationPacks -Destination $gamePaks -Force
    Set-Content -LiteralPath (Join-Path $gameBin 'ue4ss\Mods\MoraleMatters\enabled.txt') -Value '' -NoNewline
}
Write-Output "Built and tested: $archive"
