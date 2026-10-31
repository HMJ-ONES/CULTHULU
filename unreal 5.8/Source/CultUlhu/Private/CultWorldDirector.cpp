#include "CultReplicatedBody.h"
#include "CultWorldDirector.h"
#include "CultStructurePlacement.h"
#include "CultActorLabels.h"
#include "ProfilingDebugging/CsvProfiler.h"
CSV_DECLARE_CATEGORY_EXTERN(CultRuntime);
#include "CultMatchDirector.h"
#include "CultWorldState.h"
#include "CultLivingLandscape.h"
#include "Engine/StaticMesh.h"
#include "CultPopulation.h"
#include "CultUlhuPlayerPawn.h"
#include "CultUlhuGameInstance.h"
#include "CultDestructible.h"
#include "CultFeralBeast.h"
#include "EngineUtils.h"
#include "Engine/StaticMeshActor.h"
#include "Components/StaticMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "core/EventBus.h"
#include "core/GameClock.h"
#include "core/RNG.h"
#include "beliefs/BeliefSystem.h"
#include "cult/CultManager.h"
#include "commands/CommandSystem.h"
#include "commands/DirectiveExecutor.h"
#include "ai/AmbientBehavior.h"
#include "exertion/ExertionSystem.h"
#include "war/WarSystem.h"
#include "chaos/LunaticSystem.h"
#include "entities/Structures.h"
#include "ui/CommandMenu.h"
#include <memory>

