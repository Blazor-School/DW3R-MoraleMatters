using System.Diagnostics;
using System.Reflection;
using System.Reflection.PortableExecutable;
using System.Security.Cryptography;
using System.Text;
using System.Text.Json;
using System.Text.RegularExpressions;

internal static class Program
{
    internal const string SupportedHash = "b57a2d4cbd01c9c5abd651fa42f43a4b8fdae88da6403106cd5f5c1bf4dade70";
    internal const string GameName = "DW3CE_RE-Win64-Shipping.exe";
    internal const int BlockSize = 65536;
    private static readonly JsonSerializerOptions JsonOptions = new() { WriteIndented = true };

    private static int Main(string[] args)
    {
        bool interactive = args.Contains("--interactive");
        try
        {
            if (args.Contains("--help") || args.Contains("-h"))
            {
                Console.WriteLine("MoraleMatters read-only compatibility checker 1.0.0\nUsage: MoraleMatters-CompatibilityChecker.exe [EXE-or-game-folder] [--output report-folder] [--interactive]\nNo game code is executed. Only new report files are written.\nExit codes: 0 = report created (read its verdict); 1 = error; 2 = usage error.");
                return 0;
            }
            string? input = null, output = null, source = null;
            bool createReference = false;
            for (int i = 0; i < args.Length; ++i)
            {
                switch (args[i])
                {
                    case "--interactive": break;
                    case "--create-reference": createReference = true; break;
                    case "--output": output = Next(args, ref i); break;
                    case "--source-root": source = Next(args, ref i); break;
                    default:
                        if (args[i].StartsWith('-') || input != null) throw new ArgumentException("Unrecognized or extra argument. Use --help.");
                        input = args[i];
                        break;
                }
            }
            Console.WriteLine("MoraleMatters read-only compatibility checker 1.0.0");
            Console.WriteLine("Reads the EXE; never launches the game, installs hooks, or changes game/mod files.\n");
            if (createReference)
            {
                if (input == null || output == null || source == null) throw new ArgumentException("Reference generation requires EXE, --source-root and --output (a new JSON file).");
                byte[] referenceBytes = ReadFile(input);
                if (Hash(referenceBytes) != SupportedHash) throw new InvalidDataException("Refusing to generate a baseline from an unsupported EXE.");
                var probes = CollectProbes(source);
                Snapshot snapshot = Inspect(referenceBytes, input, probes, true);
                if (snapshot.Probes.Any(p => p.Match != true)) throw new InvalidDataException("Production expected bytes do not match the supported EXE.");
                WriteNew(output, JsonSerializer.Serialize(snapshot, JsonOptions));
                Console.WriteLine($"Reference created: {output}");
                return 0;
            }
            if (source != null) throw new ArgumentException("--source-root is only for reference generation.");
            string? executable = input != null ? FindGame(input) : FindNearbyGame();
            if (executable == null && interactive)
            {
                Console.WriteLine("Paste the full path to the shipping EXE or game folder, then press Enter.");
                Console.WriteLine("You can also drag the EXE into this window. Do not select the small DW3CE_RE.exe launcher.");
                string entered = (Console.ReadLine() ?? "").Trim().Trim('"');
                if (entered.Length != 0) executable = FindGame(entered);
            }
            if (executable == null) throw new ArgumentException("Game EXE not found. Supply its path or the game installation folder.");
            using Stream referenceStream = Assembly.GetExecutingAssembly().GetManifestResourceStream("MoraleMatters.Reference.json")
                ?? throw new InvalidDataException("This checker has no embedded reference; use the published package.");
            Snapshot reference = JsonSerializer.Deserialize<Snapshot>(referenceStream) ?? throw new InvalidDataException("Invalid embedded reference.");
            if (reference.Sha256 != SupportedHash || reference.Probes.Count == 0) throw new InvalidDataException("Incorrect embedded reference.");
            Console.WriteLine($"Reading {GameName} ...");
            var probeDefinitions = reference.Probes.Select(p => new ProbeDefinition { Rva = p.Rva, ExpectedSha256 = p.ExpectedSha256, Sources = p.Sources }).ToList();
            Snapshot actual = Inspect(ReadFile(executable), executable, probeDefinitions);
            Report report = Compare(reference, actual);
            report.Steam = FindSteamBuild(executable);
            string reportFolder = Path.GetFullPath(output ?? Path.Combine(AppContext.BaseDirectory, "Reports"));
            Directory.CreateDirectory(reportFolder);
            string prefix = "MoraleMatters-compatibility-" + DateTime.UtcNow.ToString("yyyyMMdd-HHmmss") + "-" + Guid.NewGuid().ToString("N")[..8];
            string jsonPath = Path.Combine(reportFolder, prefix + ".json");
            string textPath = Path.Combine(reportFolder, prefix + ".txt");
            WriteNew(jsonPath, JsonSerializer.Serialize(report, JsonOptions));
            WriteNew(textPath, Summary(report));
            Console.WriteLine(Summary(report));
            Console.WriteLine($"Reports saved:\n{textPath}\n{jsonPath}\n\nSend both report files to the mod author. No upload happens automatically.");
            return 0;
        }
        catch (Exception ex) when (ex is IOException or InvalidDataException or UnauthorizedAccessException or ArgumentException or InvalidOperationException or BadImageFormatException or JsonException or OverflowException or System.Security.SecurityException)
        {
            Console.Error.WriteLine("Checker could not complete: " + ex.Message);
            return ex is ArgumentException ? 2 : 1;
        }
        finally
        {
            if (interactive && !Console.IsInputRedirected)
            {
                Console.WriteLine("\nPress Enter to close.");
                Console.ReadLine();
            }
        }
    }

