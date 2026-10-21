#include "beliefs/BeliefSystem.h"
#include "beliefs/InteractionMatrix.h"
#include "core/EventBus.h"
#include "core/GameClock.h"
#include "core/RNG.h"
#include "cult/CultManager.h"
#include "exertion/ExertionSystem.h"
#include "exertion/ActionExertionTable.h"
#include "ai/AmbientBehavior.h"
#include "dreams/DreamSystem.h"
#include "combat/KitCaster.h"
#include "combat/Attacks.h"
#include "rituals/Ritual.h"
#include "characters/CharacterDef.h"
#include <cmath>
#include <iostream>
using namespace cultulhu;
int failures=0,checks=0;
void check(bool b,const char* why) { ++checks; if(!b) { ++failures; std::cerr<<"FAIL "<<why<<"\n"; } }
bool close(float a,float b) { return std::fabs(a-b)<0.001f; }
struct World { EventBus bus; GameClock clock; RNG rng{123}; BeliefSystem beliefs{bus,clock}; CultManager cult{bus,clock,rng}; PowerSystem power; ExertionSystem ex{bus,beliefs,power,cult,rng}; };
int main() {
 for(const auto& feed:ACTION_EXERTION_TABLE) for(bool active:{false,true}) {
  World w; if(active) w.beliefs.restoreActive({feed.belief});
  GameEvent e(feed.type); e.amount=1; w.bus.publish(e);
  const float expected=std::max(0.f,std::min(100.f,10.f+feed.amount*(active?1.f:.5f)));
  check(close(w.ex.exertion(feed.belief),expected),"canonical action table, active/full and inactive/half");
 }
 { World w; check(w.beliefs.requestChange(Belief::Fear,Belief::Count),"reserve first"); check(w.beliefs.requestChange(Belief::Magic,Belief::Count),"reserve second"); check(w.beliefs.requestChange(Belief::Dreams,Belief::Count),"reserve third"); check(!w.beliefs.requestChange(Belief::War,Belief::Count),"fourth pending belief rejected"); w.beliefs.update(120); check(w.beliefs.active().size()==3,"three requested creeds really adopt"); }
 { World w; w.beliefs.restoreActive({Belief::Chaos}); w.beliefs.requestChange(Belief::Fear,Belief::Count); w.beliefs.update(30); check(close(float(w.beliefs.adoptionRemaining(Belief::Fear)),60),"Chaos adoption doubles speed"); w.beliefs.punishInfringer(); check(close(float(w.beliefs.adoptionRemaining(Belief::Fear)),30),"punishment halves remaining adoption"); check(close(w.cult.insurrectionRisk(),20),"Chaos punishment adds both original risks"); }
 { World w; w.beliefs.restoreActive({Belief::Dreams}); auto& c=w.cult.recruit(); DreamSystem d(w.bus,w.rng,w.beliefs,w.cult,.5f,1.f,1.f); d.setCivilianPicker([]{return uint64_t(0);}); d.startRest(c.id()); int converts=0; w.bus.subscribe(EventType::ConversionPerformed,[&](const GameEvent&){++converts;}); d.setNight(true); check(close(d.update(1),.75f),"night rest produces original power"); check(converts==0,"no phantom dream converts in a bound empty world"); check(c.state()==CultistState::Lunatic,"no-target whispers do not suppress nightmare rolls"); }
 { World w; auto& a=w.cult.recruit(); auto& b=w.cult.recruit(); a.setPosition({0,0,0}); b.setPosition({100,0,0}); b.takeDamage(40); AmbientDirector ad(w.bus,w.rng,w.beliefs,w.cult); ad.setTargetFilter([](const Cultist& x,const Cultist& y){return x.position().distance(y.position())<=6;}); int heals=0,brawls=0; w.bus.subscribe(EventType::HealPerformed,[&](const GameEvent&){++heals;}); w.bus.subscribe(EventType::BrawlBrokeOut,[&](const GameEvent&){++brawls;}); ad.forceAction(0,AmbientAction::TendWounded); ad.forceAction(0,AmbientAction::Brawl); check(heals==0&&brawls==0,"physical context prevents distant brawls and healing"); b.setPosition({2,0,0}); ad.forceAction(0,AmbientAction::TendWounded); check(heals==1&&close(b.hp(),78),"original tending dose reaches actual nearby patient"); }
 { World w; w.beliefs.restoreActive({Belief::Reconstruction}); Entity caster(EntityType::EldritchAvatar,0,{},500); caster.takeDamage(100); SpellDef s; s.effectKind="heal"; s.effectPower=10; s.name="test"; ActiveEffects fx; int casts=0,heals=0; w.bus.subscribe(EventType::SpellCast,[&](const GameEvent&){++casts;}); w.bus.subscribe(EventType::HealPerformed,[&](const GameEvent&){++heals;}); auto r=castKitSpell(s,caster,nullptr,{},w.beliefs,w.bus,fx,1.f); check(r.ok&&close(caster.hp(),415)&&casts==1&&heals==1,"canonical heal multiplier and exactly-once spell/heal events"); }
 { World w; w.beliefs.restoreActive({Belief::Onslaught}); ActiveEffects fx; Monstrosity monster(-1,{},"feral",true,20); Civilian civilian({}); float before=w.power.value(); strikeMelee(1,EntityType::EldritchAvatar,monster,100,w.beliefs,w.bus,fx,EventType::CivilianSlain); check(close(w.power.value(),before),"monster kills cannot earn civilian Onslaught power"); strikeMelee(1,EntityType::EldritchAvatar,civilian,100,w.beliefs,w.bus,fx,EventType::CivilianSlain); check(close(w.power.value(),before+3.6f),"real civilian kill earns canonical Onslaught power once"); }
 { World w; BeliefSystem defender(w.bus,w.clock); defender.restoreActive({Belief::Magic}); ActiveEffects fx; fx.setVictimBeliefs([&](uint64_t id){return id==1?&defender:nullptr;}); fx.apply(1,CCType::Stun,10,w.beliefs,w.bus); fx.apply(2,CCType::Stun,10,defender,w.bus); fx.tick(6.1); check(!fx.isStunned(1)&&fx.isStunned(2),"Magic resists incoming CC on its owner, not the caster's targets"); }
 { World w; w.beliefs.restoreActive({Belief::Fear,Belief::Torture}); w.ex.addExertion(Belief::Fear,80); w.ex.addExertion(Belief::Torture,80); GameEvent e(EventType::TorturePerformed); e.amount=1; w.bus.publish(e); check(close(w.power.value(),106),"Terror applies original synergy and two-belief income dilution"); }
 { World w; w.beliefs.restoreActive({Belief::Conversion}); w.cult.recruit(); AmbientDirector ad(w.bus,w.rng,w.beliefs,w.cult); ad.setCivilianPicker([]{return uint64_t(0);}); int n=0; w.bus.subscribe(EventType::ConversionPerformed,[&](const GameEvent&){++n;}); for(int i=0;i<100;++i) { ad.forceAction(0,AmbientAction::Preach); ad.forceAction(0,AmbientAction::WhisperCampaign); } check(n==0,"empty bound worlds cannot generate sermon or rumor phantom converts"); }
 { World w; w.beliefs.restoreActive({Belief::Fear}); w.beliefs.setArmyFearSources(2); w.beliefs.setInCombat(true); w.beliefs.tick(1); check(close(w.beliefs.fearLevel(),2),"Fear uses living army sources at original in-combat rate"); w.beliefs.setArmyFearSources(0); w.beliefs.tick(1); check(close(w.beliefs.fearLevel(),2),"lost army sources stop generating fear"); }
 { World w; SacrificeRitual r(w.bus); int n=0; w.bus.subscribe(EventType::SacrificeInterrupted,[&](const GameEvent&){++n;}); r.start(); r.interrupt(false); check(!r.active()&&r.wasInterrupted()&&n==0,"consuming an existing ritual interruption cannot double-award penalties"); }
 { World w; w.cult.recruit(); CommandSystem commands(w.bus,w.rng,w.beliefs,w.cult); commands.setExternalExecution(true); int raids=0,converts=0; w.bus.subscribe(EventType::RaidPerformed,[&](const GameEvent&){++raids;}); w.bus.subscribe(EventType::ConversionPerformed,[&](const GameEvent&){++converts;}); for(int i=0;i<30;++i) { commands.issueCommand(DirectiveType::RaidCity,{}); commands.issueCommand(DirectiveType::ConvertCampaign,{}); } w.cult.update(10000); check(raids==0&&converts==0,"physical directives defer raid/conversion rewards until actual execution"); }
 { World w; w.beliefs.restoreActive({Belief::Conversion}); float receipt=0; w.ex.setPowerObserver([&](const GameEvent&,float delta){receipt=delta;}); GameEvent converted(EventType::ConversionPerformed); converted.amount=1; w.bus.publish(converted); check(close(receipt,4),"contribution receipt reports actual canonical power gain"); w.power.set(999); w.bus.publish(converted); check(close(receipt,1),"contribution receipt respects the actual resource cap"); }
 std::cout<<checks<<" belief integration checks, "<<failures<<" failures\n"; return failures?1:0;
}
