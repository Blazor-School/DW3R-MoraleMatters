@echo off
setlocal
rem Run pack.bat --no-pause from a terminal to skip the final pause.
pushd "%~dp0"
if errorlevel 1 exit /b 1

set "PACK_PROJECT=MoraleMattersUI.csproj"
set "PACK_FRAMEWORK=net10.0-windows10.0.19041.0"
set "PACK_APP_VERSION="
set "PACK_VELOPACK_VERSION="
set "PACK_EXIT_CODE=1"

where dotnet >nul 2>&1
if errorlevel 1 (
    echo ERROR: Install the .NET 10 SDK and the MAUI Windows workload first.
    goto :finish
)

if not exist "bin" mkdir "bin"
if errorlevel 1 goto :finish

echo Reading packaging versions from %PACK_PROJECT%...
dotnet msbuild "%PACK_PROJECT%" -nologo -p:TargetFramework=%PACK_FRAMEWORK% -p:Configuration=Release -getProperty:ApplicationDisplayVersion -getItem:PackageReference > "bin\pack-metadata.json"
if errorlevel 1 (
    type "bin\pack-metadata.json"
    goto :finish
)

powershell -NoProfile -Command "$ErrorActionPreference = 'Stop'; $metadata = Get-Content -LiteralPath 'bin\pack-metadata.json' -Raw | ConvertFrom-Json; $version = $metadata.Properties.ApplicationDisplayVersion; $velopackVersion = ($metadata.Items.PackageReference | Where-Object Identity -eq 'Velopack').Version; if ([string]::IsNullOrWhiteSpace($version) -or [string]::IsNullOrWhiteSpace($velopackVersion)) { throw 'ApplicationDisplayVersion or the Velopack package version is missing.' }; 'PACK_APP_VERSION=' + $version; 'PACK_VELOPACK_VERSION=' + $velopackVersion" > "bin\pack-metadata.env"
if errorlevel 1 goto :finish
for /f "usebackq tokens=1,* delims==" %%A in ("bin\pack-metadata.env") do set "%%A=%%B"

set "PACK_TOOLS_DIR=%CD%\bin\pack-tools\%PACK_VELOPACK_VERSION%"
set "PACK_PUBLISH_DIR=%CD%\bin\publish\win-x64"
set "PACK_OUTPUT_DIR=%CD%\bin\velopack"

if exist "%PACK_TOOLS_DIR%\vpk.exe" goto :publish
echo Installing Velopack CLI %PACK_VELOPACK_VERSION% locally...
dotnet tool install vpk --tool-path "%PACK_TOOLS_DIR%" --version "%PACK_VELOPACK_VERSION%"
if errorlevel 1 goto :finish

:publish
rem Rebuild generated publish/package output so the same version can be packed again.
echo Clearing previous generated publish and installer output...
powershell -NoProfile -Command "$ErrorActionPreference = 'Stop'; $root = (Get-Location).ProviderPath; $binRoot = [IO.Path]::GetFullPath((Join-Path $root 'bin')) + [IO.Path]::DirectorySeparatorChar; foreach ($relative in @('bin\publish\win-x64', 'bin\velopack')) { $target = [IO.Path]::GetFullPath((Join-Path $root $relative)); if (-not $target.StartsWith($binRoot, [StringComparison]::OrdinalIgnoreCase)) { throw 'Generated output must stay inside the project bin folder.' }; if (Test-Path -LiteralPath $target) { Remove-Item -LiteralPath $target -Recurse -Force } }"
if errorlevel 1 goto :finish

echo Publishing Morale Matters UI %PACK_APP_VERSION% for Windows x64...
dotnet publish "%PACK_PROJECT%" -t:Rebuild -f %PACK_FRAMEWORK% -p:TargetFrameworks=%PACK_FRAMEWORK% -c Release -r win-x64 --self-contained true -p:Version="%PACK_APP_VERSION%" -p:WindowsPackageType=None -p:WindowsAppSDKSelfContained=true -o "%PACK_PUBLISH_DIR%"
if errorlevel 1 goto :finish

echo Creating the Velopack installer for version %PACK_APP_VERSION%...
"%PACK_TOOLS_DIR%\vpk.exe" pack --packId MoraleMattersUI --packVersion "%PACK_APP_VERSION%" --packDir "%PACK_PUBLISH_DIR%" --mainExe MoraleMattersUI.exe --packTitle "Morale Matters UI" --icon "%PACK_PUBLISH_DIR%\appicon.ico" --runtime win-x64 --channel ui-win-x64 --framework webview2 --noPortable --outputDir "%PACK_OUTPUT_DIR%"
if errorlevel 1 goto :finish
rem Some Velopack failures return exit code zero, so also check its output.
if not exist "%PACK_OUTPUT_DIR%\MoraleMattersUI-ui-win-x64-Setup.exe" goto :finish
if not exist "%PACK_OUTPUT_DIR%\releases.ui-win-x64.json" goto :finish
if not exist "%PACK_OUTPUT_DIR%\MoraleMattersUI-%PACK_APP_VERSION%-ui-win-x64-full.nupkg" goto :finish

rem Manual GitHub uploads do not need Velopack's deployment metadata or legacy feed.
powershell -NoProfile -Command "$ErrorActionPreference = 'Stop'; Remove-Item -LiteralPath 'bin\velopack\assets.ui-win-x64.json', 'bin\velopack\RELEASES-ui-win-x64' -Force"
if errorlevel 1 goto :finish

echo.
echo Packaging complete. Upload all three files from this folder to GitHub Releases:
echo %PACK_OUTPUT_DIR%
set "PACK_EXIT_CODE=0"

:finish
if not "%PACK_EXIT_CODE%"=="0" echo ERROR: Packaging failed. See the output above.
if /i not "%~1"=="--no-pause" pause
popd
exit /b %PACK_EXIT_CODE%
