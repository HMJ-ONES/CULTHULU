#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "Camera/CameraComponent.h"
#include "CultCanonicalKit.h"
#include "CultUlhuPlayerPawn.h"
#include "CultUlhuGameInstance.h"
#include "CultPopulation.h"
#include "CultWorldDirector.h"
#include "CultDestructible.h"
#include "CultBlastFX.h"
#include "CultSkillAttack.h"
#include "Components/StaticMeshComponent.h"
#include "CultLivingLandscape.h"
#include "Engine/StaticMesh.h"
#include "commands/CommandSystem.h"
#include "ui/CommandMenu.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCultCanonicalGameplayTest,"CultUlhu.CanonicalGameplay",
 EAutomationTestFlags::ClientContext | EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCultCanonicalGameplayTest::RunTest(const FString& Parameters)
{
 for(const auto& Entry:UCultUlhuGameInstance::Roster()) {
  cultulhu::CharacterDef D; if(!TestTrue(Entry.Id+TEXT(" loads original kit"),CultCanonicalKit::Load(Entry.Id,D))) continue;
  TestTrue(Entry.Id+TEXT(" has all three named abilities"),!D.qAbility.id.empty()&&!D.fAbility.id.empty()&&!D.rAbility.id.empty());
 }
 UWorld* W=nullptr; for(const auto& C:GEngine->GetWorldContexts()) if(C.World()&&C.World()->IsGameWorld()) W=C.World();
 if(!W) { AddError(TEXT("Dark Wilds game world required")); return false; }
 auto* Director=ACultWorldDirector::Find(W); if(!TestNotNull(TEXT("Living world director exists"),Director)) return false;
 ACultUlhuPlayerPawn* Player=nullptr; for(TActorIterator<ACultUlhuPlayerPawn> It(W);It;++It) if(!It->IsBot()) { Player=*It; break; }
 if(!TestNotNull(TEXT("Playable entity exists"),Player)) return false;
 TestEqual(TEXT("Cthulhu original max HP"),Player->GetMaxHealth(),500.f);
 auto* Camera=Player->FindComponentByClass<UCameraComponent>();
 Camera->SetWorldLocation(Player->FeetLocation()+FVector(-1200,0,2500)); Camera->SetWorldRotation(FRotator(-45,0,0));
 const auto Ground=Player->CameraAim();
 TestTrue(TEXT("Rendered camera picks terrain"),Ground.bBlockingHit);
 FActorSpawnParameters Spawn; Spawn.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
 auto* Building=W->SpawnActor<ACultDestructible>(Ground.ImpactPoint+FVector(0,0,50),FRotator::ZeroRotator,Spawn);
 Building->SetMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube")));
 const auto Expected=Player->CameraAim(); const FVector Point=Expected.bBlockingHit?Expected.ImpactPoint:Expected.TraceEnd;
 const float Hp=Building->GetHealth(); Player->AbilityQ();
 TestTrue(TEXT("Tentacle Slam appears at the actual camera pick"),Player->GetLastEffectLocation().Equals(Point,1));
 bool RenderedAtPick=false;
 for(TActorIterator<ACultSkillAttack> It(W);It;++It) if(It->GetActorLocation().Equals(Point,1)) {
  TArray<UStaticMeshComponent*> Puffs; It->GetComponents(Puffs);
  for(auto* Puff:Puffs) if(FVector::Dist(Puff->GetComponentLocation(),Point)<1500) RenderedAtPick=true;
 }
 TestTrue(TEXT("Actual spawned effect meshes appear at the camera pick, not world origin"),RenderedAtPick);
 TestTrue(TEXT("Tentacle Slam warning does not damage a structure instantly"),Building->GetHealth()==Hp);
 Player->AbilityF(); TestTrue(TEXT("Nightmare Veil stays at its documented caster location"),Player->GetLastEffectLocation().Equals(Player->FeetLocation(),1));
 int Before=0; for(TActorIterator<ACultPopulation> It(W);It;++It) if(It->GetFaction()==0) ++Before;
 Player->AbilityR(); int After=0; for(TActorIterator<ACultPopulation> It(W);It;++It) if(It->GetFaction()==0) ++After;
 // Q+F costs 60 stamina; R correctly waits until at least 50 is available.
 TestEqual(TEXT("Original stamina costs prevent over-casting"),After,Before);
 const float BuildingHp=Building->GetHealth(); Player->AbilityQ(); TestEqual(TEXT("Original cooldown blocks an immediate repeat cast"),Building->GetHealth(),BuildingHp);
 Building->Destroy();
 auto* Pickable=W->SpawnActor<ACultPopulation>(Player->FeetLocation()+FVector(1200,0,100),FRotator::ZeroRotator,Spawn);
 Camera->SetWorldLocation(Pickable->GetActorLocation()+FVector(-1000,0,50)); Camera->SetWorldRotation((Pickable->GetActorLocation()-Camera->GetComponentLocation()).Rotation());
 TestTrue(TEXT("Camera trace picks NPC capsule directly"),Player->CameraAim().GetActor()==Pickable);
 Pickable->Destroy();
 Camera->SetWorldLocation(Player->FeetLocation()+FVector(-1200,0,2500)); Camera->SetWorldRotation(FRotator(-45,0,0));
 for(int I=0;I<5;++I) Player->Tick(1.f);
 Player->AbilityR(); int Summons=0; for(TActorIterator<ACultPopulation> It(W);It;++It) if(It->IsSummoned()&&It->GetFaction()==0) ++Summons;
 TestEqual(TEXT("Call of the Deep summons the original four allied creatures"),Summons,4);
 if(auto* Land=ACultLivingLandscape::Find(W)) {
  const FVector Cave=Land->CaveSites()[0]; FVector Floor;
  if(ACultLivingLandscape::Surface(W,Cave,Floor)) {
   Camera->SetWorldLocation(Floor+FVector(-1000,-1000,1000)); Camera->SetWorldRotation((Floor-Camera->GetComponentLocation()).Rotation());
   Player->Tick(46.f); Player->AbilityR(); int Inside=0;
   for(TActorIterator<ACultPopulation> It(W);It;++It) if(It->IsSummoned()&&It->GetFaction()==0&&FVector::Dist2D(It->GetActorLocation(),Floor)<2500&&FMath::Abs(It->GetActorLocation().Z-Floor.Z)<300) ++Inside;
   TestEqual(TEXT("Camera-aimed cavern summons stay inside on the floor, below the roof"),Inside,4);
  }
 }
 int Bots=0; for(TActorIterator<ACultUlhuPlayerPawn> It(W);It;++It) if(It->IsBot()) { ++Bots; TestTrue(TEXT("Rival starts well separated from player"),FVector::Dist2D(It->GetActorLocation(),Player->GetActorLocation())>18000); }
 TestEqual(TEXT("Two rival entities inhabit the free-roam world"),Bots,2);
 for(int32 Stunned=0;Stunned<2;++Stunned) {
  FTransform Transform(Player->GetActorLocation()+FVector(10000,5000+Stunned*4000,0));
  auto* Hydra=W->SpawnActorDeferred<ACultUlhuPlayerPawn>(ACultUlhuPlayerPawn::StaticClass(),Transform,nullptr,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
  Hydra->ConfigureBot(TEXT("mother_hydra"),1); Hydra->FinishSpawning(Transform);
  Hydra->SetControlState(true,false,0,Hydra->GetActorLocation(),Stunned!=0,101); Hydra->Tick(.01f);
  Hydra->ReceiveDamage(100.f); Hydra->AbilityF();
  TestEqual(Stunned?TEXT("Stun prevents Brine Blessing"):TEXT("Root allows Brine Blessing"),Hydra->GetHealth(),Hydra->GetMaxHealth()-(Stunned?100.f:0.f));
  Hydra->Destroy();
 }
 const int Cities=Director->CityCount(); Director->Tick(181);
 TestTrue(TEXT("New settlements form over game time"),Director->CityCount()>Cities);
 AddInfo(Director->Status());
 return true;
}
#endif
