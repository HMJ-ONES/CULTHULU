#include "CultFreeRoamSave.h"
#include "CultWorldState.h"
#include "CultKitRuntime.h"
#include "CultMatchDirector.h"
#include "CultSkillAttack.h"
#include "Serialization/BufferArchive.h"
#include "Serialization/MemoryReader.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/Crc.h"
#include "HAL/FileManager.h"
#include <type_traits>

namespace {
constexpr uint32 Magic=0x43554C54,Version=2;
constexpr int32 MaxBytes=16*1024*1024,MaxActors=12000;
FString Path(const FString& Slot) { FString Safe; for(TCHAR C:Slot) if(FChar::IsAlnum(C)||C==TEXT('_')) Safe.AppendChar(C); return FPaths::ProjectSavedDir()/TEXT("SaveGames")/(Safe.Left(64)+TEXT(".sav")); }
struct FRecord {
 uint8 Type=0; FTransform Transform; TArray<FName> Tags; FString Model,Character;
 TArray<uint8> State;
 void Archive(FArchive& A) { A<<Type<<Transform;
  TArray<FString> Names; if(A.IsSaving()) for(auto N:Tags) Names.Add(N.ToString()); A<<Names;
  if(A.IsLoading()) { Tags.Reset(); for(auto& N:Names) Tags.Add(FName(*N)); }
  A<<Model<<Character<<State; }
};
struct FSnapshot {
 int32 Seed=0; FString Character; TArray<int32> Beliefs; FDateTime Date;
 TArray<FRecord> Actors; TArray<uint8> Director;
 void Archive(FArchive& A) {
  A<<Seed<<Character<<Beliefs<<Date;
  int32 N=Actors.Num(); A<<N; if(N<1||N>MaxActors) { A.SetError(); return; }
  if(A.IsLoading()) Actors.SetNum(N);
  for(auto& R:Actors) { R.Archive(A); if(R.Type>3||R.State.Num()>MaxBytes) A.SetError(); }
  A<<Director;
 }
};
bool Decode(const TArray<uint8>& Data,FSnapshot& S,FString& Error) {
 if(Data.Num()<16||Data.Num()>MaxBytes) { Error=TEXT("Save is missing or has an invalid size."); return false; }
 FMemoryReader A(Data); uint32 M=0,V=0,Size=0,CRC=0; A<<M<<V<<Size<<CRC;
 if(M!=Magic||V!=Version||Size!=uint32(Data.Num()-16)||CRC!=FCrc::MemCrc32(Data.GetData()+16,Size)) {
  Error=TEXT("Save is damaged or belongs to an incompatible version."); return false;
 }
 S.Archive(A);
 if(A.IsError()||A.Tell()!=A.TotalSize()||!CultCanonicalKit::IsReleased(S.Character)||S.Beliefs.Num()!=3) { Error=TEXT("Save validation failed."); return false; }
 return true;
}
// The visitor also works with headless core containers, without an Unreal dependency there.
struct FCoreArchive {
 FArchive& A;
 template<class... T> void operator()(T&... V) { (Value(V),...); }
 template<class T> void Value(T& V) {
  if constexpr(std::is_enum_v<T>) { int32 I=int32(V); A<<I; if(A.IsLoading()) V=T(I); }
  else if constexpr(std::is_arithmetic_v<T>) A<<V;
  else V.visitPersistentState(*this);
 }
 template<class T,size_t N> void Value(T (&V)[N]) { for(auto& X:V) Value(X); }
 template<class T> void Value(std::vector<T>& V) {
  int32 N=int32(V.size()); A<<N; if(N<0||N>MaxActors) { A.SetError(); return; }
  if(A.IsLoading()) V.resize(N); for(auto& X:V) Value(X);
 }
 template<class K,class V> void Value(std::unordered_map<K,V>& M) {
  int32 N=int32(M.size()); A<<N; if(N<0||N>MaxActors) { A.SetError(); return; }
  if(A.IsLoading()) { M.clear(); for(int32 I=0;I<N;++I) { K Key{}; V Val{}; Value(Key); Value(Val); M.emplace(Key,Val); } }
  else for(auto& P:M) { K Key=P.first; Value(Key); Value(P.second); }
 }
};
}

