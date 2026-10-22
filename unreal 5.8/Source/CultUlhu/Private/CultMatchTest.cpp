#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "CultAutomationPIE.h"
#include "CultMatchState.h"
#include "CultMobaJungle.h"
#include "CultDominationRoute.h"
#include "CultSkillAttack.h"
#include "CultWorldDirector.h"
#include "CultLivingLandscape.h"
#include "CultFeralBeast.h"
#include "CultUlhuGameInstance.h"
#include "Components/CapsuleComponent.h"
#include "Kismet/GameplayStatics.h"
#include "EngineUtils.h"
#include "UnrealClient.h"
#include "Misc/Paths.h"
#include "GameFramework/PlayerController.h"
#include "Camera/CameraActor.h"
#include "Components/PoseableMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Components/StaticMeshComponent.h"
#include "beliefs/BeliefSystem.h"
#include "beliefs/Belief.h"
#include "CultKitRuntime.h"
#include "CultPlayHUD.h"
#include "CultPopulation.h"
#include "CultFeralBeast.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "UI/CultBeliefSelectWidget.h"
#include "GameFramework/CharacterMovementComponent.h"

struct FCultMatchTestAccess {
 static TArray<TWeakObjectPtr<ACultMatchUnit>> AlliedObjectiveRouteProbes(ACultMatchDirector* M) {
  M->State_->Warmup=0; M->State_->NextWave=1000; HoldBots(M,true); Park(M);
  for(auto& F:M->State_->Fighters) if(auto* P=F.Pawn.Get()) P->GetCharacterMovement()->DisableMovement();
  for(auto& U:M->State_->Units) if(U.IsValid()) { U->Range=0; U->Dps=0; if(!U->IsStructure()) U->Destroy(); }
  TArray<TWeakObjectPtr<ACultMatchUnit>> Probes;
  for(int Team=0;Team<2;++Team) for(int Kind=0;Kind<3;++Kind) {
   auto* U=M->AddUnit(Team,1,Kind,-1,FVector(Team?27700:-27700,0,0),120,0,0,600); U->Waypoint=2; Probes.Add(U);
  }
  return Probes;
 }
 static ACultUlhuPlayerPawn* PreparePhysicalDodge(ACultMatchDirector* M,bool Defend,TSharedRef<bool> Contact) {
  M->SetActorTickEnabled(false); M->State_->Warmup=0; M->State_->Over=false;
  for(TActorIterator<ACultSkillAttack> It(M->GetWorld());It;++It) It->Destroy();
  M->SpawnFighter(1); HoldBots(M,true); Unshield(M);
  auto* Bot=M->Fighters()[1].Pawn.Get(); auto* Caster=M->Fighters()[5].Pawn.Get();
  Bot->RelocateMatch(FVector(0,0,60050)); Caster->RelocateMatch(FVector(-5000,0,60050));
  Bot->bBotControlled_=Defend; Bot->DashCooldown_=Bot->EvadeTime_=0;
  if(Defend) Bot->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
  const FTransform Transform(Bot->GetActorLocation()+FVector(-5000,0,0));
  auto* A=M->GetWorld()->SpawnActorDeferred<ACultSkillAttack>(ACultSkillAttack::StaticClass(),Transform,Caster,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
  A->Shape=ACultSkillAttack::EShape::Bolt; A->Direction=FVector::ForwardVector; A->Reach=10000; A->Radius=40; A->Speed=4000; A->Windup=.4f; A->Duration=4;
  A->Hit=[Bot,Caster,Contact](AActor* Target,FVector Point) { if(Target==Bot) { *Contact=true; ACultSkillAttack::Hurt(Target,20,Caster,Point); } };
  A->FinishSpawning(Transform); return Bot;
 }
 static bool ObservePhysicalDodge(ACultMatchDirector* M,float Now) { return CultBotThreat::Respond(M->Fighters()[1].Pawn.Get(),M->State_->BotThreats[1],Now,1); }
 static void VerifyDashSafety(FAutomationTestBase* T,ACultUlhuPlayerPawn* Bot) {
  T->TestTrue(TEXT("Open supported ground permits a defensive dash"),CultBotThreat::SafeDash(Bot,FVector::YAxisVector));
  auto* Wall=Bot->GetWorld()->SpawnActor<AStaticMeshActor>(Bot->GetActorLocation()+FVector(0,Bot->DashTravelDistance()*.65f,0),FRotator::ZeroRotator);
  Wall->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable); Wall->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube"))); Wall->SetActorScale3D(FVector(100,1,100));
  T->TestFalse(TEXT("A solid wall rejects the dash path"),CultBotThreat::SafeDash(Bot,FVector::YAxisVector)); Wall->Destroy();
  Bot->RelocateMatch(FVector(19000,0,60050));
  T->TestFalse(TEXT("An unsupported landing beyond the platform rejects a dash"),CultBotThreat::SafeDash(Bot,FVector::ForwardVector));
  Bot->RelocateMatch(FVector(0,0,60050));
 }
 static void Unshield(ACultMatchDirector* M) { for(auto& F:M->State_->Fighters) { F.Shield=0; if(F.Pawn.IsValid()) F.Pawn->EvadeTime_=0; } }
 static void VerifyBotThreatDecisions(FAutomationTestBase* T,ACultMatchDirector* M) {
  M->State_->Warmup=0; M->State_->NextWave=1000; M->State_->Time=10; HoldBots(M,true); Unshield(M);
  auto* Bot=M->Fighters()[1].Pawn.Get(); auto* Caster=M->Fighters()[5].Pawn.Get(); Bot->bBotControlled_=true;
  auto Reset=[&]() {
   for(TActorIterator<ACultSkillAttack> It(M->GetWorld());It;++It) It->Destroy();
   M->SpawnFighter(1); Unshield(M); Bot->RelocateMatch(FVector::ZeroVector); Caster->RelocateMatch(FVector(-2500,0,0));
   Bot->GetCharacterMovement()->SetMovementMode(MOVE_Walking); Bot->DashTime_=Bot->DashCooldown_=Bot->EvadeTime_=0;
   Bot->GetCharacterMovement()->RemoveRootMotionSource(TEXT("CultDash")); Bot->SmashCooldown_=10;
   for(int I=0;I<3;++I) Bot->Kit_->SetCooldown(I,999);
   M->State_->Fighters[1].Think=0; M->State_->Fighters[1].RouteStep=2; M->State_->Time+=1;
  };
  auto Attack=[&](ACultSkillAttack::EShape Shape,FVector Origin,FVector Direction,float Windup) {
   FTransform Transform(Origin); auto* A=M->GetWorld()->SpawnActorDeferred<ACultSkillAttack>(ACultSkillAttack::StaticClass(),Transform,Caster,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
   A->Shape=Shape; A->Direction=Direction; A->Reach=4000; A->Radius=40; A->Speed=5000; A->Windup=Windup; A->Duration=2;
   A->Hit=[](AActor*,FVector){}; A->FinishSpawning(Transform); return A;
  };
  auto ObserveTwice=[&]() { M->BotThink(1,.001f); M->State_->Time+=.25f; M->BotThink(1,.001f); };
  Reset(); Attack(ACultSkillAttack::EShape::Beam,Bot->GetActorLocation()+FVector(-2500,0,0),FVector::YAxisVector,.45f); ObserveTwice();
  T->TestEqual(TEXT("An aimed-away beam does not waste a defensive dash"),Bot->DashTime_,0.f);
  Reset(); auto* Wall=M->GetWorld()->SpawnActor<AStaticMeshActor>(Bot->GetActorLocation()+FVector(-1250,0,0),FRotator::ZeroRotator);
  Wall->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable); Wall->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube"))); Wall->SetActorScale3D(FVector(5,100,100));
  Attack(ACultSkillAttack::EShape::Beam,Bot->GetActorLocation()+FVector(-2500,0,0),FVector::ForwardVector,.45f); ObserveTwice();
  T->TestEqual(TEXT("A windup blocked by solid cover does not waste a dash"),Bot->DashTime_,0.f); Wall->Destroy();
  Reset(); auto* Above=Attack(ACultSkillAttack::EShape::Sphere,Bot->GetActorLocation()+FVector(0,0,7000),FVector::ForwardVector,.45f); Above->Reach=2000; ObserveTwice();
  T->TestEqual(TEXT("An aura outside the actual vertical volume does not waste a dash"),Bot->DashTime_,0.f);
  Reset(); Attack(ACultSkillAttack::EShape::Bolt,Bot->GetActorLocation()+FVector(-2500,0,0),FVector::ForwardVector,0); M->BotThink(1,.001f);
  T->TestEqual(TEXT("A newly observed flying projectile cannot cause an instant reaction"),Bot->DashTime_,0.f);
  M->State_->Time+=.25f; M->State_->Fighters[1].Think=0; M->BotThink(1,.001f);
  T->TestTrue(TEXT("An actual incoming flying projectile can trigger a delayed defensive dash"),Bot->DashTime_>0);
  T->AddInfo(FString::Printf(TEXT("Bot threat decisions mode=%d final dash=%.3f"),int(M->Mode()),Bot->DashTime_));
 }
 static void VerifySummonCredit(FAutomationTestBase* T,ACultMatchDirector* M,ACultUlhuPlayerPawn* P) {
  M->State_->Warmup=0; Unshield(M);
  auto* Enemy=M->Fighters()[5].Pawn.Get(); auto* Support=M->Fighters()[3].Pawn.Get();
  FActorSpawnParameters Params; Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
  auto* Summon=M->GetWorld()->SpawnActor<ACultPopulation>(P->FeetLocation()+FVector(0,1500,100),FRotator::ZeroRotator,Params);
  Summon->SetOwner(P); Summon->SummonTo(0); Summon->SetActorTickEnabled(false);
  P->Kit_->Damage(30); const float Hp=P->GetHealth(),Done=M->Fighters()[0].DamageDone; const int Texts=P->ImpactTexts().Num();
  ACultSkillAttack::Hurt(Enemy,10,Summon,Enemy->GetActorLocation());
  T->TestEqual(TEXT("Summoned hero damage is recorded for its actual summoner"),M->Fighters()[0].DamageDone-Done,10.f);
  T->TestTrue(TEXT("Summoned damage registers the summoner as a recent attacker"),M->State_->Fighters[5].RecentAttackers.Contains(0));
  T->TestEqual(TEXT("Original Thousand Young returns ten percent of actual damage"),P->GetHealth()-Hp,1.f);
  T->TestEqual(TEXT("Summoned contact gives the local summoner one impact entry"),P->ImpactTexts().Num(),Texts+1);
  T->TestTrue(TEXT("Incoming direction remains the physical creature, not its distant summoner"),Enemy->IncomingOrigin().Equals(Summon->GetActorLocation(),1));
  const float FriendlyHp=Support->GetHealth(); ACultSkillAttack::Hurt(Support,10,Summon,Support->GetActorLocation());
  T->TestEqual(TEXT("Friendly summoned attacks cannot damage allied heroes"),Support->GetHealth(),FriendlyHp);
  ACultSkillAttack::Hurt(Enemy,5,Support,Enemy->GetActorLocation());
  const int XP=M->Fighters()[0].XP,AssistXP=M->Fighters()[3].XP,Assists=M->Fighters()[3].Assists; const float Score=M->Score(0);
  const float BeforeKillHp=P->GetHealth(); ACultSkillAttack::Hurt(Enemy,1000,Summon,Enemy->GetActorLocation());
  T->TestEqual(TEXT("The summoner receives the hero elimination"),M->Fighters()[0].Kills,1);
  T->TestEqual(TEXT("A distinct contributing hero retains its assist"),M->Fighters()[3].Assists,Assists+1);
  T->TestEqual(TEXT("Only actual remaining health contributes to summoned damage statistics"),M->Fighters()[0].DamageDone-Done,95.f);
  T->TestTrue(TEXT("Overkill does not inflate the original summon healing passive"),FMath::IsNearlyEqual(P->GetHealth()-BeforeKillHp,8.5f,.01f));
  T->TestTrue(TEXT("Elimination feedback identifies the owner and local involvement"),!M->Eliminations().IsEmpty()&&M->Eliminations().Last().Attacker==M->Fighters()[0].Name&&M->Eliminations().Last().Local);
  if(M->Mode()==ECultPlayMode::Dotgoo) { T->TestEqual(TEXT("Summoned hero last-hit grants the existing hero kill XP"),M->Fighters()[0].XP-XP,75); T->TestEqual(TEXT("Assist retains its existing XP"),M->Fighters()[3].XP-AssistXP,35); }
  else T->TestEqual(TEXT("A summoned hero kill retains Domination's five points"),M->Score(0)-Score,5.f);
  const float AfterKill=P->GetHealth(); ACultSkillAttack::Hurt(Enemy,1000,Summon,Enemy->GetActorLocation());
  T->TestEqual(TEXT("Dead targets pay no repeated kills"),M->Fighters()[0].Kills,1); T->TestEqual(TEXT("Prevented repeated damage supplies no passive healing"),P->GetHealth(),AfterKill);
  M->SpawnFighter(5); Unshield(M);
  if(M->Mode()==ECultPlayMode::Dotgoo) {
   auto* Minion=M->AddUnit(1,1,0,-1,P->FeetLocation()+FVector(3000,0,0),80,0,0,0); const int Before=M->Fighters()[0].XP;
   ACultSkillAttack::Hurt(Minion,1000,Summon,Minion->GetActorLocation());
   T->TestEqual(TEXT("Summoned lane last-hit grants 60 XP rather than only nearby shared XP"),M->Fighters()[0].XP-Before,60);
   for(auto& Weak:M->State_->Units) if(auto* Tower=Weak.Get()) if(Tower->Team==1&&Tower->Kind==3&&!M->UnitProtected(Tower)) {
    const int TowerXP=M->Fighters()[0].XP; ACultSkillAttack::Hurt(Tower,10000,Summon,Tower->GetActorLocation());
    T->TestEqual(TEXT("Summoned tower last-hit grants its owner's 800 XP reward"),M->Fighters()[0].XP-TowerXP,800); break;
   }
  }
  Summon->SetOwner(Support); const int BotKills=M->Fighters()[3].Kills,PlayerKills=M->Fighters()[0].Kills;
  ACultSkillAttack::Hurt(Enemy,1000,Summon,Enemy->GetActorLocation());
  T->TestEqual(TEXT("A bot summoner receives its own summoned kill"),M->Fighters()[3].Kills,BotKills+1);
  T->TestEqual(TEXT("A bot's summon cannot award the human its kill"),M->Fighters()[0].Kills,PlayerKills);
  M->SpawnFighter(5); Unshield(M); Summon->SetOwner(P); Summon->SummonTo(1); P->Kit_->Damage(10);
  const float InvalidHp=P->GetHealth(),InvalidDone=M->Fighters()[0].DamageDone;
  ACultSkillAttack::Hurt(Support,10,Summon,Support->GetActorLocation());
  T->TestEqual(TEXT("A stale owner from a different faction receives no damage credit"),M->Fighters()[0].DamageDone,InvalidDone);
  T->TestEqual(TEXT("A converted or mismatched summon cannot heal its previous owner"),P->GetHealth(),InvalidHp);
  auto* Civilian=M->GetWorld()->SpawnActor<ACultPopulation>(P->FeetLocation()+FVector(0,2200,100),FRotator::ZeroRotator,Params); Civilian->SetOwner(P);
  const float OrdinaryDone=M->Fighters()[0].DamageDone; ACultSkillAttack::Hurt(Enemy,10,Civilian,Enemy->GetActorLocation());
  T->TestEqual(TEXT("Merely owning an ordinary civilian does not grant its damage credit"),M->Fighters()[0].DamageDone,OrdinaryDone); Civilian->Destroy();
  M->SpawnFighter(5); Unshield(M); Summon->SetOwner(P); Summon->SummonTo(0);
  ACultSkillAttack::Hurt(P,1000,Enemy,P->GetActorLocation()); const int Posthumous=M->Fighters()[0].Kills;
  ACultSkillAttack::Hurt(Enemy,1000,Summon,Enemy->GetActorLocation());
  T->TestEqual(TEXT("A surviving summon still credits its defeated summoner"),M->Fighters()[0].Kills,Posthumous+1);
  T->TestTrue(TEXT("Summon healing cannot resurrect a defeated vessel"),P->MatchDead()&&P->GetHealth()<=0);
  T->AddInfo(FString::Printf(TEXT("Summon credit mode=%d local kills=%d bot kills=%d damage=%.1f XP=%d"),int(M->Mode()),M->Fighters()[0].Kills,M->Fighters()[3].Kills,M->Fighters()[0].DamageDone,M->Fighters()[0].XP));
  Summon->Destroy();
 }
 static void Park(ACultMatchDirector* M) { int I=0; for(auto& F:M->State_->Fighters) if(F.Pawn.IsValid()) F.Pawn->RelocateMatch(M->Base(F.Team)+FVector(F.Team?-2600:2600,(I++%5-2)*1500,0)); }
 static TArray<ACultMatchUnit*> Units(ACultMatchDirector* M,int Team,int Kind,int Lane=-1) { TArray<ACultMatchUnit*> Out; for(auto& W:M->State_->Units) if(W.IsValid()&&W->Team==Team&&W->Kind==Kind&&(Lane<0||W->Lane==Lane)) Out.Add(W.Get()); return Out; }
 static void HoldBots(ACultMatchDirector* M,bool Hold) {
  for(int I=1;I<M->State_->Fighters.Num();++I) if(auto* P=M->State_->Fighters[I].Pawn.Get()) {
   P->bBotControlled_=!Hold; P->DashTime_=0; P->GetCharacterMovement()->RemoveRootMotionSource(TEXT("CultDash")); P->GetCharacterMovement()->StopMovementImmediately();
   if(Hold) P->GetCharacterMovement()->DisableMovement(); else P->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
  }
 }
 static void VerifyHealing(FAutomationTestBase* T,ACultMatchDirector* M,ACultUlhuPlayerPawn* Patient) {
  auto* H=M->State_->Fighters[3].Pawn.Get(); auto* Enemy=M->State_->Fighters[5].Pawn.Get();
  T->TestEqual(TEXT("Mirrored support bot retains Mother Hydra's canonical healing slot"),H->HealingAbilitySlot(),1);
  const FVector OldH=H->FeetLocation(),OldP=Patient->FeetLocation();
  H->RelocateMatch(FVector(-14000,17000,0)); Patient->RelocateMatch(FVector(-12000,17000,0));
  M->GrantXP(3,100); H->Kit_->SetCooldown(1,0); Patient->Kit_->Damage(60); H->SetBotAim(Patient);
  const float Before=Patient->GetHealth();
  T->TestTrue(TEXT("Original Brine Blessing casts onto an aimed allied hero"),H->Kit_->Cast(1));
  T->TestTrue(TEXT("An already wounded native hero actually receives healing"),Patient->GetHealth()>Before+30);
  T->TestTrue(TEXT("Successful healing consumes its original cooldown"),H->Kit_->Cooldown(1)>0);
  H->Kit_->SetCooldown(1,0); Patient->Kit_->Damage(45); const float BotBefore=Patient->GetHealth(); H->SetBotAim(nullptr); M->State_->Fighters[3].Think=0; M->BotThink(3,.35f);
  T->TestTrue(TEXT("Support bot proactively heals an injured allied hero"),Patient->GetHealth()>BotBefore+30);
  T->TestFalse(TEXT("Healing rejects opposing heroes"),H->CanHealActor(Enemy));
  for(auto& U:M->State_->Units) if(U.IsValid()) T->TestFalse(TEXT("MOBA minions, towers, and gods cannot receive hero healing"),H->CanHealActor(U.Get()));
  FActorSpawnParameters Params; Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
  auto* Cultist=M->GetWorld()->SpawnActor<ACultPopulation>(FVector(-12800,17000,100),FRotator::ZeroRotator,Params); Cultist->SummonTo(0); Cultist->ApplyFaithState(50,80,false);
  T->TestFalse(TEXT("Match summons cannot receive healing either"),H->CanHealActor(Cultist));
  T->TestEqual(TEXT("Summon damage also bypasses all belief/exertion modifiers in matches"),ACultWorldDirector::Find(M->GetWorld())->CultDamage(Cultist,10.f),10.f);
  H->Kit_->Sprint(false,10.f); H->Kit_->SetCooldown(1,0); H->SetBotAim(Cultist);
  const float CultHp=Cultist->GetHealth(); T->TestFalse(TEXT("A real cast aimed at a match summon is refused"),H->Kit_->Cast(1)); T->TestEqual(TEXT("Refused target receives no healing"),Cultist->GetHealth(),CultHp); T->TestEqual(TEXT("Refused heal does not spend cooldown"),H->Kit_->Cooldown(1),0.f);
  auto* GI=Cast<UCultUlhuGameInstance>(H->GetGameInstance()); const auto Mode=GI->SelectedMode; GI->SelectedMode=ECultPlayMode::FreeRoam;
  T->TestTrue(TEXT("Friendly cult creatures are healable in Free Roam"),H->CanHealActor(Cultist));
  T->TestTrue(TEXT("A Free Roam heal actually reaches its friendly cult creature"),H->Kit_->Cast(1)); T->TestTrue(TEXT("Cult creature gains real native health"),Cultist->GetHealth()>CultHp);
  Cultist->SummonTo(1); T->TestFalse(TEXT("Other cults cannot receive the healer's blessing"),H->CanHealActor(Cultist));
  auto* Beast=M->GetWorld()->SpawnActor<ACultFeralBeast>(FVector(-10000,18000,100),FRotator::ZeroRotator,Params); T->TestFalse(TEXT("Wild creatures are not cult members"),H->CanHealActor(Beast)); Beast->Destroy();
  GI->SelectedMode=Mode; Cultist->SetOwner(H); Cultist->SummonTo(0);
  const FString Identity=H->CharacterId_; H->CharacterId_=TEXT("shub_niggurath"); H->Kit_->Damage(40); const float ShubBefore=H->GetHealth();
  Unshield(M); const float EnemyBefore=Enemy->GetHealth(); ACultSkillAttack::Hurt(Enemy,10,Cultist,Enemy->GetActorLocation());
  T->TestTrue(TEXT("Thousand Young returns 10 percent of actual summon damage to its owner"),FMath::IsNearlyEqual(H->GetHealth()-ShubBefore,(EnemyBefore-Enemy->GetHealth())*.1f,.01f)&&H->GetHealth()>ShubBefore);
  H->CharacterId_=Identity; H->Kit_->Heal(1000); Enemy->Kit_->Heal(1000); Cultist->Destroy();
  Patient->Kit_->Heal(1000); Patient->Kit_->Damage(40); Patient->LastCombatAt_=M->GetWorld()->GetTimeSeconds()-4.9f;
  float Hp=Patient->GetHealth(); Patient->UpdateRecovery(.1f); T->TestEqual(TEXT("No passive healing during the five-second combat lock"),Patient->GetHealth(),Hp);
  Patient->LastCombatAt_=M->GetWorld()->GetTimeSeconds()-5.5f; Patient->UpdateRecovery(1.f);
  T->TestTrue(TEXT("Only the post-delay half second regenerates: 0.25 HP"),FMath::IsNearlyEqual(Patient->GetHealth()-Hp,.25f,.01f));
  Patient->MarkCombat(); Hp=Patient->GetHealth(); Patient->UpdateRecovery(1.f); T->TestEqual(TEXT("A new combat action immediately stops passive regeneration"),Patient->GetHealth(),Hp);
  Patient->Kit_->Heal(1000); H->Kit_->SetCooldown(1,0); H->SetBotAim(nullptr); H->RelocateMatch(OldH); Patient->RelocateMatch(OldP);
 }
 static FString Describe(ACultUlhuPlayerPawn* P) { return FString::Printf(TEXT("%s hp%.1f at%s stop%d stun%d fear%d speed%.1f dash%.2f"),*P->CharacterId_,P->GetHealth(),*P->FeetLocation().ToCompactString(),P->Control_.Stop,P->Control_.Stun,P->Control_.Fear,P->Control_.Speed,P->DashTime_); }
 static void ReadySurge(ACultUlhuPlayerPawn* P) { if(P->Kit_) P->Kit_->SetCooldown(2,0); }
 static void VerifyDominationLayout(FAutomationTestBase* T,ACultMatchDirector* M) {
  T->TestTrue(TEXT("Rectangle captures the expanded corner outside the old circle"),M->InsideCapture(FVector(5100,3600,0)));
  T->TestFalse(TEXT("Rectangle excludes beyond its short edge"),M->InsideCapture(FVector(0,3800,0)));
  T->TestFalse(TEXT("Battlement height cannot capture the ground objective"),M->InsideCapture(FVector(0,0,6000)));
  int Decks=0,Ramps=0,Bridges=0,TunnelRamps=0,CryptRamps=0;
  for(TActorIterator<AActor> It(M->GetWorld());It;++It) {
   CryptRamps+=It->ActorHasTag(TEXT("CultDominationCryptRamp")); TunnelRamps+=It->ActorHasTag(TEXT("CultDominationTunnelRamp")); Decks+=It->ActorHasTag(TEXT("CultDominationDeck")); Ramps+=It->ActorHasTag(TEXT("CultDominationRamp")); Bridges+=It->ActorHasTag(TEXT("CultDominationOverpass"));
  }
  T->TestEqual(TEXT("Four physical upper fighting decks"),Decks,4); T->TestEqual(TEXT("Each keep has a walkable access ramp"),Ramps,4); T->TestEqual(TEXT("Both side routes connect by overpasses"),Bridges,2);
  T->TestEqual(TEXT("Two real walkable underground entrances"),TunnelRamps,2);
  T->TestEqual(TEXT("Each keep has a direct catacomb ramp"),CryptRamps,4);
  T->TestFalse(TEXT("Underground heroes cannot capture through the surface"),M->InsideCapture(FVector(0,0,-5000)));
  FCollisionQueryParams Q(SCENE_QUERY_STAT(DominationLayout),false);
  FHitResult H;
  T->TestTrue(TEXT("Broken nave walls actually stop a world collision trace"),M->GetWorld()->LineTraceSingleByObjectType(H,FVector(-15000,2400,2200),FVector(-7000,2400,2200),FCollisionObjectQueryParams(ECC_WorldStatic),Q));
  const auto* P=M->Fighters()[0].Pawn.Get(); const auto* Cap=P->GetCapsuleComponent();
  T->TestFalse(TEXT("The surface slab blocks attacks between the catacomb and seal"),ACultSkillAttack::Clear(M->GetWorld(),FVector(0,0,-3500),FVector(0,0,1500),nullptr,nullptr));
  T->TestFalse(TEXT("Underground buttress cover blocks a physical line of attack"),ACultSkillAttack::Clear(M->GetWorld(),FVector(2850,3000,-3000),FVector(2850,8000,-3000),nullptr,nullptr));
  const float Half=Cap->GetScaledCapsuleHalfHeight(),Radius=Cap->GetScaledCapsuleRadius();
  T->TestFalse(TEXT("A full-size hero capsule fits through the keep's covered lower passage"),M->GetWorld()->SweepSingleByObjectType(H,FVector(4000,12500,Half+3),FVector(17000,12500,Half+3),FQuat::Identity,FCollisionObjectQueryParams(ECC_WorldStatic),FCollisionShape::MakeCapsule(Radius,Half),Q));
 }
 static ACultUlhuPlayerPawn* StartRampWalk(ACultMatchDirector* M) {
  HoldBots(M,true); auto* P=M->State_->Fighters[1].Pawn.Get();
  P->RelocateMatch(FVector(10500,26200,0)); return P;
 }
 static void VerifyMobaTerrain(FAutomationTestBase* T,ACultMatchDirector* M) {
  FCollisionQueryParams Q(SCENE_QUERY_STAT(MobaTerrain),false); FHitResult Hit;
  int Plateaus=0,Ramps=0;
  for(TActorIterator<AActor> It(M->GetWorld());It;++It) { Plateaus+=It->ActorHasTag(TEXT("CultMobaJungleMesa")); Ramps+=It->ActorHasTag(TEXT("CultMobaJungleRamp")); }
  T->TestEqual(TEXT("Four actual jungle plateaus"),Plateaus,4); T->TestEqual(TEXT("Each plateau connects to mid and an outer lane"),Ramps,8);
  for(int Camp=0;Camp<4;++Camp) {
   const auto Ground=FCultMobaJungle::Ground(Camp);
   const bool Found=M->GetWorld()->LineTraceSingleByObjectType(Hit,Ground+FVector(0,0,5000),Ground-FVector(0,0,5000),FCollisionObjectQueryParams(ECC_WorldStatic),Q);
   T->TestTrue(TEXT("Jungle ground collision matches the intended elevation"),Found&&FMath::Abs(Hit.ImpactPoint.Z-Ground.Z)<5);
  }
  auto* Cap=M->Fighters()[0].Pawn->GetCapsuleComponent(); int ObjectivePassages=0;
  for(int Lane=0;Lane<3;++Lane) for(int I=0;I<=FCultMobaRoute::Segments*4;++I) {
   const FVector Center=FCultMobaRoute::Point(0,Lane,I/float(FCultMobaRoute::Segments*4))+FVector(0,0,Cap->GetScaledCapsuleHalfHeight());
   const auto Shape=FCollisionShape::MakeCapsule(Cap->GetScaledCapsuleRadius(),Cap->GetScaledCapsuleHalfHeight());
   const bool Supported=M->GetWorld()->SweepSingleByObjectType(Hit,Center+FVector(0,0,1000),Center-FVector(0,0,1000),FQuat::Identity,FCollisionObjectQueryParams(ECC_WorldStatic),Shape,Q);
   bool Clear=Supported&&!Hit.bStartPenetrating&&Hit.ImpactNormal.Z>.7f&&Hit.ImpactPoint.Z<750;
   // Solid objectives intentionally occupy some lane centres. Keep every
   // terrain sample, and require a supported full-size side passage there.
   if(!Clear) if(auto* Blocker=Cast<ACultMatchUnit>(Hit.GetActor())) if(Blocker->IsStructure()) {
    const auto* Obstacle=Blocker->FindComponentByClass<UCapsuleComponent>();
    const auto Tangent=FCultMobaRoute::Project(0,Lane,Center).Tangent; const FVector Side(-Tangent.Y,Tangent.X,0);
    const float Offset=Cap->GetScaledCapsuleRadius()+Obstacle->GetScaledCapsuleRadius()+150;
    for(int Sign:{-1,1}) {
     const FVector Around=Center+Side*Offset*Sign; FHitResult Passage;
     if(M->GetWorld()->SweepSingleByObjectType(Passage,Around+FVector(0,0,1000),Around-FVector(0,0,1000),FQuat::Identity,FCollisionObjectQueryParams(ECC_WorldStatic),Shape,Q)&&!Passage.bStartPenetrating&&Passage.ImpactNormal.Z>.7f&&Passage.ImpactPoint.Z<750) { Clear=true; break; }
    }
    if(Clear) ++ObjectivePassages;
   }
   T->TestTrue(TEXT("Every lane sample supports the full-size capsule or a real side passage at its solid objective"),Clear);
   if(!Clear) T->AddInfo(FString::Printf(TEXT("Lane %d sample %d found %d penetrating %d actor %s normal %s impact %s location %s"),Lane,I,Supported,Hit.bStartPenetrating,*GetNameSafe(Hit.GetActor()),*Hit.ImpactNormal.ToCompactString(),*Hit.ImpactPoint.ToCompactString(),*Hit.Location.ToCompactString()));
  }
  T->AddInfo(FString::Printf(TEXT("Lane terrain: %d full-size samples; %d solid-objective samples have supported side passages"),3*(FCultMobaRoute::Segments*4+1),ObjectivePassages));
 }
 static void VerifyRelicHeight(FAutomationTestBase* T,ACultMatchDirector* M,ACultUlhuPlayerPawn* P) {
  auto& F=M->State_->Fighters[0]; const int OriginalLevel=F.Level; F.Level=20;
  auto& Relic=M->State_->Relics[2]; Relic.Channel=0; Relic.ChannelSlot=-1;
  P->RelocateMatch(FVector(Relic.Point.X,Relic.Point.Y,0)); M->UpdateRelics(.2f);
  T->TestEqual(TEXT("A hero below the plateau cannot collect its relic through solid ground"),Relic.Channel,0.f);
  P->RelocateMatch(Relic.Point); M->UpdateRelics(.2f);
  T->TestTrue(TEXT("A hero on the plateau can actually channel its elevated relic"),Relic.Channel>.1f);
  Relic.Channel=0; Relic.ChannelSlot=-1; F.Level=OriginalLevel;
 }
 static ACultMatchUnit* TerrainReturn(ACultMatchDirector* M,ACultUlhuPlayerPawn* Player,int Camp=2,int Lane=2) {
  auto* U=M->AddUnit(1,Lane,0,-1,FCultMobaJungle::Ground(Camp),80,8,800,600);
  U->TryLift(Player); U->LandMiss(U->GetActorLocation()+FVector(200,0,100)); return U;
 }
 static void IsolateJungleReturns(ACultMatchDirector* M) {
  HoldBots(M,true); Park(M); M->State_->NextWave=1000;
  for(auto& Weak:M->State_->Units) if(auto* U=Weak.Get()) { if(U->IsStructure()) { U->Range=0; U->Dps=0; } else U->Destroy(); }
 }
 static void ToMatchPoint(ACultMatchDirector* M) { while(M->Score(0)<400) M->State_->Capture->notePlayerKill(0); }
 static float Time(ACultMatchDirector* M) { return M->State_->Time; }
 static void WindClock(ACultMatchDirector* M,float Amount) { M->State_->Capture->update(Amount); }
};
class FCultMatchObservation:public IAutomationLatentCommand {
 FAutomationTestBase* Test; bool Moba; int Phase=0; double Began=FPlatformTime::Seconds(),At=0; TWeakObjectPtr<ACultMatchDirector> Previous; float InitialCore=0; int InitialWaves=0; bool IntroShot=false,LiveShot=false,CombatSignalShot=false; TWeakObjectPtr<ACameraActor> Overview; TWeakObjectPtr<ACultUlhuPlayerPawn> SurgeVictim; float LiveDamageStart=0,LivePointStart[10]={}; float SurgeBefore=0; float RouteProgress[2][3]={}; int BranchCase=0,BranchLeg=0; bool TunnelScoreChecked=false; bool TerrainDone=false; TWeakObjectPtr<ACultMatchUnit> TerrainUnit;
public:
 FCultMatchObservation(FAutomationTestBase* T,bool B):Test(T),Moba(B) {}
 bool Update() override {
  if(FPlatformTime::Seconds()-Began>360) { Test->AddError(TEXT("Match verification timed out")); return true; }
  UWorld* W=nullptr; for(auto& C:GEngine->GetWorldContexts()) if(C.World()&&C.World()->IsGameWorld()) W=C.World();
  if(!W) return false;
  auto* GI=Cast<UCultUlhuGameInstance>(W->GetGameInstance());
  if(W->GetName().Contains(TEXT("L_MainMenu"))) {
   if(Phase==0) { GI->SelectedCharacterId=TEXT("cthulhu_avatar"); GI->OpenCharacterSelect(Moba?ECultPlayMode::Dotgoo:ECultPlayMode::Domination); Test->TestFalse(TEXT("Competitive menu refuses initial belief choices"),GI->SetInitialBeliefs({5,6,11})); GI->OpenBeliefSelect(); Phase=1; }
   return false;
  }
  auto* M=ACultMatchDirector::Find(W); if(!M||M->Fighters().Num()!=10) return false;
  auto* P=Cast<ACultUlhuPlayerPawn>(UGameplayStatics::GetPlayerPawn(W,0)); if(!P) return false;
  if(Phase==1) {
   Test->TestTrue(TEXT("Selected mode reaches its actual match director"),M->Mode()==(Moba?ECultPlayMode::Dotgoo:ECultPlayMode::Domination));
   Test->TestEqual(TEXT("Two complete teams are physically present"),M->Fighters().Num(),10);
   int Teams[2]={},Bots=0; for(const auto& F:M->Fighters()) { if(F.Pawn.IsValid()) { ++Teams[F.Team]; Bots+=F.Pawn->IsBot(); Test->TestEqual(TEXT("Match uses source 100-HP competitive normalization"),F.Pawn->GetMaxHealth(),100.f); } }
   Test->TestEqual(TEXT("Five allied heroes"),Teams[0],5); Test->TestEqual(TEXT("Five opposing heroes"),Teams[1],5); Test->TestEqual(TEXT("Nine bots including same-faction allies"),Bots,9);
   Test->TestTrue(TEXT("Canonical character height survives mode entry"),FMath::IsNearlyEqual(P->GetActorScale3D().Z,30.f/1.8f));
   Test->TestTrue(TEXT("Player is grounded, never dropped from a huge height"),FMath::Abs(P->FeetLocation().Z)<10);
   Test->TestNull(TEXT("Free-roam procedural landscape does not overwrite the arena"),ACultLivingLandscape::Find(W));
   for(TActorIterator<AActor> It(W);It;++It) if(It->ActorHasTag(TEXT("CultLegacyGeometry"))) Test->TestFalse(TEXT("Hidden authored terrain cannot collide with the replacement arena"),It->GetActorEnableCollision());
   Test->TestNotNull(TEXT("Canonical ability services remain available"),ACultWorldDirector::Find(W));
   Test->TestEqual(TEXT("Competitive entry carries no initial beliefs"),GI->InitialBeliefs().Num(),0);
   TArray<UUserWidget*> Pickers; UWidgetBlueprintLibrary::GetAllWidgetsOfClass(W,Pickers,UCultBeliefSelectWidget::StaticClass(),true); Test->TestEqual(TEXT("Competitive character selection skips the belief picker"),Pickers.Num(),0);
   float Before=P->GetHealth(); P->ReceiveDamage(50); Test->TestEqual(TEXT("Preparation period is protected"),P->GetHealth(),Before);
   Phase=2; return false;
  }
  if(Phase==2) {
   if(M->Warmup()>0||M->Elapsed()<1) return false;
   if(!IntroShot) { FScreenshotRequest::RequestScreenshot(FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()/TEXT("Screenshots")/(Moba?TEXT("MobaHUD.png"):TEXT("DominationHUD.png"))),true,false); IntroShot=true; return false; }
   if(Moba&&!TerrainDone) {
    FCultMatchTestAccess::VerifyMobaTerrain(Test,M);
    P->RelocateMatch(FCultMobaJungle::Entry(2,false)); At=W->GetTimeSeconds(); Phase=10; return false;
   }
   float PausedTime=M->Elapsed(); M->TogglePause(); Test->TestTrue(TEXT("Escape pause freezes the actual local game"),UGameplayStatics::IsGamePaused(W)); M->Tick(1); Test->TestEqual(TEXT("Paused match cannot advance"),M->Elapsed(),PausedTime); M->TogglePause(); Test->TestFalse(TEXT("Resume unpauses the game"),UGameplayStatics::IsGamePaused(W));
   Test->TestTrue(TEXT("Spawn view faces the battlefield rather than the back of its temple"),FMath::Abs(FMath::FindDeltaAngleDegrees(W->GetFirstPlayerController()->GetControlRotation().Yaw,0.f))<2);
   auto* Doctrines=ACultWorldDirector::Find(W);
   Test->TestFalse(TEXT("Competitive belief changes are refused by the game service"),Doctrines->RequestBelief(0,5));
   Doctrines->Tick(120.1f); for(int F=0;F<2;++F) { Test->TestEqual(TEXT("Both teams have no active beliefs"),int(Doctrines->Beliefs(F).active().size()),0); Test->TestEqual(TEXT("No exertion combat multiplier leaks into a match"),Doctrines->CombatMultiplier(F),1.f); }
   if(auto* HUD=Cast<ACultPlayHUD>(W->GetFirstPlayerController()->GetHUD())) { P->ToggleFaith(); Test->TestFalse(TEXT("B cannot open beliefs in either competitive mode"),HUD->FaithVisible()); }
   FCultMatchTestAccess::Unshield(M); FCultMatchTestAccess::Park(M);
   FCultMatchTestAccess::VerifyHealing(Test,M,P);
   if(!Moba) {
    FCultMatchTestAccess::VerifyDominationLayout(Test,M);
    P->RelocateMatch(FVector(5100,3600,0)); float CornerScore=M->Score(0); M->Tick(1);
    Test->TestTrue(TEXT("Standing in the expanded rectangle corner awards actual score"),FMath::IsNearlyEqual(M->Score(0)-CornerScore,1.f,.02f));
    P->RelocateMatch(FVector(0,3800,0)); CornerScore=M->Score(0); M->Tick(1);
    Test->TestEqual(TEXT("Standing outside the painted edge does not bank score"),M->Score(0),CornerScore);
    P->RelocateMatch(FVector::ZeroVector); float Before=M->Score(0); M->Tick(1);
    Test->TestTrue(TEXT("Actual player presence earns one hold point per second"),FMath::IsNearlyEqual(M->Score(0)-Before,1.f,.02f));
    auto* Enemy=M->Fighters()[5].Pawn.Get(); Enemy->RelocateMatch(FVector(1000,0,0)); M->Tick(.05f);
    Test->TestTrue(TEXT("An enemy standing on the seal contests it"),M->Contested());
    Before=M->Score(0); M->Tick(.05f); Test->TestEqual(TEXT("Contested points stop scoring"),M->Score(0),Before);
    FCultMatchTestAccess::Unshield(M); int Deaths=M->Fighters()[5].Deaths;
    const FString ObjectiveNotice=M->Announcement();
    ACultSkillAttack::Hurt(Enemy,1000,P,Enemy->GetActorLocation());
    Test->TestEqual(TEXT("A physical hero elimination preserves the current objective announcement"),M->Announcement(),ObjectiveNotice);
    Test->TestTrue(TEXT("A physical hero elimination instead enters the separate event feed"),!M->Eliminations().IsEmpty()&&M->Eliminations().Last().Victim==M->Fighters()[5].Name&&M->Eliminations().Last().Local);
    Test->TestTrue(TEXT("A real hero kill scores exactly five points"),FMath::IsNearlyEqual(M->Score(0)-Before,5.f));
    Test->TestEqual(TEXT("Death is recorded once"),M->Fighters()[5].Deaths,Deaths+1);
    Test->TestTrue(TEXT("Defeated hero enters a timed respawn rather than reforming immediately"),M->RespawnFor(Enemy)>0&&Enemy->MatchDead());
    ACultSkillAttack::Hurt(Enemy,1000,P,Enemy->GetActorLocation()); Test->TestEqual(TEXT("Dead targets cannot award repeated kills"),M->Fighters()[5].Deaths,Deaths+1);
    M->Tick(5.1f); Test->TestTrue(TEXT("The same hero returns alive at its team temple"),Enemy->GetHealth()>0&&!Enemy->MatchDead()&&FVector::Dist2D(Enemy->FeetLocation(),M->Base(1))<6500);
    FCultMatchTestAccess::HoldBots(M,true); FCultMatchTestAccess::Park(M); P->RelocateMatch(FVector::ZeroVector); for(TActorIterator<ACultSkillAttack> It(W);It;++It) It->Destroy();
    SurgeVictim=M->Fighters()[6].Pawn; SurgeVictim->RelocateMatch(P->FeetLocation()+FVector(1200,0,0)); FCultMatchTestAccess::Unshield(M); SurgeBefore=SurgeVictim->GetHealth();
    FCultMatchTestAccess::ReadySurge(P); P->AbilityR(); Test->TestEqual(TEXT("The Domination summon surge does not hit instantly"),SurgeVictim->GetHealth(),SurgeBefore);
    bool Cone=false; for(TActorIterator<ACultSkillAttack> It(W);It;++It) if(It->GetOwner()==P&&It->Shape==ACultSkillAttack::EShape::Cone&&It->IsWindingUp()) {
     Cone=true;
     // Stage a grounded victim inside the actual committed volume. A shoulder camera's
     // crosshair is offset from the pawn axis; nearby +X alone is not an aimed hit.
     const FVector Axis=It->GetActorLocation()+It->Direction*It->Reach*.6f;
     SurgeVictim->RelocateMatch(FVector(Axis.X,Axis.Y,0));
     Test->AddInfo(FString::Printf(TEXT("Surge volume origin %s direction %s target %s"),*It->GetActorLocation().ToCompactString(),*It->Direction.ToCompactString(),*SurgeVictim->GetActorLocation().ToCompactString()));
     Test->TestTrue(TEXT("No unrelated body or world cover masks the isolated surge fixture"),ACultSkillAttack::Clear(W,It->GetActorLocation()+FVector(0,0,40),SurgeVictim->GetActorLocation(),P,SurgeVictim.Get()));
    }
    Test->TestTrue(TEXT("A canonical summon kit becomes its physical frontal mauling in Domination"),Cone);
    Phase=7; At=W->GetTimeSeconds(); return false;
   } else {
    Test->TestEqual(TEXT("Six towers per team"),M->TowersAlive(0),6); Test->TestEqual(TEXT("Six enemy towers"),M->TowersAlive(1),6); Test->TestEqual(TEXT("Original Great Old One HP"),M->CoreHealth(1),1500.f);
    Test->TestFalse(TEXT("Level one cannot use its ultimate"),M->AbilityAvailable(P,2));
    Test->TestTrue(TEXT("First Q choice is available"),M->AbilityAvailable(P,0,true)); Test->TestFalse(TEXT("Second ability waits for level three"),M->AbilityAvailable(P,1));
    M->GrantXP(0,100); Test->TestTrue(TEXT("Source level three unlocks the second ability"),M->AbilityAvailable(P,1)); M->GrantXP(0,150); Test->TestTrue(TEXT("Source level six unlocks R"),M->AbilityAvailable(P,2));
    auto Cores=FCultMatchTestAccess::Units(M,1,4); Test->TestEqual(TEXT("One actual enemy god actor"),Cores.Num(),1); if(Cores.Num()) { float HP=Cores[0]->Health; Cores[0]->Hurt(400,P); Test->TestEqual(TEXT("Backdoor damage is rejected before a lane opens"),Cores[0]->Health,HP); }
    auto Lane=FCultMatchTestAccess::Units(M,1,3,1); if(Lane.Num()==2) { auto* Inner=M->UnitProtected(Lane[0])?Lane[0]:Lane[1]; auto* Outer=Inner==Lane[0]?Lane[1]:Lane[0]; float Hp=Inner->Health; Inner->Hurt(100,P); Test->TestEqual(TEXT("Inner ward is protected by its actual outer tower"),Inner->Health,Hp); Outer->Hurt(10000,P); Test->TestFalse(TEXT("Outer tower destruction exposes that lane's inner ward"),M->UnitProtected(Inner)); }
    auto Wave=FCultMatchTestAccess::Units(M,0,0); Test->TestTrue(TEXT("The first physical wave has marching melee units"),Wave.Num()>=3);
    if(Wave.Num()) { Test->TestNotNull(TEXT("Wave melee uses the retained authored cultist rig"),Wave[0]->FindComponentByClass<UPoseableMeshComponent>()); float Hp=Wave[0]->Health; Wave[0]->Hurt(60,P); Test->TestEqual(TEXT("Allied waves cannot be killed by friendly attacks"),Wave[0]->Health,Hp); }
    M->StartRecall(P); Test->TestTrue(TEXT("Recall starts from its real player command"),M->Fighter(P)->Recall>0); M->OnAction(P); Test->TestEqual(TEXT("An attack cancels recall"),M->Fighter(P)->Recall,0.f);
    M->StartRecall(P); P->RelocateMatch(P->FeetLocation()+FVector(500,0,0)); M->Tick(.02f); Test->TestEqual(TEXT("Movement cancels recall"),M->Fighter(P)->Recall,0.f);
    P->RelocateMatch(M->Base(0)+FVector(2600,0,0)); M->GrantXP(0,200); M->Interact(P); Test->TestTrue(TEXT("Eligible temple interaction starts artifact invocation"),M->Fighter(P)->Invoke>0);
    const auto DuelPoint=FCultMobaRoute::Point(0,2,.5f); M->Fighters()[2].Pawn->RelocateMatch(DuelPoint+FVector(-900,0,0)); M->Fighters()[7].Pawn->RelocateMatch(DuelPoint+FVector(900,0,0));
    InitialWaves=0; for(TActorIterator<ACultMatchUnit> It(W);It;++It) if(!It->IsStructure()) ++InitialWaves;
    Test->TestTrue(TEXT("Initial waves populate both teams and all lanes"),InitialWaves>=24);
    At=FPlatformTime::Seconds(); Phase=3; return false;
   }
   At=FPlatformTime::Seconds(); Phase=4; return false;
  }
  if(Phase==7) {
   if(W->GetTimeSeconds()-At<1.f) return false;
   Test->AddInfo(FString::Printf(TEXT("Committed surge after %.2f game seconds: %.1f -> %.1f HP"),W->GetTimeSeconds()-At,SurgeBefore,SurgeVictim.IsValid()?SurgeVictim->GetHealth():-1.f));
   Test->TestTrue(TEXT("A tall hero entering the committed surge volume takes its delayed physical hit"),SurgeVictim.IsValid()&&SurgeVictim->GetHealth()<SurgeBefore-30);
   FCultMatchTestAccess::StartRampWalk(M); At=W->GetTimeSeconds(); Phase=9; return false;
  }
  if(Phase==9) {
   auto* Walker=M->Fighters()[1].Pawn.Get(); Walker->MoveMatchToward(FVector(10500,12500,6000),.016f);
   if(Walker->FeetLocation().Z>5800&&FVector::Dist2D(Walker->FeetLocation(),FVector(10500,12500,6000))<1400) {
    Test->TestTrue(TEXT("A real hero walks the full ramp onto the battlement without fixed-height teleports"),true);
    Test->AddInfo(FString::Printf(TEXT("Ramp arrival feet: %s"),*Walker->FeetLocation().ToCompactString()));
   } else if(W->GetTimeSeconds()-At<15) return false;
   else { Test->AddError(FString::Printf(TEXT("Ramp walk failed at %s"),*Walker->FeetLocation().ToCompactString())); }
   // Use the full-height player, exercising the player's terrain safety check.
   P->RelocateMatch(FVector(0,-24700,0)); W->GetFirstPlayerController()->SetControlRotation(FRotator(-8,90,0)); At=W->GetTimeSeconds(); Phase=12; return false;
  }
  if(Phase>=12&&Phase<=14) {
   const FVector Goal=Phase==12?FVector(0,-13000,-5000):Phase==13?FVector(0,13000,-5000):FVector(0,24700,0);
   P->MoveMatchToward(Goal,.016f);
   if(Phase==13&&!TunnelScoreChecked&&FMath::Abs(P->FeetLocation().Y)<500) {
    const float Score=M->Score(0); M->Tick(.5f);
    Test->TestEqual(TEXT("Standing directly beneath the seal cannot bank capture score"),M->Score(0),Score);
    Test->TestTrue(TEXT("Player remains below the seal through boundary recovery"),P->FeetLocation().Z<-4500);
    FScreenshotRequest::RequestScreenshot(FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()/TEXT("Screenshots/DominationCatacomb.png")),true,false);
    TunnelScoreChecked=true;
   }
   if(FVector::Dist2D(P->FeetLocation(),Goal)<450&&FMath::Abs(P->FeetLocation().Z-Goal.Z)<100) {
    Test->AddInfo(FString::Printf(TEXT("Full-height tunnel traversal phase %d feet %s"),Phase,*P->FeetLocation().ToCompactString()));
    if(Phase==12) {
     const float Score=M->Score(0); M->Tick(.5f); Test->TestEqual(TEXT("Below-ground movement survives the match safety check"),P->FeetLocation().Z<-4500,true);
     Test->TestEqual(TEXT("Underground traversal earns no capture points"),M->Score(0),Score);
    }
    if(Phase<14) { ++Phase; At=W->GetTimeSeconds(); return false; }
    Test->TestTrue(TEXT("Full-size player walks into, through and out of the catacomb"),true);
    Test->TestTrue(TEXT("Actual traversal crossed directly beneath the objective"),TunnelScoreChecked);
   } else if(W->GetTimeSeconds()-At<30) return false;
   else Test->AddError(FString::Printf(TEXT("Tunnel traversal phase %d failed at %s"),Phase,*P->FeetLocation().ToCompactString()));
   P->RelocateMatch(FVector(0,-12500,-5000)); At=W->GetTimeSeconds(); Phase=15; return false;
  }
  if(Phase==15) {
   const int Side=BranchCase<2?-1:1,End=BranchCase%2?-1:1;
   const FVector Goal(BranchLeg?0:End*15000,Side*FCultDominationRoute::BranchY,BranchLeg?-5000:0);
   P->MoveMatchToward(Goal,.016f);
   if(FVector::Dist2D(P->FeetLocation(),Goal)<350&&FMath::Abs(P->FeetLocation().Z-Goal.Z)<100) {
    Test->AddInfo(FString::Printf(TEXT("Crypt branch %d %s feet %s"),BranchCase,BranchLeg?TEXT("descent"):TEXT("ascent"),*P->FeetLocation().ToCompactString()));
    if(!BranchLeg) {
     if(BranchCase==0) {
      W->GetFirstPlayerController()->SetControlRotation(FRotator(-8,180,0));
      FScreenshotRequest::RequestScreenshot(FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()/TEXT("Screenshots/DominationCryptPortal.png")),true,false);
     }
     BranchLeg=1; At=W->GetTimeSeconds(); return false; }
    if(++BranchCase<4) { BranchLeg=0; P->RelocateMatch(FVector(0,BranchCase<2?-12500:12500,-5000)); At=W->GetTimeSeconds(); return false; }
    Test->TestTrue(TEXT("All four keep exits support actual full-height walking in both directions"),true);
   } else if(W->GetTimeSeconds()-At<25) return false;
   else Test->AddError(FString::Printf(TEXT("Crypt branch %d leg %d failed at %s"),BranchCase,BranchLeg,*P->FeetLocation().ToCompactString()));
   FCultMatchTestAccess::Park(M);
   for(int I=0;I<M->Fighters().Num();++I) { LiveDamageStart+=M->Fighters()[I].DamageTaken; LivePointStart[I]=M->Fighters()[I].PointSeconds; }
   FCultMatchTestAccess::HoldBots(M,false); P->RelocateMatch(M->Base(0)+FVector(2600,0,0)); At=W->GetTimeSeconds(); Phase=8; return false;
  }
  if(Phase==8) {
   if(W->GetTimeSeconds()-At>12&&!LiveShot) {
    auto* Camera=W->SpawnActor<ACameraActor>(FVector(-34000,-35000,43000),FRotator::ZeroRotator); Camera->SetActorRotation((FVector(0,0,2000)-Camera->GetActorLocation()).Rotation()); Overview=Camera; W->GetFirstPlayerController()->SetViewTarget(Camera);
    FScreenshotRequest::RequestScreenshot(FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()/TEXT("Screenshots/DominationLive.png")),true,false); LiveShot=true; return false;
   }
   if(W->GetTimeSeconds()-At>25&&!CombatSignalShot&&Overview.IsValid()) {
    for(TActorIterator<ACultSkillAttack> It(W);It;++It) if(It->IsWindingUp()&&Cast<ACultUlhuPlayerPawn>(It->GetOwner())&&FVector::Dist2D(It->GetActorLocation(),M->Objective())<9000) {
     const FVector Focus=It->GetActorLocation(); auto* Camera=Overview.Get(); Camera->SetActorLocation(Focus+FVector(-5500,-6500,6500));
     Camera->SetActorRotation((Focus+FVector(0,0,1000)-Camera->GetActorLocation()).Rotation());
     FScreenshotRequest::RequestScreenshot(FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()/TEXT("Screenshots/DominationCombatSignals.png")),true,false); CombatSignalShot=true; break;
    }
   }
   if(W->GetTimeSeconds()-At<65) return false;
   for(const auto& F:M->Fighters()) if(F.Pawn.IsValid()) Test->AddInfo(FCultMatchTestAccess::Describe(F.Pawn.Get()));
   if(Overview.IsValid()) { W->GetFirstPlayerController()->SetViewTarget(P); Overview->Destroy(); }
   int Underground[2]={},Flankers[2]={}; for(const auto& F:M->Fighters()) { Underground[F.Team]+=F.DomTunnelVisited; Flankers[F.Team]+=F.DomFlankCompleted; }
   for(int I:{4,9}) { const auto& F=M->Fighters()[I]; Test->AddInfo(FString::Printf(TEXT("Flanker team%d underground%d completed%d step%d/%d escape%d pointSeconds%.1f"),F.Team,F.DomTunnelVisited,F.DomFlankCompleted,F.DomPathIndex,F.DomPath.Num(),F.DomEscape,F.PointSeconds)); }
   Test->TestTrue(TEXT("Both teams independently use the underground connections"),Underground[0]>0&&Underground[1]>0);
   Test->TestTrue(TEXT("Both teams complete their physical flank routes and rejoin the seal fight"),Flankers[0]>0&&Flankers[1]>0);
   int Capturers[2]={}; float Damage=-LiveDamageStart; for(int I=0;I<M->Fighters().Num();++I) { const auto& F=M->Fighters()[I]; if(F.PointSeconds-LivePointStart[I]>1) ++Capturers[F.Team]; Damage+=F.DamageTaken; }
   Test->TestTrue(TEXT("Allied bots independently reach and hold the actual seal"),Capturers[0]>0);
   Test->TestTrue(TEXT("Enemy bots independently reach and contest the actual seal"),Capturers[1]>0);
   Test->TestTrue(TEXT("Independent bot play deals new physical damage beyond every scripted fixture"),Damage>0);
   Test->AddInfo(FString::Printf(TEXT("Objective bots: %d allied and %d enemy seal participants; %.1f real damage"),Capturers[0],Capturers[1],Damage));
    FCultMatchTestAccess::ToMatchPoint(M); M->Tick(.02f);
    Test->TestTrue(TEXT("Target score finishes the match"),M->IsOver()&&M->Winner()==0);
    Test->TestFalse(TEXT("Results stop hero simulation while leaving rematch input available"),P->IsActorTickEnabled());
    float Time=M->Elapsed(),Score=M->Score(0); M->Tick(5); Test->TestEqual(TEXT("Finished match freezes its clock"),M->Elapsed(),Time); Test->TestEqual(TEXT("Finished match freezes its score"),M->Score(0),Score);
   Phase=4; At=FPlatformTime::Seconds(); return false;
  }
  if(Phase==10) {
   const auto Goal=FCultMobaJungle::Ground(2); P->MoveMatchToward(Goal,.016f);
   if(FVector::Dist2D(P->FeetLocation(),Goal)<450&&P->FeetLocation().Z>3400) {
    Test->AddInfo(FString::Printf(TEXT("Jungle ramp arrival feet %s"),*P->FeetLocation().ToCompactString()));
    Test->TestTrue(TEXT("The full-size player walks a real jungle ramp onto the higher plateau"),true);
    FCultMatchTestAccess::VerifyRelicHeight(Test,M,P); TerrainUnit=FCultMatchTestAccess::TerrainReturn(M,P); Test->TestEqual(TEXT("A miss on elevated jungle ground preserves half the minion's remaining life"),TerrainUnit->Health,40.f);
    Test->TestTrue(TEXT("Elevated miss actually lands on the plateau"),TerrainUnit->GetActorLocation().Z>3500);
    P->RelocateMatch(M->Base(0)+FVector(2600,0,0)); At=W->GetTimeSeconds(); Phase=11; return false;
   }
   if(W->GetTimeSeconds()-At<12) return false;
   Test->AddError(FString::Printf(TEXT("Jungle ramp traversal failed at %s"),*P->FeetLocation().ToCompactString())); TerrainDone=true; Phase=2; return false;
  }
  if(Phase==11) {
   if(TerrainUnit.IsValid()&&!TerrainUnit->RejoiningLane&&FCultMobaRoute::Project(1,2,TerrainUnit->GetActorLocation()).Distance<500&&TerrainUnit->GetActorLocation().Z<400) {
    Test->TestTrue(TEXT("A thrown minion physically descends the jungle ramp and rejoins its original curved lane"),true);
   } else if(W->GetTimeSeconds()-At<22) return false;
   else Test->AddError(FString::Printf(TEXT("Elevated minion return failed at %s"),TerrainUnit.IsValid()?*TerrainUnit->GetActorLocation().ToCompactString():TEXT("destroyed")));
   if(TerrainUnit.IsValid()) TerrainUnit->Destroy(); TerrainDone=true; Phase=2; return false;
  }
  if(Phase==3) {
   for(TActorIterator<ACultMatchUnit> It(W);It;++It) if(!It->IsStructure()&&!It->IsInTransit()) {
    const auto OnLane=FCultMobaRoute::Project(It->Team,It->Lane,It->GetActorLocation());
    if(OnLane.Distance<1000) RouteProgress[It->Team][It->Lane]=FMath::Max(RouteProgress[It->Team][It->Lane],OnLane.Progress);
   }
   if(M->Elapsed()>45&&!LiveShot) {
    auto* Camera=W->SpawnActor<ACameraActor>(FVector(0,-28000,36000),FRotator(-55,90,0)); Camera->SetActorRotation((FVector::ZeroVector-Camera->GetActorLocation()).Rotation()); Overview=Camera; W->GetFirstPlayerController()->SetViewTarget(Camera);
    FScreenshotRequest::RequestScreenshot(FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()/TEXT("Screenshots/MobaLive.png")),true,false); LiveShot=true; return false;
   }
   if(M->Elapsed()<70) return false;
   if(Overview.IsValid()) { W->GetFirstPlayerController()->SetViewTarget(P); Overview->Destroy(); }
   Test->TestTrue(TEXT("An uninterrupted temple invocation actually grants an artifact"),M->Fighter(P)->RelicMult>1.f);
   float Damage=0; for(const auto& F:M->Fighters()) Damage+=F.DamageTaken;
   Test->TestTrue(TEXT("Actual bot attacks inflict physical damage on other heroes"),Damage>0);
   Test->AddInfo(FString::Printf(TEXT("Real hero damage during MOBA observation: %.1f"),Damage));
   Test->TestTrue(TEXT("Real match clock advances and the second wave arrives"),M->Elapsed()>30);
   int Routed=0; for(const auto& F:M->Fighters()) if(F.Pawn.IsValid()&&F.Pawn->IsBot()&&F.LaneEntered) ++Routed;
   for(const auto& F:M->Fighters()) if(F.Pawn.IsValid()&&F.Pawn->IsBot()&&!F.LaneEntered) Test->AddInfo(FString::Printf(TEXT("Route pending: %s team %d lane %d step %d at %s"),*F.Name,F.Team,F.Lane,F.RouteStep,*F.Pawn->FeetLocation().ToCompactString()));
   Test->TestTrue(TEXT("Every bot follows its assigned lane entrance"),Routed==9); Test->AddInfo(FString::Printf(TEXT("Assigned-lane entries observed: %d / 9 bots"),Routed));
   for(int Team=0;Team<2;++Team) for(int Lane=0;Lane<3;++Lane) {
    Test->TestTrue(TEXT("Each team physically advances a wave along every assigned curved route"),RouteProgress[Team][Lane]>.22f);
    Test->AddInfo(FString::Printf(TEXT("Team %d lane %d observed route progress %.3f"),Team,Lane,RouteProgress[Team][Lane]));
   }
   int Fighting=0; for(const auto& F:M->Fighters()) Fighting+=F.Kills+F.Deaths;
   int Advanced=0; for(TActorIterator<ACultMatchUnit> It(W);It;++It) if(!It->IsStructure()&&FMath::Abs(It->GetActorLocation().X)<18000) ++Advanced;
   Test->TestTrue(TEXT("Lane units physically advance into contested territory"),Advanced>0);
   int Animated=0;
   for(TActorIterator<ACultMatchUnit> It(W);It;++It) if(!It->IsStructure()) if(auto* Pose=It->FindComponentByClass<UPoseableMeshComponent>()) if(auto* Mesh=Cast<USkeletalMesh>(Pose->GetSkinnedAsset())) {
    const auto& Rest=Mesh->GetRefSkeleton().GetRefBonePose();
    for(int I=0;I<FMath::Min(Rest.Num(),Pose->BoneSpaceTransforms.Num());++I) if(!Rest[I].Equals(Pose->BoneSpaceTransforms[I],.001f)) { ++Animated; break; }
   }
   Test->TestTrue(TEXT("Retained wave character rigs actually animate"),Animated>0);
   Test->AddInfo(FString::Printf(TEXT("Animated authored lane rigs: %d"),Animated));
   Test->AddInfo(FString::Printf(TEXT("Live MOBA: %d advanced minions, %d combined hero kills/deaths after %.1fs"),Advanced,Fighting,M->Elapsed()));
   auto Lane=FCultMatchTestAccess::Units(M,1,3,1); for(auto* U:Lane) if(U->Health>0) U->Hurt(10000,P);
   Test->TestFalse(TEXT("Clearing only one lane leaves the enemy god protected"),M->CoreOpen(1));
   auto Nexus=FCultMatchTestAccess::Units(M,1,5); Test->TestEqual(TEXT("One physical enemy nexus ward"),Nexus.Num(),1);
   if(Nexus.Num()) { float Hp=Nexus[0]->Health; Nexus[0]->Hurt(10000,P); Test->TestEqual(TEXT("Nexus rejects backdoor damage while other lanes survive"),Nexus[0]->Health,Hp); }
   for(int L:{0,2}) { auto Wards=FCultMatchTestAccess::Units(M,1,3,L); for(int Pass=0;Pass<2;++Pass) for(auto* U:Wards) if(U->Health>0&&!M->UnitProtected(U)) U->Hurt(10000,P); }
   Test->TestTrue(TEXT("All six lane wards are destroyed"),M->LaneWardsCleared(1));
   Test->TestFalse(TEXT("God remains protected behind its nexus ward"),M->CoreOpen(1));
   if(Nexus.Num()) { Test->TestFalse(TEXT("Clearing all lanes exposes the nexus ward"),M->UnitProtected(Nexus[0])); Nexus[0]->Hurt(10000,P); }
   Test->TestTrue(TEXT("Destroying the exposed nexus ward finally exposes the god"),M->CoreOpen(1));
   auto Cores=FCultMatchTestAccess::Units(M,1,4); if(Cores.Num()) Cores[0]->Hurt(10000,P);
   Test->TestTrue(TEXT("Destroying the actual exposed god produces victory"),M->IsOver()&&M->Winner()==0);
   Phase=4; At=FPlatformTime::Seconds(); return false;
  }
  if(Phase==4) {
   if(FPlatformTime::Seconds()-At<.3) return false;
   FScreenshotRequest::RequestScreenshot(FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()/TEXT("Screenshots")/(Moba?TEXT("MobaVictory.png"):TEXT("DominationVictory.png"))),true,false);
   Previous=M; At=FPlatformTime::Seconds(); Phase=5; return false;
  }
  if(Phase==5) { if(FPlatformTime::Seconds()-At<.3) return false; M->Rematch(); Phase=6; return false; }
  if(Phase==6) { if(M==Previous.Get()||M->Fighters().Num()!=10) return false; Test->TestFalse(TEXT("Rematch creates a fresh unfinished round"),M->IsOver()); Test->TestEqual(TEXT("Rematch resets elapsed time"),M->Elapsed(),0.f); Test->TestFalse(TEXT("Rematch restores mouse look instead of leaving the result cursor"),W->GetFirstPlayerController()->bShowMouseCursor); Test->TestEqual(TEXT("Rematch retains the selected mode"),M->Mode(),Moba?ECultPlayMode::Dotgoo:ECultPlayMode::Domination); return true; }
  return false;
 }
};
class FCultMobaClearanceObservation:public IAutomationLatentCommand {
 FAutomationTestBase* T; bool Started=false; double At=FPlatformTime::Seconds();
public:
 explicit FCultMobaClearanceObservation(FAutomationTestBase* In):T(In) {}
 bool Update() override {
  if(FPlatformTime::Seconds()-At>45) { T->AddError(TEXT("MOBA clearance setup timed out")); return true; }
  UWorld* W=nullptr; for(auto& C:GEngine->GetWorldContexts()) if(C.World()&&C.World()->IsGameWorld()) W=C.World(); if(!W) return false;
  if(W->GetName().Contains(TEXT("L_MainMenu"))) {
   if(!Started) { Started=true; auto* GI=Cast<UCultUlhuGameInstance>(W->GetGameInstance()); GI->SelectedCharacterId=TEXT("cthulhu_avatar"); GI->OpenCharacterSelect(ECultPlayMode::Dotgoo); GI->OpenBeliefSelect(); } return false;
  }
  auto* M=ACultMatchDirector::Find(W); if(!M||M->Fighters().Num()!=10) return false;
  FCultMatchTestAccess::VerifyMobaTerrain(T,M); return true;
 }
};
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCultMobaClearanceTest,"CultUlhu.Menu.MobaLaneClearance",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCultMobaClearanceTest::RunTest(const FString&) { ADD_LATENT_AUTOMATION_COMMAND(FCultStartFreshPIE()); ADD_LATENT_AUTOMATION_COMMAND(FCultMobaClearanceObservation(this)); ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand()); return true; }
class FCultMobaJungleReturnsObservation:public IAutomationLatentCommand {
 FAutomationTestBase* T; int Phase=0,Completed=0; double Start=FPlatformTime::Seconds(); float At=0;
 TArray<TWeakObjectPtr<ACultMatchUnit>> Returning;
public:
 explicit FCultMobaJungleReturnsObservation(FAutomationTestBase* In):T(In) {}
 bool Update() override {
  if(FPlatformTime::Seconds()-Start>100) { T->AddError(TEXT("Jungle return verification timed out")); return true; }
  UWorld* W=nullptr; for(auto& C:GEngine->GetWorldContexts()) if(C.World()&&C.World()->IsGameWorld()) W=C.World(); if(!W) return false;
  if(W->GetName().Contains(TEXT("L_MainMenu"))) {
   if(Phase==0) { Phase=1; auto* GI=Cast<UCultUlhuGameInstance>(W->GetGameInstance()); GI->SelectedCharacterId=TEXT("cthulhu_avatar"); GI->OpenCharacterSelect(ECultPlayMode::Dotgoo); GI->OpenBeliefSelect(); } return false;
  }
  auto* M=ACultMatchDirector::Find(W); if(!M||M->Warmup()>0||M->Fighters().Num()!=10) return false;
  if(Phase==1) {
   FCultMatchTestAccess::IsolateJungleReturns(M); auto* P=M->Fighters()[0].Pawn.Get();
   for(int Camp=0;Camp<4;++Camp) for(int Lane=0;Lane<3;++Lane) Returning.Add(FCultMatchTestAccess::TerrainReturn(M,P,Camp,Lane));
   At=W->GetTimeSeconds(); Phase=2; return false;
  }
  for(int I=Returning.Num()-1;I>=0;--I) {
   auto* U=Returning[I].Get(); if(!U) { T->AddError(TEXT("A return fixture was destroyed before rejoining its lane")); Returning.RemoveAt(I); continue; }
   if(!U->RejoiningLane&&FCultMobaRoute::Project(U->Team,U->Lane,U->GetActorLocation()).Distance<500&&U->GetActorLocation().Z<400) {
    T->TestEqual(TEXT("Travel does not heal or consume the surviving thrown minion"),U->Health,40.f); ++Completed; U->Destroy(); Returning.RemoveAt(I);
   }
  }
  if(Returning.Num()>0&&W->GetTimeSeconds()-At<65) return false;
  for(auto& Weak:Returning) if(auto* U=Weak.Get()) T->AddError(FString::Printf(TEXT("Lane %d jungle return stuck at %s toward %s (%d path points left)"),U->Lane,*U->GetActorLocation().ToCompactString(),*U->RejoinPoint.ToCompactString(),U->RejoinPath.Num()));
  T->TestEqual(TEXT("All four plateaus return minions to all three original lanes by physical travel"),Completed,12);
  T->AddInfo(FString::Printf(TEXT("Verified jungle-to-lane returns: %d / 12"),Completed)); return true;
 }
};
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCultMobaJungleReturnsTest,"CultUlhu.Menu.MobaJungleReturns",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCultMobaJungleReturnsTest::RunTest(const FString&) { ADD_LATENT_AUTOMATION_COMMAND(FCultStartFreshPIE()); ADD_LATENT_AUTOMATION_COMMAND(FCultMobaJungleReturnsObservation(this)); ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand()); return true; }
class FCultSummonCreditObservation:public IAutomationLatentCommand {
 FAutomationTestBase* Test; ECultPlayMode Mode; bool Entered=false; double Started=FPlatformTime::Seconds();
public:
 FCultSummonCreditObservation(FAutomationTestBase* In,ECultPlayMode Choice):Test(In),Mode(Choice) {}
 bool Update() override {
  if(FPlatformTime::Seconds()-Started>90) { Test->AddError(TEXT("Summon credit verification timed out")); return true; }
  auto* W=GEditor->PlayWorld.Get(); if(!W||W->bIsTearingDown) return false;
  if(W->GetName().Contains(TEXT("L_MainMenu"))) { if(!Entered) { auto* GI=Cast<UCultUlhuGameInstance>(W->GetGameInstance()); GI->SelectedCharacterId=TEXT("shub_niggurath"); GI->OpenCharacterSelect(Mode); GI->OpenBeliefSelect(); Entered=true; } return false; }
  auto* M=ACultMatchDirector::Find(W); auto* P=Cast<ACultUlhuPlayerPawn>(UGameplayStatics::GetPlayerPawn(W,0)); if(!M||!P||W->GetTimeSeconds()<.5f) return false;
  UGameplayStatics::SetGamePaused(W,true); FCultMatchTestAccess::VerifySummonCredit(Test,M,P); UGameplayStatics::SetGamePaused(W,false); return true;
 }
};
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCultMobaSummonCreditTest,"CultUlhu.Menu.SummonCombatCredit.Moba",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCultMobaSummonCreditTest::RunTest(const FString&) { ADD_LATENT_AUTOMATION_COMMAND(FCultStartFreshPIE()); ADD_LATENT_AUTOMATION_COMMAND(FCultSummonCreditObservation(this,ECultPlayMode::Dotgoo)); ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand()); return true; }
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCultDomSummonCreditTest,"CultUlhu.Menu.SummonCombatCredit.Domination",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCultDomSummonCreditTest::RunTest(const FString&) { ADD_LATENT_AUTOMATION_COMMAND(FCultStartFreshPIE()); ADD_LATENT_AUTOMATION_COMMAND(FCultSummonCreditObservation(this,ECultPlayMode::Domination)); ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand()); return true; }
class FCultAlliedObjectiveRoutingObservation:public IAutomationLatentCommand {
 FAutomationTestBase* Test; int Phase=0; double Began=FPlatformTime::Seconds(); float At=0; TArray<TWeakObjectPtr<ACultMatchUnit>> Probes;
public:
 explicit FCultAlliedObjectiveRoutingObservation(FAutomationTestBase* In):Test(In) {}
 bool Update() override {
  if(FPlatformTime::Seconds()-Began>90) { Test->AddError(TEXT("Allied objective routing timed out")); return true; }
  auto* W=GEditor->PlayWorld.Get(); if(!W||W->bIsTearingDown) return false;
  if(W->GetName().Contains(TEXT("L_MainMenu"))) { if(!Phase) { auto* G=Cast<UCultUlhuGameInstance>(W->GetGameInstance()); G->SelectedCharacterId=TEXT("cthulhu_avatar"); G->OpenCharacterSelect(ECultPlayMode::Dotgoo); G->OpenBeliefSelect(); Phase=1; } return false; }
  auto* M=ACultMatchDirector::Find(W); if(!M||W->GetTimeSeconds()<1) return false;
  if(Phase==1) { Probes=FCultMatchTestAccess::AlliedObjectiveRouteProbes(M); At=W->GetTimeSeconds(); Phase=2; return false; }
  if(W->GetTimeSeconds()-At<15) return false;
  for(auto& Weak:Probes) {
   auto* U=Weak.Get(); Test->TestNotNull(TEXT("Allied routing probe survives its own structures"),U); if(!U) continue;
   Test->TestTrue(TEXT("Every minion class on both teams passes its own nexus using real director routing"),FMath::Abs(U->GetActorLocation().X)<24000);
   Test->AddInfo(FString::Printf(TEXT("Allied route team=%d kind=%d waypoint=%d position=%s goal=%s"),U->Team,U->Kind,U->Waypoint,*U->GetActorLocation().ToCompactString(),*M->LaneWaypoint(U->Team,1,U->Waypoint).ToCompactString()));
  }
  return true;
 }
};
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCultAlliedObjectiveRoutingTest,"CultUlhu.Menu.AlliedObjectiveRouting",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCultAlliedObjectiveRoutingTest::RunTest(const FString&) { ADD_LATENT_AUTOMATION_COMMAND(FCultStartFreshPIE()); ADD_LATENT_AUTOMATION_COMMAND(FCultAlliedObjectiveRoutingObservation(this)); ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand()); return true; }
class FCultBotThreatObservation:public IAutomationLatentCommand {
 FAutomationTestBase* Test; ECultPlayMode Mode; bool Entered=false; double Started=FPlatformTime::Seconds();
 int Phase=0; float At=0,Hp=0; bool Dashed=false; TSharedRef<bool> Contact=MakeShared<bool>(false);
public:
 FCultBotThreatObservation(FAutomationTestBase* T,ECultPlayMode M):Test(T),Mode(M) {}
 bool Update() override {
  if(FPlatformTime::Seconds()-Started>90) { Test->AddError(TEXT("Bot threat decision check timed out")); return true; }
  auto* W=GEditor->PlayWorld.Get(); if(!W||W->bIsTearingDown) return false;
  if(W->GetName().Contains(TEXT("L_MainMenu"))) { if(!Entered) { auto* G=Cast<UCultUlhuGameInstance>(W->GetGameInstance()); G->SelectedCharacterId=TEXT("cthulhu_avatar"); G->OpenCharacterSelect(Mode); G->OpenBeliefSelect(); Entered=true; } return false; }
  auto* M=ACultMatchDirector::Find(W); if(!M||W->GetTimeSeconds()<.5f) return false;
  if(Phase==0) {
   UGameplayStatics::SetGamePaused(W,true); FCultMatchTestAccess::VerifyBotThreatDecisions(Test,M);
   // Isolated supported platform: actual movement and projectile sweeps tick freely;
   // objective navigation and other heroes cannot accidentally avoid the control shot.
   auto* Floor=W->SpawnActor<AStaticMeshActor>(FVector(0,0,60000),FRotator::ZeroRotator);
   Floor->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable); Floor->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube"))); Floor->SetActorScale3D(FVector(400,400,1));
   auto* Bot=FCultMatchTestAccess::PreparePhysicalDodge(M,true,Contact); FCultMatchTestAccess::VerifyDashSafety(Test,Bot);
   Hp=Bot->GetHealth(); At=W->GetTimeSeconds(); Phase=1; UGameplayStatics::SetGamePaused(W,false); return false;
  }
  auto* Bot=M->Fighters()[1].Pawn.Get();
  if(Phase==1) Dashed|=FCultMatchTestAccess::ObservePhysicalDodge(M,W->GetTimeSeconds());
  if(W->GetTimeSeconds()-At<2.5f) return false;
  if(Phase==1) {
   Test->TestTrue(TEXT("A delayed reaction starts an actual physical dash"),Dashed);
   Test->TestTrue(TEXT("Physical dash moves the capsule outside the bolt corridor"),FMath::Abs(Bot->GetActorLocation().Y)>Bot->GetCapsuleComponent()->GetScaledCapsuleRadius()+40);
   Test->TestFalse(TEXT("The swept projectile never contacts the dodging capsule"),*Contact); Test->TestEqual(TEXT("A physically avoided projectile preserves health"),Bot->GetHealth(),Hp);
   Test->AddInfo(FString::Printf(TEXT("Physical dodge mode=%d lateral=%.1f contact=%d hp=%.1f"),int(Mode),Bot->GetActorLocation().Y,int(*Contact),Bot->GetHealth()));
   *Contact=false; Bot=FCultMatchTestAccess::PreparePhysicalDodge(M,false,Contact); Hp=Bot->GetHealth(); At=W->GetTimeSeconds(); Phase=2; return false;
  }
  Test->TestTrue(TEXT("The identical control projectile contacts a stationary capsule"),*Contact);
  Test->TestEqual(TEXT("The control projectile deals its real twenty damage"),Hp-Bot->GetHealth(),20.f);
  Test->AddInfo(FString::Printf(TEXT("Physical control mode=%d contact=%d lost=%.1f"),int(Mode),int(*Contact),Hp-Bot->GetHealth())); return true;
 }
};
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCultDomBotThreatTest,"CultUlhu.Menu.BotThreatDecisions.Domination",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCultDomBotThreatTest::RunTest(const FString&) { ADD_LATENT_AUTOMATION_COMMAND(FCultStartFreshPIE()); ADD_LATENT_AUTOMATION_COMMAND(FCultBotThreatObservation(this,ECultPlayMode::Domination)); ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand()); return true; }
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCultMobaBotThreatTest,"CultUlhu.Menu.BotThreatDecisions.Moba",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCultMobaBotThreatTest::RunTest(const FString&) { ADD_LATENT_AUTOMATION_COMMAND(FCultStartFreshPIE()); ADD_LATENT_AUTOMATION_COMMAND(FCultBotThreatObservation(this,ECultPlayMode::Dotgoo)); ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand()); return true; }
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCultDominationMatchTest,"CultUlhu.Menu.DominationMatch",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCultDominationMatchTest::RunTest(const FString&) { ADD_LATENT_AUTOMATION_COMMAND(FCultStartFreshPIE()); ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(.5f)); ADD_LATENT_AUTOMATION_COMMAND(FCultMatchObservation(this,false)); ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand()); return true; }
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCultMobaMatchTest,"CultUlhu.Menu.MobaMatch",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCultMobaMatchTest::RunTest(const FString&) { ADD_LATENT_AUTOMATION_COMMAND(FCultStartFreshPIE()); ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(.5f)); ADD_LATENT_AUTOMATION_COMMAND(FCultMatchObservation(this,true)); ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand()); return true; }
#endif