namespace { cultulhu::Vec3 CP(FVector P) { return {float(P.X/100),float(P.Z/100),float(P.Y/100)}; }
ACultPopulation* PersonAt(UWorld* W,FVector Ground) {
 const FVector P=Ground+FVector(0,0,100); FCollisionQueryParams Q(SCENE_QUERY_STAT(CultPersonSpawn),false);
 if(W->OverlapBlockingTestByChannel(P,FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(42,88),Q)) return nullptr;
 FActorSpawnParameters Params; Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn; return W->SpawnActor<ACultPopulation>(P,FRotator::ZeroRotator,Params);
} }
ACultWorldDirector::ACultWorldDirector() { bReplicates=true; bAlwaysRelevant=true; CreateDefaultSubobject<UCultReplicatedBody>(TEXT("NetworkBody")); PrimaryActorTick.bCanEverTick=true; }
ACultWorldDirector* ACultWorldDirector::Find(UWorld* W) { for(TActorIterator<ACultWorldDirector> It(W);It;++It) return *It; return nullptr; }
void ACultWorldDirector::BeginPlay() { Super::BeginPlay(); State_=new FState; InitializeFaith(); if(!HasAuthority()) return; if(ACultMatchDirector::IsMatch(GetWorld())) { return; } for(TActorIterator<ACultPopulation> It(GetWorld());It;++It) if(It->IsCultist()) Register(*It); BuildTemples(); SpawnRivals(); for(int I=0;I<7;++I) NewCity(); PopulateSociety(); }
void ACultWorldDirector::EndPlay(const EEndPlayReason::Type R) { delete State_; State_=nullptr; Super::EndPlay(R); }
cultulhu::EventBus& ACultWorldDirector::Bus(int32 F) { return State_->Faiths[FMath::Clamp(F,0,2)]->Bus; }
cultulhu::BeliefSystem& ACultWorldDirector::Beliefs(int32 F) { return State_->Faiths[FMath::Clamp(F,0,2)]->Beliefs; }
cultulhu::RNG& ACultWorldDirector::Rng(int32 F) { return State_->Faiths[FMath::Clamp(F,0,2)]->Rng; }
bool ACultWorldDirector::Ground(FVector P,FVector& Out) const
{
 return ACultLivingLandscape::Surface(GetWorld(),P,Out,.8f);
}
void ACultWorldDirector::Register(ACultPopulation* P)
{
 if(!State_||!P||!P->IsCultist()) return;
 for(auto& Row:State_->People) if(Row.Actor==P) {
  if(Row.Faction==P->GetFaction()) return;
  auto& Old=*State_->Faiths[Row.Faction]; Old.Dreams.endRest(Row.Id); for(size_t I=0;I<Old.Cult.size();++I) if(Old.Cult.at(I).id()==Row.Id) { Old.Cult.at(I).setState(cultulhu::CultistState::Converted); Old.Cult.at(I).takeDamage(100000); }
  Row.Actor.Reset(); Row.Lost=true; cultulhu::GameEvent Lost(cultulhu::EventType::CultistLost); Lost.sourceId=Row.Id; Old.Bus.publish(Lost);
 }
 int F=FMath::Clamp(P->GetFaction(),0,2); auto& Faith=*State_->Faiths[F]; size_t Index=Faith.Cult.size(); auto& C=Faith.Cult.recruit(); C.setPosition(CP(P->GetActorLocation()));
 State_->People.push_back({P,F,Index,C.id(),Faith.Rng.uniform(1.f,30.f)});
}
void ACultWorldDirector::SpawnRivals()
{
 if(!State_) return;
 const TArray<FString> Ids={TEXT("nyarlathotep"),TEXT("yog_sothoth")}; const TArray<FVector> Positions={FVector(75000,65000,0),FVector(-85000,70000,0)};
 for(int I=0;I<2;++I) {
  bool Exists=false; for(auto& B:State_->Bots) if(B.IsValid()&&B->GetFaction()==I+1) Exists=true; if(Exists) continue;
  FVector P; if(!Ground(Positions[I]+FVector(0,-11500,0),P)) continue;
  auto* Player=UGameplayStatics::GetPlayerPawn(GetWorld(),0); if(Player&&FVector::Dist2D(P,Player->GetActorLocation())<18000) continue;
  FTransform T(P+FVector(0,0,1500)); auto* B=GetWorld()->SpawnActorDeferred<ACultUlhuPlayerPawn>(ACultUlhuPlayerPawn::StaticClass(),T,nullptr,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn); B->ConfigureBot(Ids[I],I+1); B->FinishSpawning(T); B->GetCharacterMovement()->bRunPhysicsWithNoController=true; CultActorLabels::Set(B,TEXT("Rival deity: ")+Ids[I]); State_->Bots.Add(B);
  for(int N=0;N<12;++N) { FVector C; if(Ground(P+FVector(State_->Random.FRandRange(-3000.f,3000.f),State_->Random.FRandRange(2200.f,5000.f),0),C)) if(auto* Unit=PersonAt(GetWorld(),C)) Unit->RecruitTo(I+1); }
  FVector Work; if(Ground(P+FVector(4500,0,0),Work)) FactionOrder(I+1,TEXT("Build"),Work,nullptr);
  FactionOrder(I+1,TEXT("Convert"),P,nullptr);
 }
}
void ACultWorldDirector::BuildTemples()
{
 for(int F=0;F<3;++F) {
  FVector C; if(!Ground(State_->Temples[F],C)) continue;
  auto Add=[&](FString Name,FVector Offset,FVector Size,float Yaw=0.f) { FVector P; if(!Ground(C+Offset,P)) return (ACultDestructible*)nullptr; auto* B=CultStructurePlacement::Spawn(GetWorld(),Name,P,Size,Yaw,F); if(B) { B->Tags.Add(TEXT("CultTemple")); CultActorLabels::Set(B,FString::Printf(TEXT("Temple %d: %s"),F,*Name)); } return B; };
  Add(TEXT("crypt-ruined"),FVector(0,2500,0),FVector(6500,5000,6500));
  Add(TEXT("arch-dungeon"),FVector(0,-4000,0),FVector(6500,1800,5200));
  for(int Side:{-1,1}) {
   Add(TEXT("tower-square-mid"),FVector(Side*6000,-3000,0),FVector(2000,2000,9000));
   Add(TEXT("tower-square-mid"),FVector(Side*6000,6000,0),FVector(2200,2200,11000));
   Add(TEXT("stone-wall-damaged"),FVector(Side*6000,1500,0),FVector(1400,6500,4500));
   for(int I=0;I<3;++I) Add(TEXT("ruined-column-b"),FVector(Side*3500,I*1800-1500,0),FVector(600,600,4000));
  }
  if(auto* Altar=Add(TEXT("broken-obelisk"),FVector(0,0,0),FVector(1400,1400,4800))) Altar->Tags.Add(TEXT("FaithAltar"));
  if(F==0) for(int I=0;I<12;++I) { FVector P; if(Ground(C+FVector(State_->Random.FRandRange(-3000.f,3000.f),-5500-State_->Random.FRandRange(0.f,2500.f),0),P)) if(auto* U=PersonAt(GetWorld(),P)) U->RecruitTo(0); }
 }
}
void ACultWorldDirector::NewCity()
{
 if(!State_ || CityCount()>=12) return;
 const TArray<FVector> Founding={FVector(18000,8000,0),FVector(-30000,-18000,0),FVector(45000,-12000,0),FVector(65000,35000,0),FVector(-65000,35000,0),FVector(8000,70000,0),FVector(-15000,-65000,0)};
 FVector Center; bool Valid=false;
 for(int Try=0;Try<100;++Try) {
  FVector C=State_->Cities.size()<size_t(Founding.Num())?Founding[State_->Cities.size()]+FVector(State_->Random.FRandRange(-3000.f,3000.f),State_->Random.FRandRange(-3000.f,3000.f),0):FVector(State_->Random.FRandRange(-140000.f,140000.f),State_->Random.FRandRange(-140000.f,140000.f),0);
  if(!Ground(C,Center)) continue; bool Clear=true;
  for(auto& Town:State_->Cities) if(FVector::Dist2D(Town.Center,Center)<14000) Clear=false;
  for(auto& Temple:State_->Temples) if(FVector::Dist2D(Temple,Center)<20000) Clear=false;
  for(TActorIterator<ACultUlhuPlayerPawn> It(GetWorld());It;++It) if(FVector::Dist2D(It->GetActorLocation(),Center)<9000) Clear=false;
  if(Clear) { Valid=true; break; }
 }
 if(!Valid) return;
 FState::City City; City.Center=Center; TArray<FVector> Used;
 const TArray<FString> Types={TEXT("tower-square-mid"),TEXT("tower-square-base"),TEXT("crypt-ruined"),TEXT("stone-wall-damaged")};
 const int Count=State_->Random.RandRange(18,30);
 const float Heading=State_->Random.FRandRange(0.f,360.f);
 for(int I=0;I<Count;++I) {
  const bool Landmark=I==0;
  const FString Name=Types[Landmark?0:State_->Random.RandRange(0,3)];
  const FVector Size=Landmark?FVector(3000,3000,10000):FVector(State_->Random.FRandRange(900.f,1700.f),State_->Random.FRandRange(1000.f,2200.f),State_->Random.FRandRange(1200.f,4500.f));
  const float Yaw=Heading+State_->Random.FRandRange(-30.f,30.f);
  auto* Mesh=CultStructurePlacement::LoadMesh(Name); if(!Mesh) continue;
  FVector P; bool Placed=false;
  for(int Try=0;Try<30;++Try) {
   const float A=State_->Random.FRandRange(0.f,2*PI),Radius=State_->Random.FRandRange(2400.f,7500.f);
   if(!Ground(Center+FVector(FMath::Cos(A)*Radius,FMath::Sin(A)*Radius*.7f,0),P)) continue;
   bool Clear=true; for(auto& Q:Used) if(FVector::Dist2D(P,Q)<1400) Clear=false;
   if(Clear&&!CultStructurePlacement::CharactersOccupy(GetWorld(),Mesh,CultStructurePlacement::MakeTransform(Mesh,P,Size,Yaw))) { Placed=true; break; }
  }
  if(!Placed) continue;
  if(auto* B=CultStructurePlacement::Spawn(GetWorld(),Name,P,Size,Yaw,-1,true)) { Used.Add(P); City.Buildings.Add(B); B->Tags.Add(TEXT("LivingCity")); }
 }
 if(City.Buildings.Num()) {
  State_->Cities.push_back(std::move(City));
  // Populate only the final geometry. Later house placements must never bury
  // someone who passed an earlier capsule check; distribute residents through
  // the settlement instead of concentrating everyone in its central seven metres.
  int Residents=0;
  for(int I=0;I<(Count+1)/2;++I) for(int Attempt=0;Attempt<18;++Attempt) {
   const float Angle=State_->Random.FRandRange(0.f,2*PI),Radius=6000*FMath::Sqrt(State_->Random.FRand()); FVector Street;
   if(Ground(Center+FVector(FMath::Cos(Angle)*Radius,FMath::Sin(Angle)*Radius*.7f,0),Street)&&PersonAt(GetWorld(),Street)) { ++Residents; break; }
  }
  UE_LOG(LogTemp,Log,TEXT("[CultUlhu] Clustered settlement: %d buildings, %d residents near %s"),Used.Num(),Residents,*Center.ToString());
 }
}
int32 ACultWorldDirector::CityCount() const { if(!HasAuthority()) return State_?State_->RemoteCities:0; int N=0; if(State_) for(auto& C:State_->Cities) { for(auto& B:C.Buildings) if(B.IsValid()&&!B->IsDestroyed()) { ++N; break; } } return N; }
void ACultWorldDirector::AdvancePopulation()
{
 if(!State_) return; int Civs=0,Beasts=0;
 for(TActorIterator<ACultPopulation> It(GetWorld());It;++It) if(!It->IsCultist()) ++Civs;
 for(TActorIterator<ACultFeralBeast> It(GetWorld());It;++It) if(!It->IsDead()) ++Beasts;
 FVector P; if(Civs<100 && Ground(FVector(State_->Random.FRandRange(-145000.f,145000.f),State_->Random.FRandRange(-145000.f,145000.f),0),P)) PersonAt(GetWorld(),P);
 if(Beasts<36 && Ground(FVector(State_->Random.FRandRange(-145000.f,145000.f),State_->Random.FRandRange(-145000.f,145000.f),0),P)) {
  const TArray<FString> Names={TEXT("skittering_ghoul"),TEXT("charnel_imp"),TEXT("pale_wight"),TEXT("ossified_brute")}; FTransform T(P+FVector(0,0,100)); auto* B=GetWorld()->SpawnActorDeferred<ACultFeralBeast>(ACultFeralBeast::StaticClass(),T,nullptr,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn); B->SetSpecies(Names[State_->Random.RandRange(0,3)]); B->FinishSpawning(T); }
 SpawnRivals();
}
bool ACultWorldDirector::AtWar(int A,int B) const { return State_&&A>=0&&B>=0&&A<3&&B<3&&A!=B&&State_->Faiths[A]->Wars.atWar(B+1); }
void ACultWorldDirector::DeclareWar(int A,int B) { if(!State_||A<0||B<0||A==B||A>2||B>2) return; State_->Faiths[A]->Wars.declareWar(B+1); State_->Faiths[B]->Wars.declareWar(A+1); }
void ACultWorldDirector::FactionOrder(int Faction,const FString& Verb,FVector Point,AActor* Target)
{
 if(Verb==TEXT("Build")) { int Active=0; for(auto& B:State_->Builds) if(B.Faction==Faction&&!B.Site->finished()) ++Active; if(Active>=2) return; }
 auto& F=*State_->Faiths[Faction];
 using D=cultulhu::DirectiveType;
 auto Kind=Verb==TEXT("Raid")?D::RaidCity:Verb==TEXT("Attack")?D::GoToWar:Verb==TEXT("Convert")?D::ConvertCampaign:D::Defend;
 auto Result=F.Commands.issueCommand(Kind,CP(Point));
 if(Result.outcome==cultulhu::CommandOutcome::Refused||Result.outcome==cultulhu::CommandOutcome::SparksInsurrection) return;
 TArray<TWeakObjectPtr<ACultPopulation>> Workers; int Assigned=0;
 for(auto& Row:State_->People) if(Row.Faction==Faction&&Row.Actor.IsValid()&&(Verb!=TEXT("Build")||(Row.Action<0&&Row.RestRemaining<=0&&Row.Actor->GetLoyalty()>=cultulhu::BuilderAI::LOYALTY_THRESHOLD&&!Row.Actor->IsLunatic()&&F.Cult.at(Row.Index).state()==cultulhu::CultistState::Loyal))&&(!Row.Actor->HasDirective() || (Verb==TEXT("Convert")&&Row.Actor->GetActivity()==TEXT("Convert")) || ((Verb==TEXT("Attack")||Verb==TEXT("Raid"))&&Row.Actor->GetActivity()!=TEXT("Build")))) {
  if(Row.Actor->ActorHasTag(TEXT("TempleResident"))&&(Verb==TEXT("Raid")||Verb==TEXT("Attack")||Verb==TEXT("Convert"))&&FVector::Dist2D(Point,State_->Temples[Faction])>15000) continue;
  if((Verb==TEXT("Raid")||Verb==TEXT("Attack"))&&Workers.Num()>=6) continue;
  if(Verb==TEXT("Convert")&&Workers.Num()>=4) continue;
  if(Row.Actor->GetActivity()==TEXT("Torture")||Row.Actor->GetActivity()==TEXT("SacrificePerformer")||Row.Actor->GetActivity()==TEXT("SacrificeVictim")||Row.Actor->GetActivity()==TEXT("Necromancy")) continue;
  if(Result.outcome==cultulhu::CommandOutcome::PartiallyObeyed&&Assigned++%2) continue;
  if(Verb==TEXT("Build")&&Workers.Num()>=4) continue;
  const float Travel=FVector::Dist2D(Row.Actor->GetActorLocation(),Point)/250.f;
  Row.Actor->Order(Verb,Point,Target,FMath::Max(Verb==TEXT("Build")?240.f:150.f,Travel+120.f),true); Workers.Add(Row.Actor);
 }
 if(Verb==TEXT("Build")&&Workers.Num()) {
  FState::Build B; B.Point=Point; B.Faction=Faction; B.Workers=Workers; B.Site=std::make_unique<cultulhu::ConstructionSite>(Faction,CP(Point),F.Builder.pickBuildingType()); cultulhu::GameEvent Started(cultulhu::EventType::BuildStarted); Started.tag=cultulhu::buildingTypeName(B.Site->targetType()); F.Bus.publish(Started); State_->Builds.push_back(std::move(B));
  UE_LOG(LogTemp,Log,TEXT("[CultUlhu] Faction %d ordered %d builders to %s"),Faction,Workers.Num(),*Point.ToString());
 }
}
FString ACultWorldDirector::Execute(int32 Action,AActor* Target,FVector Point)
{
 if(!State_) return TEXT("The cult is unavailable"); using A=cultulhu::MenuAction; using D=cultulhu::DirectiveType; A Order=static_cast<A>(Action); if(Order==A::Cancel) return TEXT("");
 if(Action>=RestFollowers&&Action<=PunishCultist) return FaithOrder(Action,Target,Point);
 if(Action>=100||Order==A::Follow||Order==A::AttackTarget||Order==A::RaidAt||Order==A::MoveTo||Order==A::ScoutAt||Order==A::BuildAt||Order==A::BuildAltarAt||Order==A::CaptureOrder||Order==A::ConvertOrder) return PlayerOrder(Action,Target,Point);
 auto& F=*State_->Faiths[0];
 if(Order==A::RitualSacrifice) { auto Outcome=F.Commands.issueCommand(D::MassSacrifice,CP(Point)); if(Outcome.outcome==cultulhu::CommandOutcome::Refused||Outcome.outcome==cultulhu::CommandOutcome::SparksInsurrection) return UTF8_TO_TCHAR(Outcome.detail.c_str()); return FaithAction(TEXT("Sacrifice"),Target,Point); }
 if(Order==A::SacrificeOrder) return FaithAction(TEXT("Sacrifice"),Target,Point);
 if(Order==A::RitualNecromancy) return FaithAction(TEXT("Necromancy"),Target,Point);
 if(Order==A::RitualConvert) { auto Outcome=F.Commands.issueCommand(D::ConvertCampaign,CP(Point)); if(Outcome.outcome==cultulhu::CommandOutcome::Refused||Outcome.outcome==cultulhu::CommandOutcome::SparksInsurrection) return UTF8_TO_TCHAR(Outcome.detail.c_str()); F.RitualCaster.setDirectedPoint(CP(Point)); return TEXT("Sorcerers will aim their next 45s conversion ritual here (25 mana required)."); }
 return TEXT("Choose a valid command.");
}
void ACultWorldDirector::Tick(float Dt)
{
 CSV_SCOPED_TIMING_STAT(CultRuntime, WorldTick);
 Super::Tick(Dt); if(!HasAuthority()) return; if(!State_) return;
 if(ACultMatchDirector::IsMatch(GetWorld())) return; // Beliefs and exertion are Free Roam only.

 { CSV_SCOPED_TIMING_STAT(CultRuntime, FaithTick); TickFaith(Dt); }
 { CSV_SCOPED_TIMING_STAT(CultRuntime, SocietyTick); TickSociety(Dt); }
 PopulationTime_-=Dt; if(PopulationTime_<=0) { AdvancePopulation(); PopulationTime_=20; }
 CityTime_-=Dt; if(CityTime_<=0) { NewCity(); CityTime_=State_->Random.FRandRange(120.f,180.f); }
 // Abandoned work must not permanently consume the rivals' construction slots.
 // A reassigned builder belongs to its new site; old sites cannot borrow it back.
 State_->Builds.erase(std::remove_if(State_->Builds.begin(),State_->Builds.end(),[](FState::Build& B) {
  if(!B.Site||B.Site->finished()) return false;
  for(auto& W:B.Workers) if(W.IsValid()&&W->GetActivity()==TEXT("Build")&&W->GetCommandPoint().Equals(B.Point,1.f)) return false;
  if(B.Preview.IsValid()) B.Preview->Destroy(); return true;
 }),State_->Builds.end());
 for(auto& Build:State_->Builds) { if(!Build.Site||Build.Site->finished()||State_->Faiths[Build.Faction]->Cult.insurrectionRisk()>=cultulhu::BuilderAI::UNREST_HALT_RISK) continue; // Workers must actually reach the site.
  for(int I=0;I<Build.Workers.Num();++I) { auto& W=Build.Workers[I]; const uint64 Id=I+1; if(W.IsValid()&&W->GetActivity()==TEXT("Build")&&W->GetCommandPoint().Equals(Build.Point,1.f)&&W->GetLoyalty()>=40&&!W->IsLunatic()&&!W->IsCaptive()&&FVector::Dist2D(W->GetActorLocation(),Build.Point)<600) Build.Site->addBuilder(Id); else Build.Site->removeBuilder(Id); }
  Build.Site->update(Dt);
  if(!Build.Preview.IsValid()) {
   auto* Preview=GetWorld()->SpawnActor<AActor>(); auto* Root=NewObject<USceneComponent>(Preview); Preview->SetRootComponent(Root); Root->RegisterComponent(); Preview->SetActorLocation(Build.Point);
   auto* Mesh=NewObject<UStaticMeshComponent>(Preview); Mesh->SetupAttachment(Root); Mesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube"))); Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision); Mesh->SetRelativeScale3D(FVector(22,18,.2)); Mesh->RegisterComponent(); Preview->Tags.Add(TEXT("ConstructionPreview")); Build.Preview=Preview;
  }
  if(Build.Preview.IsValid()) if(auto* Mesh=Build.Preview->FindComponentByClass<UStaticMeshComponent>()) { const float Height=FMath::Max(.2f,Build.Site->progress()*22.f); Mesh->SetRelativeScale3D(FVector(22,18,Height)); Mesh->SetRelativeLocation(FVector(0,0,Height*50)); }
  for(auto& Worker:Build.Workers) if(Worker.IsValid()&&Worker->GetActivity()==TEXT("Build")&&Worker->GetCommandPoint().Equals(Build.Point,1.f)) Worker->SetWorkProgress(Build.Site->progress(),FVector::Dist2D(Worker->GetActorLocation(),Build.Point)<600);
  if(Build.Site->finished()) { if(Build.Preview.IsValid()) Build.Preview->Destroy();
   // Finishing a building must not entomb passing citizens or rival expedition members.
   // Clear every nearby body, not just the construction crew, using actual capsule placement.
   for(TActorIterator<ACultPopulation> It(GetWorld());It;++It) if(FVector::Dist2D(It->GetActorLocation(),Build.Point)<2400) {
    auto* N=*It; auto* Cap=N->GetCapsuleComponent();
    for(int Attempt=0;Attempt<24;++Attempt) {
     float Angle=Attempt*2.399963f; FVector Clear;
     FVector Trial=Build.Point+FVector(FMath::Cos(Angle),FMath::Sin(Angle),0)*(2400+(Attempt/8)*900);
     if(!Ground(Trial,Clear)) continue;
     FVector Position=Clear+FVector(0,0,Cap->GetScaledCapsuleHalfHeight()+3);
     FCollisionQueryParams Q(SCENE_QUERY_STAT(ConstructionClearance),false,N);
     if(GetWorld()->OverlapBlockingTestByChannel(Position,FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(Cap->GetScaledCapsuleRadius(),Cap->GetScaledCapsuleHalfHeight()),Q)) continue;
     N->SetActorLocation(Position,false,nullptr,ETeleportType::TeleportPhysics); N->GetCharacterMovement()->StopMovementImmediately(); N->GetCharacterMovement()->SetMovementMode(MOVE_Walking); break;
    }
   }
 Build.Site->complete(); FVector P; if(Ground(Build.Point,P)) if(auto* B=CultStructurePlacement::Spawn(GetWorld(),TEXT("crypt-ruined"),P,FVector(2200,1800,2200),State_->Random.FRandRange(0.f,360.f),Build.Faction)) { if(Build.Site->targetType()==cultulhu::BuildingType::Altar) { B->Tags.Add(TEXT("CultTemple")); B->Tags.Add(TEXT("FaithAltar")); } B->Tags.Add(TEXT("CultBuilt")); cultulhu::GameEvent Done(cultulhu::EventType::BuildCompleted); Done.tag=cultulhu::buildingTypeName(Build.Site->targetType()); for(auto& W:Build.Workers) if(W.IsValid()) { Done.sourceId=(1ull<<63)|W->GetUniqueID(); break; } Done.pos=CP(P); State_->Faiths[Build.Faction]->Bus.publish(Done); if(Build.Site->targetType()==cultulhu::BuildingType::Trap) { auto Id=State_->Faiths[Build.Faction]->Traps.placeTrap(CP(P)); State_->Faiths[Build.Faction]->Snares.push_back({Id,P,B}); } for(auto& Worker:Build.Workers) if(Worker.IsValid()&&Worker->GetActivity()==TEXT("Build")&&Worker->GetCommandPoint().Equals(Build.Point,1.f) ) Worker->Order(TEXT("Patrol"),Worker->GetActorLocation(),nullptr,1); UE_LOG(LogTemp,Log,TEXT("[CultUlhu] Faction %d completed a structure"),Build.Faction); } } }

 for(auto& B:State_->Bots) if(B.IsValid()) {
  AActor* Target=B->GetBotAim(); if(!Target||FVector::Dist2D(Target->GetActorLocation(),B->GetActorLocation())<=1800) continue;
  const FVector Toward=(Target->GetActorLocation()-B->GetActorLocation()).GetSafeNormal2D();
  const float Radius=B->GetCapsuleComponent()->GetScaledCapsuleRadius();
  for(float Turn:{0.f,35.f,-35.f,70.f,-70.f,110.f,-110.f}) {
   const FVector Heading=Toward.RotateAngleAxis(Turn,FVector::UpVector); FVector Floor;
   if(!Ground(B->FeetLocation()+Heading*(Radius+600),Floor)||FMath::Abs(Floor.Z-B->FeetLocation().Z)>1400) continue;
   FHitResult H; FCollisionQueryParams Q(SCENE_QUERY_STAT(CultRivalSteer),false,B.Get());
   if(GetWorld()->SweepSingleByChannel(H,B->GetActorLocation()+FVector(0,0,300),B->GetActorLocation()+FVector(0,0,300)+Heading*700,FQuat::Identity,ECC_Pawn,FCollisionShape::MakeSphere(Radius*.8f),Q)) {
    if(auto* Obstacle=Cast<ACultDestructible>(H.GetActor())) if(Obstacle->GetFaction()!=B->GetFaction()) { B->SetBotAim(Obstacle); B->AbilityQ(); B->Smash(); }
    continue;
   }
   B->AddMovementInput(Heading,1,true); break;
  }
 }
 BotTime_-=Dt; if(BotTime_<=0) { BotTime_=2;
  for(auto& Bot:State_->Bots) if(Bot.IsValid()) {
   const int F=Bot->GetFaction(); auto& Timer=State_->DecisionTimers.FindOrAdd(F); Timer-=2;
   AActor* Enemy=nullptr; float Best=30000;
   for(TActorIterator<ACultUlhuPlayerPawn> It(GetWorld());It;++It) if(AtWar(F,It->GetFaction())) { float D=FVector::Dist2D(It->GetActorLocation(),Bot->GetActorLocation()); if(D<Best) { Best=D; Enemy=*It; } }
   for(TActorIterator<ACultPopulation> It(GetWorld());It;++It) if(AtWar(F,It->GetFaction())) { float D=FVector::Dist2D(It->GetActorLocation(),Bot->GetActorLocation()); if(D<Best) { Best=D; Enemy=*It; } }
   for(TActorIterator<ACultFeralBeast> It(GetWorld());It;++It) if(!It->IsDead()) { float D=FVector::Dist2D(It->GetActorLocation(),Bot->GetActorLocation()); if(D<Best&&D<12000) { Best=D; Enemy=*It; } }
   if(Enemy) { if(Bot->GetBotAim()!=Enemy) FactionOrder(F,TEXT("Attack"),Enemy->GetActorLocation(),Enemy); Bot->SetBotAim(Enemy); if(Best<9000) { Bot->AbilityQ(); Bot->AbilityF(); Bot->AbilityR(); if(Best<2500) Bot->Smash(); } }
   if(Timer<=0) {
    Timer=State_->Random.FRandRange(18.f,35.f); int Choice=State_->Random.RandRange(0,4); FVector Home=State_->Temples[F];
    if(Choice==0) {
     FVector Site; if(Ground(Home+FVector(State_->Random.FRandRange(-10000.f,10000.f),State_->Random.FRandRange(-12000.f,-8000.f),0),Site)) { FactionOrder(F,TEXT("Build"),Site,nullptr); State_->Goals.Add(F,TEXT("building")); }
    } else if(Choice==1 && !State_->Cities.empty()) {
     auto& City=State_->Cities[State_->Random.RandRange(0,int(State_->Cities.size())-1)]; AActor* Building=nullptr;
     for(auto& B:City.Buildings) if(B.IsValid()&&!B->IsDestroyed()) { Building=B.Get(); break; }
     if(Building) { Bot->SetBotAim(Building); FactionOrder(F,TEXT("Raid"),City.Center,nullptr); State_->Goals.Add(F,TEXT("razing a city")); Timer=90; }
    } else if(Choice==2) {
     int Rival=F==1?2:1; auto* Human=UGameplayStatics::GetPlayerPawn(GetWorld(),0); if(Human&&FVector::Dist2D(Human->GetActorLocation(),Bot->GetActorLocation())<30000&&State_->Random.FRand()<.3f) { Rival=0; Bot->SetBotAim(Human); FactionOrder(F,TEXT("Attack"),Human->GetActorLocation(),Human); } DeclareWar(F,Rival); for(auto& Other:State_->Bots) if(Other.IsValid()&&Other->GetFaction()==Rival) { Bot->SetBotAim(Other.Get()); FactionOrder(F,TEXT("Attack"),Other->GetActorLocation(),Other.Get()); } State_->Goals.Add(F,TEXT("war with rival")); Timer=120;
    } else {
     ACultPopulation* Candidate=nullptr; float Distance=200000;
     for(TActorIterator<ACultPopulation> It(GetWorld());It;++It) if(!It->IsCultist()&&!It->IsSummoned()) { float D=FVector::Dist2D(It->GetActorLocation(),Bot->GetActorLocation()); if(D<Distance) { Candidate=*It; Distance=D; } }
     if(Candidate) { Bot->SetBotAim(Candidate); FactionOrder(F,TEXT("Convert"),Candidate->GetActorLocation(),nullptr); } State_->Goals.Add(F,TEXT("recruiting"));
    }
    UE_LOG(LogTemp,Log,TEXT("[CultUlhu] Faction %d goal: %s"),F,*State_->Goals.FindRef(F));
   }
   if(auto* Candidate=Cast<ACultPopulation>(Bot->GetBotAim())) if(!Candidate->IsCultist()&&FVector::Dist2D(Candidate->GetActorLocation(),Bot->GetActorLocation())<3000) TryConvert(Candidate,F);
   if(auto* Building=Cast<ACultDestructible>(Bot->GetBotAim())) if(!Building->IsDestroyed()&&FVector::Dist2D(Building->GetActorLocation(),Bot->GetActorLocation())<6000) { Bot->AbilityQ(); Bot->Smash(); }
  }
 }

}
FString ACultWorldDirector::Status() const
{
 if(!HasAuthority()) return State_?State_->RemoteStatus:FString();
 if(!State_) return TEXT(""); auto& F=*State_->Faiths[0]; int Live=0; for(auto& P:State_->People) if(P.Faction==0&&P.Actor.IsValid()) ++Live;
 FString Creeds; for(auto B:F.Beliefs.active()) Creeds+=FString(UTF8_TO_TCHAR(cultulhu::beliefName(B)))+TEXT(" ");
 return FString::Printf(TEXT("Cultists %d | Risk %.0f%% | Power %.0f | Wars %d | New towns %d | %s"),Live,F.Cult.insurrectionRisk(),F.Power.value(),F.Wars.warCount(),CityCount(),*Creeds);
}

bool ACultWorldDirector::TryConvert(ACultPopulation* P,int32 Faction,ACultPopulation* Agent) {
 if(!State_||!P||P->IsSummoned()||(P->IsCultist()&&P->GetFaction()==Faction)) return false; auto& F=*State_->Faiths[FMath::Clamp(Faction,0,2)];
 if(!F.Rng.chance(FMath::Min(.95f,F.Exertion.stats().conversionChance*(P->IsCultist()?Beliefs(P->GetFaction()).conversionVulnerability():1.f)))) return false;
 P->TransferTo(Faction); cultulhu::GameEvent E(cultulhu::EventType::ConversionPerformed); E.amount=1; E.sourceId=Agent?((1ull<<63)|Agent->GetUniqueID()):0; E.targetId=(1ull<<63)|P->GetUniqueID(); E.pos=CP(P->GetActorLocation()); F.Bus.publish(E); return true;
}
float ACultWorldDirector::CombatMultiplier(int32 Faction) const { return State_&&!ACultMatchDirector::IsMatch(GetWorld())?State_->Faiths[FMath::Clamp(Faction,0,2)]->Exertion.stats().combatPowerMult:1.f; }
