#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "CultHumanMesh.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCultHumanMeshTest,"CultUlhu.Runtime.HumanMeshMerge",EAutomationTestFlags::EditorContext|EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FCultHumanMeshTest::RunTest(const FString&) {
 for(const FString Skin:{FString(TEXT("civilian_villager")),FString(TEXT("cultist_hooded")),FString(TEXT("cultist_magus"))}) {
  FString Details; TestTrue(Skin+TEXT(" retains original geometry, materials and reference bones"),CultHumanMesh::Verify(Skin,Details));
  TestEqual(TEXT("All six original pieces are certified"),CultHumanMesh::SourceParts(CultHumanMesh::Find(Skin)),6);
  AddInfo(Skin+TEXT(": ")+Details);
 }
 return true;
}
#endif
