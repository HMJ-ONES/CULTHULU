#include "CultSkillAttack.h"

#include "CultAbilityFilaments.h"
#include "CultUlhuPlayerPawn.h"
#include "Camera/CameraComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Components/StaticMeshComponent.h"

void ACultSkillAttack::UpdateFirstPersonSwing() {
 const auto* Pawn=Cast<ACultUlhuPlayerPawn>(GetOwner());
 const bool Show=Pawn&&Pawn->IsLocallyControlled()&&!Pawn->IsThirdPerson()&&!Pawn->IsIncapacitated();
 if(!Show) { if(FirstPersonSwing) FirstPersonSwing->SetVisibility(false); return; }
 auto* Camera=Pawn->FindComponentByClass<UCameraComponent>();
 if(!Camera) return;
 using namespace CultAbilityFX;
 if(!FirstPersonSwing) {
  auto* Material=FilamentMaterial();
  if(!Material) return;
  FirstPersonSwing=NewObject<UProceduralMeshComponent>(this);
  FirstPersonSwing->SetupAttachment(Camera);
  FirstPersonSwing->SetAbsolute(false,false,true);
  FirstPersonSwing->SetCollisionEnabled(ECollisionEnabled::NoCollision);
  FirstPersonSwing->SetCastShadow(false);
  FirstPersonSwing->ComponentTags.Add(TEXT("CultLocalMeleeView"));
  FirstPersonSwing->SetFirstPersonPrimitiveType(EFirstPersonPrimitiveType::FirstPerson);
  FirstPersonSwing->SetMaterial(0,Material);
  auto* BodyMaterial=LoadObject<UMaterialInterface>(nullptr,TEXT("/Engine/BasicShapes/BasicShapeMaterial"));
  if(auto* Solid=UMaterialInstanceDynamic::Create(BodyMaterial,FirstPersonSwing)) {
   const bool Yig=Pawn->GetCharacterId()==TEXT("yig");
   Solid->SetVectorParameterValue(TEXT("Color"),Yig?FLinearColor(.12f,.21f,.10f):FLinearColor(.17f,.25f,.28f));
   FirstPersonSwing->SetMaterial(1,Solid);
  }
  FirstPersonSwing->RegisterComponent();
 }
 FirstPersonSwing->SetVisibility(true);
 FFilaments Lines;
 TArray<FVector> Vertices,Normals;
 TArray<int32> Indices;
 auto Limb=[&](FVector Start,FVector End,float Wide,float Narrow) {
  const FVector Axis=(End-Start).GetSafeNormal();
  const FVector Right=FVector::CrossProduct(Axis,FVector::UpVector).GetSafeNormal();
  const FVector Up=FVector::CrossProduct(Right,Axis);
  constexpr int Sides=8;
  const int32 Base=Vertices.Num();
  for(int I=0;I<Sides;++I) {
   const float Angle=I*2.f*PI/Sides;
   const FVector Normal=Right*FMath::Cos(Angle)+Up*FMath::Sin(Angle);
   Vertices.Append({Start+Normal*Wide,End+Normal*Narrow});
   Normals.Append({Normal,Normal});
   const int A=Base+2*I,B=Base+2*((I+1)%Sides);
   Indices.Append({A,A+1,B,B,A+1,B+1});
  }
 };
 const auto& Motion=Pawn->AttackPlayback();
 const float Scale=Pawn->GetActorScale3D().Z;
 const float Contact=Motion.Contact;
 const float AgeAtPose=Pawn->GetWorld()->GetTimeSeconds()-Motion.Began;
 const float RightSign=(Motion.Variant&3)==2?1.f:-1.f;
 const bool Yig=Pawn->GetCharacterId()==TEXT("yig");
 const int Count=(Motion.Variant&3)==3?2:1;
 for(int Arm=0;Arm<Count;++Arm) {
  const float Side=Arm==0?RightSign:-RightSign;
  auto HandAt=[&](float T) {
   const float Sweep=FMath::SmoothStep(Contact-.155f,Contact+.145f,T);
   const float Lift=FMath::SmoothStep(0.f,Contact-.11f,T)*(1-FMath::SmoothStep(Contact+.065f,Motion.Duration,T));
   return FVector(53+24*FMath::Sin(Sweep*PI),Side*FMath::Lerp(59.f,-47.f,Sweep),-33+23*Lift-Arm*9)*Scale;
  };
  const FVector Hand=HandAt(AgeAtPose);
  const FVector Elbow=(FVector(29,Side*43,-42-Arm*9)*Scale+Hand)*.5;
  const FVector Start=FVector(15,Side*24,-50-Arm*9)*Scale;
  Limb(Start,Elbow,12*Scale,10*Scale);
  Limb(Elbow,Hand,10*Scale,6.5f*Scale);
  const FVector Grip=Hand+(Hand-Elbow).GetSafeNormal()*7*Scale;
  if(Yig) {
   const FVector Point=Grip+FVector(12,Side*27,29)*Scale;
   Limb(Hand,Grip,7*Scale,4*Scale);
   Lines.Ribbon(Grip,Point,4.8f*Scale,1.4f*Scale,FLinearColor(.55f,.74f,.71f,.94f),FLinearColor(.81f,.95f,.94f,1));
  } else for(int Claw=-1;Claw<=1;++Claw) {
   const FVector Base=Grip+FVector(0,Claw*4,0)*Scale;
   Limb(Base,Base+FVector(10,Claw*2,-3)*Scale,2.6f*Scale,.75f*Scale);
  }
  if(Age>=Windup) for(int I=1;I<=9;++I) {
   const float Tail=float(I)/9.f;
   const FVector A=HandAt(AgeAtPose-.17f+.17f*(I-1)/9.f)+FVector(6,0,8)*Scale;
   const FVector B=HandAt(AgeAtPose-.17f+.17f*Tail)+FVector(6,0,8)*Scale;
   Lines.Ribbon(A,B,Scale*(1+Tail*3),Scale*(1+Tail*4),
    FLinearColor(.36f,.65f,1.f,.3f*Tail),FLinearColor(.83f,.94f,1.f,.85f*Tail));
  }
 }
 Lines.Apply(FirstPersonSwing);
 FirstPersonSwing->SetVisibility(true);
 if(const auto* Section=FirstPersonSwing->GetProcMeshSection(1); Section&&Section->ProcVertexBuffer.Num()==Vertices.Num())
  FirstPersonSwing->UpdateMeshSection_LinearColor(1,Vertices,Normals,{},{},{},false);
 else
  FirstPersonSwing->CreateMeshSection_LinearColor(1,Vertices,Indices,Normals,{},{},{},false,false);
}

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
  const float Visibility=FlightVisibility();
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
     Alpha(Tint,Weight*.42f*Visibility),Alpha(Tint,Weight*.85f*Visibility));
    A=B;
   }
  }
 }
 Lines.Apply(AbilityFilaments);
}
