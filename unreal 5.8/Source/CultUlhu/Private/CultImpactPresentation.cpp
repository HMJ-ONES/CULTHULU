#include "CultBlastFX.h"
#include "ProceduralMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"

namespace {
struct FImpactLines {
 TArray<FVector> V,N; TArray<int32> T; TArray<FVector2D> UV; TArray<FLinearColor> Colors;
 void Shard(FVector A,FVector B,float Width) {
  const auto Axis=(B-A).GetSafeNormal(); if(Axis.IsNearlyZero()) return;
  FVector X,Y; Axis.FindBestAxisVectors(X,Y); const FVector Center=FMath::Lerp(A,B,.55f);
  // A pointed, four-sided streak leaves the area between rays completely open.
  for(int I=0;I<4;++I) {
   const auto P=Center+(X*FMath::Cos(I*PI*.5f)+Y*FMath::Sin(I*PI*.5f))*Width;
   const auto Q=Center+(X*FMath::Cos((I+1)*PI*.5f)+Y*FMath::Sin((I+1)*PI*.5f))*Width;
   auto Triangle=[&](FVector First,FVector Second,FVector Third) {
    const int Base=V.Num(); const auto Normal=-FVector::CrossProduct(Second-First,Third-First).GetSafeNormal();
    for(auto Point:{First,Second,Third}) { V.Add(Point); N.Add(Normal); UV.Add(FVector2D::ZeroVector); Colors.Add(FLinearColor::White); }
    T.Append({Base,Base+1,Base+2});
   };
   Triangle(A,Q,P); Triangle(B,P,Q);
  }
 }
 void Apply(UProceduralMeshComponent* Mesh) {
  const auto* Old=Mesh->GetProcMeshSection(0);
  if(Old&&Old->ProcVertexBuffer.Num()==V.Num()) Mesh->UpdateMeshSection_LinearColor(0,V,N,UV,Colors,TArray<FProcMeshTangent>());
  else Mesh->CreateMeshSection_LinearColor(0,V,T,N,UV,Colors,TArray<FProcMeshTangent>(),false);
 }
};
}
void ACultBlastFX::BuildSignal() {
 Signal_=NewObject<UProceduralMeshComponent>(this,TEXT("ImpactSignal")); Signal_->SetupAttachment(RootComponent);
 Signal_->SetCollisionEnabled(ECollisionEnabled::NoCollision); Signal_->SetCastShadow(false); Signal_->bUseAsyncCooking=false;
 AddInstanceComponent(Signal_); Signal_->RegisterComponent();
 auto* Base=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/CultUlhu/Materials/M_AttackSignal"));
 if(!Base) { Destroy(); return; }
 SignalMaterial_=UMaterialInstanceDynamic::Create(Base,this); Signal_->SetMaterial(0,SignalMaterial_);
 SignalMaterial_->SetVectorParameterValue(TEXT("Color"),Kind_==ECultBlastKind::HitFlash?FLinearColor(.95f,.79f,.48f):FLinearColor(.36f,.48f,.40f));
 SignalDuration_=Kind_==ECultBlastKind::HitFlash?.18f:.55f;
 UpdateSignal();
}
void ACultBlastFX::UpdateSignal() {
 if(!Signal_||!SignalMaterial_) return;
 const float T=FMath::Clamp(SignalAge_/SignalDuration_,0.f,1.f);
 const float Fade=FMath::Square(1-T); FImpactLines Lines;
 if(Kind_==ECultBlastKind::HitFlash) {
  const float R=RadiusM_*100; FVector X,Y; ImpactDirection_.FindBestAxisVectors(X,Y);
  // Cosmetic sparks eject toward the incoming side of the contact. Authored
  // armor can overhang its gameplay capsule; keep this relief bounded, depth
  // tested and independent of the recorded hit position or damage volume.
  const FVector Burst=-ImpactDirection_*FMath::Min(R*.75f,45.f);
  for(int I=0;I<6;++I) {
   const float Angle=I*2*PI/6+.24f;
   const auto D=(X*FMath::Cos(Angle)+Y*FMath::Sin(Angle)-ImpactDirection_*(I%2?.28f:.12f)).GetSafeNormal();
   const float Extent=R*(I%2?.72f:1.f);
   Lines.Shard(Burst+D*Extent*(.10f+.55f*T),Burst+D*Extent*(.65f+.35f*T),FMath::Max(.6f,R*.055f)*Fade);
  }
  SignalMaterial_->SetScalarParameterValue(TEXT("Intensity"),2.3f*Fade);
 } else {
  // A narrow expanding rim and eight separated rune strokes replace twelve
  // opaque puffs. This is impact feedback, never a new damage/warning volume.
  const float R=FMath::Max(2.f,RadiusM_)*100;
  const float Distance=R*FMath::Lerp(.28f,1.f,1-FMath::Square(1-T));
  const float Width=FMath::Clamp(R*.008f,2.f,14.f)*FMath::Max(.08f,Fade);
  for(int I=0;I<64;++I) {
   const float A=I*2*PI/64,B=(I+1)*2*PI/64;
   Lines.Shard(FVector(FMath::Cos(A),FMath::Sin(A),0)*Distance,FVector(FMath::Cos(B),FMath::Sin(B),0)*Distance,Width);
  }
  for(int I=0;I<8;++I) {
   const float A=I*2*PI/8; const FVector D(FMath::Cos(A),FMath::Sin(A),0);
   Lines.Shard(D*Distance*.90f,D*Distance*1.06f+FVector(0,0,R*.025f*Fade),Width*1.2f);
  }
  SignalMaterial_->SetScalarParameterValue(TEXT("Intensity"),.9f*Fade);
 }
 Lines.Apply(Signal_);
}
