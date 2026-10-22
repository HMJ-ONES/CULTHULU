#include "CultSkillAttack.h"
#include "CultUlhuPlayerPawn.h"
#include "CultMatchUnit.h"
#include "CultPopulation.h"
#include "Components/StaticMeshComponent.h"
#include "ProceduralMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/PlayerController.h"

namespace {
// A low-sided tube is visible from every angle and remains depth tested.
struct FSignalLines {
 TArray<FVector> Vertices,Normals; TArray<int32> Indices; TArray<FVector2D> UV;
 TArray<FLinearColor> Colors; TArray<FProcMeshTangent> Tangents;
 void Line(FVector A,FVector B,float Width) {
  const FVector Axis=(B-A).GetSafeNormal(); if(Axis.IsNearlyZero()) return;
  FVector Right=FVector::CrossProduct(Axis,FVector::UpVector).GetSafeNormal();
  if(Right.IsNearlyZero()) Right=FVector::RightVector;
  const FVector Up=FVector::CrossProduct(Axis,Right).GetSafeNormal();
  const int Base=Vertices.Num();
  for(int End=0;End<2;++End) for(int I=0;I<4;++I) {
   const float Angle=I*PI*.5f; const FVector Normal=Right*FMath::Cos(Angle)+Up*FMath::Sin(Angle);
   Vertices.Add((End?B:A)+Normal*Width); Normals.Add(Normal); UV.Add(FVector2D(I/4.f,End)); Colors.Add(FLinearColor::White); Tangents.Add(FProcMeshTangent(Axis,false));
  }
  for(int I=0;I<4;++I) { const int A0=Base+I,A1=Base+(I+1)%4,B0=A0+4,B1=A1+4; Indices.Append({A0,B0,A1,A1,B0,B1}); }
 }
 void Arc(FVector Center,FVector X,FVector Y,float R,float From,float To,float Width,int N=40) {
  FVector Last=Center+(X*FMath::Cos(From)+Y*FMath::Sin(From))*R;
  for(int I=1;I<=N;++I) { const float A=FMath::Lerp(From,To,I/float(N)); const FVector Next=Center+(X*FMath::Cos(A)+Y*FMath::Sin(A))*R; Line(Last,Next,Width); Last=Next; }
 }
 void Apply(UProceduralMeshComponent* Mesh) {
  const auto* Existing=Mesh->GetProcMeshSection(0);
  // Moving trails keep a stable topology. Update their buffer rather than
  // rebuilding the render proxy and collision data every projectile frame.
  if(Existing&&Existing->ProcVertexBuffer.Num()==Vertices.Num()&&Existing->ProcIndexBuffer.Num()==Indices.Num()) Mesh->UpdateMeshSection_LinearColor(0,Vertices,Normals,UV,Colors,Tangents);
  else Mesh->CreateMeshSection_LinearColor(0,Vertices,Indices,Normals,UV,Colors,Tangents,false);
 }
};
}

