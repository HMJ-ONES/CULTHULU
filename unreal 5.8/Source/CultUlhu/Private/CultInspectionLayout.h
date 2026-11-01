#pragma once
#include "CoreMinimal.h"

// Bounded labels follow visible bodies while leaving HUD/menu space clear.
inline bool CultPlaceInspection(FVector2D Anchor,FVector2D Size,const FBox2D& Safe,const TArray<FBox2D>& Occupied,float Gap,FBox2D& Out)
{
 if(Size.X>Safe.GetSize().X||Size.Y>Safe.GetSize().Y) return false;
 auto Place=[&](FVector2D Desired) {
  const FVector2D Min(FMath::Clamp(Desired.X,Safe.Min.X,Safe.Max.X-Size.X),FMath::Clamp(Desired.Y,Safe.Min.Y,Safe.Max.Y-Size.Y));
  const FBox2D Box(Min,Min+Size),Padded(Min-FVector2D(Gap*.5f),Min+Size+FVector2D(Gap*.5f));
  if(Box.Max.Y>Anchor.Y-Gap+.01f) return false; // Keep the character's body visible.
  if(FVector2D::Distance(Box.GetCenter(),Anchor)>Size.X*1.65f) return false;
  for(const auto& Used:Occupied) if(Padded.Intersect(Used)) return false;
  Out=Box; return true;
 };
 for(int Row=0;Row<4;++Row) for(int Side:{0,1,-1}) {
  const float X=Anchor.X-Size.X*.5f+Side*(Size.X+Gap);
  const float Y=Anchor.Y-Size.Y-Gap-Row*(Size.Y+Gap);
  if(Place(FVector2D(X,Y))) return true;
 }
 // A detailed selected card is wider than its neighbors. Try actual occupied
 // edges instead of assuming every card can be packed on one fixed-width grid.
 for(const auto& Used:Occupied) {
  const float Y=Anchor.Y-Size.Y-Gap,X=Anchor.X-Size.X*.5f;
  for(const auto Desired:{FVector2D(Used.Min.X-Gap-Size.X,Y),FVector2D(Used.Max.X+Gap,Y),FVector2D(X,Used.Min.Y-Gap-Size.Y),FVector2D(X,Used.Max.Y+Gap)}) if(Place(Desired)) return true;
 }
 return false;
}
