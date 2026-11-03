MoraleMatters read-only compatibility checker 1.0.0
Windows x64 - no .NET installation or administrator access required

HOW TO RUN
1. Extract the ZIP into a writable folder, such as Downloads.
2. Double-click "Start Checker.cmd".
3. If asked, paste the full path to DW3CE_RE-Win64-Shipping.exe or the game
   installation folder. You can drag the shipping EXE into the console window.
   The EXE is normally in DW3CE_RE\Binaries\Win64. Do not select DW3CE_RE.exe.
4. Wait for the report. The checker will show the two report filenames.
5. Send both the .txt and .json reports from the Reports folder to the mod author.

If extracted into the game folder or Win64 folder, the checker can find the
EXE automatically. The game does not need to be running. No mod installation
is needed. The checker does not enable support for a different EXE.

WHAT IT DOES
- Opens the game EXE for reading, without loading or executing its code.
- Compares SHA-256 hashes, PE layout, every raw section and 64 KiB section blocks
  against an embedded fingerprint of the supported EXE.
- Checks all 48 production expected-code locations and additional fixed
  Image+hex-address references found in the current mod source (24 bytes each).
- Records versions and, if found next to this installation, Steam app/build IDs.
- Writes new text and JSON reports. Existing reports are never overwritten.

WHAT IS SHARED
Reports contain fingerprints, PE metadata, source filenames/RVAs identifying
code probes, and optional Steam app/build IDs. They do not contain game code
bytes, full local paths, usernames, Steam account IDs, save files or mod settings.
The checker does not upload anything, use the network, launch the game, inspect
running processes, install hooks or change game/mod files. Its bundled .NET
runtime can extract its own libraries into the normal temporary directory.

RESULTS
KNOWN_SUPPORTED_EXE
  Exact match for the EXE accepted by the mod. This does not test installed
  mods, runtime conflicts or gameplay.
SAME_SECTIONS_DIFFERENT_FILE_REVIEW_REQUIRED
  All raw sections, checked layout fields and short code probes match. The
  whole-file hash differs outside raw sections. Author review is still needed.
DIFFERENT_BUILD_REVIEW_REQUIRED
  Sections, layout or probes differ. The report identifies changed 64 KiB
  regions. Matching short probes does not prove compatibility. Do not bypass
  the mod's executable guard based on this report.

This is an offline comparison, not full function or object-layout verification.
Different on-disk packing can also require a separate loaded-memory check.
The tool is unsigned; any Windows publisher prompt will not name a verified
publisher. Obtain the package from the mod author.

COMMAND LINE (optional)
MoraleMatters-CompatibilityChecker.exe "D:\SteamLibrary\steamapps\common\GAME" --output "D:\Reports"
MoraleMatters-CompatibilityChecker.exe --help
Exit codes: 0 = reports created (read the verdict), 1 = failure, 2 = usage error.

FOR THE MOD AUTHOR
The embedded reference is created only from SHA-256:
b57a2d4cbd01c9c5abd651fa42f43a4b8fdae88da6403106cd5f5c1bf4dade70

Checker source is in CompatibilityChecker/. Run its build.ps1 with .NET SDK 10
to regenerate the hash-only reference and publish a portable Windows package.
The build script reads mod sources; it does not build, install or change the mod.
Run test_checker.py with Python, --checker pointing to the published EXE, and
--game pointing to the supported shipping EXE. Tests mutate temporary copies.
