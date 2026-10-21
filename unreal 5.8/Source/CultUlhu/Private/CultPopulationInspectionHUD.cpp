#include "CultPlayHUD.h"
#include "CultPopulation.h"
#include "CultWorldDirector.h"
#include "CultUlhuPlayerPawn.h"
#include "CultInspectionLayout.h"
#include "CultCanvasLabel.h"
#include "CultHUDSpace.h"
#include "Engine/Canvas.h"
#include "EngineUtils.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "Framework/Application/SlateApplication.h"
#include "Fonts/FontMeasure.h"
#include "Rendering/SlateRenderer.h"
#include "ProfilingDebugging/CsvProfiler.h"
CSV_DECLARE_CATEGORY_EXTERN(CultRuntime);

namespace {
FString FitInspection(FString Text,float Width,float Points)
{
 const auto Measure=FSlateApplication::Get().GetRenderer()->GetFontMeasureService(); const auto Font=CultUI::Font(Points);
 if(Measure->Measure(Text,Font).X<=Width) return Text;
 while(Text.Len()>1&&Measure->Measure(Text+TEXT("..."),Font).X>Width) Text.LeftChopInline(1);
 return Text+TEXT("...");
}
}
void ACultPlayHUD::DrawPopulationInspection()
{
 auto* PC=GetOwningPlayerController(); auto* D=ACultWorldDirector::Find(GetWorld());
 if(!Canvas||!PC||!PC->bShowMouseCursor||!PC->PlayerCameraManager||!D) return;
 CSV_SCOPED_TIMING_STAT(CultRuntime, InspectionHUD);
 const float S=FMath::Clamp(FMath::Min(Canvas->SizeX/1280.f,Canvas->SizeY/720.f),.8f,1.5f);
 const float Main=FMath::Max(14.f,15*S),Small=FMath::Max(12.f,12*S),Pad=8*S;
 const FBox2D Safe(FVector2D(Pad,FMath::Max(100.f,112*S)),FVector2D(Canvas->SizeX-Pad,Canvas->SizeY-125*S));
 if(!Safe.bIsValid||Safe.GetSize().Y<60) return;
 const FVector Camera=PC->PlayerCameraManager->GetCameraLocation();
 FVector2D Mouse; if(!CultHUDSpace::Pointer(PC,Mouse)) Mouse=FVector2D(-100000,-100000);
 const float MX=Mouse.X,MY=Mouse.Y;
 struct Candidate { ACultPopulation* Person; FVector2D Anchor; FVector Head; float Distance,Cursor; bool Selected; };
 TArray<Candidate> Candidates;
 for(TActorIterator<ACultPopulation> It(GetWorld());It;++It) {
  auto* P=*It; if(P->GetHealth()<=0) continue;
  const float Distance=FVector::DistSquared(Camera,P->GetActorLocation()); if(Distance>FMath::Square(30000.f)) continue;
  const FVector Head=P->GetActorLocation()+FVector(0,0,P->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()+25);
  FVector2D V; if(!PC->ProjectWorldLocationToScreen(Head,V,true)||V.X<Pad||V.X>Canvas->SizeX-Pad||V.Y<Safe.Min.Y||V.Y>Canvas->SizeY-Pad) continue;
  Candidates.Add({P,V,Head,Distance,float(FVector2D::DistSquared(V,FVector2D(MX,MY))),D->IsSelected(P)});
 }
 Candidates.Sort([](const Candidate& A,const Candidate& B) {
  const int AR=A.Cursor<3600?0:A.Selected?1:2,BR=B.Cursor<3600?0:B.Selected?1:2;
  if(AR!=BR) return AR<BR;
  if(AR==0&&A.Cursor!=B.Cursor) return A.Cursor<B.Cursor;
  if(A.Distance!=B.Distance) return A.Distance<B.Distance;
  return A.Person->GetUniqueID()<B.Person->GetUniqueID();
 });
 TArray<FBox2D> Occupied;
 if(bCommands_) {
  // Same menu rectangle as DrawHUD: labels never cover its clickable buttons.
  const float MenuS=FMath::Clamp(FMath::Min(Canvas->SizeX/1600.f,Canvas->SizeY/900.f),.6f,2.f);
  const float W=(340*2+8)*MenuS,H=((Actions_.Num()+1)/2)*(42+6)*MenuS;
  const FVector2D Min(FMath::Clamp(CommandScreen_.X,8.f,Canvas->SizeX-W-8),FMath::Clamp(CommandScreen_.Y,145*MenuS,Canvas->SizeY-H-24*MenuS));
  Occupied.Emplace(Min-FVector2D(8*MenuS),Min+FVector2D(W,H)+FVector2D(8*MenuS));
 }
 int Probes=0; bool HasHover=false;
 for(const auto& C:Candidates) {
  if(InspectionCards_.Num()>=8||++Probes>32) break;
  FHitResult Hit; FCollisionQueryParams Q(SCENE_QUERY_STAT(CultInspectionVisibility),false,GetOwningPawn());
  if(GetWorld()->LineTraceSingleByChannel(Hit,Camera,C.Head,ECC_Visibility,Q)&&Hit.GetActor()!=C.Person) continue;
  const bool Hovered=!HasHover&&C.Cursor<3600; HasHover|=Hovered;
  FCultInspectionCard Card; Card.Person=C.Person; Card.Anchor=C.Anchor; Card.Selected=C.Selected; Card.Hovered=Hovered;
  Card.Activity=C.Person->InspectionActivity();
  Card.Identity=ACultWorldDirector::CultName(C.Person->IsCaptive()?C.Person->GetCaptiveFaction():C.Person->GetFaction())+TEXT(" / ")+C.Person->InspectionRole();
  if(C.Selected||Hovered) Card.Contribution=C.Person->InspectionContribution();
  Card.Health=C.Person->GetHealth(); Card.MaxHealth=C.Person->GetMaxHealth();
  const int Faction=C.Person->IsCaptive()?C.Person->GetCaptiveFaction():C.Person->GetFaction();
  const auto* Hero=Cast<ACultUlhuPlayerPawn>(GetOwningPawn()); const int OwnFaction=Hero?Hero->GetFaction():0;
  Card.Accent=C.Selected?CultUI::Gold:Faction==OwnFaction?CultUI::Teal:Faction>=0&&D->AtWar(OwnFaction,Faction)?CultUI::Danger:CultUI::Bone;
  const FVector2D Size((C.Selected||Hovered?300:220)*S,(Card.Contribution.IsEmpty()?52:72)*S);
  if(!CultPlaceInspection(C.Anchor,Size,Safe,Occupied,6*S,Card.Bounds)) continue;
  // The public card snapshot is the text actually rendered. Tests must not
  // validate an untruncated model while important progress is hidden onscreen.
  const FString HP=FString::Printf(TEXT("%.0f HP"),Card.Health);
  const float HpWidth=FSlateApplication::Get().GetRenderer()->GetFontMeasureService()->Measure(HP,CultUI::Font(Small)).X;
  Card.Activity=FitInspection(Card.Activity,Size.X-Pad*3-HpWidth,Main);
  Card.Identity=FitInspection(Card.Identity,Size.X-Pad*2,Small);
  if(!Card.Contribution.IsEmpty()) Card.Contribution=FitInspection(Card.Contribution,Size.X-Pad*2,Small);
  Occupied.Add(Card.Bounds); InspectionCards_.Add(Card);
 }
 // Draw every connector before any panel, so no later line crosses older text.
 for(const auto& Card:InspectionCards_) {
  DrawLine(Card.Anchor.X,Card.Anchor.Y,Card.Bounds.GetCenter().X,Card.Bounds.Max.Y,Card.Accent.CopyWithNewOpacity(.65f),1);
  DrawRect(Card.Accent,Card.Anchor.X-2,Card.Anchor.Y-2,4,4);
 }
 for(const auto& Card:InspectionCards_) {
  const auto B=Card.Bounds; const float X=B.Min.X+Pad,Y=B.Min.Y+5*S;
  DrawRect(CultUI::Background.CopyWithNewOpacity(.97f),B.Min.X,B.Min.Y,B.GetSize().X,B.GetSize().Y);
  DrawRect(Card.Accent,B.Min.X,B.Min.Y,2*S,B.GetSize().Y);
  const FString HP=FString::Printf(TEXT("%.0f HP"),Card.Health);
  const float HpWidth=FSlateApplication::Get().GetRenderer()->GetFontMeasureService()->Measure(HP,CultUI::Font(Small)).X;
  CultCanvasLabel(Canvas,Card.Activity,CultUI::Bone,X,Y,Main);
  CultCanvasLabel(Canvas,HP,CultUI::Muted,B.Max.X-Pad-HpWidth,Y+1,Small);
  CultCanvasLabel(Canvas,Card.Identity,Card.Accent,X,Y+23*S,Small);
  if(!Card.Contribution.IsEmpty()) CultCanvasLabel(Canvas,Card.Contribution,CultUI::Bone,X,Y+43*S,Small);
  DrawRect(CultUI::Line,B.Min.X+2*S,B.Max.Y-3*S,B.GetSize().X-2*S,3*S);
  DrawRect(Card.Accent,B.Min.X+2*S,B.Max.Y-3*S,(B.GetSize().X-2*S)*FMath::Clamp(Card.Health/FMath::Max(1.f,Card.MaxHealth),0.f,1.f),3*S);
 }
}
