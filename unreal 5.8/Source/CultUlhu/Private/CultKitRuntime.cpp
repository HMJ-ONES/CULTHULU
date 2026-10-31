#include "CultKitRuntime.h"
#include "CultActorLabels.h"
#include "ProfilingDebugging/CsvProfiler.h"
CSV_DEFINE_CATEGORY(CultRuntime,true);
#include <algorithm>
#include "CultSkillAttack.h"
#include "CultMatchDirector.h"
#include "CultMatchUnit.h"
#include "CultUlhuGameInstance.h"
#include "CultUlhuPlayerPawn.h"
#include "CultPopulation.h"
#include "CultFeralBeast.h"
#include "CultDestructible.h"
#include "CultBlastFX.h"
#include "CultWorldDirector.h"
#include "EngineUtils.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Subsystems/CultBeliefSubsystem.h"
#include "combat/KitCaster.h"
#include "combat/Attacks.h"
#include "entities/Units.h"
#include "entities/Structures.h"

namespace { cultulhu::Vec3 CorePos(FVector P) { return {float(P.X / 100), float(P.Z / 100), float(P.Y / 100)}; }
FVector UnrealPos(cultulhu::Vec3 P) { return FVector(P.x, P.z, P.y) * 100; }
float ActorHp(AActor* A) { if(auto* U=::Cast<ACultMatchUnit>(A)) return U->IsInTransit()?0:U->GetHealth(); if(auto* B=::Cast<ACultFeralBeast>(A)) return B->IsDead()?0:B->GetHealth(); if(auto* P=::Cast<ACultPopulation>(A)) return P->GetHealth(); if(auto* B=::Cast<ACultDestructible>(A)) return B->IsDestroyed()?0:B->GetHealth(); if(auto* P=::Cast<ACultUlhuPlayerPawn>(A)) return P->GetHealth(); return 0; }
}
FCultKitRuntime::FCultKitRuntime(ACultUlhuPlayerPawn* InOwner):Owner(InOwner) {}
bool FCultKitRuntime::Initialize(const FString& Id)
{
 if(!CultCanonicalKit::Load(Id,Def)) return false;
 if(ACultMatchDirector::IsMatch(Owner->GetWorld())) { Def.maxHp=100; if(auto* G=::Cast<UCultUlhuGameInstance>(Owner->GetGameInstance())) if(G->SelectedMode==ECultPlayMode::Domination) Cooldowns[2]=Def.rAbility.cooldownSec+5; }
 WorldImpact=std::make_unique<cultulhu::Entity>(cultulhu::EntityType::Building,-1,cultulhu::Vec3{},1000000000.f);
 Caster=std::make_unique<cultulhu::Entity>(cultulhu::EntityType::EldritchAvatar,Owner->GetFaction(),CorePos(Owner->FeetLocation()),Def.maxHp);
 auto Chain=cultulhu::defaultBasicFlurry(); Chain.id=Def.meleeComboId; Combo=cultulhu::ComboTracker(Chain);
 Effects.setVictimBeliefs([this](uint64_t Id)->const cultulhu::BeliefSystem* { auto* D=ACultWorldDirector::Find(Owner->GetWorld()); if(!D) return nullptr; for(auto& M:Mirrors) if(M.Core->id()==Id&&M.Actor.IsValid()&&M.Core->faction()>=0) return &D->Beliefs(M.Core->faction()); return nullptr; });
 Rmb=cultulhu::createRmbAbility(Def.rmbAbilityId); Stamina.value=Def.maxStamina; return true;
}
void FCultKitRuntime::Snapshot()
{
 CSV_SCOPED_TIMING_STAT(CultRuntime, KitSnapshot);
 Mirrors.erase(std::remove_if(Mirrors.begin(),Mirrors.end(),[](const Mirror& M){return !M.Actor.IsValid();}),Mirrors.end());
 Caster->setPosition(CorePos(Owner->FeetLocation()));
 TSet<AActor*> Known; for(auto& M:Mirrors) if(M.Actor.IsValid()) Known.Add(M.Actor.Get());
 for(TActorIterator<AActor> It(Owner->GetWorld());It;++It)
 {
  AActor* A=*It; if(A==Owner || ActorHp(A)<=0) continue;
  if(Known.Contains(A)) continue;
  Mirror M; M.Actor=A; const auto P=CorePos(A->GetActorLocation()); const float Hp=ActorHp(A);
  if(auto* U=::Cast<ACultMatchUnit>(A)) { if(U->IsStructure()) M.Core=std::make_unique<cultulhu::Building>(U->Team,P,Hp); else M.Core=std::make_unique<cultulhu::Creature>(U->Team,P,"lane_minion",Hp); }
  else if(auto* C=::Cast<ACultPopulation>(A)) { if(C->IsSummoned()) M.Core=std::make_unique<cultulhu::Creature>(C->GetFaction(),P,"summoned",Hp); else if(C->IsCultist()) M.Core=std::make_unique<cultulhu::Cultist>(C->GetFaction(),P,Hp); else M.Core=std::make_unique<cultulhu::Civilian>(P,Hp); }
  else if(auto* B=::Cast<ACultFeralBeast>(A)) M.Core=std::make_unique<cultulhu::Monstrosity>(-1,P,"feral",true,Hp);
  else if(auto* Structure=::Cast<ACultDestructible>(A)) M.Core=std::make_unique<cultulhu::Building>(Structure->GetFaction(),P,Hp);
  else if(auto* Rival=::Cast<ACultUlhuPlayerPawn>(A)) M.Core=std::make_unique<cultulhu::Entity>(cultulhu::EntityType::EldritchAvatar,Rival->GetFaction(),P,Hp);
  if(M.Core) Mirrors.push_back(std::move(M));
 }
 for(auto& M:Mirrors)
 {
  if(!M.Actor.IsValid()) { M.Core->takeDamage(100000); continue; }
  M.BeforeHp=ActorHp(M.Actor.Get());
  if(auto* Person=::Cast<ACultPopulation>(M.Actor.Get())) if(Person->GetFaction()!=M.Core->faction()) { if(Person->IsCultist()) M.Core=std::make_unique<cultulhu::Cultist>(Person->GetFaction(),CorePos(Person->GetActorLocation()),100.f); else M.Core=std::make_unique<cultulhu::Civilian>(CorePos(Person->GetActorLocation()),50.f); }
  M.Core->revive(M.BeforeHp);
  M.BeforePosition=M.Actor->GetActorLocation(); M.Core->setPosition(CorePos(M.BeforePosition));
  if(auto* P=::Cast<ACultPopulation>(M.Actor.Get())) if(M.Core->type()==cultulhu::EntityType::Cultist) { auto* C=static_cast<cultulhu::Cultist*>(M.Core.get()); C->setDevotion(P->GetLoyalty()); C->setState(P->IsLunatic()?cultulhu::CultistState::Lunatic:cultulhu::CultistState::Loyal); }
 }
}
void FCultKitRuntime::Apply()
{
 CSV_SCOPED_TIMING_STAT(CultRuntime, KitApply);
 for(auto& M:Mirrors) if(M.Actor.IsValid())
 {
  AActor* A=M.Actor.Get(); const float D=M.BeforeHp-M.Core->hp();
  if(D>0 && M.Core->faction()>=0 && M.Core->faction()!=Owner->GetFaction()) if(auto* World=ACultWorldDirector::Find(Owner->GetWorld())) World->DeclareWar(Owner->GetFaction(),M.Core->faction());
  const float NativeBefore=ActorHp(A);
  if(D>0) { if(auto* U=::Cast<ACultMatchUnit>(A)) U->Hurt(D,Owner); else if(auto* B=::Cast<ACultFeralBeast>(A)) B->ApplyDamage(D); else if(auto* P=::Cast<ACultPopulation>(A)) P->ReceiveAttack(D,Owner->GetFaction(),true); else if(auto* Building=::Cast<ACultDestructible>(A)) Building->ApplyDamage(D,LastEffectLocation,Owner->GetFaction()); else if(auto* Pawn=::Cast<ACultUlhuPlayerPawn>(A)) Pawn->ReceiveDamage(D,Owner); }
  if(D>0&&!::Cast<ACultUlhuPlayerPawn>(A)) {
   const float After=IsValid(A)?ActorHp(A):0.f;
   Owner->RecordCombatHit(A,LastEffectLocation.IsNearlyZero()?M.BeforePosition:LastEffectLocation,FMath::Clamp(NativeBefore-After,0.f,NativeBefore),After<=0);
  }
  if(D<0&&CanHeal(A)) { if(auto* P=::Cast<ACultPopulation>(A)) P->Heal(-D); if(auto* P=::Cast<ACultUlhuPlayerPawn>(A)) P->HealMatch(-D); }
  if(!IsValid(A)) continue;
  const FVector Position=UnrealPos(M.Core->position());
  if(!Position.Equals(M.BeforePosition,.1)&&!(::Cast<ACultMatchUnit>(A)&&::Cast<ACultMatchUnit>(A)->IsStructure())) A->SetActorLocation(Position,true); // Native collision also constrains held and launched bodies.
  const bool Fear=Effects.has(M.Core->id(),cultulhu::CCType::Fear);
  const bool Stop=Effects.isRooted(M.Core->id()); const float Speed=Fear?1.f:Effects.moveMultiplier(M.Core->id());
  { // Publish inactive states too, so expired effects clear independently of other casters.
 if(auto* U=::Cast<ACultMatchUnit>(A)) if(!U->IsStructure()) U->SetControlState(Stop,Fear,Speed,Owner->GetActorLocation(),Effects.has(M.Core->id(),cultulhu::CCType::Stun),Owner->GetUniqueID());
 if(auto* Pawn=::Cast<ACultUlhuPlayerPawn>(A)) Pawn->SetControlState(Stop,Fear,Speed,Owner->GetActorLocation(),Effects.has(M.Core->id(),cultulhu::CCType::Stun),Owner->GetUniqueID());
 if(auto* B=::Cast<ACultFeralBeast>(A)) B->SetControlState(Stop,Fear,Speed,Owner->GetActorLocation(),Effects.has(M.Core->id(),cultulhu::CCType::Stun),Owner->GetUniqueID());
  if(auto* P=::Cast<ACultPopulation>(A)) P->SetControlState(Stop,Fear,Speed,Owner->GetActorLocation(),Effects.has(M.Core->id(),cultulhu::CCType::Stun),Owner->GetUniqueID());
 }
 }
}
cultulhu::Entity* FCultKitRuntime::Target(float MaxRange,FVector& Aim)
{
 const FHitResult Hit=Owner->CameraAim(MaxRange); Aim=Hit.bBlockingHit?Hit.ImpactPoint:Hit.TraceEnd;
 cultulhu::Entity* Best=nullptr; float Distance=200.f;
 for(auto& M:Mirrors) if(M.Actor.IsValid() && M.Core->alive() && M.Core->faction()!=Caster->faction())
 {
  const float D=FVector::Dist(M.Actor->GetActorLocation(),Aim);
  if(M.Actor==Hit.GetActor()) { Best=M.Core.get(); Distance=D; if(M.Actor==Hit.GetActor()) break; }
 }
 return Best;
}
cultulhu::RmbContext FCultKitRuntime::Context()
{
 CSV_SCOPED_TIMING_STAT(CultRuntime, KitContext);
 auto* S=ACultWorldDirector::Find(Owner->GetWorld());
 cultulhu::RmbContext C(S->Bus(Owner->GetFaction()),Effects,S->Beliefs(Owner->GetFaction()),S->Rng(Owner->GetFaction())); C.caster=Caster.get(); FVector Front=Owner->AimRotation().Vector().GetSafeNormal2D()*Owner->GetCapsuleComponent()->GetScaledCapsuleRadius(); Caster->setPosition(CorePos(Owner->FeetLocation()+Front)); C.casterYaw=FMath::DegreesToRadians(float(Owner->AimRotation().Yaw));
 FVector Aim; if(auto* T=Target(2500,Aim)) C.targetedEntityId=T->id();
 for(auto& M:Mirrors) if(M.Actor.IsValid()) { if(auto* P=::Cast<ACultUlhuPlayerPawn>(M.Actor.Get())) if(P->IsEvading()) continue; if(ACultSkillAttack::Clear(Owner->GetWorld(),Owner->FeetLocation()+Front+FVector(0,0,100),M.Actor->GetActorLocation(),Owner,M.Actor.Get())) C.entities.push_back(M.Core.get()); }
 C.spatialImpact=[this](cultulhu::Vec3 From,cultulhu::Vec3 To,const cultulhu::Entity* Self)->cultulhu::Entity* {
  AActor* Source=nullptr; for(auto& M:Mirrors) if(M.Core.get()==Self) Source=M.Actor.Get();
  FHitResult H; FCollisionQueryParams Q(SCENE_QUERY_STAT(WaveBodySweep),false,Owner); Q.AddIgnoredActor(Source);
  FCollisionShape Shape=FCollisionShape::MakeSphere(35); if(Source) if(auto* Capsule=Source->FindComponentByClass<UCapsuleComponent>()) Shape=FCollisionShape::MakeCapsule(Capsule->GetScaledCapsuleRadius(),Capsule->GetScaledCapsuleHalfHeight());
  if(!Owner->GetWorld()->SweepSingleByChannel(H,UnrealPos(From),UnrealPos(To),FQuat::Identity,ECC_Visibility,Shape,Q)) return nullptr;
  for(auto& M:Mirrors) if(M.Actor==H.GetActor()&&M.Core->alive()) { if(auto* Pawn=::Cast<ACultUlhuPlayerPawn>(M.Actor.Get())) if(Pawn->IsEvading()) return nullptr; return M.Core.get(); }
  WorldImpact->revive(1000000000.f); return WorldImpact.get();
 };
 C.waveOverlap=[this](const cultulhu::Entity& E,cultulhu::Vec3 Origin) { for(auto& M:Mirrors) if(M.Core->id()==E.id()&&M.Actor.IsValid()) { float Half=90; if(auto* Cap=M.Actor->FindComponentByClass<UCapsuleComponent>()) Half=Cap->GetScaledCapsuleHalfHeight(); const float Z=M.Actor->GetActorLocation().Z; return Z-Half<=Origin.y*100+120 && Z+Half>=Origin.y*100-50; } return false; };
 C.groundHeight=[this](cultulhu::Vec3 P) { FHitResult H; FVector V=UnrealPos(P); FCollisionQueryParams Q(SCENE_QUERY_STAT(KitGround),false,Owner); return Owner->GetWorld()->LineTraceSingleByObjectType(H,V+FVector(0,0,10000),V-FVector(0,0,20000),FCollisionObjectQueryParams(ECC_WorldStatic),Q)?float(H.ImpactPoint.Z/100):P.y; };
 return C;
}
void FCultKitRuntime::Tick(float Dt)
{
 CSV_SCOPED_TIMING_STAT(CultRuntime, KitTick);
 if(!Caster || !ACultWorldDirector::Find(Owner->GetWorld())) return; for(float& C:Cooldowns) C=FMath::Max(0.f,C-Dt); HeavyCooldown=FMath::Max(0.f,HeavyCooldown-Dt); BuffTime=FMath::Max(0.f,BuffTime-Dt); if(BuffTime<=0) BuffMult=1;
 // Check before advancing timers: the last active frame must still publish
 // cleared control state. Cast/input/contact paths take fresh snapshots themselves.
 // Keep the original RMB state machine synchronized, including its cooldown.
 const bool StatefulRmb=Rmb&&!UsesMinionThrow();
 const bool Synchronize=StatefulRmb||Effects.activeCount()>0;
 if(Synchronize) Snapshot();
 Effects.tick(Dt); TickMinionThrow(Dt);
 if(StatefulRmb) { auto C=Context(); float Remaining=Dt; while(Remaining>0) { const float Step=FMath::Min(.016f,Remaining); Rmb->update(C,Step); Remaining-=Step; } }
 if(Synchronize) Apply();
}
bool FCultKitRuntime::Cast(int Slot)
{
 if(auto* M=ACultMatchDirector::Find(Owner->GetWorld())) if(!M->AbilityAvailable(Owner,Slot,false)) return false;
 if(!ACultWorldDirector::Find(Owner->GetWorld()) || !Caster || Slot<0 || Slot>2 || Cooldowns[Slot]>0) return false;
 cultulhu::SpellDef S=Slot==0?Def.qAbility:Slot==1?Def.fAbility:Def.rAbility;
 if(ACultMatchDirector::IsMatch(Owner->GetWorld())&&S.effectKind=="heal") S.effectPower*=.4f;
 if(Stamina.value<S.staminaCost) { Owner->Notify(TEXT("Not enough stamina")); return false; }
 if(auto* M=ACultMatchDirector::Find(Owner->GetWorld())) {
  M->OnAction(Owner);
  if(M->Mode()==ECultPlayMode::Domination&&S.effectKind=="summon") {
   Owner->MarkCombat(); auto Surge=S; Surge.effectKind="domination_summon"; FVector Aim=Owner->CameraAim().ImpactPoint;
   if(!LaunchSpatial(Surge,Slot,Aim)) return false; Stamina.value-=S.staminaCost; Cooldowns[Slot]=S.cooldownSec; EmitCast(S,Slot); return true;
  }
 }
 if(S.effectKind=="heal") return CastHeal(S,Slot);
 Owner->MarkCombat();
 if(S.effectKind=="projectile"||S.effectKind=="aoe_damage"||S.effectKind=="stun"||S.effectKind=="debuff"||S.effectKind=="fear_aura"||S.effectKind=="pull") {
  const auto Pick=Owner->CameraAim(S.effectKind=="aoe_damage"?6000.f:25000.f); FVector Point=Pick.ImpactPoint;
  if(S.effectKind=="aoe_damage"&&!Pick.bBlockingHit) { Owner->Notify(TEXT("Aim the warning zone at a solid surface")); return false; }
  if(!LaunchSpatial(S,Slot,Point)) return false; Stamina.value-=S.staminaCost; Cooldowns[Slot]=S.cooldownSec; EmitCast(S,Slot); return true;
 }
 Snapshot(); FVector Aim; auto* T=Target(S.effectKind=="projectile"||S.effectKind=="stun"?S.range*100:25000,Aim);
 if(S.effectKind=="aoe_damage" && !T && !Owner->CameraAim().bBlockingHit) { Owner->Notify(TEXT("Aim at a target or solid ground")); return false; }
 cultulhu::Entity Ground(cultulhu::EntityType::Creature,-1,CorePos(Aim),1000000);
 if(S.effectKind=="aoe_damage"&&!T) T=&Ground;
 std::vector<cultulhu::Entity*> Foes;
 for(auto& M:Mirrors) if(M.Actor.IsValid() && M.Core->alive() && M.Core->faction()!=Caster->faction()) Foes.push_back(M.Core.get());
 TArray<FVector> SummonPoints;
 int Count=0; for(TActorIterator<ACultPopulation> It(Owner->GetWorld());It;++It) ++Count;
 if(S.effectKind=="summon") {
  if(Count+int(S.effectPower)>512) { Owner->Notify(TEXT("No room for the complete summon")); return false; }
  if(!Owner->CameraAim().bBlockingHit) { Owner->Notify(TEXT("Aim summons at solid ground")); return false; }
  for(int I=0;I<int(S.effectPower);++I) {
   bool Placed=false;
   for(int Try=0;Try<24;++Try) { const float A=FMath::FRand()*2*PI; const float R=Try==23?0.f:FMath::FRand()*S.range*100.f; FVector P=Aim+FVector(FMath::Cos(A)*R,FMath::Sin(A)*R,0); FHitResult H; FCollisionQueryParams Q(SCENE_QUERY_STAT(KitSummonPlan),false,Owner);
    // Begin near the aimed surface, so a cavern ceiling cannot replace its floor.
    if(Owner->GetWorld()->LineTraceSingleByObjectType(H,P+FVector(0,0,200),P-FVector(0,0,20000),FCollisionObjectQueryParams(ECC_WorldStatic),Q)&&H.ImpactNormal.Z>.65f) { SummonPoints.Add(H.ImpactPoint); Placed=true; break; }
   }
   if(!Placed) { Owner->Notify(TEXT("No safe ground for the complete summon")); return false; }
  }
 }
 auto* Core=ACultWorldDirector::Find(Owner->GetWorld());
 if(S.effectKind=="heal") { T=nullptr; auto Hit=Owner->CameraAim(); for(auto& M:Mirrors) if(M.Actor==Hit.GetActor() && M.Core->alive() && M.Core->faction()==Caster->faction()) { T=M.Core.get(); break; } }
 auto Result=cultulhu::castKitSpell(S,*Caster,T,Foes,Core->Beliefs(Owner->GetFaction()),Core->Bus(Owner->GetFaction()),Effects,BuffMult*Core->CombatMultiplier(Owner->GetFaction()),Slot);
 Owner->Notify(UTF8_TO_TCHAR(Result.message.c_str())); if(!Result.ok) return false;
 Core->CombatActivity(Owner->GetFaction());
 Stamina.value-=S.staminaCost; Cooldowns[Slot]=S.cooldownSec;
 const bool Self=S.effectKind=="dash"||S.effectKind=="fear_aura"||S.effectKind=="pull"||S.effectKind=="buff"||S.effectKind=="shield"||S.effectKind=="heal";
 LastEffectLocation=Self?Owner->FeetLocation():Aim;
 if(Result.buffSeconds>0) { BuffTime=Result.buffSeconds; BuffMult=Result.buffMult; }
 if(Result.dash.length()>0) { FVector Dir=(Aim-Owner->GetActorLocation()).GetSafeNormal2D(); Owner->AbilityDash(Dir,S.effectPower*100); }
 for(const auto& Summon:Result.summons) for(int I=0;I<Summon.second && Count<512;++I,++Count)
 {
  FActorSpawnParameters Spawn; Spawn.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
  if(auto* Unit=Owner->GetWorld()->SpawnActor<ACultPopulation>(SummonPoints[I]+FVector(0,0,100),FRotator::ZeroRotator,Spawn)) { Unit->SetOwner(Owner); Unit->SummonTo(Owner->GetFaction()); if(ACultMatchDirector::IsMatch(Owner->GetWorld())) Unit->SetLifeSpan(Def.id=="shub_niggurath"?37.5f:30.f); CultActorLabels::Set(Unit,UTF8_TO_TCHAR(Summon.first.c_str())); Unit->Tags.Add(FName(UTF8_TO_TCHAR(Summon.first.c_str()))); Unit->SetAttackOrder(true); }
 }
 Apply(); ACultBlastFX::SpawnBlast(Owner->GetWorld(),ECultBlastKind::ShockRing,LastEffectLocation,FMath::Max(1.f,FMath::Min(S.range,20.f))); return true;
}
void FCultKitRuntime::Heavy(bool Press)
{
 if(UsesMinionThrow()) { MinionThrowInput(Press); return; }
 if(!Caster || !ACultWorldDirector::Find(Owner->GetWorld())) return; if(Press) Owner->MarkCombat(); Snapshot(); auto C=Context();
 if(Rmb) {
  if(!Press) { Rmb->onRelease(C); Apply(); return; }
  if(HeavyCooldown>0||!Rmb->ready()) return;
  if(C.targetedEntityId) { cultulhu::SpellDef S; S.id="wave_direct_stun"; S.name=Def.rightClick.name; S.effectKind="stun"; S.effectPower=2.5f; S.range=25; FVector Aim; Target(2500,Aim); if(LaunchSpatial(S,-1,Aim)) { EmitCast(S,-1); HeavyCooldown=7; } }
  else { C.targetedEntityId=0; Rmb->onPress(C); Apply(); }
  return;
 }
 if(!Press || HeavyCooldown>0) return;
 cultulhu::SpellDef S; S.id=Def.rmbAbilityId; S.name=Def.rightClick.name; S.effectKind="heavy"; S.effectPower=135.f*Def.rightClick.damageMult; S.range=Def.rightClick.range;
 FVector Aim=Owner->CameraAim().ImpactPoint; if(LaunchSpatial(S,-1,Aim)) { HeavyCooldown=6; EmitCast(S,-1); }

}
bool FCultKitRuntime::LeftClick()
{
 if(UsesMinionThrow()) { if(HeldMinion.IsValid()&&HeldMinion->IsHeldBy(Owner)) { ThrowOnReady=true; if(HoldTime>=.3f) LaunchMinion(); return true; } return false; }
 if(!Rmb || !Owner->IsRmbHeld() || !ACultWorldDirector::Find(Owner->GetWorld())) return false; Snapshot(); auto C=Context(); Rmb->onLeftClick(C); Apply(); return true;
}
void FCultKitRuntime::Mouse(float X,float Y) { if(Rmb && !UsesMinionThrow() && Caster && ACultWorldDirector::Find(Owner->GetWorld())) { auto C=Context(); Rmb->onMouseMove(C,X,Y); } }
float FCultKitRuntime::Damage(float Amount) { if(Caster) Caster->takeDamage(Amount); return Hp(); }
void FCultKitRuntime::Heal(float Amount) { if(Caster) Caster->heal(Amount); }
FString FCultKitRuntime::Status() const
{
 return FString::Printf(TEXT("HP %.0f/%.0f | Stamina %.0f | Q %s %.0fs | F %s %.0fs | R %s %.0fs"),Hp(),Def.maxHp,Stamina.value,UTF8_TO_TCHAR(Def.qAbility.name.c_str()),Cooldowns[0],UTF8_TO_TCHAR(Def.fAbility.name.c_str()),Cooldowns[1],UTF8_TO_TCHAR(Def.rAbility.name.c_str()),Cooldowns[2]);
}

