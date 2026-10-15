// CULT-ULHU game module (gameplay glue: GameMode, PlayerController).
using UnrealBuildTool;

public class CultUlhu : ModuleRules
{
	public CultUlhu(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core", "CoreUObject", "Engine", "InputCore",
			"EnhancedInput", "UMG", "CultUlhuCore"
		});

		// VERIFY IN EDITOR: add "OnlineSubsystem" / "OnlineSubsystemUtils"
		// here if you adopt session-based lobby instead of direct IP open.
	}
}
