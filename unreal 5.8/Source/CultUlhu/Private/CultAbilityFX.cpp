#include "CultAbilityFX.h"

#include "CultAbilityFilaments.h"
#include "CultUlhuPlayerPawn.h"
#include "Components/CapsuleComponent.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInstanceDynamic.h"

namespace CultAbilityFX {
namespace {
using L=ELook;
struct FKitLooks { const TCHAR* Id; ELook Look[4]; };
constexpr FKitLooks Looks[]={
 {TEXT("bokrug"),          {L::Tide,L::Stone,L::Dread,L::Tide}},
 {TEXT("cthulhu_avatar"),  {L::Abyss,L::Tendrils,L::Dread,L::Brood}},
 {TEXT("dagon"),           {L::Abyss,L::Tendrils,L::Tide,L::Brood}},
 {TEXT("ghatanothoa"),     {L::Ember,L::Stasis,L::Obsidian,L::Ember}},
 {TEXT("hastur"),          {L::Dread,L::Sigil,L::Dread,L::Sigil}},
 {TEXT("mi_go"),           {L::Spore,L::Nerve,L::Stasis,L::Spore}},
 {TEXT("mother_hydra"),    {L::Tide,L::Brood,L::Mend,L::Tide}},
 {TEXT("nodens"),          {L::Abyss,L::Blade,L::Tide,L::Gate}},
 {TEXT("nug_and_yeb"),     {L::Ember,L::Venom,L::Chitin,L::Ember}},
 {TEXT("nyarlathotep"),    {L::Blade,L::Tendrils,L::Sigil,L::Dread}},
 {TEXT("rhan_tegoth"),     {L::Stone,L::Chitin,L::Chitin,L::Dread}},
 {TEXT("sghllor"),         {L::Prism,L::Prism,L::Spore,L::Prism}},
 {TEXT("shoggoths"),       {L::Tendrils,L::Stone,L::Mend,L::Dread}},
 {TEXT("shub_niggurath"),  {L::Spore,L::Brood,L::Spore,L::Brood}},
 {TEXT("yig"),             {L::Venom,L::Blade,L::Mend,L::Sigil}},
 {TEXT("yog_sothoth"),     {L::Gate,L::Gate,L::Gate,L::Abyss}},
};

struct FBurst {
 FFilaments Mesh;
 FLinearColor Body,Tip;
 float Radius,Time,Opacity;
 int32 Count;
 FVector Facing,Right;
 const FVector Up=FVector::UpVector;

 FVector Radial(float Angle) const { return Facing*FMath::Cos(Angle)+Right*FMath::Sin(Angle); }

 void Sparks(float Spread,float Lift,int32 Number) {
  for(int32 Index=0;Index<Number;++Index) {
   const float Angle=Index*2.3999632f+.22f;
   const float Gain=.54f+.46f*FMath::Frac(Index*.618f);
   const FVector D=Radial(Angle);
   const FVector Point=D*(Radius*Spread*Gain)+Up*(Radius*Lift*Gain*FMath::Sin(Time*PI));
   const FVector Tail=Point-D*(Radius*.19f*Gain)-Up*(Radius*.09f);
   Mesh.Ribbon(Tail,Point,Radius*.015f,Radius*.007f,Alpha(Body,0),Alpha(Tip,Opacity*.88f));
  }
 }

 void Curl(bool Tide) {
  const int32 Arms=Tide?3:Count;
  for(int32 Index=0;Index<Arms;++Index) {
   const float Heading=Tide?(Index-1)*.53f:2*PI*Index/Arms;
   const FVector Along=Radial(Heading),Across=Radial(Heading+PI*.5f);
   const float Stride=Tide?1.f:.76f+.27f*FMath::Frac(Index*.618f+.16f);
   const float Twist=Index%2?1.f:-1.f;
   FVector A=Along*Radius*.08f;
   for(int32 Segment=1;Segment<=12;++Segment) {
    const float T=Segment/12.f;
    const float Curl=FMath::Sin(T*5.4f+Time*2.1f)*T*T;
    const FVector B=Along*(Radius*Stride*FMath::Sin(T*1.94f))
     +Across*(Radius*(Tide?.21f:.43f)*Curl*Twist)
     +Up*(Radius*(Tide?.30f:.77f)*Stride*FMath::Sin(T*PI*.86f));
    const float Bulk=Radius*FMath::Lerp(.088f,.008f,T);
    Mesh.Ribbon(A,B,Bulk*1.17f,Bulk,Alpha(Body,Opacity*(1-T*.60f)),
     Alpha(FMath::Lerp(Body,Tip,T*.76f),Opacity*(.92f-T*.52f)));
    A=B;
   }
  }
 }