    private static string Next(string[] args, ref int index)
    {
        if (++index >= args.Length) throw new ArgumentException("Missing option value.");
        return args[index];
    }

    private static byte[] ReadFile(string path)
    {
        using var file = new FileStream(path, FileMode.Open, FileAccess.Read, FileShare.Read);
        if (file.Length < 64 || file.Length > 536870912) throw new InvalidDataException("EXE must be between 64 bytes and 512 MiB.");
        byte[] bytes = new byte[checked((int)file.Length)];
        file.ReadExactly(bytes);
        return bytes;
    }

    private static string Hash(ReadOnlySpan<byte> bytes) => Convert.ToHexStringLower(SHA256.HashData(bytes));

    private static void WriteNew(string path, string text)
    {
        using var file = new FileStream(path, FileMode.CreateNew, FileAccess.Write, FileShare.None);
        using var writer = new StreamWriter(file, new UTF8Encoding(false));
        writer.Write(text);
    }

    private static string? FindGame(string path)
    {
        path = Path.GetFullPath(path.Trim().Trim('"'));
        if (File.Exists(path))
        {
            if (!Path.GetFileName(path).Equals(GameName, StringComparison.OrdinalIgnoreCase)) throw new ArgumentException($"Select {GameName}, not the launcher or another file.");
            return path;
        }
        if (!Directory.Exists(path)) return null;
        foreach (string relative in new[] { GameName, Path.Combine("Binaries", "Win64", GameName), Path.Combine("DW3CE_RE", "Binaries", "Win64", GameName) })
        {
            string candidate = Path.Combine(path, relative);
            if (File.Exists(candidate)) return candidate;
        }
        return null;
    }

    private static string? FindNearbyGame()
    {
        foreach (string start in new[] { AppContext.BaseDirectory, Environment.CurrentDirectory }.Distinct(StringComparer.OrdinalIgnoreCase))
        {
            DirectoryInfo? directory = new(start);
            for (int depth = 0; depth < 4 && directory != null; ++depth, directory = directory.Parent)
            {
                string? path = FindGame(directory.FullName);
                if (path != null) return path;
            }
        }
        return null;
    }

