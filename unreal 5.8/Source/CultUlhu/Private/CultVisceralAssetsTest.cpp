#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Engine/StaticMesh.h"
#include "StaticMeshResources.h"
#include "Materials/MaterialInterface.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCultVisceralAssetsTest,"CultUlhu.Art.VisceralProjectileAssets",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCultVisceralAssetsTest::RunTest(const FString&) {
 for(const TCHAR* Name:{TEXT("SM_AcidGlob"),TEXT("SM_ChitinDart")}) {
  const FString Path=FString(TEXT("/Game/CultUlhu/Projectiles/Visceral20261127/"))+Name;
  auto* Mesh=LoadObject<UStaticMesh>(nullptr,*Path);TestNotNull(TEXT("Original projectile mesh imports: ")+Path,Mesh);
  if(Mesh) {
   auto* Data=Mesh->GetRenderData();TestTrue(TEXT("Projectile has lightweight render geometry"),Data&&Data->LODResources.Num()&&Data->LODResources[0].GetNumTriangles()>50&&Data->LODResources[0].GetNumTriangles()<1500);
   TestTrue(TEXT("Projectile mesh stays within its visual scale"),Mesh->GetBounds().BoxExtent.GetMax()<110);
  }
 }
 for(const TCHAR* Name:{TEXT("M_VisceralGlob"),TEXT("M_VisceralOrgan")}) TestNotNull(TEXT("Wet organic material exists"),LoadObject<UMaterialInterface>(nullptr,*(FString(TEXT("/Game/CultUlhu/Materials/"))+Name)));
 return true;
}
#endif
