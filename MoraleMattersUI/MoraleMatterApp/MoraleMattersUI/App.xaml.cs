namespace MoraleMattersUI;

public partial class App : Application
{
    private const string ThemePreferenceKey = "AppTheme";

    public App()
    {
        InitializeComponent();
        UserAppTheme = (AppTheme)Preferences.Default.Get(ThemePreferenceKey, (int)AppTheme.Unspecified);
    }

    protected override Window CreateWindow(IActivationState? activationState) => new(new MainPage())
    {
        TitleBar = (TitleBar)Resources["AppTitleBar"]
    };

    private void ToggleTheme(object? sender, EventArgs e)
    {
        UserAppTheme = RequestedTheme is AppTheme.Dark ? AppTheme.Light : AppTheme.Dark;
        Preferences.Default.Set(ThemePreferenceKey, (int)UserAppTheme);
    }

    private async void OpenRoarUI(object? sender, TappedEventArgs e) => await Launcher.Default.OpenAsync("https://www.nuget.org/packages/RoarUI");
    public TitleBar TitleBar => (TitleBar)Resources["AppTitleBar"];
}
