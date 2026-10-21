// CULT-ULHU game target (Windows shipping/client builds).
using UnrealBuildTool;
using System.Collections.Generic;

public class CultUlhuTarget : TargetRules
{
	public CultUlhuTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.V7;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.AddRange(new string[] { "CultUlhu", "CultUlhuCore" });
	}
}
