#include "CultAttackMotion.h"

namespace CultAttackMotion {
namespace {
enum class EForm : uint8 {
 Cut, Hammer, Pounce, Bite, Spit, Dart, Cannon, Ray, Gaze, Lash, Pull,
 Ward, Mend, Invoke, Surge, Lift, Toss, Lower
};
using F = EForm;

struct FDesign {
 EForm Form;
 float Strength;
 float Lead;
};

struct FCharacterMoves {
 const TCHAR* Id;
 FDesign Moves[5]; // Primary, RMB, Q, F, R
};

// Bias the lead, momentum and intensity for the move's actual body plan.
// The original kit still owns timing, projectiles, damage and control.
constexpr FCharacterMoves Roster[] = {
 {TEXT("bokrug"),         {{F::Spit,1.04f,.15f},{F::Bite,1.1f,-.4f},{F::Hammer,1.25f,0},{F::Gaze,.8f,.2f},{F::Invoke,1.42f,-.15f}}},
 {TEXT("cthulhu_avatar"), {{F::Cut,1,1},{F::Gaze,1,.7f},{F::Hammer,1.23f,.15f},{F::Ward,1.15f,0},{F::Invoke,1.45f,0}}},
 {TEXT("dagon"),          {{F::Cut,1,-1},{F::Pounce,1.15f,.7f},{F::Bite,1.25f,.05f},{F::Surge,1,-.45f},{F::Invoke,1.25f,-.5f}}},
 {TEXT("ghatanothoa"),    {{F::Cannon,1.3f,0},{F::Hammer,1,.8f},{F::Gaze,.85f,0},{F::Dart,1.1f,-.65f},{F::Ward,1.5f,.15f}}},
 {TEXT("hastur"),         {{F::Gaze,.92f,-.45f},{F::Lash,.72f,.8f},{F::Ray,1.1f,-.2f},{F::Ward,.9f,.4f},{F::Invoke,1.35f,-.35f}}},
 {TEXT("mi_go"),          {{F::Dart,1,.65f},{F::Spit,1.19f,-.2f},{F::Dart,.82f,-.85f},{F::Gaze,1.06f,.65f},{F::Pull,1.21f,-.45f}}},
 {TEXT("mother_hydra"),   {{F::Spit,1,-.2f},{F::Spit,1.32f,.7f},{F::Invoke,.83f,.55f},{F::Mend,.9f,-.15f},{F::Ward,1.33f,0}}},
 {TEXT("nodens"),         {{F::Dart,1.1f,.8f},{F::Pull,1,-.8f},{F::Dart,1.42f,-.55f},{F::Surge,.9f,.4f},{F::Invoke,1.26f,.25f}}},
 {TEXT("nug_and_yeb"),    {{F::Cannon,1.14f,.45f},{F::Cut,1.17f,-.65f},{F::Spit,.83f,.75f},{F::Ward,.96f,-.45f},{F::Hammer,1.42f,0}}},
 {TEXT("nyarlathotep"),   {{F::Cut,.97f,.9f},{F::Pounce,1.12f,-.8f},{F::Lash,1.18f,.7f},{F::Gaze,.73f,-.55f},{F::Ward,1.2f,-.35f}}},
 {TEXT("rhan_tegoth"),    {{F::Cut,1.17f,-1},{F::Hammer,1.36f,.55f},{F::Bite,1.28f,-.6f},{F::Ward,1.05f,0},{F::Invoke,1.23f,.6f}}},
 {TEXT("sghllor"),        {{F::Dart,.88f,.05f},{F::Ray,1.12f,-.3f},{F::Ray,.85f,.55f},{F::Gaze,.82f,-.6f},{F::Hammer,1.29f,.15f}}},
 {TEXT("shoggoths"),      {{F::Cut,1.04f,.85f},{F::Pull,1.22f,-.55f},{F::Hammer,1.22f,0},{F::Surge,1.12f,-.25f},{F::Ward,1.33f,.45f}}},
 {TEXT("shub_niggurath"), {{F::Spit,1.13f,.6f},{F::Cannon,.88f,-.55f},{F::Invoke,1,-.55f},{F::Lash,.88f,.65f},{F::Invoke,1.43f,.18f}}},
 {TEXT("yig"),            {{F::Cut,1,-.8f},{F::Bite,1.09f,.55f},{F::Lash,1.12f,-.7f},{F::Surge,.87f,.8f},{F::Gaze,1.35f,.45f}}},
 {TEXT("yog_sothoth"),    {{F::Cannon,1.16f,.05f},{F::Pull,.97f,.35f},{F::Lash,1.07f,-.55f},{F::Surge,.88f,.35f},{F::Pull,1.38f,-.2f}}},
};

FDesign Design(const FString& Id,const FPlayback& Playback) {
 switch(Playback.Move) {
  case EMove::Melee: {
   float Weight=Id==TEXT("rhan_tegoth")?1.15f:.34f; // Articulated melee clips already carry their full stroke.
   const int Stage=Playback.Variant&3;
   return {Stage==3?F::Lash:Stage==2?F::Hammer:F::Cut,Weight,(Playback.Variant&8)?-1.f:1.f};
  }
  case EMove::RockLift: return {F::Lift,1.2f,.04f};
  case EMove::MinionLift: return {F::Lift,.73f,.87f};
  case EMove::WaveHold: return {F::Lift,.64f,-.73f};
  case EMove::RockThrow: return {F::Toss,1.25f,.23f};
  case EMove::MinionThrow: return {F::Toss,.89f,.9f};
  case EMove::WaveThrow: return {F::Lash,1.06f,-.82f};
  case EMove::Wave: return {F::Gaze,1.06f,-.6f};
  case EMove::Lower: return {F::Lower,.77f,.1f};
  default: break;
 }
 const int Slot=int(Playback.Move)-int(EMove::Primary);
 if(Slot>=0&&Slot<5) for(const auto& Character:Roster) if(Id==Character.Id) return Character.Moves[Slot];
 return {F::Gaze,.65f,.3f};
}

struct FBeats {
 FFrame Gather;
 FFrame Contact;
 FFrame Follow;
 float Carry; // Time before the follow-through reaches its apex.
};

struct FChannels {
 double X,Y,Z,W;
 FVector4 Vector() const { return FVector4(X,Y,Z,W); }
};

FFrame Pose(FChannels Body,FChannels Limbs,FChannels Details) {
 return {Body.Vector(),Limbs.Vector(),Details.Vector(),0};
}

// Each form has a prepared silhouette, a release pose and a separate settle
// pose. Local axes keep the feet grounded and preserve supplied hand grips.
FBeats Beats(EForm Form) {
 switch(Form) {
  case F::Cut:
   return {
    Pose({-8,-27,-5,-2},{-31,22,-32,6},{5,14,-7,.03}),
    Pose({14,31,10,-2},{-35,-13,15,13},{-9,20,18,.05}),
    Pose({5,42,7,1},{-10,-25,23,5},{5,8,24,.01}),
    .08f
   };
  case F::Hammer:
   return {
    Pose({-17,-8,-4,-2},{-57,18,-37,5},{-9,22,6,.04}),
    Pose({28,6,6,-11},{13,-6,20,18},{14,25,-10,.14}),
    Pose({12,3,3,-6},{18,6,25,6},{-6,6,12,.04}),
    .13f
   };
  case F::Pounce:
   return {
    Pose({8,-18,-11,-9},{-31,12,-22,12},{-6,-12,3,-.03}),
    Pose({20,12,15,-4},{-47,-8,14,17},{-9,13,11,.02}),
    Pose({-6,8,4,1},{-8,5,18,5},{6,8,16,0}),
    .08f
   };
  case F::Bite:
   return {
    Pose({-9,-11,-8,-4},{-12,10,-20,28},{-19,10,0,.04}),
    Pose({17,10,13,-6},{-21,-10,8,5},{18,12,4,.07}),
    Pose({4,4,5,1},{-5,4,15,10},{-6,3,13,.02}),
    .09f
   };
  case F::Spit:
   return {
    Pose({-12,-8,-6,-4},{-14,16,-32,19},{-13,9,-5,.17}),
    Pose({13,6,8,-3},{-8,8,-13,34},{12,16,8,-.1}),
    Pose({-7,2,-3,2},{-2,9,8,8},{-9,8,12,.03}),
    .11f
   };
  case F::Dart:
   return {
    Pose({-7,-12,-4,-3},{-19,10,-31,9},{-6,13,-3,-.08}),
    Pose({8,9,4,1},{-26,-6,9,20},{7,17,9,.05}),
    Pose({-9,4,-5,0},{-6,8,12,4},{-7,8,13,.02}),
    .065f
   };
  case F::Cannon:
   return {
    Pose({-18,-12,-12,-9},{-28,27,-37,17},{-15,24,-11,.23}),
    Pose({10,10,11,-6},{-37,17,12,28},{10,36,17,.28}),
    Pose({-19,5,-12,-5},{-12,29,21,9},{-11,26,23,-.07}),
    .15f
   };
  case F::Ray:
   return {
    Pose({-7,-10,-5,-1},{-19,16,-22,12},{-9,16,-3,-.13}),
    Pose({7,8,9,2},{-33,-8,-2,25},{7,31,13,.07}),
    Pose({4,10,7,1},{-23,-6,7,13},{3,27,17,.02}),
    .12f
   };
  case F::Gaze:
   return {
    Pose({-8,-13,-3,0},{-12,13,-24,6},{-16,14,-5,-.07}),
    Pose({5,5,2,4},{-33,5,-9,10},{12,20,9,.16}),
    Pose({-3,8,0,2},{-16,15,8,4},{-4,10,14,.05}),
    .12f
   };
  case F::Lash:
   return {
    Pose({-5,-32,-5,-2},{-34,26,-25,8},{-7,21,-9,.07}),
    Pose({9,30,7,0},{-44,-14,17,13},{8,24,24,.12}),
    Pose({-2,36,2,0},{-15,-27,18,4},{-4,10,26,.02}),
    .105f
   };
  case F::Pull:
   return {
    Pose({8,-6,6,0},{-51,23,-15,10},{6,14,-4,.08}),
    Pose({-17,13,-9,-4},{-24,-19,-45,21},{-10,27,11,-.13}),
    Pose({-8,6,-5,0},{-12,5,-23,7},{-4,13,15,.02}),
    .135f
   };
  case F::Ward:
   return {
    Pose({9,-15,-4,-7},{-33,-14,-40,8},{9,-15,-4,-.15}),
    Pose({-12,8,0,1},{-44,36,-13,19},{-9,33,16,.24}),
    Pose({-4,5,0,-3},{-20,22,5,6},{4,24,14,.09}),
    .14f
   };
  case F::Mend:
   return {
    Pose({7,-5,-1,-3},{-24,-14,-35,3},{10,3,-3,-.06}),
    Pose({-3,8,4,4},{-40,12,-18,6},{-3,12,7,.12}),
    Pose({3,4,2,1},{-21,8,-13,3},{4,8,8,.06}),
    .16f
   };
  case F::Invoke:
   return {
    Pose({12,-16,-5,-7},{-22,-18,-42,12},{12,-18,-8,-.12}),
    Pose({-19,9,4,5},{-61,32,-13,24},{-17,33,18,.23}),
    Pose({-8,14,1,3},{-43,24,8,15},{-8,21,21,.12}),
    .17f
   };
  case F::Surge:
   return {
    Pose({12,-9,-7,-7},{-16,-14,-29,5},{-8,-13,0,-.11}),
    Pose({22,9,10,-4},{-26,-11,2,10},{-9,-15,12,-.16}),
    Pose({7,-4,3,0},{-12,7,8,3},{4,13,15,.04}),
    .09f
   };
  case F::Lift:
   return {
    Pose({10,-6,-3,-7},{-18,8,-24,8},{12,-9,-4,-.09}),
    Pose({-12,5,-2,-4},{-51,14,-30,5},{-5,13,4,.07}),
    Pose({-10,2,-2,-3},{-48,12,-33,4},{-5,13,4,.05}),
    .08f
   };
  case F::Toss:
   return {
    Pose({-12,-25,-8,-5},{-51,19,-43,8},{-8,-8,-9,.03}),
    Pose({24,22,10,-1},{-63,8,24,18},{11,27,18,.14}),
    Pose({9,32,5,1},{-19,20,18,7},{-5,18,25,.05}),
    .12f
   };
  case F::Lower:
   return {
    Pose({-7,-4,0,-2},{-32,11,-23,2},{-4,8,-2,.02}),
    Pose({8,6,2,-4},{5,4,12,3},{10,1,5,0}),
    Pose({2,2,0,0},{1,2,5,0},{2,4,5,0}),
    .1f
   };
 }
 return {};
}

float Ease(float T) { T=FMath::Clamp(T,0.f,1.f); return T*T*(3-2*T); }

FFrame EvaluateBeats(const FBeats& Motion,float Age,float Contact,float Duration,bool Held) {
 const float Strike=FMath::Max(.036f,Contact);
 const float Carry=FMath::Min(Motion.Carry,FMath::Max(.03f,(Duration-Strike)*.48f));
 const float Gather=Strike*.68f;
 if(Age<Gather) return Blend({},Motion.Gather,Ease(Age/Gather));
 if(Age<Strike) return Blend(Motion.Gather,Motion.Contact,Ease((Age-Gather)/(Strike-Gather)));
 if(Age<Strike+Carry) return Blend(Motion.Contact,Motion.Follow,Ease((Age-Strike)/Carry));
 if(Held) return Motion.Follow;
 return Blend(Motion.Follow,{},Ease((Age-Strike-Carry)/FMath::Max(.03f,Duration-Strike-Carry)));
}
}

FFrame Blend(const FFrame& A,const FFrame& B,float Alpha) {
 return {FMath::Lerp(A.Body,B.Body,Alpha),FMath::Lerp(A.Limbs,B.Limbs,Alpha),
         FMath::Lerp(A.Details,B.Details,Alpha),FMath::Lerp(A.Lead,B.Lead,Alpha)};
}

EMove AbilitySlot(int32 Slot) {
 return Slot>=0&&Slot<=2?EMove(int(EMove::Q)+Slot):EMove::Heavy;
}

bool IsSurge(const FString& Id,EMove Move) {
 FPlayback Playback;
 Playback.Move=Move;
 return Design(Id,Playback).Form==F::Surge;
}

FFrame Sample(const FString& Id,const FPlayback& Playback,float Now) {
 if(Playback.Move==EMove::None) return {};
 const float Age=FMath::Max(0.f,Now-Playback.Began);
 const bool Held=IsHold(Playback.Move);
 if(!Held&&Age>=Playback.Duration) return {};

 const FDesign Style=Design(Id,Playback);
 const FBeats Motion=Beats(Style.Form);
 auto Frame=EvaluateBeats(Motion,Age,Playback.Contact,Playback.Duration,Held);
 // Head, feelers and wing tips settle a fraction behind the body impulse.
 const auto Secondary=EvaluateBeats(Motion,FMath::Max(0.f,Age-.035f),Playback.Contact,Playback.Duration,Held);
 Frame.Details=Secondary.Details*(Held?1.f:Ease((Playback.Duration-Age)/.045f));
 const bool Ability=Playback.Move>=EMove::Heavy&&Playback.Move<=EMove::R;
 if(Ability&&Age>Playback.Contact) {
  // The arms and face absorb the release impulse after the effect leaves.
  const float After=Age-Playback.Contact;
  const float Recovery=FMath::Clamp(After/.19f,0.f,1.f);
  const float Rebound=FMath::Sin(PI*Recovery)*(1.f-Recovery);
  const bool Loaded=Style.Form==F::Cannon||Style.Form==F::Hammer||Style.Form==F::Pull;
  Frame.Body.X-=Rebound*(Loaded?11.f:3.5f);
  Frame.Limbs.Z+=Rebound*(Loaded?15.f:6.f);
  Frame.Details.Y+=Rebound*(Style.Form==F::Invoke||Style.Form==F::Ward?11.f:6.f);
 }
 if(Held&&Age>Playback.Contact) {
  const float Breath=FMath::Sin(Age*3.1f)*Ease((Age-Playback.Contact)/.28f);
  Frame.Body.X+=Breath*1.1f;
  Frame.Limbs.Z+=Breath*2.1f;
  Frame.Details.Y+=Breath*2.8f;
 }
 Frame.Body*=Style.Strength;
 Frame.Limbs*=Style.Strength;
 Frame.Details*=Style.Strength;
 Frame.Body.Y*=Style.Lead;
 Frame.Details.Z*=Style.Lead;
 Frame.Lead=Style.Lead;
 return Blend(Playback.Previous,Frame,Ease(Age/.045f));
}
}
