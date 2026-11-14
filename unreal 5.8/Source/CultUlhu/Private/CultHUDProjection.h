#pragma once
#include "Engine/Canvas.h"
namespace CultHUDProjection {
// Canvas owns the view actually being rendered, including constrained views
// and dedicated screenshot targets. The controller can describe a different
// native viewport while such a target is being rendered.
inline bool Project(UCanvas* Canvas,FVector World,FVector2D& Screen) {
 const FVector Point=Canvas->Project(World);
 if(Point.Z<=0) return false;
 Screen=FVector2D(Point.X,Point.Y); return true;
}
}
