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
void FCultInspectionHover::Update(ACultPopulation* Hit,float Dt)
{
 Dt=FMath::Clamp(Dt,0.f,.1f);
 if(Candidate.Get()!=Hit) { Candidate=Hit; Dwell=0; }
 else if(Hit) Dwell+=Dt;
 const bool Ready=Hit&&Dwell>=1.5f;
 if(Visible.Get()!=Hit||!Ready) {
  Opacity=FMath::Max(0.f,Opacity-Dt/.22f);
  if(Opacity<=0) { Visible.Reset(); if(Ready) Visible=Hit; }
 }
 if(Ready&&Visible.Get()==Hit) Opacity=FMath::Min(1.f,Opacity+Dt/.22f);
}
void ACultPlayHUD::DrawPopulationInspection()
{
 auto* PC=GetOwningPlayerController(); auto* D=ACultWorldDirector::Find(GetWorld());
 if(!Canvas||!PC||!PC->PlayerCameraManager||!D) { InspectionHover_.Reset(); return; }
 CSV_SCOPED_TIMING_STAT(CultRuntime, InspectionHUD);
 ACultPopulation* Hovered=nullptr;
 if(PC->bShowMouseCursor&&!bCommands_&&!bSelecting_) {
  // Trace the actual body under the cursor, rather than a generous head radius.
  FVector Origin,Direction;
  if(PC->DeprojectMousePositionToWorld(Origin,Direction)) {
   FHitResult Hit; FCollisionQueryParams Q(SCENE_QUERY_STAT(CultInspectionVisibility),false,GetOwningPawn());
   if(GetWorld()->LineTraceSingleByChannel(Hit,Origin,Origin+Direction*30000,ECC_Visibility,Q)) {
    auto* Person=Cast<ACultPopulation>(Hit.GetActor());
    if(Person&&Person->IsCultist()&&Person->GetHealth()>0) Hovered=Person;
   }
  }
 }
 InspectionHover_.Update(Hovered,GetWorld()->GetDeltaSeconds());
 auto* Person=InspectionHover_.Visible.Get();
 if(!Person||Person->GetHealth()<=0||InspectionHover_.Opacity<=0) return;
 const float S=FMath::Clamp(FMath::Min(Canvas->SizeX/1280.f,Canvas->SizeY/720.f),.8f,1.5f);
 const float Main=FMath::Max(14.f,15*S),Small=FMath::Max(12.f,12*S),Pad=8*S;
 const FBox2D Safe(FVector2D(Pad,FMath::Max(100.f,112*S)),FVector2D(Canvas->SizeX-Pad,FMath::Min(Canvas->SizeY-125*S,CombatBottomTop_-Pad)));
 if(!Safe.bIsValid||Safe.GetSize().Y<72*S||bCommands_) return;
 FCultInspectionCard Card; Card.Person=Person; Card.Selected=D->IsSelected(Person); Card.Hovered=Hovered==Person; Card.Opacity=InspectionHover_.Opacity;
 const FVector Head=Person->GetActorLocation()+FVector(0,0,Person->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()+25);
 if(!PC->ProjectWorldLocationToScreen(Head,Card.Anchor,true)) return;
 Card.Activity=Person->InspectionActivity();
 Card.Identity=ACultWorldDirector::CultName(Person->IsCaptive()?Person->GetCaptiveFaction():Person->GetFaction())+TEXT(" / ")+Person->InspectionRole();
 Card.Contribution=Person->InspectionContribution(); Card.Health=Person->GetHealth(); Card.MaxHealth=Person->GetMaxHealth();
 const int Faction=Person->IsCaptive()?Person->GetCaptiveFaction():Person->GetFaction();
 const auto* Hero=Cast<ACultUlhuPlayerPawn>(GetOwningPawn()); const int OwnFaction=Hero?Hero->GetFaction():0;
 Card.Accent=Card.Selected?CultUI::Gold:Faction==OwnFaction?CultUI::Teal:Faction>=0&&D->AtWar(OwnFaction,Faction)?CultUI::Danger:CultUI::Bone;
 const FVector2D Size(300*S,(Card.Contribution.IsEmpty()?52:72)*S); TArray<FBox2D> Occupied;
 if(!CultPlaceInspection(Card.Anchor,Size,Safe,Occupied,6*S,Card.Bounds)) return;
 const FString HP=FString::Printf(TEXT("%.0f HP"),Card.Health);
 const float HpWidth=FSlateApplication::Get().GetRenderer()->GetFontMeasureService()->Measure(HP,CultUI::Font(Small)).X;
 Card.Activity=FitInspection(Card.Activity,Size.X-Pad*3-HpWidth,Main);
 Card.Identity=FitInspection(Card.Identity,Size.X-Pad*2,Small);
 Card.Contribution=FitInspection(Card.Contribution,Size.X-Pad*2,Small);
 InspectionCards_.Add(Card);
 auto Fade=[&](FLinearColor C) { C.A*=Card.Opacity; return C; };
 DrawLine(Card.Anchor.X,Card.Anchor.Y,Card.Bounds.GetCenter().X,Card.Bounds.Max.Y,Fade(Card.Accent.CopyWithNewOpacity(.65f)),1);
 const auto B=Card.Bounds; const float X=B.Min.X+Pad,Y=B.Min.Y+5*S;
 DrawRect(Fade(CultUI::Background.CopyWithNewOpacity(.97f)),B.Min.X,B.Min.Y,B.GetSize().X,B.GetSize().Y);
 DrawRect(Fade(Card.Accent),B.Min.X,B.Min.Y,2*S,B.GetSize().Y);
 CultCanvasLabel(Canvas,Card.Activity,Fade(CultUI::Bone),X,Y,Main);
 CultCanvasLabel(Canvas,HP,Fade(CultUI::Muted),B.Max.X-Pad-HpWidth,Y+1,Small);
 CultCanvasLabel(Canvas,Card.Identity,Fade(Card.Accent),X,Y+23*S,Small);
 if(!Card.Contribution.IsEmpty()) CultCanvasLabel(Canvas,Card.Contribution,Fade(CultUI::Bone),X,Y+43*S,Small);
 DrawRect(Fade(CultUI::Line),B.Min.X+2*S,B.Max.Y-3*S,B.GetSize().X-2*S,3*S);
 DrawRect(Fade(Card.Accent),B.Min.X+2*S,B.Max.Y-3*S,(B.GetSize().X-2*S)*FMath::Clamp(Card.Health/FMath::Max(1.f,Card.MaxHealth),0.f,1.f),3*S);
}