    internal static Snapshot Inspect(byte[] bytes, string path, List<ProbeDefinition> probes, bool generateReference = false)
    {
        using var stream = new MemoryStream(bytes, false);
        using var pe = new PEReader(stream);
        PEHeaders headers = pe.PEHeaders;
        PEHeader header = headers.PEHeader ?? throw new InvalidDataException("Missing PE optional header.");
        if (headers.CoffHeader.Machine != Machine.Amd64 || header.Magic != PEMagic.PE32Plus) throw new InvalidDataException("Expected a 64-bit AMD64 PE executable.");
        if (headers.CoffHeader.NumberOfSections is < 1 or > 96 || header.SizeOfHeaders < 64 || header.SizeOfHeaders > bytes.Length || header.SizeOfImage <= 0) throw new InvalidDataException("Invalid PE layout.");
        var snapshot = new Snapshot
        {
            FileName = GameName,
            FileSize = bytes.Length,
            Sha256 = Hash(bytes),
            Machine = "AMD64",
            ImageBase = header.ImageBase,
            SizeOfImage = header.SizeOfImage,
            EntryPointRva = header.AddressOfEntryPoint,
            SectionAlignment = header.SectionAlignment,
            FileAlignment = header.FileAlignment,
            SizeOfHeaders = header.SizeOfHeaders,
            CoffTimestamp = unchecked((uint)headers.CoffHeader.TimeDateStamp),
            CoffCharacteristics = (int)headers.CoffHeader.Characteristics,
            DllCharacteristics = (int)header.DllCharacteristics,
            Subsystem = (int)header.Subsystem
        };
        var version = FileVersionInfo.GetVersionInfo(path);
        snapshot.FileVersion = version.FileVersion;
        snapshot.ProductVersion = version.ProductVersion;
        DirectoryEntry[] entries = [header.ExportTableDirectory, header.ImportTableDirectory, header.ResourceTableDirectory, header.ExceptionTableDirectory, header.CertificateTableDirectory, header.BaseRelocationTableDirectory, header.DebugTableDirectory, header.CopyrightTableDirectory, header.GlobalPointerTableDirectory, header.ThreadLocalStorageTableDirectory, header.LoadConfigTableDirectory, header.BoundImportTableDirectory, header.ImportAddressTableDirectory, header.DelayImportTableDirectory, header.CorHeaderTableDirectory];
        snapshot.DataDirectories = entries.Select((entry, index) => new DataDirectory { Index = index, Address = unchecked((uint)entry.RelativeVirtualAddress), Size = entry.Size }).ToList();
        var ranges = new List<(int Start, int End)>();
        var virtualRanges = new List<(long Start, long End)>();
        foreach (SectionHeader section in headers.SectionHeaders)
        {
            int start = section.PointerToRawData, size = section.SizeOfRawData;
            long virtualEnd = (long)section.VirtualAddress + Math.Max(section.VirtualSize, size);
            if (start < 0 || size < 0 || (long)start + size > bytes.Length || section.VirtualAddress < 0 || section.VirtualSize < 0 || virtualEnd > header.SizeOfImage) throw new InvalidDataException("Section is outside the file or image.");
            if (size > 0)
            {
                if (start < header.SizeOfHeaders || ranges.Any(r => start < r.End && (long)start + size > r.Start)) throw new InvalidDataException("Overlapping raw sections.");
                ranges.Add((start, start + size));
            }
            if (virtualEnd > section.VirtualAddress)
            {
                if (virtualRanges.Any(r => section.VirtualAddress < r.End && virtualEnd > r.Start)) throw new InvalidDataException("Overlapping virtual sections.");
                virtualRanges.Add((section.VirtualAddress, virtualEnd));
            }
            var record = new SectionRecord { Name = section.Name, Rva = section.VirtualAddress, VirtualSize = section.VirtualSize, RawOffset = start, RawSize = size, Characteristics = unchecked((uint)section.SectionCharacteristics), Sha256 = Hash(bytes.AsSpan(start, size)) };
            for (int offset = 0; offset < size; offset += BlockSize) record.BlockHashes.Add(Hash(bytes.AsSpan(start + offset, Math.Min(BlockSize, size - offset))));
            snapshot.Sections.Add(record);
        }
        int cursor = 0;
        foreach (var range in ranges.OrderBy(r => r.Start))
        {
            if (cursor < range.Start) snapshot.OutsideSections.Add(new FileRegion { Offset = cursor, Size = range.Start - cursor, Sha256 = Hash(bytes.AsSpan(cursor, range.Start - cursor)) });
            cursor = range.End;
        }
        if (cursor < bytes.Length) snapshot.OutsideSections.Add(new FileRegion { Offset = cursor, Size = bytes.Length - cursor, Sha256 = Hash(bytes.AsSpan(cursor)) });
        foreach (ProbeDefinition probe in probes)
        {
            SectionRecord? section = snapshot.Sections.FirstOrDefault(s => probe.Rva >= s.Rva && (long)probe.Rva + 24 <= (long)s.Rva + s.RawSize);
            string? actualHash = section == null ? null : Hash(bytes.AsSpan(checked(section.RawOffset + probe.Rva - section.Rva), 24));
            string expectedHash = generateReference && probe.ExpectedSha256.Length == 0 ? actualHash ?? throw new InvalidDataException("Reference probe is outside raw sections.") : probe.ExpectedSha256;
            snapshot.Probes.Add(new ProbeResult { Rva = probe.Rva, ExpectedSha256 = expectedHash, Sources = probe.Sources, ActualSha256 = actualHash, Match = actualHash != null && actualHash == expectedHash });
        }
        return snapshot;
    }

