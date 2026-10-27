namespace MoraleMattersUI;

public partial class App : Application
{
    public App()
    {
        InitializeComponent();
    }

    protected override Window CreateWindow(IActivationState? activationState) => new(new MainPage())
    {
        TitleBar = (TitleBar)Resources["AppTitleBar"]
    };

    private async void OpenRoarUI(object? sender, TappedEventArgs e) => await Launcher.Default.OpenAsync("https://www.nuget.org/packages/RoarUI");
    public TitleBar TitleBar => (TitleBar)Resources["AppTitleBar"];
}
