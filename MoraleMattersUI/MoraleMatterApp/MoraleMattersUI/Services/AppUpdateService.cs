using Microsoft.Extensions.Logging;
using Roar.DependencyInjection;
using Velopack;
using Velopack.Sources;

namespace MoraleMattersUI.Services;

[SingletonService]
public class AppUpdateService(ILogger<AppUpdateService> Logger)
{
    private readonly UpdateManager _manager = new(new GithubSource("https://github.com/Blazor-School/DW3R-MoraleMatters", null, false));

    public VelopackAsset? ReadyUpdate { get; private set; }
    public event Action? UpdateReady;

    public async Task CheckAndDownloadAsync()
    {
        try
        {
            var update = await _manager.CheckForUpdatesAsync();

            if (update is not null)
            {
                await _manager.DownloadUpdatesAsync(update);
                ReadyUpdate = update.TargetFullRelease;
                UpdateReady?.Invoke();
            }
        }
        catch (Exception exception)
        {
            Logger.LogWarning(exception, "Unable to check for or download a UI update.");
        }
    }

    public void RestartToUpdate() => _manager.ApplyUpdatesAndRestart(ReadyUpdate);
}