    private static List<ProbeDefinition> CollectProbes(string root)
    {
        string sourceRoot = Path.Combine(root, "src");
        string expectedSource = File.ReadAllText(Path.Combine(sourceRoot, "ExpectedBytes.h"));
        var probes = new Dictionary<int, ProbeDefinition>();
        foreach (Match match in Regex.Matches(expectedSource, @"\{0x([0-9A-Fa-f]+),\s*\{([^}]+)\}"))
        {
            int rva = Convert.ToInt32(match.Groups[1].Value, 16);
            string expected = string.Concat(Regex.Matches(match.Groups[2].Value, @"0x([0-9A-Fa-f]{2})").Select(m => m.Groups[1].Value)).ToLowerInvariant();
            if (expected.Length != 48) throw new InvalidDataException("Expected a 24-byte production probe.");
            probes.Add(rva, new ProbeDefinition { Rva = rva, ExpectedSha256 = Hash(Convert.FromHexString(expected)), Sources = ["ExpectedBytes.h"] });
        }
        if (probes.Count != 48) throw new InvalidDataException("Expected 48 production probes. Review the baseline generator for source changes.");
        // Include helpers and interior return-address checks absent from ExpectedBytes.h.
        // Their expected hashes are filled from the independently hash-verified baseline.
        foreach (string file in Directory.EnumerateFiles(sourceRoot).Where(p => Path.GetExtension(p) is ".cpp" or ".inc" or ".h"))
        {
            foreach (Match match in Regex.Matches(File.ReadAllText(file), @"\bImage\s*\+\s*0x([0-9A-Fa-f]+)"))
            {
                int rva = Convert.ToInt32(match.Groups[1].Value, 16);
                if (!probes.TryGetValue(rva, out ProbeDefinition? probe))
                {
                    probe = new ProbeDefinition { Rva = rva };
                    probes.Add(rva, probe);
                }
                if (!probe.Sources.Contains(Path.GetFileName(file))) probe.Sources.Add(Path.GetFileName(file));
            }
        }
        return probes.Values.OrderBy(p => p.Rva).ToList();
    }

