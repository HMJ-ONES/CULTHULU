#include "CultPlayHUD.h"
#include "CultCanvasLabel.h"
#include "CultHUDSpace.h"
#include "CultUlhuPlayerPawn.h"
#include "CultWorldDirector.h"
#include "CultPopulation.h"
#include "GameFramework/PlayerController.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"

void ACultPlayHUD::DrawSelection()
{
 auto* PC=GetOwningPlayerController(); auto* P=Cast<ACultUlhuPlayerPawn>(GetOwningPawn()); auto* D=ACultWorldDirector::Find(GetWorld());
 if(!PC||!P||!D||!PC->bShowMouseCursor) return;
 const float S=FMath::Clamp(Canvas->SizeY/900.f,.6f,2.f);
 DrawRect(FLinearColor(.02f,.05f,.09f,.93f),20*S,75*S,Canvas->SizeX-40*S,66*S);
 CultCanvasLabel(Canvas,TEXT("ALT + drag: select followers | click friendly: nearby group | click enemy / ground: orders"),FLinearColor::White,32*S,80*S,16.f*S);
 CultCanvasLabel(Canvas,D->CommandSummary(),FLinearColor(.2f,.85f,1),32*S,108*S,16.f*S);
 for(TActorIterator<ACultPopulation> It(GetWorld());It;++It) if(D->IsSelected(*It)) {
  FVector2D V; if(PC->ProjectWorldLocationToScreen(It->GetActorLocation()+FVector(0,0,120),V,true)&&V.X>=6*S&&V.X<=Canvas->SizeX-6*S&&V.Y>=3*S&&V.Y<=Canvas->SizeY-3*S) {
   DrawRect(FLinearColor(.1f,.9f,1,.9f),V.X-6*S,V.Y-3*S,12*S,6*S);
  }
 }
 FVector2D Mouse; const bool HasMouse=CultHUDSpace::Pointer(PC,Mouse,true);
 if(bSelecting_&&HasMouse) {
  const float X=Mouse.X,Y=Mouse.Y;
  FBox2D Box(ForceInit); Box+=SelectionStart_; Box+=FVector2D(X,Y);
  DrawRect(FLinearColor(.05f,.65f,.9f,.15f),Box.Min.X,Box.Min.Y,Box.GetSize().X,Box.GetSize().Y);
  DrawLine(Box.Min.X,Box.Min.Y,Box.Max.X,Box.Min.Y,FLinearColor(.1f,.85f,1),2);
  DrawLine(Box.Min.X,Box.Max.Y,Box.Max.X,Box.Max.Y,FLinearColor(.1f,.85f,1),2);
  DrawLine(Box.Min.X,Box.Min.Y,Box.Min.X,Box.Max.Y,FLinearColor(.1f,.85f,1),2);
  DrawLine(Box.Max.X,Box.Min.Y,Box.Max.X,Box.Max.Y,FLinearColor(.1f,.85f,1),2);
 }
 if(!bCommands_) {
  if(!CultHUDSpace::Pointer(PC,Mouse)) return;
  const auto Hit=P->CameraAim(25000,true); if(!Hit.bBlockingHit) return;
  // Visible 10m selection footprint in front of the entity. Ground triangles preserve terrain relief.
  FVector2D Last=FVector2D::ZeroVector; bool HasLast=false;
  for(int I=0;I<=48;++I) {
   const float A=I*2*PI/48; FVector Point=Hit.ImpactPoint+FVector(FMath::Cos(A)*1000,FMath::Sin(A)*1000,0);
   FHitResult Floor; FCollisionQueryParams Q(SCENE_QUERY_STAT(CultSelectionFloor),false,P);
   if(GetWorld()->LineTraceSingleByObjectType(Floor,Point+FVector(0,0,2000),Point-FVector(0,0,3000),FCollisionObjectQueryParams(ECC_WorldStatic),Q)) Point=Floor.ImpactPoint+FVector(0,0,20);
   FVector2D V=FVector2D::ZeroVector; const bool Visible=PC->ProjectWorldLocationToScreen(Point,V,true)&&V.X>=0&&V.X<=Canvas->SizeX&&V.Y>=0&&V.Y<=Canvas->SizeY;
   if(HasLast&&Visible) DrawLine(Last.X,Last.Y,V.X,V.Y,FLinearColor(.1f,.85f,1,.8f),1.5f);
   Last=V; HasLast=Visible;
  }
 }
}
