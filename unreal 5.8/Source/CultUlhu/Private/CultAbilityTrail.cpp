#include "CultSkillAttack.h"

#include "CultAbilityFilaments.h"
#include "CultUlhuPlayerPawn.h"
#include "Materials/MaterialInstanceDynamic.h"

void ACultSkillAttack::EmitAbilityFX(uint8 Event,FVector Point,FVector Axis,float Size) {
 if(!AbilityLook||!HasAuthority()) return;
 if(auto* Pawn=Cast<ACultUlhuPlayerPawn>(GetOwner())) Pawn->EmitAbilityBurst(AbilityLook,Event,Point,Axis,Size);
}

void ACultSkillAttack::UpdateAbilityFilaments() {
 using namespace CultAbilityFX;
 if(!AbilityLook||GetNetMode()==NM_DedicatedServer) return;
 if(Shape!=EShape::Bolt&&Shape!=EShape::Sweep&&Shape!=EShape::Beam) return;
 if(IsWindingUp()&&!bRockGuard) {
  if(AbilityFilaments) AbilityFilaments->SetVisibility(false);
  FilamentPath.Reset();
  return;
 }
 const float Now=GetWorld()->GetTimeSeconds();
 if(Now-LastFilamentUpdate<.028f) return;
 LastFilamentUpdate=Now;
 const auto Look=ELook(AbilityLook);
 const auto& Color=Palette(Look);

 if(!AbilityFilaments) {
  auto* Base=FilamentMaterial();
  if(!Base) return;
  AbilityFilaments=NewObject<UProceduralMeshComponent>(this);
  AbilityFilaments->SetupAttachment(GetRootComponent());
  AbilityFilaments->SetAbsolute(true,true,true);
  AbilityFilaments->SetCollisionEnabled(ECollisionEnabled::NoCollision);
  AbilityFilaments->SetGenerateOverlapEvents(false);
  AbilityFilaments->SetCanEverAffectNavigation(false);
  AbilityFilaments->SetCastShadow(false);
  auto* Instance=UMaterialInstanceDynamic::Create(Base,AbilityFilaments);
  Instance->SetVectorParameterValue(TEXT("Color"),Color.Body);
  Instance->SetScalarParameterValue(TEXT("Intensity"),2.2f);
  AbilityFilaments->SetMaterial(0,Instance);
  AbilityFilaments->RegisterComponent();
 }

 FFilaments Lines;
 const FVector Up=FVector::UpVector;
 if(bRockGuard) {
  const float Scale=FMath::Clamp(float(RockExtents.GetMax()),90.f,680.f);
  const FVector Front=GetActorForwardVector(),Side=GetActorRightVector();
  const float Spin=Age*3.4f;
  AbilityFilaments->SetWorldLocation(GetActorLocation());
  Lines.Arc(FVector::ZeroVector,Side,(Up+Front*.45).GetSafeNormal(),Scale*1.04f,
   Spin,Spin+4.2f,Scale*.024f,Alpha(Color.Body,.15f),Alpha(Color.Tip,.86f),20);
  Lines.Arc(FVector::ZeroVector,Side,(Up-Front*.45).GetSafeNormal(),Scale*.83f,
   -Spin,-Spin+3.8f,Scale*.016f,Alpha(Color.Tip,.68f),Alpha(Color.Body,.06f),16);
 } else if(Shape==EShape::Sweep) {
  AbilityFilaments->SetWorldLocation(Origin);
  const float Progress=FMath::Clamp((Age-Windup)/FMath::Max(.01f,Duration),0.f,1.f);
  const float First=FMath::Max(0.f,Progress-.41f);
  for(int32 Strand=0;Strand<2;++Strand) {
   const FVector Offset=Up*(Strand?Radius*.54:0);
   const float Distance=Reach*(Strand?.89f:1.f);
   auto Point=[&](float T) {
    return Offset+Direction.RotateAngleAxis(FMath::Lerp(-HalfAngle,HalfAngle,T),Up)*Distance;
   };
   FVector A=Point(First);
   for(int32 Segment=1;Segment<=12;++Segment) {
    const float Weight=Segment/12.f;
    const FVector B=Point(FMath::Lerp(First,Progress,Weight));
    Lines.Ribbon(A,B,Radius*(.06f+.36f*Weight),Radius*(.08f+.36f*Weight),
     Alpha(Color.Body,Weight*.48f),Alpha(Color.Tip,Weight*.90f));
    A=B;
   }
  }
 } else if(Shape==EShape::Beam) {
  AbilityFilaments->SetWorldLocation(Origin);
  const FVector End=Previous.Equals(Origin)?Direction*Reach:Previous-Origin;
  const FVector Along=End.GetSafeNormal();
  const FVector Side=FVector::CrossProduct(Along,Up).GetSafeNormal();
  const FVector Cross=FVector::CrossProduct(Side,Along).GetSafeNormal();
  for(int32 Strand=0;Strand<3;++Strand) {
   FVector A=FVector::ZeroVector;
   for(int32 Step=1;Step<=14;++Step) {
    const float T=Step/14.f;
    const float Angle=Age*20.f+Strand*2*PI/3+T*11.f;
    const FVector Offset=(Side*FMath::Cos(Angle)+Cross*FMath::Sin(Angle))*Radius*1.9f*FMath::Sin(T*PI);
    const FVector B=End*T+Offset;
    Lines.Ribbon(A,B,Radius*.20f,Radius*.12f,Alpha(Color.Tip,.89f),Alpha(Color.Body,.68f));
    A=B;
   }
  }
 } else {
  const FVector Head=HasAuthority()?Previous:GetActorLocation();
  AbilityFilaments->SetWorldLocation(Head);
  if(FilamentPath.IsEmpty()||FVector::DistSquared(FilamentPath.Last(),Head)>FMath::Square(900.f)) FilamentPath.Reset();
  if(FilamentPath.IsEmpty()||!FilamentPath.Last().Equals(Head,2.f)) FilamentPath.Add(Head);
  if(FilamentPath.Num()>9) FilamentPath.RemoveAt(0,FilamentPath.Num()-9,EAllowShrinking::No);
  const FVector Along=FilamentPath.Num()>1?(Head-FilamentPath[FilamentPath.Num()-2]).GetSafeNormal():Direction;
  FVector Side=FVector::CrossProduct(Along,Up).GetSafeNormal();
  if(Side.IsNearlyZero()) Side=FVector::RightVector;
  const FVector Cross=FVector::CrossProduct(Side,Along);
  const float Scale=FMath::Clamp(Radius*.69f,13.f,145.f);
  const bool Straight=Color.Form==EForm::Lance;
  const int32 Strands=Look==ELook::Prism?3:2;
  for(int32 Strand=0;Strand<Strands;++Strand) {
   FVector A=FilamentPath[0]-Head;
   for(int32 Index=1;Index<FilamentPath.Num();++Index) {
    const float Weight=Index/float(FilamentPath.Num()-1);
    const float Angle=Age*12.f+Strand*2*PI/Strands-Weight*5.9f;
    const FVector Helix=(Side*FMath::Cos(Angle)+Cross*FMath::Sin(Angle))*Scale*(Straight?.32f:1.f)*FMath::Sin(Weight*PI);
    const FVector B=FilamentPath[Index]-Head+Helix;
    const auto Tint=Strand==0?Color.Body:Strand==1?Color.Tip:FLinearColor(.31f,1,.65f);
    Lines.Ribbon(A,B,Scale*(.06f+.22f*Weight),Scale*(.10f+.26f*Weight),
     Alpha(Tint,Weight*.42f),Alpha(Tint,Weight*.85f));
    A=B;
   }
  }
 }
 Lines.Apply(AbilityFilaments);
}