 void Rune() {
  const float Spin=Time*.77f;
  for(int32 Index=0;Index<8;++Index) {
   const float Angle=Spin+Index*PI*.25f;
   Mesh.Arc(Up*6,Facing,Right,Radius,Angle,Angle+.56f,Radius*.014f,
    Alpha(Body,Opacity*.65f),Alpha(Tip,Opacity),4);
   const FVector D=Radial(Angle+.28f);
   const FVector R=Radial(Angle+.28f+PI*.5f);
   const FVector Root=D*(Radius*.69f)+Up*8;
   const FVector Point=D*(Radius*.89f)+Up*8;
   Mesh.Ribbon(Root+R*Radius*.08f,Point,Radius*.014f,Radius*.008f,Alpha(Body,Opacity*.7f),Alpha(Tip,Opacity));
   Mesh.Ribbon(Point,Root-R*Radius*.08f,Radius*.008f,Radius*.012f,Alpha(Tip,Opacity),Alpha(Body,Opacity*.56f));
  }
 }

 void Crack() {
  for(int32 Index=0;Index<Count;++Index) {
   const float Heading=Index*2*PI/Count+.26f*FMath::Sin(Index*2.3f);
   const FVector Along=Radial(Heading),Across=Radial(Heading+PI*.5f);
   const FVector A=Along*(Radius*.13f)+Up*8;
   const FVector B=Along*(Radius*.54f)+Across*(Radius*.12f)+Up*10;
   const FVector C=Along*(Radius*.92f)+Up*12;
   Mesh.Ribbon(A,B,Radius*.033f,Radius*.024f,Alpha(Tip,Opacity),Alpha(Body,Opacity));
   Mesh.Ribbon(B,C,Radius*.025f,Radius*.004f,Alpha(Body,Opacity),Alpha(Body,0));
   const float Heat=.32f+.40f*FMath::Frac(Index*.618f);
   const FVector Top=B+Up*(Radius*Heat*(.65f+.58f*FMath::Sin(Time*PI)))+Across*Radius*.10f;
   Mesh.Ribbon(B,Top,Radius*.16f,Radius*.012f,Alpha(Body,Opacity*.22f),Alpha(Body,0));
   Mesh.Ribbon(B,Top,Radius*.063f,Radius*.003f,Alpha(Tip,Opacity*.95f),Alpha(Body,0));
  }
 }

 void Bloom() {
  for(int32 Index=0;Index<6;++Index) {
   const FVector Along=Radial(Index*PI/3+Time*.8f);
   const FVector Across=FVector::CrossProduct(Up,Along);
   for(int32 Side:{-1,1}) {
    FVector A=Along*Radius*.13f;
    for(int32 Segment=1;Segment<=7;++Segment) {
     const float T=Segment/7.f;
     const float Open=FMath::Sin(T*PI);
     const FVector B=Radius*(Along*(.13f+.72f*Open)+Up*(.78f*T)+Across*(Side*.14f*Open));
     const float Petal=Radius*(.014f+.05f*Open);
     Mesh.Ribbon(A,B,Petal,Petal*.87f,
      Alpha(Body,Opacity*.62f),Alpha(Tip,Opacity*(.82f-.34f*T)));
     A=B;
    }
   }
  }
  Sparks(.92f,.83f,Count+3);
 }

 void Spiral() {
  constexpr float Pitch=.63f;
  for(int32 Band=0;Band<3;++Band) {
   const float Angle=Band*2*PI/3-Time*2.3f;
   const FVector Tilt=(Right+Up*(Band==1?-.77f:.60f)).GetSafeNormal();
   const FVector Base=Up*Radius*Pitch*(.27f+.33f*Band);
   Mesh.Arc(Base,Facing,Tilt,Radius*(.69f+.10f*Band),Angle,Angle+3.9f,Radius*.043f,
    Alpha(Body,Opacity*.29f),Alpha(Tip,Opacity*.86f),20);
  }
  const FVector Center=Up*(Radius*.65f);
  Mesh.Arc(Center,Right*.76f,Up,Radius*.72f,Time,Time+2*PI,Radius*.067f,
   Alpha(Body,Opacity*.58f),Alpha(Tip,Opacity*.21f),24);
  Sparks(.92f,.42f,Count+3);
 }