    internal static Report Compare(Snapshot reference, Snapshot actual)
    {
        var report = new Report { CreatedUtc = DateTime.UtcNow, ReferenceSha256 = reference.Sha256, Actual = actual };
        void Field(string name, object expected, object observed) { if (!expected.Equals(observed)) report.LayoutDifferences.Add(name); }
        Field("Machine", reference.Machine, actual.Machine);
        Field("ImageBase", reference.ImageBase, actual.ImageBase);
        Field("SizeOfImage", reference.SizeOfImage, actual.SizeOfImage);
        Field("EntryPointRva", reference.EntryPointRva, actual.EntryPointRva);
        Field("SectionAlignment", reference.SectionAlignment, actual.SectionAlignment);
        Field("FileAlignment", reference.FileAlignment, actual.FileAlignment);
        Field("SizeOfHeaders", reference.SizeOfHeaders, actual.SizeOfHeaders);
        Field("CoffCharacteristics", reference.CoffCharacteristics, actual.CoffCharacteristics);
        Field("DllCharacteristics", reference.DllCharacteristics, actual.DllCharacteristics);
        Field("Subsystem", reference.Subsystem, actual.Subsystem);
        Field("DataDirectories", JsonSerializer.Serialize(reference.DataDirectories), JsonSerializer.Serialize(actual.DataDirectories));
        Field("SectionCount", reference.Sections.Count, actual.Sections.Count);
        for (int i = 0; i < Math.Max(reference.Sections.Count, actual.Sections.Count); ++i)
        {
            SectionRecord? expected = reference.Sections.ElementAtOrDefault(i), observed = actual.Sections.ElementAtOrDefault(i);
            if (expected == null || observed == null)
            {
                report.SectionComparisons.Add(new SectionComparison { Index = i, Name = observed?.Name ?? expected!.Name, LayoutMatch = false, HashMatch = false });
                continue;
            }
            bool layoutMatch = expected.Name == observed.Name && expected.Rva == observed.Rva && expected.VirtualSize == observed.VirtualSize && expected.RawOffset == observed.RawOffset && expected.RawSize == observed.RawSize && expected.Characteristics == observed.Characteristics;
            var comparison = new SectionComparison { Index = i, Name = observed.Name, LayoutMatch = layoutMatch, HashMatch = expected.Sha256 == observed.Sha256, ReferenceSha256 = expected.Sha256 };
            for (int block = 0; block < Math.Max(expected.BlockHashes.Count, observed.BlockHashes.Count); ++block)
            {
                string? expectedHash = expected.BlockHashes.ElementAtOrDefault(block), actualHash = observed.BlockHashes.ElementAtOrDefault(block);
                if (expectedHash == actualHash) continue;
                int offset = block * BlockSize;
                comparison.ChangedBlocks.Add(new BlockDifference { SectionOffset = offset, ActualRva = offset < observed.RawSize ? observed.Rva + offset : null, ActualFileOffset = offset < observed.RawSize ? observed.RawOffset + offset : null, ActualSize = Math.Max(0, Math.Min(BlockSize, observed.RawSize - offset)), ReferenceSha256 = expectedHash, ActualSha256 = actualHash });
            }
            report.SectionComparisons.Add(comparison);
        }
        bool allLayouts = report.LayoutDifferences.Count == 0 && report.SectionComparisons.All(s => s.LayoutMatch);
        bool allSections = report.SectionComparisons.All(s => s.HashMatch);
        bool allProbes = actual.Probes.All(p => p.Match);
        report.Verdict = actual.Sha256 == reference.Sha256 ? "KNOWN_SUPPORTED_EXE" : allLayouts && allSections && allProbes ? "SAME_SECTIONS_DIFFERENT_FILE_REVIEW_REQUIRED" : "DIFFERENT_BUILD_REVIEW_REQUIRED";
        report.Explanation = report.Verdict switch
        {
            "KNOWN_SUPPORTED_EXE" => "The EXE exactly matches the build accepted by the mod. This offline check does not test installed mods or runtime conflicts.",
            "SAME_SECTIONS_DIFFERENT_FILE_REVIEW_REQUIRED" => "All raw sections, checked layout fields and code probes match, but the whole-file hash differs. Differences are outside raw sections. Author review is required before adding support.",
            _ => "Section contents, layout or code probes differ. Do not bypass the mod's executable guard based on this report. Matching short probes alone does not establish compatibility."
        };
        return report;
    }

