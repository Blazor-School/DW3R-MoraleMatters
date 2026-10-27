using Microsoft.Extensions.Logging;
using Roar.DependencyInjection;
using Velopack.Logging;

namespace MoraleMattersUI.Services;

[SingletonService]
public class VelopackLogger(ILogger<VelopackLogger> Logger) : IVelopackLogger
{
    public void Log(VelopackLogLevel logLevel, string? message, Exception? exception)
    {
        switch (logLevel)
        {
            case VelopackLogLevel.Trace:
                Logger.LogTrace(exception, message);
                break;
            case VelopackLogLevel.Debug:
                Logger.LogDebug(exception, message);
                break;
            case VelopackLogLevel.Information:
                Logger.LogInformation(exception, message);
                break;
            case VelopackLogLevel.Warning:
                Logger.LogWarning(exception, message);
                break;
            case VelopackLogLevel.Error:
                Logger.LogError(exception, message);
                break;
            case VelopackLogLevel.Critical:
                Logger.LogCritical(exception, message);
                break;
            default:
                Logger.LogInformation(exception, message);
                break;
        }
    }
}
