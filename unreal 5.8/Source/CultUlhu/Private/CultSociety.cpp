#include "CultWorldState.h"
#include "CultActorLabels.h"
#include "CultBeliefCatalog.h"
#include "exertion/ActionExertionTable.h"
#include "Components/TextRenderComponent.h"
#include "DrawDebugHelpers.h"
#include "Components/PointLightComponent.h"

namespace {
uint64 SocietyId(AActor* A) { return A?((1ull<<63)|A->GetUniqueID()):0; }
ACultPopulation* SpawnPerson(UWorld* W,FVector P,int Faction=-1,float SearchRadius=400) {
 for(int Attempt=0;Attempt<18;++Attempt) {
 const float A=Attempt*2.399963f, R=Attempt==0?0:SearchRadius*FMath::Sqrt(Attempt/17.f);
 const FVector Candidate=P+FVector(FMath::Cos(A)*R,FMath::Sin(A)*R,0);
 FVector Floor; if(!ACultLivingLandscape::Surface(W,Candidate,Floor,.8f)) continue;
 FCollisionQueryParams Q(SCENE_QUERY_STAT(SocietySpawn),false);
 if(W->OverlapBlockingTestByChannel(Floor+FVector(0,0,100),FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(42,88),Q)) continue;
 FActorSpawnParameters Params; Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
 auto* N=W->SpawnActor<ACultPopulation>(Floor+FVector(0,0,100),FRotator::ZeroRotator,Params);
 if(N&&Faction>=0) N->RecruitTo(Faction); return N;
 } return nullptr;
}
}
FString ACultWorldDirector::CultName(int32 Faction) { return Faction==0?TEXT("Your cult"):Faction==1?TEXT("Nyarlathotep cult"):Faction==2?TEXT("Yog-Sothoth cult"):TEXT("Townspeople"); }
void ACultWorldDirector::PopulateSociety()
{
 if(!State_) return;
 for(int Faction=0;Faction<3;++Faction) {
  auto& F=*State_->Faiths[Faction]; const FVector Home=State_->Temples[Faction]; FVector Floor;
  // A low courtyard altar keeps participants visible beside the original monumental temple.
  if(Ground(Home+FVector(-2200,-2600,0),Floor)) {
   FActorSpawnParameters P; P.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
   auto* Table=GetWorld()->SpawnActor<ACultDestructible>(Floor+FVector(0,0,55),FRotator::ZeroRotator,P);
   Table->SetMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube"))); Table->SetActorScale3D(FVector(3.5,2.5,1.1));
   Table->Tags.Add(TEXT("FaithAltar")); Table->Tags.Add(TEXT("CultTemple")); Table->Tags.Add(FName(*FString::Printf(TEXT("Faction%d"),Faction))); F.WorkAltar=Table;
   auto* Light=NewObject<UPointLightComponent>(Table); Light->SetupAttachment(Table->GetRootComponent()); Light->SetAbsolute(false,false,true); Light->SetRelativeLocation(FVector(0,0,750)); Light->SetIntensity(180000); Light->SetLightColor(FLinearColor(1.f,.74f,.47f)); Light->SetAttenuationRadius(5500); Light->SetCastShadows(false); Light->RegisterComponent();
   auto* Sign=NewObject<UTextRenderComponent>(Table); Sign->SetupAttachment(Table->GetRootComponent()); Sign->SetAbsolute(false,false,true); Sign->SetWorldSize(65); Sign->SetTextMaterial(LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/CultUlhu/Materials/M_CultActivityText"))); Sign->SetText(FText::FromString(CultName(Faction)+TEXT("\nRITES / PRAYER / CAPTIVES"))); Sign->SetRelativeLocation(FVector(0,0,600)); Sign->SetHorizontalAlignment(EHTA_Center); Sign->RegisterComponent();
   for(int I=0;I<3;++I) if(auto* Captive=SpawnPerson(GetWorld(),Floor+FVector(150+I*190,550,0))) { Captive->Capture(nullptr,Faction); Captive->Tags.Add(TEXT("TemplePrisoner")); }
   for(int I=0;I<8;++I) {
    const float A=I*PI/4; if(auto* N=SpawnPerson(GetWorld(),Floor+FVector(FMath::Cos(A)*800,FMath::Sin(A)*800,0),Faction)) {
     N->Tags.Add(TEXT("TempleResident")); CultActorLabels::Set(N,CultName(Faction)+TEXT(" temple follower"));
     if(I<3) BeginAmbient(N,int(cultulhu::AmbientAction::Pray));
    }
   }
  }
  // Real pilgrims provide an audience; they can flee, be converted, be captured, or die.
  for(int I=0;I<8;++I) SpawnPerson(GetWorld(),Home+FVector(2000+(I%4)*500,-7500-(I/4)*600,0));
  if(State_->Cities.empty()) continue;
  const FState::City* Closest=nullptr; double Best=1e12;
  for(auto& City:State_->Cities) { double D=FVector::DistSquared2D(Home,City.Center); if(D<Best) { Best=D; Closest=&City; } }
  if(!Closest) continue;
  TArray<ACultPopulation*> Expedition;
  for(int I=0;I<4;++I) {
   ACultPopulation* N=nullptr;
   for(int Attempt=0;Attempt<8&&!N;++Attempt) {
    const float Angle=Attempt*PI*.25f;
    const FVector Offset=FVector(FMath::Cos(Angle),FMath::Sin(Angle),0)*10000+FVector(I*180,-800,0);
    N=SpawnPerson(GetWorld(),Closest->Center+Offset,Faction,1300);
   }
   if(N) { Expedition.Add(N); N->Tags.Add(TEXT("OutreachResident")); CultActorLabels::Set(N,CultName(Faction)+TEXT(" expedition follower")); N->Order(I<2?TEXT("Convert"):TEXT("Patrol"),Closest->Center,nullptr,80,I<2); }
  }
  // Place rivals beside an expedition that actually spawned, on the same walkable patch.
  // Independent +100m offsets could put the two groups on different sides of a ridge.
  if(Faction==1&&!Expedition.IsEmpty()) for(int I=0;I<3;++I) {
   auto* Nearby=Expedition[I%Expedition.Num()];
   if(auto* Guard=SpawnPerson(GetWorld(),Nearby->GetActorLocation()+FVector(250,500+I*170,0),2,1300)) Guard->Tags.Add(TEXT("OutreachResident"));
  }
 }
 UE_LOG(LogTemp,Log,TEXT("[CultUlhu] Populated temple courtyards, pilgrims, prisoners, and city expeditions"));
}
void ACultWorldDirector::TickSociety(float Dt)
{
 if(!State_||!State_->SocietyEnabled) return;
 // Work is suspended, rather than rewarded, when its physical participants are interrupted.
 for(auto& Job:State_->Labors) if(Job.Worker.IsValid()) {
  auto* Worker=Job.Worker.Get(); auto* Victim=Job.Victim.Get(); auto* Altar=Job.Altar.Get();
  bool Valid=Victim&&Victim->IsCaptive()&&Altar&&!Altar->IsDestroyed()&&Worker->IsCultist()&&!Worker->IsCaptive()&&!Worker->IsLunatic()&&Worker->GetFaction()==Victim->GetCaptiveFaction();
  if(!Valid||Worker->GetHealth()<Job.WorkerHp||Victim->GetHealth()<Job.VictimHp||Worker->GetActivity()!=TEXT("Torture")) {
   if(Worker->GetActivity()==TEXT("Torture")) Worker->Order(TEXT("Patrol"),Worker->GetActorLocation(),nullptr,1); Job.Worker.Reset(); continue;
  }
  Job.Elapsed+=Dt; if(Job.Elapsed>100) { Worker->Order(TEXT("Patrol"),Worker->GetActorLocation(),nullptr,1); Job.Worker.Reset(); continue; }
  const float Distance=FVector::Dist2D(Worker->GetActorLocation(),Victim->GetActorLocation());
  Worker->SetWorkProgress(1.f-Job.Remaining/20.f,Distance<=450);
  if(Distance>450||Worker->IsStunned()) continue;
  Job.Remaining-=Dt;
  if(Job.Remaining<=0) { FaithAction(TEXT("Torture"),Victim,Altar->GetActorLocation(),Worker->GetFaction(),Worker); Worker->Order(TEXT("Patrol"),Worker->GetActorLocation(),nullptr,1); Job.Worker.Reset(); }
 }
 State_->Labors.erase(std::remove_if(State_->Labors.begin(),State_->Labors.end(),[](const FState::Labor& J){return !J.Worker.IsValid();}),State_->Labors.end());
 for(int Faction=0;Faction<3;++Faction) {
  auto& F=*State_->Faiths[Faction]; F.SocietyTimer-=Dt; if(F.SocietyTimer>0) continue; F.SocietyTimer=5;
  auto* Altar=F.WorkAltar.Get();
  if(!Altar||Altar->IsDestroyed()) { F.WorkAltar.Reset(); for(TActorIterator<ACultDestructible> It(GetWorld());It;++It) if(It->GetFaction()==Faction&&It->ActorHasTag(TEXT("FaithAltar"))&&!It->IsDestroyed()) { F.WorkAltar=*It; Altar=*It; break; } }
  auto Available=[&](ACultPopulation* N) { return N&&N->IsCultist()&&N->GetFaction()==Faction&&!N->IsCaptive()&&!N->IsLunatic()&&!N->HasDirective()&&N->GetLoyalty()>=40; };
  bool BusyRite=false; for(auto& R:F.Rites) if(R.Core->active()) BusyRite=true;
  int Labor=0; for(auto& J:State_->Labors) if(J.Worker.IsValid()&&J.Worker->GetFaction()==Faction) ++Labor;
  if(Altar&&!BusyRite&&Labor==0) {
   ACultPopulation* Worker=nullptr; ACultPopulation* Prisoner=nullptr;
   for(auto& Row:State_->People) if(Available(Row.Actor.Get())&&FVector::Dist2D(Row.Actor->GetActorLocation(),Altar->GetActorLocation())<1200) { Worker=Row.Actor.Get(); break; }
   for(TActorIterator<ACultPopulation> It(GetWorld());It;++It) if(It->IsCaptive()&&It->GetCaptiveFaction()==Faction&&FVector::Dist2D(It->GetActorLocation(),Altar->GetActorLocation())<1200) { Prisoner=*It; if(!F.Tortured.Contains(It->GetUniqueID())) break; }
   if(Worker&&Prisoner) {
    // Every person is accounted for. Torture first, then a separate interruptible source sacrifice.
    if(!F.Tortured.Contains(Prisoner->GetUniqueID())) {
     Worker->Order(TEXT("Torture"),Prisoner->GetActorLocation(),Prisoner,100,true); Worker->SetWorkProgress(0,false);
     State_->Labors.push_back({Worker,Prisoner,Altar,TEXT("Torture"),20,0,Worker->GetHealth(),Prisoner->GetHealth()});
    } else FaithAction(TEXT("Sacrifice"),Prisoner,Altar->GetActorLocation(),Faction,Worker);
   } else if(Worker&&F.Beliefs.isActive(cultulhu::Belief::Magic)) FaithAction(TEXT("Necromancy"),nullptr,Altar->GetActorLocation(),Faction,Worker);
  }
  // Captors find an actual civilian. The captive follows its captor back to the courtyard.
  int Captives=0; for(TActorIterator<ACultPopulation> It(GetWorld());It;++It) Captives+=It->IsCaptive()&&It->GetCaptiveFaction()==Faction;
  if(Altar&&Captives<2) {
   ACultPopulation* Target=nullptr; float Best=18000;
   for(TActorIterator<ACultPopulation> It(GetWorld());It;++It) if(!It->IsCultist()&&!It->IsCaptive()&&!It->IsSummoned()) { float D=FVector::Dist2D(It->GetActorLocation(),Altar->GetActorLocation()); if(D<Best) { Target=*It; Best=D; } }
   if(Target) for(auto& Row:State_->People) if(Available(Row.Actor.Get())&&FVector::Dist2D(Row.Actor->GetActorLocation(),Target->GetActorLocation())<18000) { Row.Actor->Order(TEXT("Capture"),Target->GetActorLocation(),Target,100,true); break; }
  }
  // Expedition groups work locally. Their distant temple still receives every event.
  for(auto& City:State_->Cities) {
   TArray<ACultPopulation*> Free; for(auto& Row:State_->People) if(Available(Row.Actor.Get())&&!Row.Actor->ActorHasTag(TEXT("TempleResident"))&&FVector::Dist2D(Row.Actor->GetActorLocation(),City.Center)<13000) Free.Add(Row.Actor.Get());
   if(Free.IsEmpty()) continue;
   bool Martial=F.Beliefs.isActive(cultulhu::Belief::War)||F.Beliefs.isActive(cultulhu::Belief::Onslaught)||F.Wars.warCount()>0;
   ACultPopulation* Enemy=nullptr;
   for(TActorIterator<ACultPopulation> It(GetWorld());It;++It) if(It->IsCultist()&&It->GetFaction()!=Faction&&It->GetFaction()>=0&&FVector::Dist2D(It->GetActorLocation(),City.Center)<13000&&(AtWar(Faction,It->GetFaction())||(Faction==1&&It->GetFaction()==2))) { Enemy=*It; break; }
   if(Enemy&&Martial) DeclareWar(Faction,Enemy->GetFaction());
   int Assigned=0; for(auto* N:Free) {
    if(++Assigned>3) break;
    if(Enemy&&Martial) N->Order(TEXT("Attack"),Enemy->GetActorLocation(),Enemy,55,true);
    else N->Order(Martial?TEXT("Raid"):TEXT("Convert"),City.Center,nullptr,55,true);
   }
  }
 }
}
void ACultWorldDirector::OnFaithReward(int32 Faction,const cultulhu::GameEvent& E,float Delta)
{
 if(!State_) return; auto& F=*State_->Faiths[Faction];
 FString Feeds; for(auto& Feed:cultulhu::ACTION_EXERTION_TABLE) if(Feed.type==E.type) { if(!Feeds.IsEmpty()) Feeds+=TEXT(" / "); Feeds+=UTF8_TO_TCHAR(cultulhu::beliefName(Feed.belief)); }
 if(Feeds.IsEmpty()&&FMath::IsNearlyZero(Delta)) return;
 FString Result=FString::Printf(TEXT("%s: %s | %+.2f power%s%s"),*CultName(Faction),*CultFaith::EventName(int(E.type)),Delta,Feeds.IsEmpty()?TEXT(""):TEXT(" | "),*Feeds);
 F.LastContribution=Result; F.ContributionUntil=GetWorld()->GetTimeSeconds()+8;
 for(auto& Row:State_->People) if(Row.Faction==Faction&&Row.Actor.IsValid()&&(Row.Id==E.sourceId||SocietyId(Row.Actor.Get())==E.sourceId||(Row.Caster&&Row.Caster->id()==E.sourceId))) { Row.Actor->ShowContribution(FString::Printf(TEXT("%+.2f POWER | %s"),Delta,*Feeds)); break; }
 if(E.type!=cultulhu::EventType::MeleeAttack&&E.type!=cultulhu::EventType::BuildProgress) { F.Journal.Insert(Result,0); if(F.Journal.Num()>48) F.Journal.SetNum(48); }
}
FString ACultWorldDirector::RecentContribution(int32 Faction) const { if(!State_) return TEXT(""); auto& F=*State_->Faiths[FMath::Clamp(Faction,0,2)]; return GetWorld()->GetTimeSeconds()<=F.ContributionUntil?F.LastContribution:TEXT(""); }
FString ACultWorldDirector::ActivitySummary(int32 Faction) const
{
 if(!HasAuthority()) return State_?State_->RemoteActivities[FMath::Clamp(Faction,0,2)]:FString();
 if(!State_) return TEXT(""); TMap<FString,int32> Counts;
 for(auto& Row:State_->People) if(Row.Faction==Faction&&Row.Actor.IsValid()) Counts.FindOrAdd(Row.Actor->GetActivity())++;
 FString Out=CultName(Faction)+FString::Printf(TEXT(" | power %.0f | strength x%.2f | "),State_->Faiths[FMath::Clamp(Faction,0,2)]->Power.value(),CombatMultiplier(Faction)); for(const FString Key:{TEXT("Pray"),TEXT("Preach"),TEXT("Convert"),TEXT("Torture"),TEXT("SacrificePerformer"),TEXT("Necromancy"),TEXT("Build"),TEXT("Raid"),TEXT("Attack"),TEXT("Capture"),TEXT("Rest")}) if(Counts.FindRef(Key)>0) Out+=FString::Printf(TEXT("%s %d  "),*Key,Counts[Key]); return Out;
}
