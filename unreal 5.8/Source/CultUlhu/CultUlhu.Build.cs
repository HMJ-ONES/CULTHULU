// CULT-ULHU game module (gameplay glue: GameMode, PlayerController).
using UnrealBuildTool;
using System.IO;
using System.Text.RegularExpressions;

public class CultUlhu : ModuleRules
{
	public CultUlhu(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		// UI translation units contain file-local helpers with shared names.
		bUseUnity = false;
 if (Target.bBuildEditor) PrivateDependencyModuleNames.AddRange(new string[] { "UnrealEd", "RHI", "RenderCore" });
 PrivateDependencyModuleNames.Add("Slate");

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core", "CoreUObject", "Engine", "InputCore",
			"EnhancedInput", "UMG", "SlateCore", "CultUlhuCore", "ProceduralMeshComponent"
		});

  // Runtime Slate font data is explicitly staged, including its redistribution license.
  foreach(string FontFile in Directory.GetFiles(Path.Combine(ModuleDirectory,"..","..","Content","UI","Fonts")))
   RuntimeDependencies.Add("$(ProjectDir)/Content/UI/Fonts/"+Path.GetFileName(FontFile),StagedFileType.NonUFS);

  // The allowlist is shared with C++; never stage the whole source character folder.
  string ReleaseRoster=Path.Combine(ModuleDirectory,"Public","CultReleaseRoster.inl");
  ExternalDependencies.Add(ReleaseRoster);
  foreach(Match Entry in Regex.Matches(File.ReadAllText(ReleaseRoster), "CULT_RELEASE_CHARACTER\\(\"([a-z0-9_]+)\"\\)"))
  {
   string Id=Entry.Groups[1].Value;
   string Definition=Path.GetFullPath(Path.Combine(ModuleDirectory,"..","..","..","assets","characters",Id,"character.def"));
   RuntimeDependencies.Add("$(ProjectDir)/Content/CharacterDefinitions/"+Id+"/character.def",Definition,StagedFileType.UFS);
  }

  // std::ifstream consumes loose .canim files, so these must be NonUFS.
  string HumanManifest=Path.Combine(ModuleDirectory,"Public","CultHumanAnimations.inl");
  ExternalDependencies.Add(HumanManifest);
  string ManifestText=File.ReadAllText(HumanManifest);
  foreach(Match Skin in Regex.Matches(ManifestText,"CULT_HUMAN_SKIN\\(\"([a-z0-9_]+)\"\\)"))
   foreach(Match Clip in Regex.Matches(ManifestText,"CULT_HUMAN_CLIP\\(\"([a-z0-9_]+)\"\\)")) {
    string Id=Skin.Groups[1].Value,Name=Clip.Groups[1].Value;
    string Animation=Path.GetFullPath(Path.Combine(ModuleDirectory,"..","..","..","assets","characters",Id,"animations",Name+".canim"));
    if(!File.Exists(Animation)) throw new BuildException("Missing native humanoid animation: "+Animation);
    ExternalDependencies.Add(Animation);
    RuntimeDependencies.Add("$(ProjectDir)/Content/CharacterAnimations/"+Id+"/"+Name+".canim",Animation,StagedFileType.NonUFS);
   }

		// VERIFY IN EDITOR: add "OnlineSubsystem" / "OnlineSubsystemUtils"
		// here if you adopt session-based lobby instead of direct IP open.
	}
}
