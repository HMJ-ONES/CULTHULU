#pragma once
#include "CoreMinimal.h"
#include "CultLivingLandscape.h"

namespace CultRegionMap {
constexpr int Count=6;
constexpr float Span=ACultLivingLandscape::Extent*2/Count;
inline int Index(FVector P) {
 if(!FMath::IsFinite(P.X)||!FMath::IsFinite(P.Y)||FMath::Abs(P.X)>=ACultLivingLandscape::Extent||FMath::Abs(P.Y)>=ACultLivingLandscape::Extent) return INDEX_NONE;
 return FMath::FloorToInt((P.Y+ACultLivingLandscape::Extent)/Span)*Count+FMath::FloorToInt((P.X+ACultLivingLandscape::Extent)/Span);
}
inline FVector Center(int Region) { return FVector(-ACultLivingLandscape::Extent+(Region%Count+.5f)*Span,-ACultLivingLandscape::Extent+(Region/Count+.5f)*Span,0); }
inline FString Name(int Region) { return Region<0?TEXT("Uncharted"):FString::Printf(TEXT("%c%d"),TCHAR('A'+Region%Count),Region/Count+1); }
inline FVector2D Project(FVector P,const FBox2D& Box) { return Box.Min+FVector2D((P.X+ACultLivingLandscape::Extent)/(2*ACultLivingLandscape::Extent),(ACultLivingLandscape::Extent-P.Y)/(2*ACultLivingLandscape::Extent))*Box.GetSize(); }
inline FVector Unproject(FVector2D P,const FBox2D& Box) { const auto UV=(P-Box.Min)/Box.GetSize(); return FVector((UV.X*2-1)*ACultLivingLandscape::Extent,(1-UV.Y*2)*ACultLivingLandscape::Extent,0); }
}
