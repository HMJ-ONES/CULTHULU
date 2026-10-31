#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "CultAutomationPIE.h"
#include "CultUlhuGameInstance.h"
#include "CultUlhuPlayerController.h"
#include "CultUlhuPlayerPawn.h"
#include "CultCharacterModel.h"
#include "CultWorldDirector.h"
#include "CultMatchDirector.h"
#include "Components/MeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Components/CapsuleComponent.h"
#include "Kismet/GameplayStatics.h"

namespace {
void BodyMeshes(AActor* Actor,TArray<UMeshComponent*>& Meshes) {
 Actor->GetComponents(Meshes); TArray<AActor*> Children; Actor->GetAttachedActors(Children);
 for(auto* Child:Children) if(Child->IsA<ACultCharacterModel>()) { TArray<UMeshComponent*> Parts; Child->GetComponents(Parts); Meshes.Append(Parts); }
 for(auto* Mesh:Meshes) { Mesh->UpdateComponentToWorld(); Mesh->UpdateBounds(); }
}
bool AnyHidden(AActor* Actor,const TSet<FPrimitiveComponentId>& Hidden) {
 TArray<UMeshComponent*> Meshes; BodyMeshes(Actor,Meshes);
 for(auto* Mesh:Meshes) if(Mesh->Bounds.SphereRadius>10&&Hidden.Contains(Mesh->GetPrimitiveSceneId())) return true;
 return false;
}
class FCameraSafetyObservation:public IAutomationLatentCommand {
 FAutomationTestBase* T; ECultPlayMode Mode; int32 Phase=0; float At=0;
 double Started=FPlatformTime::Seconds(); TWeakObjectPtr<ACultUlhuPlayerPawn> Friend,Enemy;
public:
 FCameraSafetyObservation(FAutomationTestBase* Test,ECultPlayMode Choice):T(Test),Mode(Choice) {}
 ~FCameraSafetyObservation() { if(Friend.IsValid()) Friend->Destroy(); if(Enemy.IsValid()) Enemy->Destroy(); }
 bool Update() override {
  if(FPlatformTime::Seconds()-Started>120) { T->AddError(TEXT("Crowded camera test timed out")); return true; }
  auto* W=GEditor->PlayWorld.Get(); if(!W||W->bIsTearingDown) return false;
  auto* G=Cast<UCultUlhuGameInstance>(W->GetGameInstance()); if(!G) return false;
  if(W->GetName().Contains(TEXT("L_MainMenu"))) {
   if(!Phase) { G->OpenCharacterSelect(Mode); G->SelectedCharacterId=TEXT("cthulhu_avatar"); if(Mode==ECultPlayMode::FreeRoam) { G->FreeRoamSaveSlot=TEXT("Automation_CameraSafety"); G->SetInitialBeliefs({1,2,3}); } G->StartPlay(); Phase=1; }
   return false;
  }
  auto* PC=Cast<ACultUlhuPlayerController>(W->GetFirstPlayerController()); auto* P=PC?Cast<ACultUlhuPlayerPawn>(PC->GetPawn()):nullptr;
  if(!P||!ACultWorldDirector::Find(W)||W->GetTimeSeconds()<.6f) return false;
  if(Phase==1) {
   auto Spawn=[&](int32 Faction,const FString& Id) { FTransform Transform(P->GetActorLocation()+FVector(8000,Faction?4000:-4000,0)); auto* Bot=W->SpawnActorDeferred<ACultUlhuPlayerPawn>(ACultUlhuPlayerPawn::StaticClass(),Transform,nullptr,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn); if(!Bot) return (ACultUlhuPlayerPawn*)nullptr; Bot->ConfigureBot(Id,Faction); Bot->FinishSpawning(Transform); return Bot; };
   Friend=Spawn(0,TEXT("mi_go")); Enemy=Spawn(1,TEXT("yig"));
   if(!Friend.IsValid()||!Enemy.IsValid()) { T->AddError(TEXT("Camera safety fixtures could not spawn")); return true; }
   FVector Eye; FRotator Rotation; PC->GetPlayerViewPoint(Eye,Rotation);
   Friend->SetActorLocation(Eye+Rotation.Vector()*1000); Enemy->SetActorLocation(Eye+Rotation.Vector()*1000);
   TArray<UMeshComponent*> Meshes; BodyMeshes(Friend.Get(),Meshes); BodyMeshes(Enemy.Get(),Meshes);
   const auto Collision=Friend->GetCapsuleComponent()->GetCollisionEnabled();
   TSet<FPrimitiveComponentId> Hidden; PC->UpdateHiddenComponents(Eye,Hidden);
   T->TestTrue(TEXT("A nearby ally's actual render meshes are excluded from this third-person view"),AnyHidden(Friend.Get(),Hidden));
   T->TestFalse(TEXT("An enemy in the same camera volume remains visible"),AnyHidden(Enemy.Get(),Hidden));
   T->TestFalse(TEXT("Hiding is not a global actor visibility change"),Friend->IsHidden());
   T->TestEqual(TEXT("Friendly physical collision is preserved"),Friend->GetCapsuleComponent()->GetCollisionEnabled(),Collision);
   Friend->SetActorLocation(Eye+FVector(0,50000,0)); BodyMeshes(Friend.Get(),Meshes); Hidden.Reset(); PC->UpdateHiddenComponents(Eye,Hidden);
   T->TestFalse(TEXT("The ally reappears immediately after leaving the view corridor"),AnyHidden(Friend.Get(),Hidden));
   auto* Tail=NewObject<UStaticMeshComponent>(Friend.Get()); Tail->SetupAttachment(Friend->GetRootComponent()); Tail->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube"))); Tail->SetCollisionEnabled(ECollisionEnabled::NoCollision); Tail->RegisterComponent(); Tail->SetWorldLocation(Eye+Rotation.Vector()*500); Tail->SetWorldScale3D(FVector(4)); Tail->UpdateBounds();
   Hidden.Reset(); PC->UpdateHiddenComponents(Eye,Hidden);
   T->TestTrue(TEXT("A long attached body part near the eye is handled even when its hero root is distant"),Hidden.Contains(Tail->GetPrimitiveSceneId())); Tail->DestroyComponent();
   P->ToggleCamera(); At=W->GetTimeSeconds(); Phase=2; return false;
  }
  if(Phase==2&&W->GetTimeSeconds()-At>.15f) {
   FVector Eye; FRotator Rotation; PC->GetPlayerViewPoint(Eye,Rotation);
   Friend->SetActorLocation(Eye); Enemy->SetActorLocation(Eye);
   TArray<UMeshComponent*> Meshes; BodyMeshes(Friend.Get(),Meshes); BodyMeshes(Enemy.Get(),Meshes);
   TSet<FPrimitiveComponentId> Hidden; PC->UpdateHiddenComponents(Eye,Hidden);
   T->TestTrue(TEXT("An ally enclosing the first-person eye cannot black out the view"),AnyHidden(Friend.Get(),Hidden));
   T->TestTrue(TEXT("The player's own first-person body is excluded before rendering"),AnyHidden(P,Hidden));
   T->TestFalse(TEXT("First-person occlusion never hides enemy bodies"),AnyHidden(Enemy.Get(),Hidden));
   T->TestTrue(TEXT("The final character capsule retains the canonical height"),FMath::IsNearlyEqual(P->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()*2,2933.333f,1.f));
   return true;
  }
  return false;
 }
};
}
#define CULT_CAMERA_SAFETY_TEST(Type,Name,Mode) \
 IMPLEMENT_SIMPLE_AUTOMATION_TEST(Type,Name,EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter) \
 bool Type::RunTest(const FString&) { ADD_LATENT_AUTOMATION_COMMAND(FCultStartFreshPIE()); ADD_LATENT_AUTOMATION_COMMAND(FCameraSafetyObservation(this,Mode)); ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand()); return true; }
CULT_CAMERA_SAFETY_TEST(FCultFreeCameraSafety,"CultUlhu.CameraSafety.FreeRoam",ECultPlayMode::FreeRoam)
CULT_CAMERA_SAFETY_TEST(FCultDomCameraSafety,"CultUlhu.CameraSafety.Domination",ECultPlayMode::Domination)
CULT_CAMERA_SAFETY_TEST(FCultMobaCameraSafety,"CultUlhu.CameraSafety.Dotgoo",ECultPlayMode::Dotgoo)
#undef CULT_CAMERA_SAFETY_TEST
#endif
