#include "CultMatchNetwork.h"
#include "CultMatchState.h"
#include "CultUlhuPlayerController.h"
#include "CultMatchSpawn.h"
#include "CultCanonicalKit.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/StaticMesh.h"
struct FCultMatchNetAccess {
 static void Serialize(FArchive& A,ACultMatchDirector* M) {
  if(!M->State_) return; auto& S=*M->State_; int32 Mode=int(S.Mode); A<<Mode; if(Mode<1||Mode>2) { A.SetError(); return; }
  if(A.IsLoading()&&S.Mode!=ECultPlayMode(Mode)) { S.Mode=ECultPlayMode(Mode); if(auto* G=Cast<UCultUlhuGameInstance>(M->GetGameInstance())) G->SelectedMode=S.Mode; }
  A<<S.Time<<S.Warmup<<S.Over<<S.Winner<<S.NextWave<<S.Wave<<S.Notice<<S.NoticeTime<<S.PingPoint<<S.PingTime<<S.PingTeam;
  int32 Holder=M->Holder(); bool Contested=M->Contested(),Overtime=M->Overtime(); A<<Holder<<Contested<<Overtime;
  if(A.IsLoading()) { S.RemoteHolder=Holder; S.RemoteContested=Contested; S.RemoteOvertime=Overtime; }
  for(int Team=0;Team<2;++Team) { float Score=M->Score(Team),Core=M->CoreHealth(Team); int Towers=M->TowersAlive(Team); A<<Score<<Core<<Towers<<S.NexusDestroyed[Team]; if(A.IsLoading()) { S.RemoteScores[Team]=Score; S.RemoteCore[Team]=Core; S.RemoteTowers[Team]=Towers; } }
  int32 N=S.Fighters.Num(); A<<N; if(N<0||N>10) { A.SetError(); return; } if(A.IsLoading()) S.Fighters.SetNum(N);
  for(auto& F:S.Fighters) A<<F.Kit<<F.Name<<F.Team<<F.Lane<<F.XP<<F.Level<<F.Kills<<F.Deaths<<F.Assists<<F.Invokes<<F.FirstPick<<F.Respawn<<F.Shield<<F.Recall<<F.Invoke<<F.RelicMult<<F.DamageDone<<F.DamageTaken;
  N=S.Relics.Num(); A<<N; if(N<0||N>8) { A.SetError(); return; } if(A.IsLoading()) S.Relics.SetNum(N);
  for(auto& R:S.Relics) A<<R.Point<<R.Name<<R.Tier<<R.Amp<<R.Spawn<<R.Channel<<R.Need<<R.Claim<<R.ChannelSlot;
  N=S.Eliminations.Num(); A<<N; if(N<0||N>32) { A.SetError(); return; } if(A.IsLoading()) S.Eliminations.SetNum(N);
  for(auto& E:S.Eliminations) A<<E.Attacker<<E.Victim<<E.AttackerTeam<<E.VictimTeam<<E.Remaining<<E.Local<<E.AttackerSlot<<E.VictimSlot;
  if(A.IsLoading()) { if(S.Over) S.LocalMenu=false; S.RemoteReady=true; M->RefreshRemoteMatch(); if(S.Over) if(auto* PC=M->GetWorld()->GetFirstPlayerController()) { PC->bShowMouseCursor=true; FInputModeGameAndUI Input; Input.SetHideCursorDuringCapture(false); PC->SetInputMode(Input); } }
 }
};
bool CultMatchNetwork::Body(FArchive& A,AActor* Actor) {
 if(auto* M=Cast<ACultMatchDirector>(Actor)) { FCultMatchNetAccess::Serialize(A,M); return true; }
 return Actor->IsA<ACultMatchUnit>(); // Unit fields and movement use native actor replication.
}
void ACultMatchDirector::RefreshRemoteMatch() {
 if(HasAuthority()||!State_) return; auto& S=*State_;
 if(S.Mode==ECultPlayMode::Dotgoo&&!S.Moba) { S.Moba=std::make_unique<FCultNativeMoba>(S.Bus,S.Clock,S.Rng); S.Moba->Setup(); }
 if(S.Mode==ECultPlayMode::Domination&&!S.Capture) { S.Capture=std::make_unique<cultulhu::CapturePointMode>(S.Bus,S.Clock); S.Capture->addPoint({0,0,0},35); }
 for(auto& R:S.Relics) {
  const bool Visible=R.Claim<0&&S.Time>=R.Spawn;
  if(!Visible&&R.Visual.IsValid()) { R.Visual->Destroy(); R.Visual.Reset(); }
  if(Visible&&!R.Visual.IsValid()) {
   auto* A=GetWorld()->SpawnActor<AStaticMeshActor>(R.Point+FVector(0,0,300),FRotator::ZeroRotator); auto* C=A->GetStaticMeshComponent(); C->SetMobility(EComponentMobility::Movable); C->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Sphere"))); C->SetWorldScale3D(FVector(R.Tier+1)); C->SetCollisionProfileName(TEXT("NoCollision")); R.Visual=A;
  }
 }
 S.Units.Reset(); for(TActorIterator<ACultMatchUnit> It(GetWorld());It;++It) S.Units.Add(*It);
 for(int I=0;I<S.Fighters.Num();++I) {
  auto& F=S.Fighters[I]; const FName Tag(*FString::Printf(TEXT("MatchFighter%d"),I));
  for(TActorIterator<ACultUlhuPlayerPawn> It(GetWorld());It;++It) if(It->ActorHasTag(Tag)) { F.Pawn=*It; break; }
 }
}
bool ACultMatchDirector::AddHumanPlayer(APlayerController* PC,const FString& Character) {
 if(!HasAuthority()||!State_||!PC||!CultCanonicalKit::IsReleased(Character)) return false;
 for(const auto& F:State_->Fighters) if(F.Pawn.IsValid()&&F.Pawn->GetController()==PC) return true;
 int Humans[2]={}; for(const auto& F:State_->Fighters) if(F.Pawn.IsValid()&&!F.Pawn->IsBot()) ++Humans[F.Team];
 const int Team=Humans[0]<=Humans[1]?0:1; int SlotIndex=-1;
 for(int I=0;I<State_->Fighters.Num();++I) if(State_->Fighters[I].Team==Team&&(!State_->Fighters[I].Pawn.IsValid()||State_->Fighters[I].Pawn->IsBot())) { SlotIndex=I; break; }
 if(SlotIndex<0) return false;
 auto& F=State_->Fighters[SlotIndex]; auto* OldBot=F.Pawn.Get(); APawn* OldPawn=PC->GetPawn();
 const FVector Feet=CultMatchSpawn::Feet(Mode(),Team,SlotIndex%5); FTransform Transform(Feet+FVector(0,0,2000));
 auto* P=GetWorld()->SpawnActorDeferred<ACultUlhuPlayerPawn>(ACultUlhuPlayerPawn::StaticClass(),Transform,PC,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn); if(!P) return false;
 P->ConfigurePlayer(Character,Team); P->Tags.Add(FName(*FString::Printf(TEXT("MatchFighter%d"),SlotIndex))); P->FinishSpawning(Transform); PC->Possess(P);
 if(auto* C=Cast<ACultUlhuPlayerController>(PC)) C->MatchSlot=SlotIndex;
 F.Pawn=P; F.Kit=Character; FString Name=Character; for(const auto& Entry:UCultUlhuGameInstance::Roster()) if(Entry.Id==Character) { Name=Entry.DisplayName; int Comma; if(Name.FindChar(',',Comma)) Name.LeftInline(Comma); break; } F.Name=FString::Printf(TEXT("Player %d / %s"),SlotIndex+1,*Name); F.Respawn=0; F.Recall=F.Invoke=0; F.Retreating=false; F.Shield=3; P->RespawnMatch(Feet);
 if(OldBot&&OldBot!=P) OldBot->Destroy(); if(OldPawn&&OldPawn!=OldBot&&OldPawn!=P) OldPawn->Destroy(); ForceNetUpdate();
 UE_LOG(LogTemp,Display,TEXT("[CultRoom] Human joined team %d slot %d as %s"),Team,SlotIndex,*Character); return true;
}
void ACultMatchDirector::RemoveHumanPlayer(AController* PC) {
 if(!HasAuthority()||!State_||GetWorld()->bIsTearingDown) return;
 auto* Controller=Cast<ACultUlhuPlayerController>(PC); int I=Controller?Controller->MatchSlot:-1;
 if(I<0) for(int SlotIndex=0;SlotIndex<State_->Fighters.Num();++SlotIndex) if(State_->Fighters[SlotIndex].Pawn.IsValid()&&State_->Fighters[SlotIndex].Pawn->GetController()==PC) { I=SlotIndex; break; }
 if(!State_->Fighters.IsValidIndex(I)) return;
 if(Controller) Controller->MatchSlot=-1; PC->UnPossess();
 if(State_->Fighters[I].Pawn.IsValid()) State_->Fighters[I].Pawn->Destroy(); State_->Fighters[I].Pawn.Reset(); State_->Fighters[I].Name=State_->Fighters[I].Kit+TEXT(" (BOT)"); SpawnFighter(I); ForceNetUpdate();
}
