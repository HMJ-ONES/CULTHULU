#pragma once

#include "CultAbilityFX.h"
#include "ProceduralMeshComponent.h"

namespace CultAbilityFX {
enum class EForm : uint8 { Curl, Rune, Crack, Arc, Bloom, Spiral, Lance, Shell };
struct FPalette {
 FLinearColor Body,Tip;
 EForm Form;
};

const FPalette& Palette(ELook Look);
UMaterialInterface* FilamentMaterial();

struct FFilaments {
 TArray<FVector> Vertices;
 TArray<int32> Indices;
 TArray<FVector> Normals;
 TArray<FVector2D> UVs;
 TArray<FLinearColor> Colors;
 TArray<FProcMeshTangent> Tangents;

 void Ribbon(FVector A,FVector B,float WidthA,float WidthB,FLinearColor Start,FLinearColor End) {
  const FVector Axis=(B-A).GetSafeNormal();
  if(Axis.IsNearlyZero()) return;
  FVector Right=FVector::CrossProduct(Axis,FVector::UpVector).GetSafeNormal();
  if(Right.IsNearlyZero()) Right=FVector::RightVector;
  const FVector Up=FVector::CrossProduct(Axis,Right).GetSafeNormal();
  for(const FVector Side:{Right,Up}) {
   const int32 First=Vertices.Num();
   Vertices.Append({A-Side*WidthA,A+Side*WidthA,B-Side*WidthB,B+Side*WidthB});
   UVs.Append({{0,0},{1,0},{0,1},{1,1}});
   Colors.Append({Start,Start,End,End});
   Indices.Append({First,First+2,First+1,First+1,First+2,First+3});
  }
 }

 void Arc(FVector Center,FVector X,FVector Y,float Radius,float From,float To,
          float Width,FLinearColor Start,FLinearColor End,int32 Segments) {
  auto Point=[&](float T) {
   const float Angle=FMath::Lerp(From,To,T);
   return Center+(X*FMath::Cos(Angle)+Y*FMath::Sin(Angle))*Radius;
  };
  FVector A=Point(0);
  for(int32 Index=1;Index<=Segments;++Index) {
   const float T=float(Index)/Segments;
   const FVector B=Point(T);
   Ribbon(A,B,Width,Width,FMath::Lerp(Start,End,T-1.f/Segments),FMath::Lerp(Start,End,T));
   A=B;
  }
 }

 void Apply(UProceduralMeshComponent* Mesh) {
  if(Vertices.IsEmpty()) { Mesh->SetVisibility(false); return; }
  Mesh->SetVisibility(true);
  const auto* Existing=Mesh->GetProcMeshSection(0);
  if(Existing&&Existing->ProcVertexBuffer.Num()==Vertices.Num()&&Existing->ProcIndexBuffer.Num()==Indices.Num())
   Mesh->UpdateMeshSection_LinearColor(0,Vertices,Normals,UVs,Colors,Tangents,false);
  else
   Mesh->CreateMeshSection_LinearColor(0,Vertices,Indices,Normals,UVs,Colors,Tangents,false,false);
 }
};

inline FLinearColor Alpha(FLinearColor Color,float Opacity) { Color.A=FMath::Clamp(Opacity,0.f,1.f); return Color; }
}
