// CULT-ULHU core wrapper module.
using UnrealBuildTool;
using System.IO;

public class CultUlhuCore : ModuleRules
{
	public CultUlhuCore(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core", "CoreUObject", "Engine", "InputCore",
			"EnhancedInput", "UMG", "Slate", "SlateCore"
		});

		// ---- Engine-agnostic core (../src) ----
		// Headers are included directly: #include "beliefs/BeliefSystem.h" etc.
		string RepoRoot = Path.GetFullPath(Path.Combine(ModuleDirectory, "..", "..", ".."));
		PublicIncludePaths.Add(Path.Combine(RepoRoot, "src"));

		// Link the prebuilt core static lib. Build it with the SAME MSVC
		// toolchain as the engine (see unreal/README.md), then place it at
		// unreal/lib/Win64/cultulhu.lib (and lib/Linux/libcultulhu.a for Linux).
		// VERIFY IN EDITOR: confirm the lib path resolves on your machine;
		// adjust for your platform/config (Debug vs Release CRT must match).
		string LibDir = Path.Combine(RepoRoot, "unreal", "lib", Target.Platform.ToString());
		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			PublicAdditionalLibraries.Add(Path.Combine(LibDir, "cultulhu.lib"));
		}
		else if (Target.Platform == UnrealTargetPlatform.Linux)
		{
			PublicAdditionalLibraries.Add(Path.Combine(LibDir, "libcultulhu.a"));
		}
		// VERIFY IN EDITOR: add other platforms (Mac, etc.) as needed.

		// The core uses only the C++17 standard library: no extra defines needed.
		// VERIFY IN EDITOR: if the core ever needs exceptions/RTTI toggles,
		// set bEnableExceptions / bUseRTTI here (default in UE is off/off).
	}
}
