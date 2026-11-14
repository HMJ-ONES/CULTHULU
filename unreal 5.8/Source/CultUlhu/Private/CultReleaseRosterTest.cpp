#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "CultCanonicalKit.h"
#include "CultUlhuGameInstance.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCultReleaseRosterTest,"CultUlhu.ReleaseRoster",
 EAutomationTestFlags::ClientContext|EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCultReleaseRosterTest::RunTest(const FString&)
{
 const auto& Roster=UCultUlhuGameInstance::Roster();
 TestEqual(TEXT("Current release contains sixteen entities"),Roster.Num(),16);
 TSet<FString> Unique;
 for(const auto& Entry:Roster) {
  Unique.Add(Entry.Id); cultulhu::CharacterDef Definition;
  TestTrue(Entry.Id+TEXT(" remains playable with its original kit"),CultCanonicalKit::Load(Entry.Id,Definition));
  FString Staged;
  TestTrue(Entry.Id+TEXT(" has its packaged definition"),FFileHelper::LoadFileToString(Staged,*(FPaths::ProjectContentDir()/TEXT("CharacterDefinitions")/Entry.Id/TEXT("character.def"))));
#if WITH_EDITOR
  FString Original; FFileHelper::LoadFileToString(Original,*(FPaths::ProjectDir()/TEXT("../assets/characters")/Entry.Id/TEXT("character.def")));
  TestEqual(Entry.Id+TEXT(" staged kit is unchanged"),Staged,Original);
#endif
 }
 TestEqual(TEXT("No duplicate release entries"),Unique.Num(),16);
 for(const TCHAR* Id:{TEXT("hypnos"),TEXT("bast"),TEXT("howling_eye"),TEXT("elder_mind")}) {
  cultulhu::CharacterDef Definition;
  TestFalse(FString(Id)+TEXT(" is absent from selection"),Unique.Contains(Id));
  TestFalse(FString(Id)+TEXT(" cannot be loaded by stale selection or bot configuration"),CultCanonicalKit::Load(Id,Definition));
  TestFalse(FString(Id)+TEXT(" is not staged for publication"),FPaths::FileExists(FPaths::ProjectContentDir()/TEXT("CharacterDefinitions")/Id/TEXT("character.def")));
#if WITH_EDITOR
  TestTrue(FString(Id)+TEXT(" original file is preserved for a future release"),FPaths::FileExists(FPaths::ProjectDir()/TEXT("../assets/characters")/Id/TEXT("character.def")));
#endif
 }
 TestFalse(TEXT("Unknown and path-based kit IDs are rejected"),CultCanonicalKit::IsReleased(TEXT("../hypnos")));
 return true;
}
#endif
