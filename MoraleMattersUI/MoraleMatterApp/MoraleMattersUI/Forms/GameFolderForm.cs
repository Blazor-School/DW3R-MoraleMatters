using System.ComponentModel.DataAnnotations;

namespace MoraleMattersUI.Forms;

public class GameFolderForm : IValidatableObject
{
    public string GamePath { get; set; } = "";

    public IEnumerable<ValidationResult> Validate(ValidationContext validationContext)
    {
        string exePath = Path.Combine(GamePath, "DW3CE_RE-Win64-Shipping.exe");
        string tnkPath = Path.Combine(GamePath, "DW3CE_RE-Win64-Shipping.tnk");

        if (!File.Exists(exePath) || !File.Exists(tnkPath))
        {
            yield return new ValidationResult("Select the folder containing DW3CE_RE-Win64-Shipping.exe and DW3CE_RE-Win64-Shipping.tnk.", [nameof(GamePath)]);
        }
    }
}
