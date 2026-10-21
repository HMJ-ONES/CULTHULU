#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "CultLivingLandscape.h"
#include "CultWorldDirector.h"
#include "CultDestructible.h"
#include "CultPopulation.h"
#include "CultBlastFX.h"
#include "ProceduralMeshComponent.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCultLivingWorldTest,"CultUlhu.LivingWorld",
 EAutomationTestFlags::ClientContext | EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCultLivingWorldTest::RunTest(const FString& Parameters)
{
 UWorld* W=nullptr; for(auto& C:GEngine->GetWorldContexts()) if(C.World()&&C.World()->IsGameWorld()) W=C.World();
 if(!W) { AddError(TEXT("Game world required")); return false; }
 auto* Land=ACultLivingLandscape::Find(W); if(!TestNotNull(TEXT("Expanded procedural terrain exists"),Land)) return false;
 TestTrue(TEXT("Map is at least 3.5 km across"),Land->GroundBounds().GetSize().X>=350000);
 float Low=1e9,High=-1e9;
 for(int X=-150000;X<150000;X+=10000) for(int Y=-150000;Y<150000;Y+=10000) { float H=Land->Height(X,Y); Low=FMath::Min(Low,H); High=FMath::Max(High,H); }
 TestTrue(TEXT("Terrain has dramatic elevation variation"),High-Low>7000);
 for(auto& Cave:ACultLivingLandscape::CaveSites()) {
  FVector Floor; TestTrue(TEXT("Cavern has a grounded walkable floor"),ACultLivingLandscape::Surface(W,Cave,Floor));
  FHitResult H; FCollisionQueryParams Q(SCENE_QUERY_STAT(CaveTest),true);
  TestFalse(TEXT("Giant-sized passage remains open through cavern"),W->LineTraceSingleByChannel(H,Cave+FVector(0,-7500,3500),Cave+FVector(0,7500,3500),ECC_Visibility,Q));
 }
 int Temples[3]={0,0,0},CityBuildings=0;
 for(TActorIterator<ACultDestructible> It(W);It;++It) {
  if(It->ActorHasTag(TEXT("CultTemple"))&&It->GetFaction()>=0) ++Temples[It->GetFaction()];
  if(It->ActorHasTag(TEXT("LivingCity"))) ++CityBuildings;
 }
 for(int F=0;F<3;++F) TestTrue(FString::Printf(TEXT("Faction %d starts with a substantial built temple"),F),Temples[F]>=10);
 TestTrue(TEXT("Cities begin with at least sixty clustered structures"),CityBuildings>=60);
 auto* D=ACultWorldDirector::Find(W); if(!TestNotNull(TEXT("World factions exist"),D)) return false;
 D->DeclareWar(1,2); TestTrue(TEXT("Rival factions can fight each other"),D->AtWar(1,2)&&D->AtWar(2,1));
 TestFalse(TEXT("A rival war does not automatically target the human"),D->AtWar(0,1)||D->AtWar(0,2));
 auto* A=ACultBlastFX::SpawnBlast(W,ECultBlastKind::HitFlash,FVector(12345,7654,3500),2);
 auto* B=ACultBlastFX::SpawnBlast(W,ECultBlastKind::HitFlash,FVector(-12345,13579,5500),2);
 TestTrue(TEXT("Different ability impacts retain distinct requested world transforms"),A&&B&&A->GetActorLocation().Equals(FVector(12345,7654,3500),1)&&B->GetActorLocation().Equals(FVector(-12345,13579,5500),1));
 if(A) A->Destroy(); if(B) B->Destroy();
 AddInfo(FString::Printf(TEXT("Living world: %d city structures, temples %d/%d/%d, elevation span %.0fm"),CityBuildings,Temples[0],Temples[1],Temples[2],(High-Low)/100));
 return true;
}
#endif
