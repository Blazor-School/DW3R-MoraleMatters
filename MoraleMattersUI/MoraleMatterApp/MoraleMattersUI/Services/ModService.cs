using Roar.DependencyInjection;
using System.IO.Compression;
using System.Security.Cryptography;
using Velopack;
using Velopack.Sources;

namespace MoraleMattersUI.Services;

[ScopedService]
public class ModService(VelopackLogger VelopackLogger)
{
    private Dictionary<string, string> _versionByHashData { get; set; } = new()
    {
        ["52AA3B8550ACE9323F9BC86CFDB1303923229398EAF20B4567358621DF59070E"] = "1.0.0",
        ["61197B2EFD59DD87C8464C63E7BA540B96156FCBC02D210AE66F0A35224A0190"] = "1.0.1",
        ["26FA725C5D86066BA1DDA8439F72A5F0A020536CA9E4E0665BF2F1C7EC1FF987"] = "1.0.2",
        ["E0813C70A1C03152E1390BA0CF8948796068EA1CE5D97BD8CC378C8C7C02B851"] = "1.1.0",
        ["0FFE48E1125FC05B4F72E3112F7045FEB6FC1A8724579995E91EF16AB658191D"] = "1.2.0",
        ["9FBAC6971331040B4224EAD8AFEDB6E245CDC42F6C83572626F86FF49E3E45F0"] = "1.3.0",
        ["27FE4941F57B6127FF00D57487EF6D121C1E6511C3956E0E3EF0B55BA052D864"] = "1.4.0",
        ["ACA5B2C33699DAD670D4428197CEB07BF8C6950705CE1A16C2CD893F8A87A5C6"] = "1.5.0",
        ["BD385C84D631510CD424F0B3681FC24D31EE2F37469E2644E28EF9182ECF0B51"] = "1.6.0",
        ["1115DDD3E9883DD2B454618A8CDDAEED734AFE05F86FE089DC9A619C39D7F410"] = "1.7.0",
        ["4E1D5AB73A04C624070E567866767C8DD50B9DFA62C6884812B43D98B4F20F34"] = "1.7.1",
        ["4F8B43A40ECA3DB75474FDBF1E3F75BF163DEA7BC8FEB65710D9211066109253"] = "1.7.2"
    };

    private const string _appId = "MoraleMatters";
    private const string _channel = "mod";

    public async Task<string> GetLocalModVersionAsync(string gamePath)
    {
        string asiFilePath = Path.Combine(gamePath, "MoraleMatters.asi");

        if (!File.Exists(asiFilePath))
        {
            return "";
        }
        else
        {
            await using var stream = new FileStream(asiFilePath, FileMode.Open);
            byte[] hash = await SHA256.HashDataAsync(stream);
            string currentHash = Convert.ToHexString(hash);

            return _versionByHashData.TryGetValue(currentHash, out string? version) ? version : "";
        }
    }

    public async Task<List<VelopackAsset>> GetModReleasesAsync()
    {
        var source = GetGithubSource();
        var feed = await source.GetReleaseFeed(VelopackLogger, _appId, _channel);

        var releases = feed.Assets
            .OrderByDescending(asset => asset.Version)
            .ToList();

        return releases;
    }

    public async Task InstallModAsync(string gamePath, VelopackAsset release)
    {
        string tempFolder = Path.Combine(Path.GetTempPath(), Guid.NewGuid().ToString("N"));
        Directory.CreateDirectory(tempFolder);
        string archivePath = Path.Combine(tempFolder, "mod.zip");
        var source = GetGithubSource();
        await source.DownloadReleaseEntry(VelopackLogger, release, archivePath, _ => { }, CancellationToken.None);
        ZipFile.ExtractToDirectory(archivePath, tempFolder);
        string sourceFolder = Path.GetDirectoryName(Directory.EnumerateFiles(tempFolder, "MoraleMatters.asi", SearchOption.AllDirectories).First())!;

        foreach (string sourceFile in Directory.EnumerateFiles(sourceFolder, "*", SearchOption.AllDirectories))
        {
            string destinationFile = Path.Combine(gamePath, Path.GetRelativePath(sourceFolder, sourceFile));
            Directory.CreateDirectory(Path.GetDirectoryName(destinationFile)!);
            File.Copy(sourceFile, destinationFile, overwrite: true);
        }

        Directory.Delete(tempFolder, recursive: true);
    }

    private GithubSource GetGithubSource() => new("https://github.com/Blazor-School/DW3R-MoraleMatters", "", false);
}