 void Shell() {
  for(int32 Index=0;Index<6;++Index) {
   const float Angle=Index*PI/3;
   const FVector Left=Radial(Angle-.46f),RightEdge=Radial(Angle+.46f),Top=Radial(Angle);
   const FVector A=Radius*(Left*.88f+Up*.14f);
   const FVector B=Radius*(Left*.94f+Up*.54f);
   const FVector C=Radius*(Top*.71f+Up*.84f);
   const FVector D=Radius*(RightEdge*.94f+Up*.54f);
   const FVector E=Radius*(RightEdge*.88f+Up*.14f);
   Mesh.Ribbon(A,B,Radius*.015f,Radius*.024f,Alpha(Body,Opacity*.19f),Alpha(Tip,Opacity*.80f));
   Mesh.Ribbon(B,C,Radius*.024f,Radius*.017f,Alpha(Tip,Opacity*.80f),Alpha(Body,Opacity*.67f));
   Mesh.Ribbon(C,D,Radius*.017f,Radius*.024f,Alpha(Body,Opacity*.67f),Alpha(Tip,Opacity*.80f));
   Mesh.Ribbon(D,E,Radius*.024f,Radius*.015f,Alpha(Tip,Opacity*.80f),Alpha(Body,Opacity*.19f));
  }
 }

 void ArcFan(bool Lance) {
  for(int32 Index=0;Index<Count;++Index) {
   const float Side=Index%2?-1.f:1.f;
   const float Spread=Index/float(Count);
   const FVector End=Facing*Radius*(.73f+Spread*.24f)
    +Right*(Side*Radius*Spread*(Lance?.18f:.68f))+Up*Radius*(.10f+Spread*.32f);
   const FVector Bend=End*.52+Right*Side*Radius*.065f+Up*Radius*.07f;
   Mesh.Ribbon(FVector::ZeroVector,Bend,Radius*.021f,Radius*.014f,Alpha(Body,Opacity*.42f),Alpha(Tip,Opacity*.92f));
   Mesh.Ribbon(Bend,End,Radius*.014f,Radius*.002f,Alpha(Tip,Opacity*.92f),Alpha(Body,0));
  }
 }
};
}

ELook ForSlot(const FString& Id,int32 Slot) {
 if(Slot<-1||Slot>2) return ELook::None;
 for(const auto& Kit:Looks) if(Id==Kit.Id) return Kit.Look[Slot+1];
 return ELook::None;
}

const FPalette& Palette(ELook Look) {
 using F=EForm;
 static const FPalette Colors[]={
  {{.22f,.7f,.8f},{.67f,.95f,1},F::Arc},          // none/fallback
  {{.045f,.63f,.54f},{.58f,1,.76f},F::Spiral},   // abyss
  {{.03f,.61f,.29f},{.49f,1,.67f},F::Curl},      // tendrils
  {{.37f,.07f,.82f},{.94f,.36f,.83f},F::Rune},   // dread
  {{.04f,.49f,.98f},{.64f,1,1},F::Curl},         // tide
  {{1,.12f,.015f},{1,.79f,.24f},F::Crack},       // ember
  {{.52f,.12f,.79f},{1,.51f,.92f},F::Crack},     // obsidian
  {{1,.49f,.034f},{1,.93f,.41f},F::Rune},        // sigil
  {{.54f,.93f,.06f},{.91f,1,.57f},F::Lance},     // nerve
  {{.14f,.55f,1},{.72f,.88f,1},F::Rune},        // stasis
  {{.37f,.94f,.038f},{.92f,1,.34f},F::Bloom},    // spore
  {{.10f,.83f,.48f},{1,.98f,.57f},F::Bloom},     // mend
  {{.14f,.76f,.99f},{.78f,.99f,1},F::Lance},     // blade
  {{.28f,.89f,.095f},{.80f,1,.26f},F::Arc},      // venom
  {{.57f,.19f,1},{.46f,1,1},F::Arc},            // prism
  {{.31f,.26f,1},{.91f,.49f,1},F::Spiral},      // gate
  {{.78f,.065f,.29f},{1,.38f,.54f},F::Bloom},    // brood
  {{.92f,.38f,.061f},{1,.82f,.32f},F::Shell},    // chitin
  {{.58f,.37f,.15f},{1,.80f,.43f},F::Crack},     // stone
 };
 static_assert(UE_ARRAY_COUNT(Colors)==int(ELook::Count));
 const int32 Index=int32(Look);
 return Colors[Index>=0&&Index<UE_ARRAY_COUNT(Colors)?Index:0];
}

UMaterialInterface* FilamentMaterial() {
 static TWeakObjectPtr<UMaterialInterface> Cached;
 if(Cached.IsValid()) return Cached.Get();
 auto* Asset=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/CultUlhu/Materials/M_AbilityFilament"));
 if(!Asset) Asset=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/CultUlhu/Materials/M_AttackSignal"));
 Cached=Asset;
 return Asset;
}

void CastCue(ACultUlhuPlayerPawn* Pawn,const FString& Id,uint8 Move,float Windup,float Age) {
 using M=CultAttackMotion::EMove;
 ELook Look=ELook::None;
 const auto Motion=M(Move);
 if(Motion==M::Heavy) Look=ForSlot(Id,-1);
 else if(Motion>=M::Q&&Motion<=M::R) Look=ForSlot(Id,int(Motion)-int(M::Q));
 else if(Motion==M::RockLift||Motion==M::RockThrow) Look=ELook::Abyss;
 else if(Motion==M::Wave||Motion==M::WaveHold||Motion==M::WaveThrow) Look=ELook::Dread;
 else if(Motion==M::MinionLift||Motion==M::MinionThrow) Look=ELook::Tendrils;
 if(Look==ELook::None) return;
 const float Radius=FMath::Clamp(Pawn->GetSimpleCollisionRadius()*.72f,85.f,440.f);
 ACultAbilityFX::Spawn(Pawn->GetWorld(),Look,EEvent::Release,Pawn->GetActorLocation(),
  Pawn->AimRotation().Vector(),Radius,Pawn,Windup,Age,CultAttackMotion::IsSurge(Id,Motion));
}
}

