using System.ComponentModel.DataAnnotations;

namespace MoraleMattersUI.Forms;

public class GameFolderForm : IValidatableObject
{
    public string GamePath { get; set; } = "";

    public IEnumerable<ValidationResult> Validate(ValidationContext validationContext)
    {
        string exePath = Path.Combine(GamePath, "DW3CE_RE-Win64-Shipping.exe");

        if (!File.Exists(exePath))
        {
            yield return new ValidationResult("Select the folder containing DW3CE_RE-Win64-Shipping.exe.", [nameof(GamePath)]);
        }
    }
}