    private static SteamBuild? FindSteamBuild(string executable)
    {
        try
        {
            DirectoryInfo? directory = new FileInfo(executable).Directory;
            for (int depth = 0; depth < 8 && directory != null; ++depth, directory = directory.Parent)
            {
                DirectoryInfo? common = directory.Parent;
                DirectoryInfo? steamapps = common?.Parent;
                if (common == null || steamapps == null || !string.Equals(common.Name, "common", StringComparison.OrdinalIgnoreCase) || !string.Equals(steamapps.Name, "steamapps", StringComparison.OrdinalIgnoreCase)) continue;
                foreach (string manifest in Directory.EnumerateFiles(steamapps.FullName, "appmanifest_*.acf", SearchOption.TopDirectoryOnly))
                {
                    if (new FileInfo(manifest).Length > 1048576) continue;
                    string text = File.ReadAllText(manifest);
                    string Value(string key) => Regex.Match(text, "\"" + key + "\"\\s*\"([^\"]*)\"", RegexOptions.IgnoreCase).Groups[1].Value;
                    if (Value("installdir").Equals(directory.Name, StringComparison.OrdinalIgnoreCase)) return new SteamBuild { AppId = Value("appid"), BuildId = Value("buildid") };
                }
            }
        }
        catch (Exception ex) when (ex is IOException or UnauthorizedAccessException or System.Security.SecurityException) { }
        return null;
    }

    private static string Summary(Report report)
    {
        var text = new StringBuilder();
        text.AppendLine("MoraleMatters read-only compatibility report 1.0.0");
        text.AppendLine($"UTC: {report.CreatedUtc:O}");
        text.AppendLine($"Verdict: {report.Verdict}");
        text.AppendLine(report.Explanation);
        text.AppendLine($"\nEXE SHA-256:       {report.Actual.Sha256}");
        text.AppendLine($"Supported SHA-256: {report.ReferenceSha256}");
        text.AppendLine($"File size: {report.Actual.FileSize}; File version: {report.Actual.FileVersion ?? "unavailable"}; Product version: {report.Actual.ProductVersion ?? "unavailable"}");
        text.AppendLine(report.Steam == null ? "Steam build ID: unavailable (manifest not found beside this installation)." : $"Steam app ID: {report.Steam.AppId}; Build ID: {report.Steam.BuildId}");
        text.AppendLine($"Image size: 0x{report.Actual.SizeOfImage:X}; Entry point RVA: 0x{report.Actual.EntryPointRva:X}");
        text.AppendLine($"Layout field differences: {(report.LayoutDifferences.Count == 0 ? "none" : string.Join(", ", report.LayoutDifferences))}");
        text.AppendLine($"Code probes: {report.Actual.Probes.Count(p => p.Match)}/{report.Actual.Probes.Count} match (24 bytes each; not full-function verification).");
        foreach (ProbeResult probe in report.Actual.Probes.Where(p => !p.Match)) text.AppendLine($"  Probe differs or is unavailable at RVA 0x{probe.Rva:X} ({string.Join(", ", probe.Sources)})");
        text.AppendLine("\nSections (block offsets refer to 64 KiB regions, not individual changed bytes):");
        foreach (SectionComparison section in report.SectionComparisons)
        {
            text.AppendLine($"  [{section.Index}] {section.Name}: layout={(section.LayoutMatch ? "match" : "DIFFERENT")}, SHA-256={(section.HashMatch ? "match" : "DIFFERENT")}, changed blocks={section.ChangedBlocks.Count}");
            foreach (BlockDifference block in section.ChangedBlocks.Take(12)) text.AppendLine($"    Section offset 0x{block.SectionOffset:X}, actual RVA {(block.ActualRva is int rva ? $"0x{rva:X}" : "absent")}");
            if (section.ChangedBlocks.Count > 12) text.AppendLine("    Remaining block differences are listed in the JSON report.");
        }
        text.AppendLine("\nLimits: offline file comparison only. No loaded-memory, gameplay, object-layout or runtime-conflict validation. No EXE bytes, full local paths, usernames, account IDs or save files are included. No network requests are made. The tool writes only new report files.");
        return text.ToString();
    }
}

