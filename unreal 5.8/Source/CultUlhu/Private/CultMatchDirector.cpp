#include "CultMatchDirector.h"
#include "CultHUDSpace.h"
#include "CultMatchState.h"
#include "CultMobaJungle.h"
#include "CultDominationRoute.h"
#include "CultJungleCamp.h"
#include "CultUlhuPlayerPawn.h"
#include "CultMatchUnit.h"
#include "CultWorldDirector.h"
#include "CultSkillAttack.h"
#include "CultKitRuntime.h"
#include "CultCorpse.h"
#include "CultBlastFX.h"
#include "Kismet/GameplayStatics.h"
#include "EngineUtils.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "modes/Match.h"

ACultMatchDirector::ACultMatchDirector() { PrimaryActorTick.bCanEverTick=true; }
bool ACultMatchDirector::IsMatch(UWorld* W) { auto* G=W?Cast<UCultUlhuGameInstance>(W->GetGameInstance()):nullptr; return G&&G->SelectedMode!=ECultPlayMode::FreeRoam&&!W->GetName().Contains(TEXT("L_MainMenu")); }
ACultMatchDirector* ACultMatchDirector::Find(UWorld* W) { if(W) for(TActorIterator<ACultMatchDirector> It(W);It;++It) return *It; return nullptr; }
ECultPlayMode ACultMatchDirector::Mode() const { return State_?State_->Mode:ECultPlayMode::FreeRoam; }
bool ACultMatchDirector::IsOver() const { return State_&&State_->Over; }
bool ACultMatchDirector::IsPaused() const { return State_&&State_->Paused; }
int ACultMatchDirector::Winner() const { return State_?State_->Winner:-1; }
float ACultMatchDirector::Elapsed() const { return State_?State_->Time:0; }
float ACultMatchDirector::Warmup() const { return State_?State_->Warmup:0; }
float ACultMatchDirector::Score(int T) const { return State_&&State_->Capture?State_->Capture->score(FMath::Clamp(T,0,1)):0; }
float ACultMatchDirector::CoreHealth(int T) const { return State_&&State_->Moba?State_->Moba->baseHp(FMath::Clamp(T,0,1)):0; }
int ACultMatchDirector::TowersAlive(int T) const { return State_&&State_->Moba?State_->Moba->aliveTowerCount(FMath::Clamp(T,0,1)):0; }
bool ACultMatchDirector::LaneWardsCleared(int T) const { return State_&&State_->Moba&&TowersAlive(T)==0; }
bool ACultMatchDirector::NexusAlive(int T) const { return State_&&State_->Moba&&!State_->NexusDestroyed[FMath::Clamp(T,0,1)]; }
bool ACultMatchDirector::CoreOpen(int T) const { return LaneWardsCleared(T)&&!NexusAlive(T); }
FVector ACultMatchDirector::LaneWaypoint(int T,int L,int Step) const { return State_&&State_->Moba?State_->Moba->Waypoint(T,L,Step):FVector::ZeroVector; }
bool ACultMatchDirector::Contested() const { return State_&&State_->Capture&&State_->Capture->contested(); }
bool ACultMatchDirector::Overtime() const { return State_&&State_->Capture&&State_->Capture->overtime(); }
int ACultMatchDirector::Holder() const { return State_&&State_->Capture?State_->Capture->holder():-1; }
FVector ACultMatchDirector::Base(int T) const { return FVector((T==0?-1:1)*(Mode()==ECultPlayMode::Dotgoo?30000.f:23000.f),0,0); }
const TArray<FCultMatchFighter>& ACultMatchDirector::Fighters() const { static const TArray<FCultMatchFighter> Empty; return State_?State_->Fighters:Empty; }
int ACultMatchDirector::Slot(const AActor* P) const { if(State_) for(int I=0;I<State_->Fighters.Num();++I) if(State_->Fighters[I].Pawn==P) return I; return -1; }
const FCultMatchFighter* ACultMatchDirector::Fighter(const ACultUlhuPlayerPawn* P) const { int I=Slot(P); return I>=0?&State_->Fighters[I]:nullptr; }
float ACultMatchDirector::RespawnFor(const ACultUlhuPlayerPawn* P) const { auto* F=Fighter(P); return F?F->Respawn:0; }
bool ACultMatchDirector::CanAct(const ACultUlhuPlayerPawn* P) const { auto* F=Fighter(P); return F&&F->Respawn<=0&&P->GetHealth()>0&&!State_->Over&&!State_->Paused&&State_->Warmup<=0; }
void ACultMatchDirector::BeginPlay() {
 Super::BeginPlay(); State_=new FState;
 auto* GI=Cast<UCultUlhuGameInstance>(GetGameInstance()); State_->Mode=GI->SelectedMode;
 if(Mode()==ECultPlayMode::Domination) { State_->Capture=std::make_unique<cultulhu::CapturePointMode>(State_->Bus,State_->Clock); State_->Capture->addPoint({0,0,0},35); }
 else {
  State_->Moba=std::make_unique<FCultNativeMoba>(State_->Bus,State_->Clock,State_->Rng); State_->Moba->Setup();
  int I=0; for(const auto& T:State_->Moba->Towers()) { AddUnit(T.team,T.lane,3,I++,FVector(T.pos.x,T.pos.z,0)*200,T.hp,T.dps,T.range*200+1800,0); }
  for(int T=0;T<2;++T) {
   AddUnit(T,1,4,T,Base(T),cultulhu::MobaDefense::GOO_MAX_HP,0,0,0);
   AddUnit(T,-1,5,T,Base(T)+FVector(T?-4000:4000,0,0),4000,22,4600,0);
  }
  State_->Relics={{FCultMobaJungle::Ground(0),TEXT("Jungle relic"),1,.12f,0,0,0}, {FCultMobaJungle::Ground(1),TEXT("Jungle relic"),1,.12f,0,0,0}, {FCultMobaJungle::Ground(2),TEXT("Eldritch relic"),2,.23f,0,0,4}, {FCultMobaJungle::Ground(3),TEXT("Eldritch relic"),2,.23f,0,0,4}, {FVector(0,0,0),TEXT("Mythic relic"),3,.38f,600,0,6}};
 }
 const auto& Roster=UCultUlhuGameInstance::Roster();
 // Mirrored compositions keep the local match fair; deferred IDs cannot enter here.
 const FString Picks[]={GI->SelectedCharacterId,TEXT("dagon"),TEXT("yig"),TEXT("mother_hydra"),TEXT("nyarlathotep")};
 for(int I=0;I<10;++I) {
  FCultMatchFighter F; F.Team=I/5; F.Lane=(I%5)%3; F.Kit=Picks[I%5];
  for(const auto& R:Roster) if(R.Id==F.Kit) { F.Name=R.DisplayName; int Comma; if(F.Name.FindChar(',',Comma)) F.Name.LeftInline(Comma); }
  if(I==0) { F.Pawn=Cast<ACultUlhuPlayerPawn>(UGameplayStatics::GetPlayerPawn(GetWorld(),0)); F.Name+=TEXT(" (YOU)"); }
  State_->Fighters.Add(F); SpawnFighter(I);
 }
 if(Mode()==ECultPlayMode::Dotgoo) for(int Camp=0;Camp<FCultMobaJungle::Camps;++Camp) {
  FTransform Transform(FCultMobaJungle::Ground(Camp));
  auto* Encounter=GetWorld()->SpawnActorDeferred<ACultJungleCamp>(ACultJungleCamp::StaticClass(),Transform,this,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
  Encounter->CampIndex=Camp; Encounter->FinishSpawning(Transform);
 }
 Announce(Mode()==ECultPlayMode::Domination?TEXT("DOMINATION | Hold the seal. Hero kills +5. First to 400."):TEXT("DEFENSE OF THE GREAT OLD ONES | Break all six lane wards, then the nexus ward and their god."));
 UE_LOG(LogTemp,Log,TEXT("[CultMatch] %s ready: 10 heroes, %d objective actors"),*UCultUlhuGameInstance::ModeDisplayName(Mode()),State_->Units.Num());
}
void ACultMatchDirector::EndPlay(const EEndPlayReason::Type R) { delete State_; State_=nullptr; Super::EndPlay(R); }
void ACultMatchDirector::SpawnFighter(int I) {
 auto& F=State_->Fighters[I]; const FVector Offset((F.Team==0?1:-1)*2600,((I%5)-2)*1500,0); const FVector Feet=Base(F.Team)+Offset;
 if(!F.Pawn.IsValid()) {
  FTransform T(Feet+FVector(0,0,1500)); auto* P=GetWorld()->SpawnActorDeferred<ACultUlhuPlayerPawn>(ACultUlhuPlayerPawn::StaticClass(),T,nullptr,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
  P->ConfigureBot(F.Kit,F.Team); F.Pawn=P; P->FinishSpawning(T);
 }
 if(F.Pawn.IsValid()) { F.Pawn->RespawnMatch(Feet); F.Pawn->SetActorRotation(FRotator(0,F.Team?180:0,0)); }
 F.DomPath.Reset(); F.DomPathIndex=0; F.DomEscape=false;
 if(Mode()==ECultPlayMode::Domination&&I%5==4) F.DomPath=FCultDominationRoute::Flank(F.Team,(F.Team?-1:1)*(F.Deaths%2?-1:1));
 F.Shield=3; F.Respawn=0; F.Recall=F.Invoke=0; F.RouteStep=1; F.Retreating=false; if(F.Pawn.IsValid()) F.Pawn->MarkCombat();
}
void ACultMatchDirector::Announce(const FString& Text) { State_->Notice=Text; State_->NoticeTime=6; UE_LOG(LogTemp,Log,TEXT("[CultMatch] %s"),*Text); }
FString ACultMatchDirector::Announcement() const { return State_&&State_->NoticeTime>0?State_->Notice:FString(); }
void ACultMatchDirector::Tick(float Dt) {
 Super::Tick(Dt); if(!State_) return; State_->NoticeTime=FMath::Max(0.f,State_->NoticeTime-Dt);
 if(State_->Over||State_->Paused) return;
 if(State_->Warmup>0) { State_->Warmup=FMath::Max(0.f,State_->Warmup-Dt); if(State_->Warmup<=0) Announce(TEXT("The battle begins. Follow your objective marker.")); return; }
 State_->Time+=Dt; State_->Clock.advance(Dt); State_->PingTime=FMath::Max(0.f,State_->PingTime-Dt);
 UpdateFighters(Dt);
 if(State_->Capture) {
  int Counts[2]={0,0};
  for(const auto& F:State_->Fighters) if(F.Pawn.IsValid()&&F.Respawn<=0&&F.Pawn->GetHealth()>0&&InsideCapture(F.Pawn->FeetLocation())) ++Counts[F.Team];
  int Prev=Holder(); bool WasOT=Overtime(); State_->Capture->setOccupants(0,Counts[0],Counts[1]); State_->Capture->update(Dt);
  if(Prev!=Holder()&&Holder()>=0) Announce(Holder()==0?TEXT("Your team controls the seal +1 / second"):TEXT("Enemy team controls the seal. Contest it to stop scoring."));
  if(!WasOT&&Overtime()) Announce(TEXT("SUDDEN DEATH | First score wins. Contest the seal!"));
  if(State_->Capture->isOver()) Finish(State_->Capture->winner());
 } else {
  State_->Moba->Advance(Dt); State_->NextWave-=Dt; if(State_->NextWave<=0) { SpawnWaves(); State_->NextWave=cultulhu::MobaDefense::WAVE_INTERVAL; }
  UpdateUnits(Dt); UpdateRelics(Dt);
  if(State_->Moba->isOver()) Finish(State_->Moba->winner());
 }
}
void ACultMatchDirector::UpdateFighters(float Dt) {
 for(int I=0;I<State_->Fighters.Num();++I) {
  auto& F=State_->Fighters[I]; F.Shield=FMath::Max(0.f,F.Shield-Dt);
  if(F.Respawn>0) { F.Respawn-=Dt; if(F.Respawn<=0) SpawnFighter(I); continue; }
  if(!F.Pawn.IsValid()) continue; auto* P=F.Pawn.Get();
  if(FVector::Dist2D(P->FeetLocation(),Base(F.Team))<3400&&State_->Time-F.LastHurt>2) P->HealMatch(P->GetMaxHealth()*.12f*Dt);
  if(P->FeetLocation().Z<(Mode()==ECultPlayMode::Domination?-6500.f:-500.f)||FMath::Abs(P->GetActorLocation().X)>(Mode()==ECultPlayMode::Dotgoo?39000:31000)||FMath::Abs(P->GetActorLocation().Y)>(Mode()==ECultPlayMode::Dotgoo?22000:29000)) P->RelocateMatch(Base(F.Team)+FVector(F.Team?-2600:2600,0,0));
  if(F.Recall>0||F.Invoke>0) {
   if(FVector::Dist2D(P->FeetLocation(),F.ChannelOrigin)>80) { F.Recall=F.Invoke=0; P->Notify(TEXT("Channel interrupted.")); }
   else if(F.Recall>0) { F.Recall-=Dt; if(F.Recall<=0) { P->RelocateMatch(Base(F.Team)+FVector(F.Team?-2600:2600,0,0)); P->Notify(TEXT("Returned to your temple. Healing.")); } }
   else { F.Invoke+=Dt; if(F.Invoke>=20) { --F.Invokes; F.RelicMult=FMath::Min(3.f,F.RelicMult+(F.Level>=30?.38f:F.Level>=20?.23f:.12f)); F.Invoke=0; P->Notify(TEXT("Artifact invoked. Damage power increased.")); } }
  }
  if(P->IsBot()) BotThink(I,Dt);
 }
}
bool ACultMatchDirector::AbilityAvailable(ACultUlhuPlayerPawn* P,int Which,bool Choose) {
 auto* Existing=Fighter(P); if(!Existing||!CanAct(P)) return false; if(Mode()!=ECultPlayMode::Dotgoo||Which<0) return true;
 auto& F=State_->Fighters[Slot(P)];
 if(Which==2) return F.Level>=6;
 if(F.Level>=3) return true;
 if(F.FirstPick==Which) return true;
 if(F.FirstPick<0&&Choose) { F.FirstPick=Which; P->Notify(TEXT("First ability chosen. The other unlocks at level 3; R at level 6.")); return true; }
 return false;
}
void ACultMatchDirector::OnAction(ACultUlhuPlayerPawn* P) { int I=Slot(P); if(I>=0) { auto& F=State_->Fighters[I]; F.Recall=F.Invoke=0; F.LastAction=State_->Time; F.Shield=0; } }
bool ACultMatchDirector::DamageAllowed(ACultUlhuPlayerPawn* P,AActor* Source) {
 if(!CanAct(P)) return false; int I=Slot(P); if(State_->Fighters[I].Shield>0) return false;
 if(Source) { if(auto* Hero=Cast<ACultUlhuPlayerPawn>(Source)) if(Hero->GetFaction()==P->GetFaction()) return false; if(auto* Unit=Cast<ACultMatchUnit>(Source)) if(Unit->Team==P->GetFaction()) return false; }
 return true;
}
void ACultMatchDirector::NoteDamage(ACultUlhuPlayerPawn* P,AActor* Source,float Actual) {
 if(Actual<=0) return; int I=Slot(P); if(I<0) return; auto& F=State_->Fighters[I]; F.LastHurt=State_->Time; F.Recall=F.Invoke=0;
 F.DamageTaken+=Actual; int From=Slot(Source); if(From>=0&&From!=I) { F.RecentAttackers.Add(From,State_->Time); State_->Fighters[From].DamageDone+=Actual; }
}
void ACultMatchDirector::HeroDied(ACultUlhuPlayerPawn* P,AActor* Source) {
 int I=Slot(P); if(I<0||State_->Fighters[I].Respawn>0||State_->Over) return;
 auto& Victim=State_->Fighters[I]; ++Victim.Deaths; int Killer=Slot(Source);
 if(Killer<0) { float Latest=-100; for(auto& Hit:Victim.RecentAttackers) if(State_->Time-Hit.Value<8&&Hit.Value>Latest) { Latest=Hit.Value; Killer=Hit.Key; } }
 if(Killer>=0&&State_->Fighters[Killer].Team!=Victim.Team) {
  auto& K=State_->Fighters[Killer]; ++K.Kills;
  if(State_->Capture) State_->Capture->notePlayerKill(K.Team);
  GrantXP(Killer,50+25*Victim.Level);
  Announce(K.Name+TEXT(" eliminated ")+Victim.Name+(State_->Capture?TEXT("  +5"):TEXT("")));
 }
 for(auto& Hit:Victim.RecentAttackers) if(Hit.Key!=Killer&&State_->Time-Hit.Value<8) { ++State_->Fighters[Hit.Key].Assists; GrantXP(Hit.Key,35); }
 Victim.RecentAttackers.Reset(); Victim.Respawn=Mode()==ECultPlayMode::Domination?5:FMath::Min(20.f,5.f+State_->Time/60); Victim.Recall=Victim.Invoke=0;
 P->SetMatchDead();
}
float ACultMatchDirector::DamageMultiplier(const ACultUlhuPlayerPawn* P) const { auto* F=Fighter(P); return F?(1.f+.02f*F->Level)*F->RelicMult:1.f; }
void ACultMatchDirector::GrantXP(int I,int Amount) {
 if(!State_||!State_->Fighters.IsValidIndex(I)||Mode()!=ECultPlayMode::Dotgoo||Amount<=0) return;
 auto& F=State_->Fighters[I]; int Old=F.Level; F.XP+=Amount; F.Level=FMath::Min(30,1+F.XP/50);
 if(F.Level>Old) { F.Invokes+=F.Level-Old; if(I==0&&F.Pawn.IsValid()) F.Pawn->Notify(FString::Printf(TEXT("LEVEL %d%s"),F.Level,F.Level>=6&&Old<6?TEXT(" | Ultimate unlocked!"):F.Level>=3&&Old<3?TEXT(" | Both Q and F unlocked!"):TEXT(""))); }
}
void ACultMatchDirector::StartRecall(ACultUlhuPlayerPawn* P) {
 if(!CanAct(P)) return; auto& F=State_->Fighters[Slot(P)];
 if(F.Recall>0) { F.Recall=0; return; } F.Invoke=0; F.Recall=8; F.ChannelOrigin=P->FeetLocation(); P->Notify(TEXT("Recalling: remain still for 8 seconds. Damage or attacking cancels."));
}
void ACultMatchDirector::Interact(ACultUlhuPlayerPawn* P) {
 if(!CanAct(P)) return; auto& F=State_->Fighters[Slot(P)];
 if(Mode()==ECultPlayMode::Dotgoo&&FVector::Dist2D(P->FeetLocation(),Base(F.Team))<3400) {
  if(F.Level<10||F.Invokes<=0) { P->Notify(TEXT("Artifact invocation requires level 10 and a level-up charge. The temple heals you automatically.")); return; }
  F.Recall=0; F.Invoke=.001f; F.ChannelOrigin=P->FeetLocation(); P->Notify(TEXT("Invoking an artifact: stay still for 20 seconds.")); return;
 }
 P->Notify(TEXT("G: recall to your healing temple. Jungle relics channel automatically when eligible and uncontested."));
}
void ACultMatchDirector::Ping(ACultUlhuPlayerPawn* P) {
 if(!CanAct(P)) return; auto Hit=P->CameraAim(25000,true); if(!Hit.bBlockingHit) { P->Notify(TEXT("Aim your rally ping at solid ground.")); return; } State_->PingPoint=Hit.ImpactPoint; State_->PingPoint.X=FMath::Clamp(State_->PingPoint.X,Mode()==ECultPlayMode::Dotgoo?-35000.f:-28000.f,Mode()==ECultPlayMode::Dotgoo?35000.f:28000.f); State_->PingPoint.Y=FMath::Clamp(State_->PingPoint.Y,Mode()==ECultPlayMode::Dotgoo?-18000.f:-27000.f,Mode()==ECultPlayMode::Dotgoo?18000.f:27000.f); State_->PingTime=12; Announce(TEXT("Rally ping sent. Nearby allies will support this position."));
}
void ACultMatchDirector::Finish(int T) {
 if(State_->Over) return; State_->Over=true; State_->Winner=T;
 for(TActorIterator<ACultSkillAttack> It(GetWorld());It;++It) It->Destroy();
 for(auto& F:State_->Fighters) if(F.Pawn.IsValid()) { F.Pawn->GetCharacterMovement()->DisableMovement(); F.Pawn->SetActorTickEnabled(false); }
 for(auto& U:State_->Units) if(U.IsValid()) U->SetActorTickEnabled(false);
 Announce(T<0?TEXT("DRAW"):T==0?TEXT("VICTORY"):TEXT("DEFEAT"));
 if(auto* PC=GetWorld()->GetFirstPlayerController()) { PC->bShowMouseCursor=true; FInputModeGameAndUI Input; Input.SetHideCursorDuringCapture(false); PC->SetInputMode(Input); }
}
void ACultMatchDirector::TogglePause() {
 if(!State_||State_->Over) return; State_->Paused=!State_->Paused; UGameplayStatics::SetGamePaused(this,State_->Paused);
 if(auto* PC=GetWorld()->GetFirstPlayerController()) { PC->bShowMouseCursor=State_->Paused; if(State_->Paused) { FInputModeGameAndUI Input; Input.SetHideCursorDuringCapture(false); PC->SetInputMode(Input); } else PC->SetInputMode(FInputModeGameOnly()); }
}
void ACultMatchDirector::Rematch() { if(State_&&(State_->Over||State_->Paused)) { auto* G=Cast<UCultUlhuGameInstance>(GetGameInstance()); G->bPlayChosen=true; UGameplayStatics::SetGamePaused(this,false); if(auto* PC=GetWorld()->GetFirstPlayerController()) { PC->bShowMouseCursor=false; PC->SetInputMode(FInputModeGameOnly()); } UGameplayStatics::OpenLevel(this,TEXT("/Game/Maps/L_DarkWilds")); } }
void ACultMatchDirector::ClickResult() {
 if(!IsOver()&&!IsPaused()) return; auto* PC=GetWorld()->GetFirstPlayerController(); FVector2D Pointer; if(!CultHUDSpace::Pointer(PC,Pointer)) return;
 for(int I=0;I<State_->ResultButtons.Num();++I) if(State_->ResultButtons[I].IsInside(Pointer)) {
  if(IsPaused()&&I==0) TogglePause();
  else if((IsOver()&&I==0)||(IsPaused()&&I==1)) Rematch();
  else if(auto* G=Cast<UCultUlhuGameInstance>(GetGameInstance())) { UGameplayStatics::SetGamePaused(this,false); G->ReturnToMenu(); } return;
 }
}
FString ACultMatchDirector::Hint(const ACultUlhuPlayerPawn* P) const {
 auto* F=Fighter(P); if(!F) return FString();
 if(F->Respawn>0) return FString::Printf(TEXT("Reforming at your temple in %.0fs"),FMath::CeilToFloat(F->Respawn));
 if(State_->Warmup>0) return FString::Printf(TEXT("BATTLE STARTS IN %.0f"),FMath::CeilToFloat(State_->Warmup));
 if(F->Recall>0) return FString::Printf(TEXT("RECALLING  %.1fs | Movement or damage cancels"),F->Recall);
 if(F->Invoke>0) return FString::Printf(TEXT("INVOKING ARTIFACT  %.0f%%"),F->Invoke/20*100);
 if(Mode()==ECultPlayMode::Domination&&P->FeetLocation().Z< -800) return TEXT("CATACOMB | Side ramps exit through the keeps | North / south ramps reach the outer courts");
 if(Mode()==ECultPlayMode::Domination) return TEXT("Hold the central seal +1/s | Hero kills +5 | G recall | ALT + click rally");
 for(TActorIterator<ACultJungleCamp> It(GetWorld());It;++It) if(FVector::Dist2D(P->FeetLocation(),It->GetActorLocation())<3500&&FMath::Abs(P->FeetLocation().Z-It->GetActorLocation().Z)<800) {
  if(It->Resetting()) return It->Title()+TEXT(" is resetting | Return to the plateau to fight");
  if(It->RespawnRemaining()>0) return FString::Printf(TEXT("%s returns in %.0fs"),*It->Title(),FMath::CeilToFloat(It->RespawnRemaining()));
  return FString::Printf(TEXT("%s | +%d XP | Dodge its committed swing; stay on the plateau"),*It->Title(),It->RewardXP());
 }
 if(F->Level<3&&F->FirstPick<0) return TEXT("CHOOSE YOUR FIRST ABILITY: press Q or F | Clear waves to level up");
 return TEXT("Push all three lanes | Six lane wards > nexus ward > god | G recall | E invoke");
}