ACultAbilityFX::ACultAbilityFX() {
 PrimaryActorTick.bCanEverTick=true;
 PrimaryActorTick.TickInterval=.033f;
 Filaments=CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("AbilityFilaments"));
 RootComponent=Filaments;
 Filaments->SetCollisionEnabled(ECollisionEnabled::NoCollision);
 Filaments->SetGenerateOverlapEvents(false);
 Filaments->SetCanEverAffectNavigation(false);
 Filaments->SetCastShadow(false);
}

ACultAbilityFX* ACultAbilityFX::Spawn(UWorld* World,CultAbilityFX::ELook Style,CultAbilityFX::EEvent Stage,
 FVector Point,FVector Direction,float Size,ACultUlhuPlayerPawn* Anchor,float Charge,float InitialAge,bool TrailCaster) {
 using namespace CultAbilityFX;
 if(!World||World->GetNetMode()==NM_DedicatedServer||Style==ELook::None||Style>=ELook::Count||Stage>=EEvent::Count||
    Point.ContainsNaN()||Direction.ContainsNaN()||!FMath::IsFinite(Size)) return nullptr;
 if(auto* PC=World->GetFirstPlayerController()) {
  FVector Eye; FRotator View;
  PC->GetPlayerViewPoint(Eye,View);
  const float CullDistance=FMath::Max(15500.f,Size*13.f);
  if(FVector::DistSquared(Eye,Point)>FMath::Square(CullDistance)) return nullptr;
 }
 int32 Existing=0;
 for(TActorIterator<ACultAbilityFX> It(World);It;++It) if(++Existing>=24) return nullptr;

 auto* Base=FilamentMaterial();
 if(!Base) return nullptr;
 auto* FX=World->SpawnActor<ACultAbilityFX>(Point,FRotator::ZeroRotator);
 if(!FX) return nullptr;
 FX->Look=Style;
 FX->Event=Stage;
 FX->Scale=FMath::Clamp(Size,35.f,2600.f);
 FX->Axis=Direction.GetSafeNormal2D();
 if(FX->Axis.IsNearlyZero()) FX->Axis=FVector::ForwardVector;
 FX->Follow=Anchor;
 FX->bFollowsCaster=Anchor!=nullptr;
 FX->bTrailCaster=TrailCaster;
 FX->Windup=FMath::Clamp(Charge,0.f,2.f);
 FX->Age=FMath::Clamp(InitialAge,0.f,1.f);
 FX->Life=FX->Windup+(Anchor?.38f:Stage==EEvent::Arrival?.88f:Stage==EEvent::Recovery?.76f:.68f);
 FX->Material=UMaterialInstanceDynamic::Create(Base,FX);
 FX->Filaments->SetMaterial(0,FX->Material);
 FX->SetLifeSpan(FMath::Max(.03f,FX->Life-FX->Age+.06f));
 FX->Draw();
 return FX;
}

void ACultAbilityFX::Tick(float DeltaSeconds) {
 Super::Tick(DeltaSeconds);
 Age+=DeltaSeconds;
 if(Age>=Life) { Destroy(); return; }
 Draw();
}