struct FCultSaveAccess {
 static int32 Ref(AActor* P,const TArray<AActor*>& Actors) { return P?Actors.IndexOfByKey(P):INDEX_NONE; }
 template<class T> static void Link(FArchive& A,TWeakObjectPtr<T>& P,const TArray<AActor*>& Actors) {
  int32 Id=Ref(P.Get(),Actors); A<<Id; if(A.IsLoading()) P=Actors.IsValidIndex(Id)?Cast<T>(Actors[Id]):nullptr;
 }
 static void Actor(FArchive& A,AActor* Actor,const TArray<AActor*>& Actors,bool Network=false) {
  if(auto* P=Cast<ACultPopulation>(Actor)) {
   const FString OldSkin=P->HumanSkin_;
   A<<P->bCultist_<<P->bSummoned_<<P->bAttack_<<P->Health_<<P->CreatureMaxHp_<<P->bSorcerer_<<P->AttackTime_<<P->Home_<<P->Faction_<<P->Loyalty_<<P->bLunatic_<<P->bCaptive_<<P->CaptiveFaction_<<P->Activity_<<P->OrderPoint_<<P->OrderTime_<<P->bDirective_<<P->bPlayerBattle_<<P->HumanSkin_;
   Link(A,P->Captor_,Actors); Link(A,P->OrderTarget_,Actors);
   if(A.IsLoading()) { P->HumanSkin_=OldSkin; P->AppearanceFaction_=-99; P->RefreshAuthoredHuman(); }
  } else if(auto* Building=Cast<ACultDestructible>(Actor)) {
   bool WasDestroyed=Building->bDestroyed_; A<<Building->Health_<<Building->MaxHealth_<<Building->HealthMultiplier_<<Building->bDestroyed_;
   if(A.IsLoading()&&Building->bDestroyed_&&!WasDestroyed) Building->Collapse(Building->GetActorLocation());
  } else if(auto* Beast=Cast<ACultFeralBeast>(Actor)) {
   A<<Beast->Health_<<Beast->MaxHealth_<<Beast->bCaptured_<<Beast->CaptiveFaction_<<Beast->Home_<<Beast->MoveTarget_<<Beast->AttackCooldown_;
   Link(A,Beast->Captor_,Actors);
  } else if(auto* Hero=Cast<ACultUlhuPlayerPawn>(Actor)) {
   A<<Hero->Faction_<<Hero->HomeLocation_<<Hero->LastSafeLocation_<<Hero->Health_<<Hero->DashCooldown_;
   float CombatAge=Hero->GetWorld()->GetTimeSeconds()-Hero->LastCombatAt_; A<<CombatAge; if(A.IsLoading()) Hero->LastCombatAt_=Hero->GetWorld()->GetTimeSeconds()-CombatAge;
   float Hp=Hero->GetHealth(); A<<Hp;
   if(Hero->Kit_) {
    A<<Hero->Kit_->Stamina.value<<Hero->Kit_->HeavyCooldown;
    for(auto& C:Hero->Kit_->Cooldowns) A<<C;
    if(A.IsLoading()) Hero->Kit_->Caster->revive(Hp);
   }
   FRotator R=Hero->GetController()?Hero->GetController()->GetControlRotation():Hero->GetActorRotation(); A<<R;
   if(A.IsLoading()&&!Network) {
    Hero->GetCharacterMovement()->StopMovementImmediately();
    Hero->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
    if(Hero->GetController()) Hero->GetController()->SetControlRotation(R);
   }
  }
 }
 static void World(FArchive& A,ACultWorldDirector* D,const TArray<AActor*>& Actors) {
  if(A.IsLoading()) {
   delete D->State_; D->State_=new ACultWorldDirector::FState; D->InitializeFaith();
   for(AActor* Actor:Actors) {
    if(auto* P=Cast<ACultPopulation>(Actor)) D->Register(P);
    if(auto* P=Cast<ACultUlhuPlayerPawn>(Actor)) if(P->IsBot()) D->State_->Bots.Add(P);
   }
  }
  auto& S=*D->State_; FCoreArchive Core{A};
  int32 RandomSeed=S.Random.GetCurrentSeed(); A<<RandomSeed; if(A.IsLoading()) S.Random.Initialize(RandomSeed);
  A<<D->PopulationTime_<<D->CityTime_<<D->BotTime_<<S.Temples<<S.Goals<<S.DecisionTimers<<S.PreparedActive;
  float PreachRemaining=S.NextPreach-D->GetWorld()->GetTimeSeconds(); A<<PreachRemaining; if(A.IsLoading()) S.NextPreach=D->GetWorld()->GetTimeSeconds()+PreachRemaining;
  for(auto& F:S.Faiths) {
   double Clock=F->Clock.now(); float Power=F->Power.value(); FString RNG=UTF8_TO_TCHAR(F->Rng.state().c_str());
   A<<Clock<<Power<<RNG<<F->Journal<<F->EventCounts<<F->LastContribution<<F->ContributionUntil<<F->SocietyTimer<<F->WorkTimer<<F->CombatRemaining<<F->InCombat;
   Core(F->Beliefs,F->Exertion,F->Cult,F->Wars,F->Traps);
   if(A.IsLoading()) { F->Clock.reset(); F->Clock.advance(Clock); F->Power.set(Power); F->Rng.restoreState(TCHAR_TO_UTF8(*RNG)); }
   Link(A,F->WorkAltar,Actors); Link(A,F->ConversionTarget,Actors);
   int32 Count=int32(F->Rites.size()); A<<Count; if(Count<0||Count>MaxActors) { A.SetError(); return; }
   if(A.IsLoading()) F->Rites.resize(Count);
   for(auto& R:F->Rites) {
    A<<R.Sacrifice<<R.PerformerHp<<R.VictimHp; Link(A,R.Performer,Actors); Link(A,R.Victim,Actors); Link(A,R.Altar,Actors);
    if(A.IsLoading()) {
     if(R.Sacrifice) { auto Rite=std::make_unique<cultulhu::SacrificeRitual>(F->Bus); Rite->setPerformer(R.Performer.IsValid()?(1ull<<63)|R.Performer->GetUniqueID():0); Rite->setVictim(R.Victim.IsValid()?(1ull<<63)|R.Victim->GetUniqueID():0); R.Core=std::move(Rite); }
     else R.Core=std::make_unique<cultulhu::NecromancyRitual>(F->Bus);
    }
    Core(*R.Core);
   }
   Count=int32(F->Snares.size()); A<<Count; if(Count<0||Count>MaxActors) { A.SetError(); return; }
   if(A.IsLoading()) F->Snares.resize(Count);
   for(auto& R:F->Snares) { A<<R.Id<<R.Point; Link(A,R.Marker,Actors); }
   TArray<int32> Tortured; if(A.IsSaving()) for(auto* Actor:Actors) if(F->Tortured.Contains(Actor->GetUniqueID())) Tortured.Add(Ref(Actor,Actors));
   A<<Tortured; if(A.IsLoading()) for(int32 Id:Tortured) if(Actors.IsValidIndex(Id)) F->Tortured.Add(Actors[Id]->GetUniqueID());
  }
  int32 N=int32(S.Cities.size()); A<<N;
  if(N<0||N>100) { A.SetError(); return; }
  if(A.IsLoading()) S.Cities.resize(N);
  for(auto& City:S.Cities) {
   A<<City.Center; int32 Count=City.Buildings.Num(); A<<Count;
   if(Count<0||Count>MaxActors) { A.SetError(); return; }
   if(A.IsLoading()) City.Buildings.SetNum(Count);
   for(auto& B:City.Buildings) Link(A,B,Actors);
  }
  // All people are rebuilt in actor order, including the core HP/loyalty mirror.
  N=int32(S.People.size()); A<<N;
  if(N<0||N>MaxActors) { A.SetError(); return; }
  for(int32 I=0;I<N;++I) {
   int32 Id=INDEX_NONE,Faction=0,CultState=0; float Timer=0,Rest=0,RestDue=90; bool Lost=false;
   if(A.IsSaving()) { auto& P=S.People[I]; Id=Ref(P.Actor.Get(),Actors); Faction=P.Faction; Timer=P.Timer; Rest=P.RestRemaining; RestDue=P.RestDue; Lost=P.Lost; if(P.Faction>=0&&P.Faction<3&&P.Index<S.Faiths[P.Faction]->Cult.size()) CultState=int32(S.Faiths[P.Faction]->Cult.at(P.Index).state()); if(P.Actor.IsValid()&&P.Actor->IsLunatic()) CultState=int32(cultulhu::CultistState::Lunatic); }
   A<<Id<<Faction<<Timer<<Rest<<RestDue<<Lost<<CultState;
   if(A.IsLoading()&&Actors.IsValidIndex(Id)) for(auto& P:S.People) if(P.Actor==Actors[Id]) {
    P.Timer=Timer; P.RestRemaining=Rest; P.RestDue=RestDue;
    auto* Actor=P.Actor.Get(); auto& C=S.Faiths[P.Faction]->Cult.at(P.Index);
    C.revive(Actor->GetHealth()); C.setDevotion(Actor->GetLoyalty()); C.setState(cultulhu::CultistState(FMath::Clamp(CultState,0,4)));
   }
  }
  auto Links=[&](auto& List) { int32 Count=List.Num(); A<<Count; if(Count<0||Count>MaxActors) { A.SetError(); return; } if(A.IsLoading()) List.SetNum(Count); for(auto& P:List) Link(A,P,Actors); };
  Links(S.Selected); Links(S.Prepared);
  N=int32(S.Builds.size()); A<<N; if(N<0||N>MaxActors) { A.SetError(); return; }
  if(A.IsLoading()) S.Builds.resize(N);
  for(auto& B:S.Builds) {
   int32 Kind=A.IsSaving()?int32(B.Site->targetType()):0; float Progress=A.IsSaving()?B.Site->progress():0;
   A<<B.Point<<B.Faction<<B.LastDecile<<Kind<<Progress; Links(B.Workers);
   if(A.IsLoading()) { B.Site=std::make_unique<cultulhu::ConstructionSite>(B.Faction,cultulhu::Vec3{float(B.Point.X/100),float(B.Point.Z/100),float(B.Point.Y/100)},cultulhu::BuildingType(Kind)); B.Site->restoreProgress(Progress); }
  }
  N=int32(S.Labors.size()); A<<N; if(N<0||N>MaxActors) { A.SetError(); return; }
  if(A.IsLoading()) S.Labors.resize(N);
  for(auto& J:S.Labors) { Link(A,J.Worker,Actors); Link(A,J.Victim,Actors); Link(A,J.Altar,Actors); A<<J.Kind<<J.Remaining<<J.Elapsed<<J.WorkerHp<<J.VictimHp; }
  if(A.IsLoading()) for(auto& P:S.People) if(P.RestRemaining>0) S.Faiths[P.Faction]->Dreams.startRest(P.Id);
 }
 static void Body(FArchive& A,AActor* Body) {
  if(auto* D=Cast<ACultWorldDirector>(Body)) {
   if(!D->State_) return;
   FString Summary=D->Status(); int32 Cities=D->CityCount(),Prepared=D->PreparedCount(); bool IsPrepared=D->BattlePrepared();
   A<<Summary<<Cities<<Prepared<<IsPrepared;
   if(A.IsLoading()) { D->State_->RemoteStatus=Summary; D->State_->RemoteCities=Cities; D->State_->RemotePrepared=Prepared; D->State_->PreparedActive=IsPrepared; }
   for(int32 I=0;I<3;++I) { FString Activity=D->ActivitySummary(I),Faith=D->FaithSummary(I); A<<Activity<<Faith; if(A.IsLoading()) { D->State_->RemoteActivities[I]=Activity; D->State_->RemoteFaith[I]=Faith; } }
   FCoreArchive Core{A}; for(auto& F:D->State_->Faiths) {
    float Power=F->Power.value(); A<<Power<<F->Journal<<F->EventCounts; Core(F->Beliefs,F->Exertion);
    if(A.IsLoading()) F->Power.set(Power);
   }
   return;
  }
  if(auto* Skill=Cast<ACultSkillAttack>(Body)) {
   int32 Shape=int32(Skill->Shape),Old=Shape;
   A<<Shape<<Skill->Windup<<Skill->Duration<<Skill->Radius<<Skill->Reach<<Skill->Speed<<Skill->HalfAngle<<Skill->Direction<<Skill->Age<<Skill->Origin<<Skill->Previous<<Skill->SweepOffset;
   if(A.IsLoading()) { Skill->Shape=ACultSkillAttack::EShape(Shape); if(Old!=Shape) Skill->BuildPresentation(); }
   return;
  }
  if(auto* Hero=Cast<ACultUlhuPlayerPawn>(Body)) { A<<Hero->Notice_<<Hero->NoticeTime_<<Hero->SwingTime_<<Hero->bRmbHeld_; }
  TArray<FString> Tags; if(A.IsSaving()) for(auto Tag:Body->Tags) Tags.Add(Tag.ToString()); A<<Tags; if(A.IsLoading()) { Body->Tags.Reset(); for(auto& Tag:Tags) Body->Tags.Add(FName(*Tag)); }
  if(auto* Building=Cast<ACultDestructible>(Body)) {
   FString Mesh=Building->MeshComp_->GetStaticMesh()?Building->MeshComp_->GetStaticMesh()->GetPathName():FString(); A<<Mesh;
   if(A.IsLoading()&&!Mesh.IsEmpty()) Building->SetMesh(LoadObject<UStaticMesh>(nullptr,*Mesh));
  }
  if(auto* Beast=Cast<ACultFeralBeast>(Body)) {
   FString Species=Beast->Species(); A<<Species;
   if(A.IsLoading()&&Species!=Beast->Species()) { Beast->SpeciesId_=Species; Beast->ApplySpeciesStats(); Beast->AssembleCreature(); }
  }
  const TArray<AActor*> Empty;
  Actor(A,Body,Empty,true);
 }
 static bool Capture(UWorld* W,FSnapshot& S,FString& Error) {
  auto* GI=Cast<UCultUlhuGameInstance>(W->GetGameInstance()); auto* D=ACultWorldDirector::Find(W);
  if(!GI||!D||!D->State_||ACultMatchDirector::IsMatch(W)||W->GetNetMode()==NM_Client) { Error=TEXT("Only the Free Roam host can save the world."); return false; }
  S.Seed=GI->FreeRoamSeed; S.Character=GI->SelectedCharacterId; S.Beliefs=GI->InitialBeliefs(); S.Date=FDateTime::UtcNow();
  TArray<AActor*> Actors;
  if(auto* P=UGameplayStatics::GetPlayerPawn(W,0)) Actors.Add(P);
  for(TActorIterator<AActor> It(W);It;++It) {
   if(Actors.Contains(*It)||It->ActorHasTag(TEXT("CultLegacyGeometry"))) continue;
   if(auto* Hero=Cast<ACultUlhuPlayerPawn>(*It)) { if(Hero->IsBot()) Actors.Add(Hero); }
   else if(It->IsA<ACultPopulation>()||It->IsA<ACultDestructible>()) Actors.Add(*It);
   else if(auto* B=Cast<ACultFeralBeast>(*It)) if(!B->IsDead()) Actors.Add(B);
  }
  for(auto* Actor:Actors) {
   FRecord R; R.Transform=Actor->GetActorTransform(); R.Tags=Actor->Tags;
   if(auto* Hero=Cast<ACultUlhuPlayerPawn>(Actor)) { R.Type=0; R.Character=Hero->CharacterId_; }
   else if(auto* P=Cast<ACultPopulation>(Actor)) R.Type=1;
   else if(auto* Building=Cast<ACultDestructible>(Actor)) { R.Type=2; if(Building->MeshComp_->GetStaticMesh()) R.Model=Building->MeshComp_->GetStaticMesh()->GetPathName(); }
   else if(auto* Beast=Cast<ACultFeralBeast>(Actor)) { R.Type=3; R.Character=Beast->Species(); }
   FBufferArchive A; FCultSaveAccess::Actor(A,Actor,Actors); R.State=A; S.Actors.Add(MoveTemp(R));
  }
  FBufferArchive A; World(A,D,Actors); S.Director=A;
  return !A.IsError();
 }
 static bool Restore(UWorld* W,FSnapshot& S,FString& Error) {
  auto* D=ACultWorldDirector::Find(W); auto* Player=Cast<ACultUlhuPlayerPawn>(UGameplayStatics::GetPlayerPawn(W,0));
  if(!D||!Player||S.Actors.IsEmpty()||S.Actors[0].Type!=0||W->GetNetMode()==NM_Client) { Error=TEXT("Free Roam is not ready to load."); return false; }
  // Resolve assets before touching the current world. Unknown paths never spawn arbitrary classes.
  for(auto& R:S.Actors) if(R.Type==2&&!LoadObject<UStaticMesh>(nullptr,*R.Model)) { Error=TEXT("A saved building asset is unavailable."); return false; }
  TArray<AActor*> Old;
  for(TActorIterator<AActor> It(W);It;++It) if(*It!=Player&&!It->ActorHasTag(TEXT("CultLegacyGeometry"))&&
   (It->IsA<ACultPopulation>()||It->IsA<ACultDestructible>()||It->IsA<ACultFeralBeast>()||It->IsA<ACultUlhuPlayerPawn>()||It->ActorHasTag(TEXT("CultRubble"))||It->ActorHasTag(TEXT("ConstructionPreview")))) Old.Add(*It);
  for(auto* A:Old) A->Destroy();
  TArray<AActor*> Actors; Actors.Add(Player); Player->SetActorTransform(S.Actors[0].Transform,false,nullptr,ETeleportType::TeleportPhysics);
  for(int32 I=1;I<S.Actors.Num();++I) {
   auto& R=S.Actors[I]; AActor* Actor=nullptr;
   if(R.Type==0) { auto* P=W->SpawnActorDeferred<ACultUlhuPlayerPawn>(ACultUlhuPlayerPawn::StaticClass(),R.Transform,nullptr,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn); P->ConfigureBot(R.Character,1); Actor=P; }
   if(R.Type==1) Actor=W->SpawnActorDeferred<ACultPopulation>(ACultPopulation::StaticClass(),R.Transform,nullptr,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
   if(R.Type==2) { auto* B=W->SpawnActorDeferred<ACultDestructible>(ACultDestructible::StaticClass(),R.Transform,nullptr,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn); B->SetMesh(LoadObject<UStaticMesh>(nullptr,*R.Model)); Actor=B; }
   if(R.Type==3) { auto* B=W->SpawnActorDeferred<ACultFeralBeast>(ACultFeralBeast::StaticClass(),R.Transform,nullptr,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn); B->SetSpecies(R.Character); Actor=B; }
   if(!Actor) { Error=TEXT("Unable to restore a saved actor."); return false; }
   Actor->Tags=R.Tags; Actor->Tags.Remove(TEXT("CorpseRecorded")); Actor->FinishSpawning(R.Transform); Actors.Add(Actor);
  }
  for(int32 I=0;I<Actors.Num();++I) { FMemoryReader A(S.Actors[I].State); Actor(A,Actors[I],Actors); if(A.IsError()) { Error=TEXT("Actor save data is damaged."); return false; } }
  FMemoryReader A(S.Director); World(A,D,Actors);
  if(A.IsError()) { Error=TEXT("World save data is damaged."); return false; }
  UE_LOG(LogTemp,Display,TEXT("[CultSave] Restored %d actors, %d cities"),Actors.Num(),D->CityCount()); return true;
 }
};

bool CultFreeRoamSave::Capture(UWorld* W,TArray<uint8>& Data,FString& Error) {
 FSnapshot S; if(!FCultSaveAccess::Capture(W,S,Error)) return false;
 FBufferArchive Payload; S.Archive(Payload); if(Payload.IsError()) { Error=TEXT("Could not capture the world."); return false; }
 FBufferArchive File; uint32 M=Magic,V=Version,Size=Payload.Num(),CRC=FCrc::MemCrc32(Payload.GetData(),Size); File<<M<<V<<Size<<CRC; File.Append(Payload); Data=File; return true;
}
bool CultFreeRoamSave::Restore(UWorld* W,const TArray<uint8>& Data,FString& Error) { FSnapshot S; return Decode(Data,S,Error)&&FCultSaveAccess::Restore(W,S,Error); }
bool CultFreeRoamSave::Inspect(const TArray<uint8>& Data,int32& Seed,FString& Character,TArray<int32>& Beliefs,FString& Error) { FSnapshot S; if(!Decode(Data,S,Error)) return false; Seed=S.Seed; Character=S.Character; Beliefs=S.Beliefs; return true; }
bool CultFreeRoamSave::Write(const TArray<uint8>& Data,FString& Error,const FString& Slot) {
 FSnapshot S; if(!Decode(Data,S,Error)) return false;
 const FString File=Path(Slot),Temp=File+TEXT(".tmp"),Backup=File+TEXT(".bak"); auto& FM=IFileManager::Get(); FM.MakeDirectory(*FPaths::GetPath(File),true);
 if(!FFileHelper::SaveArrayToFile(Data,*Temp)) { Error=TEXT("Unable to write save. Check free space and folder permissions."); return false; }
 TArray<uint8> Check; if(!FFileHelper::LoadFileToArray(Check,*Temp)||Check!=Data) { Error=TEXT("Save verification failed; your previous save is intact."); return false; }
 TArray<uint8> Old; FSnapshot Previous; FString Ignored;
 if(FM.FileExists(*File)&&FFileHelper::LoadFileToArray(Old,*File)&&Decode(Old,Previous,Ignored)) if(FM.Copy(*Backup,*File,true,true)!=COPY_OK) { Error=TEXT("Could not preserve the backup; save cancelled."); return false; }
 if(!FM.Move(*File,*Temp,true,true,false,true)) { Error=TEXT("Could not finalize save; your previous save is intact."); return false; }
 return true;
}
bool CultFreeRoamSave::Read(TArray<uint8>& Data,FString& Error,const FString& Slot) {
 for(const FString& File:{Path(Slot),Path(Slot)+TEXT(".bak")}) { FSnapshot S; if(IFileManager::Get().FileSize(*File)>MaxBytes) continue; if(IFileManager::Get().FileExists(*File)&&FFileHelper::LoadFileToArray(Data,*File)&&Decode(Data,S,Error)) { Error.Reset(); return true; } }
 Error=TEXT("No usable Free Roam save was found."); return false;
}
bool CultFreeRoamSave::Exists(const FString& Slot) { return IFileManager::Get().FileExists(*Path(Slot))||IFileManager::Get().FileExists(*(Path(Slot)+TEXT(".bak"))); }

void CultFreeRoamSave::CaptureBody(AActor* Actor,TArray<uint8>& Data) { FBufferArchive A; FCultSaveAccess::Body(A,Actor); Data=A; }
void CultFreeRoamSave::ApplyBody(AActor* Actor,const TArray<uint8>& Data) { FMemoryReader A(Data); FCultSaveAccess::Body(A,Actor); }