void ACultSkillAttack::BuildPresentation() {
 auto* Base=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/CultUlhu/Materials/M_AttackSignal"));
 if(!Base) Base=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/CultUlhu/Materials/M_Glow_Eye"));
 SignalMaterial=UMaterialInstanceDynamic::Create(Base,this);
 auto* PC=GetWorld()->GetFirstPlayerController(); auto* Local=PC?Cast<ACultUlhuPlayerPawn>(PC->GetPawn()):nullptr;
 int Team=-1; if(auto* P=Cast<ACultUlhuPlayerPawn>(GetOwner())) Team=P->GetFaction();
 if(auto* U=Cast<ACultMatchUnit>(GetOwner())) Team=U->Team;
 if(auto* U=Cast<ACultPopulation>(GetOwner())) Team=U->GetFaction();
 const bool Friendly=Team>=0&&Team==(Local?Local->GetFaction():0);
 SignalMaterial->SetVectorParameterValue(TEXT("Color"),Friendly?FLinearColor(.08f,.62f,.76f):FLinearColor(1.f,.22f,.07f));
 Visual->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Sphere")));
 Visual->SetMaterial(0,SignalMaterial); Visual->SetWorldScale3D(FVector(Radius/50));
 Visual->SetVisibility(Shape==EShape::Bolt||Shape==EShape::Beam);
 Telegraph=NewObject<UProceduralMeshComponent>(this,TEXT("CommittedAttackSignal"));
 Telegraph->SetupAttachment(RootComponent); Telegraph->SetAbsolute(true,true,true);
 Telegraph->SetCollisionEnabled(ECollisionEnabled::NoCollision); Telegraph->SetCastShadow(false);
 Telegraph->bUseAsyncCooking=false; Telegraph->SetMaterial(0,SignalMaterial); Telegraph->RegisterComponent();
 Telegraph->SetWorldLocation(Origin);
 if(Shape==EShape::Sweep&&Cast<ACultUlhuPlayerPawn>(GetOwner())) {
  for(int I=0;I<2;++I) {
   auto* C=NewObject<UStaticMeshComponent>(this); C->SetupAttachment(RootComponent);
   C->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cylinder")));
   C->SetMaterial(0,LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/CultUlhu/Materials/M_PawnVessel")));
   C->SetCollisionEnabled(ECollisionEnabled::NoCollision); C->SetCastShadow(false); C->RegisterComponent(); Ring.Add(C);
  }
 }
 BuildWarningGeometry(); UpdatePresentation();
}

void ACultSkillAttack::BuildWarningGeometry(bool Trail) {
 if(!Telegraph) return;
 FSignalLines Lines; const float Width=FMath::Clamp(Reach*.004f,8.f,24.f);
 const FVector D=Direction.GetSafeNormal(); FVector Right=FVector::CrossProduct(D,FVector::UpVector).GetSafeNormal();
 if(Right.IsNearlyZero()) Right=FVector::RightVector;
 const FVector Up=FVector::CrossProduct(Right,D).GetSafeNormal();
 if(Shape==EShape::GroundDisk) {
  Lines.Arc(FVector(0,0,12),FVector::ForwardVector,FVector::RightVector,Reach,0,2*PI,Width);
  for(int I=0;I<8;++I) { const float A=I*PI*.25f; const FVector Axis(FMath::Cos(A),FMath::Sin(A),0); Lines.Line(Axis*(Reach-Width*6)+FVector(0,0,12),Axis*(Reach+Width*2)+FVector(0,0,12),Width); }
 } else if(Shape==EShape::Sphere) {
  Lines.Arc(FVector::ZeroVector,FVector::ForwardVector,FVector::RightVector,Reach,0,2*PI,Width);
  Lines.Arc(FVector::ZeroVector,FVector::ForwardVector,FVector::UpVector,Reach,0,2*PI,Width);
  Lines.Arc(FVector::ZeroVector,FVector::RightVector,FVector::UpVector,Reach,0,2*PI,Width);
 } else if(Shape==EShape::Cone) {
  const float A=FMath::DegreesToRadians(FMath::Abs(HalfAngle)); const FVector Center=D*Reach*FMath::Cos(A); const float R=Reach*FMath::Sin(A);
  Lines.Arc(Center,Right,Up,R,0,2*PI,Width);
  for(int I=0;I<4;++I) { const float B=I*PI*.5f; Lines.Line(FVector::ZeroVector,Center+(Right*FMath::Cos(B)+Up*FMath::Sin(B))*R,Width); }
 } else if(Shape==EShape::Beam) {
  for(int I=0;I<16;++I) Lines.Line(D*Reach*(I/16.f),D*Reach*((I+.55f)/16.f),FMath::Max(3.f,Radius*.2f));
  Lines.Arc(D*Reach,Right,Up,Radius*2,0,2*PI,FMath::Max(3.f,Radius*.15f),16);
 } else if(Shape==EShape::Sweep) {
  const float Span=FMath::DegreesToRadians(FMath::Abs(HalfAngle));
  if(!Trail) {
   FVector X=D.GetSafeNormal2D(); if(X.IsNearlyZero()) X=FVector::ForwardVector;
   const FVector Y=FVector::CrossProduct(FVector::UpVector,X);
   // Rotation around world up preserves the committed pitch. A downward
   // swing is a sloped fan, not a flat floor sector with the full 3D radius.
   Lines.Arc(FVector(0,0,D.Z*Reach),X,Y,Reach*D.Size2D(),-Span,Span,Width);
   for(int Sign:{-1,1}) Lines.Line(FVector::ZeroVector,D.RotateAngleAxis(Sign*FMath::Abs(HalfAngle),FVector::UpVector)*Reach,Width*.65f);
  } else {
   const float T=FMath::Clamp((Age-Windup)/FMath::Max(.001f,Duration),0.f,1.f);
   FVector Last=D.RotateAngleAxis(FMath::Lerp(-HalfAngle,HalfAngle,FMath::Max(0.f,T-.24f)),FVector::UpVector)*Reach;
   for(int I=1;I<=16;++I) { const float Sample=FMath::Lerp(FMath::Max(0.f,T-.24f),T,I/16.f); const FVector Tip=D.RotateAngleAxis(FMath::Lerp(-HalfAngle,HalfAngle,Sample),FVector::UpVector)*Reach; Lines.Line(Last,Tip,FMath::Max(4.f,Radius*.25f)*I/16.f); Last=Tip; }
  }
 } else if(Shape==EShape::Bolt) {
  if(Trail) {
   const FVector Head=Previous-Origin;
   Lines.Line(Head-D*FMath::Min(Travel,Speed*.065f),Head,FMath::Max(3.f,Radius*.28f));
  } else {
   // Charge rings identify the muzzle; they do not promise a guaranteed impact.
   Lines.Arc(FVector::ZeroVector,Right,Up,Radius*2.5f,0,2*PI,FMath::Max(3.f,Radius*.18f),20);
  }
 }
 Lines.Apply(Telegraph);
}

void ACultSkillAttack::UpdatePresentation() {
 if(!Telegraph||!SignalMaterial) return;
 const bool Charging=Age<Windup; const float Progress=WindupProgress();
 SignalMaterial->SetScalarParameterValue(TEXT("Intensity"),Charging?.35f+.85f*Progress:1.7f);
 Telegraph->SetWorldLocation(Origin);
 if(Shape==EShape::Bolt) {
  Visual->SetWorldScale3D(FVector(Radius/50*(Charging?.45f+.55f*Progress:1.f)));
  if(!Charging) BuildWarningGeometry(true);
 } else if(Shape==EShape::Beam) {
  Visual->SetVisibility(!Charging);
  if(!Charging) ShowSegment(Origin,Origin+Direction*Reach,Radius);
  Telegraph->SetVisibility(Charging);
 } else if(Shape==EShape::Sweep) {
  if(!Charging) BuildWarningGeometry(true);
  const float T=Charging?0.f:FMath::Clamp((Age-Windup)/FMath::Max(.001f,Duration),0.f,1.f);
  const FVector D=Direction.RotateAngleAxis(FMath::Lerp(-HalfAngle,HalfAngle,T),FVector::UpVector),Tip=Origin+D*Reach*(Charging?.8f:1.f);
  if(Ring.Num()==2) {
   const FVector Elbow=Origin+D*Reach*.48f+FVector(0,0,Radius*2);
   for(int I=0;I<2;++I) { const FVector A=I?Elbow:Origin,B=I?Tip:Elbow; auto* C=Ring[I]; C->SetWorldLocation((A+B)*.5f); C->SetWorldRotation(FRotationMatrix::MakeFromZ(B-A).Rotator()); const float R=Radius*(I?.55f:.7f); C->SetWorldScale3D(FVector(R/50,R/50,FVector::Dist(A,B)/100)); }
  }
 }
}
