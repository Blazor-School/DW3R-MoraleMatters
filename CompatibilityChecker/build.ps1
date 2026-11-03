param(
    [string]$GameExecutable = (Join-Path (Split-Path -Parent (Split-Path -Parent $PSScriptRoot)) 'DW3CE_RE\Binaries\Win64\DW3CE_RE-Win64-Shipping.exe')
)

$ErrorActionPreference = 'Stop'
$project = Join-Path $PSScriptRoot 'CompatibilityChecker.csproj'
$modRoot = Split-Path -Parent $PSScriptRoot
$buildRoot = Join-Path $modRoot 'build\CompatibilityChecker'
$publishRoot = Join-Path $buildRoot 'publish'
$packageRoot = Join-Path $buildRoot 'MoraleMatters-CompatibilityChecker'
New-Item -ItemType Directory -Path $buildRoot, $packageRoot -Force | Out-Null

& dotnet build $project -c Release --nologo
if ($LASTEXITCODE -ne 0) { throw 'Checker build failed.' }
$reference = Join-Path $buildRoot ('reference-' + [Guid]::NewGuid().ToString('N') + '.json')
& dotnet (Join-Path $PSScriptRoot 'bin\Release\net10.0\MoraleMatters-CompatibilityChecker.dll') --create-reference $GameExecutable --source-root $modRoot --output $reference
if ($LASTEXITCODE -ne 0) { throw 'Reference generation failed; the supported EXE and current production probes must match.' }
Copy-Item -LiteralPath $reference -Destination (Join-Path $PSScriptRoot 'reference.json') -Force

& dotnet publish $project -c Release -r win-x64 --self-contained true -p:PublishSingleFile=true -p:EnableCompressionInSingleFile=true -p:IncludeNativeLibrariesForSelfExtract=true -p:DebugType=None -p:DebugSymbols=false -o $publishRoot --nologo
if ($LASTEXITCODE -ne 0) { throw 'Portable checker publish failed.' }
Copy-Item -LiteralPath (Join-Path $publishRoot 'MoraleMatters-CompatibilityChecker.exe') -Destination $packageRoot -Force
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'Start Checker.cmd'), (Join-Path $PSScriptRoot 'README.txt') -Destination $packageRoot -Force
$archive = Join-Path $buildRoot 'MoraleMatters-CompatibilityChecker-v1.0.0-win-x64.zip'
Compress-Archive -LiteralPath (Join-Path $packageRoot 'MoraleMatters-CompatibilityChecker.exe'), (Join-Path $packageRoot 'Start Checker.cmd'), (Join-Path $packageRoot 'README.txt') -DestinationPath $archive -Force
Get-Item -LiteralPath $archive | Select-Object FullName, Length
Get-FileHash -LiteralPath $archive -Algorithm SHA256 | Format-List
