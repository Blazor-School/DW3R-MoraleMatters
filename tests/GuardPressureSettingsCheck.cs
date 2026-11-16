using Microsoft.Extensions.Logging.Abstractions;
using MoraleMattersUI.Forms;
using MoraleMattersUI.Services;
using System.ComponentModel.DataAnnotations;
using System.Globalization;

int assertions = 0;
void Check(bool condition, string message)
{
    assertions++;
    if (!condition) throw new Exception(message);
}

CultureInfo.CurrentCulture = CultureInfo.GetCultureInfo("de-DE");
var properties = typeof(ModSettingForm).GetProperties().Where(property => property.Name.StartsWith("GuardPressure", StringComparison.Ordinal)).ToArray();
Check(properties.Length == 19, "All guard settings are represented.");
var service = new ModService(new VelopackLogger(NullLogger<VelopackLogger>.Instance));
string folder = Path.Combine(Path.GetTempPath(), "MoraleMatters-guard-settings-" + Guid.NewGuid().ToString("N"));
Directory.CreateDirectory(folder);
try
{
    var defaults = new ModSettingForm();
    string root = Path.GetFullPath(Path.Combine(AppContext.BaseDirectory, "../../../../"));
    var shipped = new ModSettingForm();
    shipped.LoadModSettings(root);
    foreach (var property in properties) Check(Equals(property.GetValue(shipped), property.GetValue(defaults)), property.Name + " shipped default");
    var custom = new ModSettingForm();
    foreach (var property in properties)
    {
        if (property.PropertyType == typeof(bool)) property.SetValue(custom, false);
        else if (property.PropertyType == typeof(int)) property.SetValue(custom, 5);
        else property.SetValue(custom, 2.5);
    }
    await service.SaveModSettingsAsync(folder, custom);
    string ini = await File.ReadAllTextAsync(Path.Combine(folder, "MoraleMatters.ini"));
    Check(ini.Contains("[GuardPressure]"), "Guard section saved");
    Check(ini.Contains("OpeningSeconds=2.5") && !ini.Contains("OpeningSeconds=2,5"), "Invariant decimals");
    var loaded = new ModSettingForm();
    loaded.LoadModSettings(folder);
    foreach (var property in properties) Check(Equals(property.GetValue(custom), property.GetValue(loaded)), property.Name + " round trip");
    foreach (var property in properties.Where(property => property.PropertyType != typeof(bool)))
    {
        var range = (RangeAttribute)Attribute.GetCustomAttribute(property, typeof(RangeAttribute))!;
        foreach (bool upper in new[] { false, true })
        {
            var invalid = new ModSettingForm();
            double value = Convert.ToDouble(upper ? range.Maximum : range.Minimum, CultureInfo.InvariantCulture) + (upper ? 1 : -1);
            object converted = property.PropertyType == typeof(int) ? (object)(int)value : value;
            property.SetValue(invalid, converted);
            var errors = new List<ValidationResult>();
            Check(!Validator.TryValidateObject(invalid, new ValidationContext(invalid), errors, true) && errors.Any(error => error.MemberNames.Contains(property.Name)), property.Name + " bounds");
        }
    }
    await File.WriteAllTextAsync(Path.Combine(folder, "MoraleMatters.ini"), "[General]\nEnabled=1\n");
    loaded = new ModSettingForm();
    loaded.LoadModSettings(folder);
    foreach (var property in properties) Check(Equals(property.GetValue(defaults), property.GetValue(loaded)), property.Name + " old INI fallback");
    Check(await service.GetLocalModVersionAsync(Path.Combine(root, "package/DW3CE_RE/Binaries/Win64")) == "1.9.0", "Packaged ASI recognized as 1.9.0");
    Console.WriteLine($"Guard pressure UI settings: {assertions} assertions passed.");
}
finally
{
    Directory.Delete(folder, recursive: true);
}
