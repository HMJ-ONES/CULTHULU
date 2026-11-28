#include "CultCharacterModel.h"
#include "Components/MeshComponent.h"

// Sghllor is the original light-and-mist entity. Its basic motions deform that
// existing form; no humanoid skeleton, imported creature or ability animation.
void ACultCharacterModel::UpdateOriginalOrb() {
 if(Parts_.Num()!=3) return;
 const FString& R=CurrentRole_;
 const bool Attack=R.StartsWith(TEXT("melee_"))||R==TEXT("attack");
 const float Duration=R==ActionRole_&&ActionSeconds_>.01f?ActionSeconds_:R==TEXT("taunt")?2.6f:R==TEXT("dash")?.22f:R==TEXT("run")?.7f:1.2f;
 const bool Loop=R==TEXT("idle")||R==TEXT("walk")||R==TEXT("run")||R==TEXT("preview");
 const float Age=FMath::Max(0.f,Elapsed_-RoleBegan_);
 const float Progress=Loop?FMath::Fmod(Age/Duration,1.f):FMath::Clamp(Age/Duration,0.f,1.f);
 const float T=Attack&&R==ActionRole_?AuthoredMeleePhase(Progress):Progress;
 const float Pulse=FMath::Square(FMath::Sin(PI*T)),Wave=FMath::Sin(2*PI*T);
 FVector Core(0,0,90+Hover_),CoreScale(.7f),MistScale(1.8f,1.8f,1);
 float Yaw=0,Roll=0;FVector Mist=Core;
 if(R==TEXT("idle")||R==TEXT("preview")) CoreScale*=1+.045f*FMath::Sin(Elapsed_*1.9f);
 else if(R==TEXT("walk")||R==TEXT("run")) {
  const float Strength=R==TEXT("run")?1.6f:1;
  Core.Y+=Wave*2*Strength;CoreScale=FVector(.7f+.05f*Strength,.7f,.7f-.04f*Strength);
  Mist.X-=8*Strength;MistScale.X+=.15f*Strength;Yaw=Wave*7;
 } else if(Attack) {
  const int Kind=R.EndsWith(TEXT("2"))?2:R.EndsWith(TEXT("3"))?3:1;
  Core.X+=12*Pulse;CoreScale*=1-.18f*Pulse;
  Mist.X+=24*Pulse;MistScale.X+=.55f*Pulse;
  if(Kind==1) { Yaw=70*Wave;Roll=18*Pulse; }
  else if(Kind==2) { Mist.Z+=28*Pulse;Roll=-55*Wave;MistScale.Y+=.35f*Pulse; }
  else { Yaw=-80*Wave;Roll=40*Wave;MistScale+=FVector(.35f,.45f,0)*Pulse; }
 } else if(R==TEXT("dash")) {
  CoreScale=FVector(.7f+.32f*Pulse,.7f-.20f*Pulse,.7f-.12f*Pulse);
  Mist.X-=35*Pulse;MistScale.X+=.9f*Pulse;Yaw=Wave*12;
 } else if(R==TEXT("jump")||R==TEXT("land")) {
  const bool Land=R==TEXT("land");CoreScale=FVector(.7f+(Land?.10f:-.08f)*Pulse,.7f,.7f+(Land?-.10f:.14f)*Pulse);
  Mist.Z+=(Land?-8:12)*Pulse;MistScale.Y+=.2f*Pulse;
 } else if(R==TEXT("taunt")) {
  CoreScale*=1+.35f*Pulse;MistScale+=FVector(.65f,.65f,0)*Pulse;
  Yaw=Wave*32;Roll=Wave*12;
 }
 Parts_[0]->SetRelativeLocation(Core);Parts_[0]->SetRelativeScale3D(CoreScale);
 for(int I=1;I<3;++I) {
  Parts_[I]->SetRelativeLocation(Mist+FVector(0,(I==1?1:-1)*Wave*3,0));
  Parts_[I]->SetRelativeScale3D(MistScale);
  Parts_[I]->SetRelativeRotation(FRotator(90,Yaw+(I==1?0:90),Roll*(I==1?1:-1)));
 }
}
