#include "CultHumanAnimation.h"
#include "Misc/Paths.h"
#include "HAL/IConsoleManager.h"
#include "animation/ClipSerializer.h"
#include <map>

namespace {
TAutoConsoleVariable<int32> StagedClips(TEXT("cult.Animation.UseStagedClips"),0,TEXT("Editor verification: use the same staged humanoid clips as packaged games."));
bool SkinAllowed(const FString& Skin) {
#define CULT_HUMAN_SKIN(Id) if(Skin==TEXT(Id)) return true;
#define CULT_HUMAN_CLIP(Id)
#include "CultHumanAnimations.inl"
#undef CULT_HUMAN_SKIN
#undef CULT_HUMAN_CLIP
 return false;
}
bool ClipAllowed(const FString& Clip) {
#define CULT_HUMAN_SKIN(Id)
#define CULT_HUMAN_CLIP(Id) if(Clip==TEXT(Id)) return true;
#include "CultHumanAnimations.inl"
#undef CULT_HUMAN_SKIN
#undef CULT_HUMAN_CLIP
 return false;
}
}
FString CultHumanAnimation::Path(const FString& Skin,const FString& Clip)
{
 if(!SkinAllowed(Skin)||!ClipAllowed(Clip)) return {};
 FString Root=FPaths::ProjectContentDir()/TEXT("CharacterAnimations");
#if WITH_EDITOR
 // Authoring uses the original files; runtime builds never fall back to a repo.
 if(!StagedClips.GetValueOnGameThread()) Root=FPaths::ProjectDir()/TEXT("../assets/characters");
 if(!StagedClips.GetValueOnGameThread()) return FPaths::ConvertRelativePathToFull(Root/Skin/TEXT("animations")/(Clip+TEXT(".canim")));
#endif
 return FPaths::ConvertRelativePathToFull(Root/Skin/(Clip+TEXT(".canim")));
}
const cultulhu::AnimationClip& CultHumanAnimation::Find(const FString& Skin,const FString& Clip)
{
 static const cultulhu::AnimationClip Empty;
 const FString File=Path(Skin,Clip); if(File.IsEmpty()) return Empty;
 // std::map keeps references stable while other skins/clips enter the cache.
 static std::map<std::string,cultulhu::AnimationClip> Cache;
 const std::string Key=TCHAR_TO_UTF8(*File); auto Found=Cache.find(Key);
 if(Found==Cache.end()) {
  cultulhu::AnimationClip Loaded;
  if(!cultulhu::ClipSerializer::load(Key,Loaded)||!Loaded.hasTrackData()||Loaded.durationSeconds<=0)
   UE_LOG(LogTemp,Error,TEXT("[CultAnimation] Missing or invalid staged/authoring clip: %s"),*File);
  Found=Cache.emplace(Key,std::move(Loaded)).first;
 }
 return Found->second;
}