float FCultKitRuntime::BeginMelee()
{
 Owner->MarkCombat();
 if(auto* D=ACultWorldDirector::Find(Owner->GetWorld())) D->CombatActivity(Owner->GetFaction());
 MeleeRewarded=false; Combo.onLmbClick(Owner->GetWorld()->GetTimeSeconds()); const auto* Stage=Combo.stageDef();
 if(!Stage) return 1.f;
 Snapshot(); auto* Director=ACultWorldDirector::Find(Owner->GetWorld()); if(!Director) return Stage->damageMult;
 Apply(); return Stage->damageMult*Director->CombatMultiplier(Owner->GetFaction())*Director->Beliefs(Owner->GetFaction()).meleeBonus()*Director->Beliefs(Owner->GetFaction()).spellEffectiveness();
}

void FCultKitRuntime::MeleeContact(AActor* Actor) {
 const auto* Stage=Combo.stageDef(); auto* D=ACultWorldDirector::Find(Owner->GetWorld()); if(D&&!MeleeRewarded) { D->CombatActivity(Owner->GetFaction(),true); MeleeRewarded=true; } if(!Stage||!D||Stage->ccType.empty()) return; Snapshot();
 for(auto& M:Mirrors) if(M.Actor==Actor&&M.Core->alive()) for(int I=0;I<4;++I) if(Stage->ccType==cultulhu::ccTypeName(static_cast<cultulhu::CCType>(I))) Effects.apply(M.Core->id(),static_cast<cultulhu::CCType>(I),Stage->ccSeconds,D->Beliefs(Owner->GetFaction()),D->Bus(Owner->GetFaction())); Apply();
}

