#include "CultKitRuntime.h"
#include "CultSkillAttack.h"
#include "CultUlhuPlayerPawn.h"
#include "CultWorldDirector.h"
#include "CultBlastFX.h"
#include "CultPopulation.h"
#include "CultMatchDirector.h"
#include "CultMatchUnit.h"
#include "Components/CapsuleComponent.h"
#include "combat/Attacks.h"

namespace {
 cultulhu::Vec3 P(FVector V) { return {float(V.X/100),float(V.Z/100),float(V.Y/100)}; }
 FVector BotCommittedAim(ACultUlhuPlayerPawn* Owner,FVector Origin,FVector Aim,float Speed,float Delay,float Reach) {
  if(!Owner->IsBot()||Speed<=0) return Aim;
  auto* Target=Owner->GetBotAim(); if(!ACultSkillAttack::Enemy(Owner,Target)) return Aim;
  // Only lead the visible body the bot actually aimed at, not a stale target
  // beyond its range or an unrelated surface picked by the camera trace.
  if(FVector::Dist2D(Aim,Target->GetActorLocation())>Target->GetSimpleCollisionRadius()+100) return Aim;
  if(auto* Capsule=Target->FindComponentByClass<UCapsuleComponent>()) if(FMath::Abs(Aim.Z-Target->GetActorLocation().Z)>Capsule->GetScaledCapsuleHalfHeight()+100) return Aim;
  FVector Velocity=Target->GetVelocity(); Velocity.Z=0;
  if(Velocity.SizeSquared()<2500) return Aim;
  // Limited linear observation, not a perfect intercept or knowledge of the
  // player's next input. Reversals, dashes and changes of elevation can beat it.
  float Horizon=FMath::Clamp(Delay+float(FVector::Dist(Origin,Aim))/Speed,0.f,.6f);
  FVector Predicted=Aim;
  for(int I=0;I<3;++I) {
   Predicted=Aim+(Velocity*Horizon).GetClampedToMaxSize(FMath::Min(1600.f,Reach*.6f));
   Horizon=FMath::Clamp(Delay+float(FVector::Dist(Origin,Predicted))/Speed,0.f,.6f);
  }
  return ACultSkillAttack::Clear(Owner->GetWorld(),Origin,Predicted,Owner,Target)?Predicted:Aim;
 }
}
void FCultKitRuntime::EmitCast(const cultulhu::SpellDef& S,int Slot) {
 if(auto* D=ACultWorldDirector::Find(Owner->GetWorld())) { D->CombatActivity(Owner->GetFaction()); for(auto Type:{cultulhu::EventType::SpellCast,cultulhu::EventType::AbilityCast}) { cultulhu::GameEvent E(Type); E.sourceId=Caster->id(); E.tag=Type==cultulhu::EventType::SpellCast?S.id:S.name; E.amount=Type==cultulhu::EventType::SpellCast?1.f:float(Slot); E.pos=P(Owner->FeetLocation()); D->Bus(Owner->GetFaction()).publish(E); } }
}
bool FCultKitRuntime::LaunchSpatial(const cultulhu::SpellDef& S,int Slot,FVector Aim) {
 using Shape=ACultSkillAttack::EShape;
 const float H=Owner->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()*2, Body=Owner->GetCapsuleComponent()->GetScaledCapsuleRadius();
 FVector Muzzle=Owner->FeetLocation()+FVector(0,0,H*.52f)+Owner->AimRotation().Vector()*Body*.8f;
 FVector Origin=Muzzle; Shape Kind=Shape::Bolt; float Delay=.20f,Life=0,Radius=35,Reach=S.range*100,Speed=2600,Angle=45;
 const auto& Id=S.id;
 if(S.effectKind=="aoe_damage") { Kind=Shape::GroundDisk; Origin=Aim; Delay=Slot==2?.9f:.5f; Life=.06f; Radius=120.f; }
 if(S.effectKind=="fear_aura"||S.effectKind=="pull") { Kind=Shape::Sphere; Origin=Owner->FeetLocation()+FVector(0,0,100); Delay=.45f; Life=.06f; Radius=Reach; }
 if(S.effectKind=="domination_summon") { Kind=Shape::Cone; Origin=Muzzle; Delay=.5f; Life=.08f; Angle=45; }
 if(S.effectKind=="heavy") { if(Def.rightClick.kind==cultulhu::HeavyAttackKind::MeleeHeavy) { Kind=Shape::Sweep; Reach+=Body; Origin=Owner->FeetLocation()+FVector(0,0,FMath::Min(H*.48f,Reach*.55f)); Radius=FMath::Clamp(H*.04f,30.f,130.f); Delay=.24f; Life=.25f; } else { Radius=Def.rightClick.kind==cultulhu::HeavyAttackKind::AcidSpit?65:40; Speed=2200; Delay=.25f; } }
 // Individual source spells keep their identity: thin fast needles, wide slow cries,
 // paired fangs, a short piercing beam, and delayed ground impacts.
 if(Id=="nerve_needle") { Radius=14; Speed=4800; Delay=.12f; }
 if(Id=="dream_pierce"||Id=="abyssal_harpoon") { Radius=24; Speed=3400; }
 if(Id=="tattered_shriek") { Radius=100; Speed=1800; Delay=.3f; }
 if(Id=="thousand_lashes") { Radius=55; Speed=3000; }
 if(Id=="temporal_lash"||Id=="gate_lash") { Radius=45; Speed=2800; }
 if(Id=="obsidian_shards") { Radius=70; Speed=2400; }
 if(Id=="serpent_strike") { Radius=28; Speed=3600; }
 if(Id=="blight_ray") { Kind=Shape::Beam; Delay=.35f; Life=.12f; Radius=25; }
 if(S.effectKind=="stun"||S.effectKind=="debuff") { Radius=40; Speed=2200; Delay=.25f; }
 if(Id=="claw_of_the_sphinx"||Id=="crushing_maw"||Id=="crusher_claw") { Kind=Shape::Cone; Origin=Muzzle; Delay=.30f; Life=.06f; Angle=Id=="crusher_claw"?28:45; }
 if(Id=="doomfall_stomp"||Id=="protoplasm_slam") { Origin=Owner->FeetLocation()+FVector(0,0,12); Delay=.55f; }
 if(Kind==Shape::Bolt&&Owner->IsBot()) {
  if(auto* Target=Owner->GetBotAim()) if(ACultSkillAttack::Enemy(Owner,Target)) if(auto* Collision=::Cast<UPrimitiveComponent>(Target->GetRootComponent())) {
   FVector Closest; const float Distance=Collision->GetClosestPointOnCollision(Origin,Closest);
   if(Distance>=0&&Distance>Reach+Radius+5) return false;
  }
  Aim=BotCommittedAim(Owner,Origin,Aim,Speed,Delay,Reach);
 }
 FVector Direction=(Aim-Origin).GetSafeNormal(); if(Direction.IsNearlyZero()) Direction=Owner->AimRotation().Vector();
 LastEffectLocation=(S.effectKind=="fear_aura"||S.effectKind=="pull")?Owner->FeetLocation():Origin;
 TWeakObjectPtr<ACultUlhuPlayerPawn> Weak(Owner);
 int Bolts=Id=="twin_fangs"?2:1;
 for(int I=0;I<Bolts;++I) {
  const FTransform T(Origin); auto* A=Owner->GetWorld()->SpawnActorDeferred<ACultSkillAttack>(ACultSkillAttack::StaticClass(),T,Owner,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn); if(!A) return false;
  A->Shape=Kind; A->Direction=Direction.RotateAngleAxis(Bolts>1?(I==0?-2.f:2.f):0.f,FVector::UpVector); A->Windup=Delay; A->Reach=Reach; A->Radius=Radius; A->Speed=Speed; A->Duration=Life>0?Life:Reach/Speed+.05f; A->HalfAngle=Angle;
  cultulhu::SpellDef HitSpell=S; if(Bolts>1) HitSpell.effectPower*=.5f;
  A->Hit=[Weak,HitSpell,Slot,Origin,Reach](AActor* Target,FVector Point) { if(Weak.IsValid()) { auto Impact=HitSpell; if(Impact.effectKind=="aoe_damage") Impact.effectPower*=1.f-.5f*FMath::Clamp(float(FVector::Dist2D(Origin,Point))/Reach,0.f,1.f); Weak->ResolveSkillHit(Impact,Target,Point,Slot); } };
  A->Finished=[Weak,Origin,Kind](FVector Point) { if(Weak.IsValid()&&Kind!=Shape::Bolt) ACultBlastFX::SpawnBlast(Weak->GetWorld(),ECultBlastKind::ShockRing,Origin,1); };
  A->FinishSpawning(T);
 }
 return true;
}
void FCultKitRuntime::ResolveSpatial(cultulhu::SpellDef S,AActor* Target,FVector Point,int Slot) {
 if(!ACultSkillAttack::Alive(Target)) return; if(auto* P=::Cast<ACultUlhuPlayerPawn>(Target)) if(P->IsEvading()) { P->ConfirmEvade(); return; }
 auto* D=ACultWorldDirector::Find(Owner->GetWorld()); if(!D) return; Snapshot(); Mirror* Found=nullptr; for(auto& M:Mirrors) if(M.Actor==Target) Found=&M; if(!Found||!Found->Core->alive()) return;
 if(auto* U=::Cast<ACultMatchUnit>(Target)) if(U->IsStructure()&&(S.effectKind=="pull"||S.effectKind=="fear_aura"||S.effectKind=="stun"||S.effectKind=="debuff")) return;
 auto& T=*Found->Core; auto& B=D->Beliefs(Owner->GetFaction()); auto& Bus=D->Bus(Owner->GetFaction()); const auto& K=S.effectKind;
 if(K=="stun"||K=="debuff"||K=="fear_aura") Effects.apply(T.id(),K=="debuff"?cultulhu::CCType::Slow:K=="fear_aura"?cultulhu::CCType::Fear:cultulhu::CCType::Stun,S.effectPower,B,Bus);
 else if(K=="pull") { FVector Delta=(Owner->FeetLocation()-Target->GetActorLocation()).GetSafeNormal2D()*FMath::Min(S.effectPower*100.f,FVector::Dist2D(Owner->FeetLocation(),Target->GetActorLocation())); Target->SetActorLocation(Target->GetActorLocation()+Delta,true); T.setPosition(P(Target->GetActorLocation())); Found->BeforePosition=Target->GetActorLocation(); }
 else {
  float Base=S.effectPower;
  if(auto* M=ACultMatchDirector::Find(Owner->GetWorld())) Base*=K=="domination_summon"?15.f:K=="heavy"?15.f/135.f:.2f;
  else if(K=="heavy"&&::Cast<ACultUlhuPlayerPawn>(Target)) Base*=10.f/135.f;
  if(auto* M=ACultMatchDirector::Find(Owner->GetWorld())) Base*=M->DamageMultiplier(Owner);
  cultulhu::CCType CC=cultulhu::CCType::Count; float Seconds=0;
  if(K=="heavy") { Seconds=Def.rightClick.ccSeconds; for(int I=0;I<4;++I) if(Def.rightClick.ccType==cultulhu::ccTypeName(static_cast<cultulhu::CCType>(I))) CC=static_cast<cultulhu::CCType>(I); }
  cultulhu::castSpell(Caster->id(),Caster->type(),T,Base,cultulhu::DamageType::Shadow,CC,Seconds,B,Bus,Effects,cultulhu::EventType::CivilianSlain,BuffMult*D->CombatMultiplier(Owner->GetFaction()));
  if(K=="heavy"&&Def.rightClick.kind==cultulhu::HeavyAttackKind::EldritchGrasp&&!(::Cast<ACultMatchUnit>(Target)&&::Cast<ACultMatchUnit>(Target)->IsStructure())) { FVector Delta=(Owner->FeetLocation()-Target->GetActorLocation())*.4f; Target->SetActorLocation(Target->GetActorLocation()+Delta,true); T.setPosition(P(Target->GetActorLocation())); Found->BeforePosition=Target->GetActorLocation(); }
 }
 LastEffectLocation=Point; Apply(); if(K=="stun"||K=="debuff"||K=="fear_aura"||K=="pull") { Owner->HitConfirm(); ACultBlastFX::SpawnBlast(Owner->GetWorld(),ECultBlastKind::HitFlash,Point,.4f); }
}
