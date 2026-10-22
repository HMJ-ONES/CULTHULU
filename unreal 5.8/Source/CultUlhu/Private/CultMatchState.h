#pragma once
#include "CultMatchDirector.h"
#include "CultMatchUnit.h"
#include "CultUlhuPlayerPawn.h"
#include "modes/CapturePointMode.h"
#include "modes/MobaDefense.h"
#include "core/GameClock.h"
#include "core/EventBus.h"
#include "core/RNG.h"
#include <memory>
#include "CultMobaRoute.h"
#include "CultBotThreat.h"
// Retain the source MOBA's objective damage, protection, wave stats and timeout rules.
// Native movement / collision attacks replace its abstract DPS simulation.
class FCultNativeMoba:public cultulhu::MobaDefense {
public:
 using MobaDefense::MobaDefense;
 void Setup() {
  setBase(0,{-150,0,0},GOO_MAX_HP); setBase(1,{150,0,0},GOO_MAX_HP);
  for(int Lane=0;Lane<3;++Lane) {
   std::vector<cultulhu::Vec3> Points;
   for(int I=0;I<=FCultMobaRoute::Segments;++I) { const auto P=FCultMobaRoute::Knot(Lane,I)/200; Points.push_back({float(P.X),0,float(P.Y)}); }
   addLane(Points);
  }
  ensureTowers(); for(auto& T:towers_) T.hp=T.maxHp=3000.f;
 }
 void Advance(float Dt) { elapsed_+=Dt; noteMatchTick("moba"); }
 const std::vector<Tower>& Towers() const { return towers_; }
 float HitTower(int I,float Damage,uint64 Id) { if(I<0||I>=int(towers_.size())) return 0; damageTower(towers_[I],Damage,Id); return FMath::Max(0.f,towers_[I].hp); }
 float HitCore(int Team,float Damage,uint64 Id) { if(aliveTowerCount(Team)>0) return baseHp(Team); damageBase(Team,Damage,Id); return baseHp(Team); }
 void Wave(int N) { minions_.clear(); for(int L=0;L<3;++L) for(int T=0;T<2;++T) { waveNumber_=N-1; spawnWave(T,L); } }
 bool TowerShielded(int I) const {
  if(I<0||I>=int(towers_.size())) return true;
  const auto& T=towers_[I];
  // Each lane's outer tower guards its inner tower.
  int Outer=T.lane*4+T.team*2+(T.team==0?1:0);
  return I!=Outer&&towers_[Outer].alive();
 }
 FVector Waypoint(int Team,int Lane,int Step) const {
  return FCultMobaRoute::Waypoint(Team,Lane,Step);
 }
};
struct ACultMatchDirector::FState {
 cultulhu::EventBus Bus;
 cultulhu::GameClock Clock;
 cultulhu::RNG Rng{72241};
 std::unique_ptr<cultulhu::CapturePointMode> Capture;
 std::unique_ptr<FCultNativeMoba> Moba;
 ECultPlayMode Mode=ECultPlayMode::Domination;
 TArray<FCultMatchFighter> Fighters;
 TArray<TWeakObjectPtr<ACultMatchUnit>> Units;
 bool NexusDestroyed[2]={false,false};
 bool Over=false,Paused=false; int Winner=-1,Wave=0; float Time=0,Warmup=5,NextWave=0,NoticeTime=0,BrainTime=0;
 FString Notice;
 TArray<FCultMatchElimination> Eliminations;
 FVector PingPoint=FVector::ZeroVector; float PingTime=0;
 struct FRelic { FVector Point; FString Name; int Tier=1; float Amp=.12f,Spawn=0,Channel=0,Need=0; int Claim=-1,ChannelSlot=-1; TWeakObjectPtr<AActor> Visual; };
 TArray<FRelic> Relics;
 TArray<FBox2D> ResultButtons;
 float LastBotInvoke[10]={};
 FCultBotThreatMemory BotThreats[10];
};
