namespace MoraleMattersUI;

public partial class App : Application
{
    public App()
    {
        InitializeComponent();
    }

    protected override Window CreateWindow(IActivationState? activationState)
    {
        var window = new Window(new MainPage())
        {
            Title = "MoraleMattersUI",
            TitleBar = new TitleBar()
            {
                IsVisible = false
            }
        };

        return window;
    }
}
