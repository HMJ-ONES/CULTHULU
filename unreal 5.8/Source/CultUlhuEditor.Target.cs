using UnrealBuildTool;

public class CultUlhuEditorTarget : TargetRules
{
    public CultUlhuEditorTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Editor;
        DefaultBuildSettings = BuildSettingsVersion.V7;
        ExtraModuleNames.Add("CultUlhu");
    }
}