bool FCultKitRuntime::CanHeal(AActor* Target) const {
 if(!IsValid(Target)||ActorHp(Target)<=0) return false;
 if(auto* Hero=::Cast<ACultUlhuPlayerPawn>(Target)) return Hero->GetFaction()==Owner->GetFaction()&&!Hero->MatchDead();
 if(ACultMatchDirector::IsMatch(Owner->GetWorld())) return false;
 if(auto* Person=::Cast<ACultPopulation>(Target)) return Person->GetFaction()==Owner->GetFaction()&&(Person->IsCultist()||Person->IsSummoned());
 return false; // Wild or imprisoned beasts are not members of a cult; buildings never receive healing.
}
bool FCultKitRuntime::CastHeal(const cultulhu::SpellDef& Spell,int Slot) {
 const FHitResult Hit=Owner->CameraAim();
 AActor* Patient=Hit.GetActor();
 if(!Patient||ActorHp(Patient)<=0) Patient=Owner;
 if(!CanHeal(Patient)) { Owner->Notify(TEXT("Heal an allied hero; in Free Roam, friendly cult members also qualify.")); return false; }
 if(Patient!=Owner) {
  FVector Closest=Patient->GetActorLocation();
  if(auto* Cap=Patient->FindComponentByClass<UCapsuleComponent>()) Cap->GetClosestPointOnCollision(Owner->GetActorLocation(),Closest);
  if(FVector::Dist2D(Owner->GetActorLocation(),Closest)>Spell.range*100.f+Owner->GetSimpleCollisionRadius()||!ACultSkillAttack::Clear(Owner->GetWorld(),Owner->GetActorLocation(),Patient->GetActorLocation(),Owner,Patient)) { Owner->Notify(TEXT("Healing target is out of reach or behind cover.")); return false; }
 }
 const float Before=ActorHp(Patient);
 const float Power=Spell.effectPower*ACultWorldDirector::Find(Owner->GetWorld())->Beliefs(Owner->GetFaction()).healMultiplier();
 if(auto* Hero=::Cast<ACultUlhuPlayerPawn>(Patient)) Hero->HealMatch(Power);
 if(auto* Person=::Cast<ACultPopulation>(Patient)) Person->Heal(Power);
 const float Actual=ActorHp(Patient)-Before;
 if(Actual<=0) { Owner->Notify(TEXT("Target is already at full health.")); return false; }
 if(auto* M=ACultMatchDirector::Find(Owner->GetWorld())) M->OnAction(Owner);
 Stamina.value-=Spell.staminaCost; Cooldowns[Slot]=Spell.cooldownSec;
 cultulhu::GameEvent E(cultulhu::EventType::HealPerformed); E.sourceId=Caster->id(); E.amount=Actual; ACultWorldDirector::Find(Owner->GetWorld())->Bus(Owner->GetFaction()).publish(E);
 LastEffectLocation=Patient->GetActorLocation(); EmitCast(Spell,Slot);
 Owner->Notify(FString::Printf(TEXT("%s: +%.0f HP"),UTF8_TO_TCHAR(Spell.name.c_str()),Actual));
 ACultBlastFX::SpawnBlast(Owner->GetWorld(),ECultBlastKind::ShockRing,Patient->GetActorLocation(),2.f);
 return true;
}
