#include "TimerManager.h"
#include "CultUlhuPlayerPawn.h"
#include "CultUlhuPlayerController.h"
#include "CultUlhuGameInstance.h"
#include "CultWorldDirector.h"
#include "CultPopulation.h"
#include "CultLivingLandscape.h"
#include "CultMatchDirector.h"
#include "CultCanonicalKit.h"
#include "Net/UnrealNetwork.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "EngineUtils.h"

void PopulateCultWilds(UWorld* World);
void ACultUlhuPlayerPawn::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const {
 Super::GetLifetimeReplicatedProps(OutLifetimeProps); DOREPLIFETIME(ACultUlhuPlayerPawn,CharacterId_); DOREPLIFETIME(ACultUlhuPlayerPawn,bBotControlled_); DOREPLIFETIME(ACultUlhuPlayerPawn,Faction_); DOREPLIFETIME(ACultUlhuPlayerPawn,bMatchDead_);
}
void ACultUlhuPlayerPawn::ServerCombatInput_Implementation(uint8 Action,bool Down,FVector_NetQuantize Origin,FVector_NetQuantizeNormal Direction) {
 if((Action!=uint8(ECultPlayerInput::Jump)&&(Action<uint8(ECultPlayerInput::Primary)||Action>uint8(ECultPlayerInput::Interact)))||IsBot()) return;
 if(auto* M=ACultMatchDirector::Find(GetWorld())) if(!M->CanAct(this)) return;
 if(Origin.ContainsNaN()||Direction.ContainsNaN()||Direction.IsNearlyZero()||FVector::Dist(Origin,GetActorLocation())>GetCapsuleComponent()->GetScaledCapsuleHalfHeight()*5) return;
 NetworkAimOrigin_=Origin; NetworkAimDirection_=FVector(Direction).GetSafeNormal(); NetworkAimAt_=GetWorld()->GetTimeSeconds();
 RoutePlayerInput(ECultPlayerInput(Action),Down);
}
void ACultUlhuPlayerPawn::ServerDash_Implementation(FVector_NetQuantizeNormal Direction) { if(!Direction.ContainsNaN()) { if(auto* M=ACultMatchDirector::Find(GetWorld())) if(!M->CanAct(this)) return; TryDash(Direction); } }
void ACultUlhuPlayerPawn::ServerSprint_Implementation(bool Sprint) { NetworkSprinting_=Sprint; SetSprinting(Sprint); }
void ACultUlhuPlayerController::ServerChooseFreeRoamCharacter_Implementation(const FString& Id) {
 if(FreeRoamCharacterChosen_||!CultCanonicalKit::IsReleased(Id)) return;
 if(ACultMatchDirector::IsMatch(GetWorld())) {
  if(auto* M=ACultMatchDirector::Find(GetWorld())) { if(M->AddHumanPlayer(this,Id)) FreeRoamCharacterChosen_=true; else ClientReturnToMainMenuWithTextReason(FText::FromString(TEXT("This room is full."))); }
  else if(++CharacterJoinAttempts_<120) GetWorld()->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this,[this,Id](){ServerChooseFreeRoamCharacter_Implementation(Id);}));
  else ClientReturnToMainMenuWithTextReason(FText::FromString(TEXT("The match did not become ready. Please reconnect.")));
  return;
 }
 FreeRoamCharacterChosen_=true;
 auto* Old=Cast<ACultUlhuPlayerPawn>(GetPawn());
 if(Old&&Old->CombatHUD().Name.IsEmpty()) return;
 const FTransform Transform=Old?Old->GetActorTransform():FTransform(FVector(0,0,2500));
 auto* NewPawn=GetWorld()->SpawnActorDeferred<ACultUlhuPlayerPawn>(ACultUlhuPlayerPawn::StaticClass(),Transform,this,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
 if(!NewPawn) return; NewPawn->ConfigurePlayer(Id); NewPawn->Tags.Add(TEXT("CoopPlayerReady")); NewPawn->FinishSpawning(Transform); Possess(NewPawn); if(Old) Old->Destroy();
 UE_LOG(LogTemp,Display,TEXT("[CultCoop] Joined player %s as %s"),*GetName(),*Id);
}
void ACultUlhuPlayerController::ServerCultOrder_Implementation(int32 Action,AActor* Target,FVector_NetQuantize Point) {
 auto* P=Cast<ACultUlhuPlayerPawn>(GetPawn()); if(!P||ACultMatchDirector::IsMatch(GetWorld())||Point.ContainsNaN()||((Action<ACultWorldDirector::ExploreRegion||Action>ACultWorldDirector::RaidRegion)&&FVector::Dist2D(Point,P->FeetLocation())>30000)) return;
 if(auto* D=ACultWorldDirector::Find(GetWorld())) P->Notify(D->Execute(Action,Target,Point));
}
void ACultUlhuPlayerController::ServerCultSelection_Implementation(const TArray<ACultPopulation*>& Units) {
 if(Units.Num()>500||ACultMatchDirector::IsMatch(GetWorld())) return;
 TArray<ACultPopulation*> Owned; for(auto* P:Units) if(IsValid(P)&&P->IsCultist()&&P->GetFaction()==0) Owned.Add(P);
 if(auto* D=ACultWorldDirector::Find(GetWorld())) D->SelectFollowers(Owned);
}
void ACultLivingLandscape::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const { Super::GetLifetimeReplicatedProps(OutLifetimeProps); DOREPLIFETIME(ACultLivingLandscape,WorldSeed); }
void ACultLivingLandscape::OnRep_Seed() {
 if(HasAuthority()) return;
 // The authored level is retained on disk; each joining client uses the shared generated world.
 for(TActorIterator<AActor> It(GetWorld());It;++It) if((It->IsNetStartupActor()||It->HasAnyFlags(RF_WasLoaded))&&*It!=this) {
  TArray<UStaticMeshComponent*> Parts; It->GetComponents(Parts);
  for(auto* C:Parts) { C->SetVisibility(false); C->SetCollisionEnabled(ECollisionEnabled::NoCollision); }
 }
 Generate(WorldSeed); PopulateCultWilds(GetWorld());
 if(auto* GI=Cast<UCultUlhuGameInstance>(GetGameInstance())) { GI->FreeRoamSeed=WorldSeed; GI->SelectedMode=ECultPlayMode::FreeRoam; GI->FreeRoamReady=true; }
 UE_LOG(LogTemp,Display,TEXT("[CultCoop] Client generated shared terrain seed %d"),WorldSeed);
}

void ACultUlhuPlayerController::ServerMatchAction_Implementation(uint8 Action,FVector_NetQuantize Point) {
 auto* P=Cast<ACultUlhuPlayerPawn>(GetPawn()); auto* M=ACultMatchDirector::Find(GetWorld()); if(!P||!M||Point.ContainsNaN()) return;
 if(Action==0) M->StartRecall(P); else if(Action==1) M->PingAt(P,Point);
}
