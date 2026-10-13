// CULT-ULHU editor target (compile the project in the UE5 editor).
using UnrealBuildTool;
using System.Collections.Generic;

public class CultUlhuEditorTarget : TargetRules
{
	public CultUlhuEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.V5;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.AddRange(new string[] { "CultUlhu", "CultUlhuCore" });
	}
}
