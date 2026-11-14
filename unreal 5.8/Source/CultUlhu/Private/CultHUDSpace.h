#pragma once
#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "Engine/LocalPlayer.h"
#include "Engine/GameViewportClient.h"
#include "SceneView.h"

namespace CultHUDSpace {
inline bool View(APlayerController* PC,FIntRect& Rect)
{
 auto* Local=PC?PC->GetLocalPlayer():nullptr;
 if(!Local||!Local->ViewportClient||!Local->ViewportClient->Viewport) return false;
 FSceneViewProjectionData Projection;
 if(!Local->GetProjectionData(Local->ViewportClient->Viewport,Projection)) return false;
 Rect=Projection.GetConstrainedViewRect(); return Rect.Width()>0&&Rect.Height()>0;
}
inline bool Pointer(APlayerController* PC,FVector2D& Local,bool ClampDrag=false)
{
 float X=0,Y=0; FIntRect Rect;
 if(!PC||!PC->GetMousePosition(X,Y)||!View(PC,Rect)) return false;
 Local=FVector2D(X-Rect.Min.X,Y-Rect.Min.Y);
 if(ClampDrag) { Local.X=FMath::Clamp(Local.X,0.,double(Rect.Width()-1)); Local.Y=FMath::Clamp(Local.Y,0.,double(Rect.Height()-1)); return true; }
 return Local.X>=0&&Local.Y>=0&&Local.X<Rect.Width()&&Local.Y<Rect.Height();
}
}
