#include "CultCanonicalKit.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "characters/CharacterDefParser.h"

bool CultCanonicalKit::IsReleased(const FString& Id)
{
#define CULT_RELEASE_CHARACTER(Value) if(Id==TEXT(Value)) return true;
#include "CultReleaseRoster.inl"
#undef CULT_RELEASE_CHARACTER
 return false;
}

bool CultCanonicalKit::Load(const FString& Id, cultulhu::CharacterDef& Out)
{
 // Gate before file access, including stale selections, bot configuration, and path traversal.
 if(!IsReleased(Id)) return false;
 FString Text;
 FString Path=FPaths::ProjectContentDir()/TEXT("CharacterDefinitions")/Id/TEXT("character.def");
#if WITH_EDITOR
 // Editing still reads the original source; packaged games use explicitly staged release kits.
 Path=FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()/TEXT("../assets/characters")/Id/TEXT("character.def"));
#endif
 if (!FFileHelper::LoadFileToString(Text, *Path)) { UE_LOG(LogTemp, Error, TEXT("Missing original character definition: %s"), *Path); return false; }
 auto Parsed = cultulhu::parseCharacterDef(TCHAR_TO_UTF8(*Text));
 if (!Parsed.ok) { UE_LOG(LogTemp, Error, TEXT("Invalid original kit %s: %s"), *Id, UTF8_TO_TCHAR(Parsed.error.c_str())); return false; }
 Out = std::move(Parsed.def);
 return true;
}

FString CultCanonicalKit::Summary(const FString& Id)
{
 cultulhu::CharacterDef D;
 if (!Load(Id, D)) return TEXT("Original kit unavailable — cannot enter play.");
 auto Spell = [](const TCHAR* Key, const cultulhu::SpellDef& S) {
  FString Kind=UTF8_TO_TCHAR(S.effectKind.c_str()); Kind.ReplaceInline(TEXT("_"),TEXT(" "));
  if(Kind==TEXT("aoe damage")) Kind=TEXT("area damage");
  return FString::Printf(TEXT("%s: %s (%s, %.0fs cooldown)"),Key,UTF8_TO_TCHAR(S.name.c_str()),*Kind,S.cooldownSec);
 };
 return Spell(TEXT("Q"), D.qAbility) + TEXT("\n") + Spell(TEXT("F"), D.fAbility) + TEXT("\n") + Spell(TEXT("R"), D.rAbility)
  + FString::Printf(TEXT("\nRMB: %s | LMB: melee combo\nHP %.0f | Stamina %.0f"), UTF8_TO_TCHAR(D.rightClick.name.c_str()), D.maxHp, D.maxStamina);
}
