using CommunityToolkit.Maui;
using CommunityToolkit.Maui.Storage;
using Microsoft.Extensions.Logging;
using Microsoft.Maui.LifecycleEvents;
using MoraleMattersUI.Services;
using RoarUI;
using Velopack;

namespace MoraleMattersUI;

public static class MauiProgram
{
    public static MauiApp CreateMauiApp()
    {
        var builder = MauiApp.CreateBuilder();
        builder
            .UseMauiApp<App>()
            .UseMauiCommunityToolkit()
            .ConfigureFonts(fonts =>
            {
                fonts.AddFont("OpenSans-Regular.ttf", "OpenSansRegular");
            })
#if WINDOWS
            .ConfigureLifecycleEvents(events =>
            {
                events.AddWindows(windows => windows.OnWindowCreated(window =>
                {
                    if (window.AppWindow.Presenter is Microsoft.UI.Windowing.OverlappedPresenter presenter)
                    {
                        presenter.Maximize();
                    }
                }));
            });
#endif
        ;

        builder.Services.AddMauiBlazorWebView();
        builder.Services.GoRoar();
        builder.Services.AddRoarServices();
        builder.Services.AddSingleton(FolderPicker.Default);

        VelopackApp
            .Build()
            .SetLogger(new VelopackLogger(builder.Services.BuildServiceProvider().GetRequiredService<ILogger<VelopackLogger>>()))
            .Run();

#if DEBUG
        builder.Services.AddBlazorWebViewDeveloperTools();
        builder.Logging.AddDebug();
#endif

        return builder.Build();
    }
}