internal sealed class Snapshot
{
    public int SchemaVersion { get; set; } = 1;
    public int BlockSize { get; set; } = Program.BlockSize;
    public string FileName { get; set; } = "";
    public int FileSize { get; set; }
    public string Sha256 { get; set; } = "";
    public string? FileVersion { get; set; }
    public string? ProductVersion { get; set; }
    public string Machine { get; set; } = "";
    public ulong ImageBase { get; set; }
    public int SizeOfImage { get; set; }
    public int EntryPointRva { get; set; }
    public int SectionAlignment { get; set; }
    public int FileAlignment { get; set; }
    public int SizeOfHeaders { get; set; }
    public uint CoffTimestamp { get; set; }
    public int CoffCharacteristics { get; set; }
    public int DllCharacteristics { get; set; }
    public int Subsystem { get; set; }
    public List<DataDirectory> DataDirectories { get; set; } = [];
    public List<SectionRecord> Sections { get; set; } = [];
    public List<FileRegion> OutsideSections { get; set; } = [];
    public List<ProbeResult> Probes { get; set; } = [];
}

internal sealed class DataDirectory
{
    public int Index { get; set; }
    public uint Address { get; set; }
    public int Size { get; set; }
}

internal sealed class SectionRecord
{
    public string Name { get; set; } = "";
    public int Rva { get; set; }
    public int VirtualSize { get; set; }
    public int RawOffset { get; set; }
    public int RawSize { get; set; }
    public uint Characteristics { get; set; }
    public string Sha256 { get; set; } = "";
    public List<string> BlockHashes { get; set; } = [];
}

internal sealed class FileRegion
{
    public int Offset { get; set; }
    public int Size { get; set; }
    public string Sha256 { get; set; } = "";
}

internal class ProbeDefinition
{
    public int Rva { get; set; }
    public string ExpectedSha256 { get; set; } = "";
    public List<string> Sources { get; set; } = [];
}

internal sealed class ProbeResult : ProbeDefinition
{
    public string? ActualSha256 { get; set; }
    public bool Match { get; set; }
}

internal sealed class Report
{
    public int SchemaVersion { get; set; } = 1;
    public string CheckerVersion { get; set; } = "1.0.0";
    public DateTime CreatedUtc { get; set; }
    public string ReferenceSha256 { get; set; } = "";
    public string Verdict { get; set; } = "";
    public string Explanation { get; set; } = "";
    public Snapshot Actual { get; set; } = new();
    public SteamBuild? Steam { get; set; }
    public List<string> LayoutDifferences { get; set; } = [];
    public List<SectionComparison> SectionComparisons { get; set; } = [];
}

internal sealed class SteamBuild
{
    public string AppId { get; set; } = "";
    public string BuildId { get; set; } = "";
}

internal sealed class SectionComparison
{
    public int Index { get; set; }
    public string Name { get; set; } = "";
    public bool LayoutMatch { get; set; }
    public bool HashMatch { get; set; }
    public string? ReferenceSha256 { get; set; }
    public List<BlockDifference> ChangedBlocks { get; set; } = [];
}

internal sealed class BlockDifference
{
    public int SectionOffset { get; set; }
    public int? ActualRva { get; set; }
    public int? ActualFileOffset { get; set; }
    public int ActualSize { get; set; }
    public string? ReferenceSha256 { get; set; }
    public string? ActualSha256 { get; set; }
}