void ACultAbilityFX::Draw() {
 using namespace CultAbilityFX;
 const bool Charging=Age<Windup;
 if(bFollowsCaster) {
  auto* Caster=Follow.Get();
  if(!Caster||Caster->MatchDead()||(Charging&&Caster->IsIncapacitated())) { Destroy(); return; }
  const FVector Chest=Caster->GetActorLocation()+FVector(0,0,Caster->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()*.29f);
  SetActorLocation(Chest+Axis*Caster->GetSimpleCollisionRadius()*.69f);
 }
 const auto& Colors=Palette(Look);
 const float Phase=Charging?FMath::Clamp(Age/FMath::Max(.01f,Windup),0.f,1.f):
  FMath::Clamp((Age-Windup)/FMath::Max(.01f,Life-Windup),0.f,1.f);
 const float Ease=1-FMath::Pow(1-Phase,2.5f);
 const float Opacity=Charging?FMath::Lerp(.38f,.91f,Phase):FMath::Pow(1-Phase,1.48f);
 const float Radius=Scale*(Charging?FMath::Lerp(.95f,.38f,Phase):FMath::Lerp(.28f,1.f,Ease));
 const FVector Right=FVector::CrossProduct(FVector::UpVector,Axis);
 FBurst Burst{{},Colors.Body,Colors.Tip,Radius,Phase,Opacity,bFollowsCaster?5:8,Axis,Right};
 if(Charging) {
  // The caster gets a small focusing seal. The physical warning is still
  // drawn at the actual target, so the seal cannot be mistaken for its area.
  const FVector Up=FVector::UpVector;
  Burst.Mesh.Arc(FVector::ZeroVector,Right,Up,Radius,Phase*2,Phase*2+5.4f,Radius*.026f,
   Alpha(Colors.Body,Opacity*.52f),Alpha(Colors.Tip,Opacity),19);
  Burst.Mesh.Arc(FVector::ZeroVector,Right,(Up+Axis*.5).GetSafeNormal(),Radius*.67f,-Phase*3,3.9f-Phase*3,Radius*.019f,
   Alpha(Colors.Tip,Opacity*.7f),Alpha(Colors.Body,Opacity*.35f),13);
 } else {
  switch(Colors.Form) {
   case EForm::Curl: Burst.Curl(Look==ELook::Tide); break;
   case EForm::Rune: Burst.Rune(); break;
   case EForm::Crack: Burst.Crack(); break;
   case EForm::Bloom: Burst.Bloom(); break;
   case EForm::Spiral: Burst.Spiral(); break;
   case EForm::Arc: Burst.ArcFan(false); break;
   case EForm::Lance: Burst.ArcFan(true); break;
   case EForm::Shell: Burst.Shell(); break;
  }
  if(Colors.Form!=EForm::Bloom&&Colors.Form!=EForm::Spiral) Burst.Sparks(.93f,.58f,Burst.Count);
  if(!bFollowsCaster&&Event==EEvent::Contact) {
   const FVector Lift=FVector::UpVector*Radius*.07;
   Burst.Mesh.Arc(Lift,Axis,Right,Radius*.91f,0,2*PI,Radius*.028f,
    Alpha(Colors.Tip,Opacity*.73f),Alpha(Colors.Body,Opacity*.32f),24);
  }
  if(bTrailCaster&&bFollowsCaster) {
   const FVector Now=GetActorLocation();
   if(MotionWake.IsEmpty()||!MotionWake.Last().Equals(Now,5.f)) MotionWake.Add(Now);
   if(MotionWake.Num()>6) MotionWake.RemoveAt(0,MotionWake.Num()-6,EAllowShrinking::No);
   for(int32 Index=1;Index<MotionWake.Num();++Index) {
    const float Weight=Index/float(MotionWake.Num()-1);
    for(int32 Side:{-1,1}) {
     const FVector Offset=Right*Side*Scale*.18;
     Burst.Mesh.Ribbon(MotionWake[Index-1]-Now+Offset,MotionWake[Index]-Now+Offset,Scale*.085f,Scale*.039f,
      Alpha(Colors.Body,Opacity*Weight*.5f),Alpha(Colors.Tip,Opacity*Weight*.86f));
    }
   }
  }
 }
 Burst.Mesh.Apply(Filaments);
 if(Material) {
  Material->SetVectorParameterValue(TEXT("Color"),Colors.Body);
  Material->SetScalarParameterValue(TEXT("Intensity"),Charging?2.2f:2.6f*Opacity);
 }
}
