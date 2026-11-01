#include "CultHeroPlates.h"
#include "CultPlayHUD.h"
#include "CultUlhuPlayerPawn.h"
#include "CultMatchDirector.h"
#include "CultCanvasLabel.h"
#include "CultHUDProjection.h"
#include "UI/CultUIStyle.h"
#include "Engine/Canvas.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "HAL/IConsoleManager.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/SlateRenderer.h"
#include "Fonts/FontMeasure.h"

namespace {
TAutoConsoleVariable<int32> Enabled(TEXT("cult.Match.HeroPlates"),1,TEXT("Visible competitive hero identity and health plates. 0 disables presentation only."));
struct FCandidate { ACultUlhuPlayerPawn* Hero; FString Name; FVector2D Anchor; float Priority; bool Friendly,Focused; };
}
void CultHeroPlates::Draw(ACultPlayHUD* HUD,UCanvas* C,ACultUlhuPlayerPawn* P,ACultMatchDirector* M,const FBox2D& Safe,const TArray<FBox2D>& Reserved) {
#if WITH_DEV_AUTOMATION_TESTS
 const bool Capture=Probe&&!ProbeReady&&ProbeRequest.Equals(FVector2D(C->SizeX,C->SizeY),1);
 if(Capture) { ProbeReady=true; ProbeSize=FVector2D(C->SizeX,C->SizeY); Captured.Reset(); CapturedSafe=Safe; }
#endif
 if(!Enabled.GetValueOnGameThread()||M->IsOver()||M->IsPaused()||P->StatsVisible()||!Safe.bIsValid) return;
 auto* PC=HUD->GetOwningPlayerController(); if(!PC||!PC->PlayerCameraManager) return;
 const float S=FMath::Clamp(FMath::Min(C->SizeX/1600.f,C->SizeY/900.f),.65f,2.f);
 const auto Measure=FSlateApplication::Get().GetRenderer()->GetFontMeasureService(); const auto Font=CultUI::Font(11*S);
 const FVector Eye=PC->PlayerCameraManager->GetCameraLocation(); TArray<FCandidate,TInlineAllocator<9>> Candidates;
 for(const auto& F:M->Fighters()) {
  auto* H=F.Pawn.Get(); if(!H||H==P||F.Respawn>0||H->GetHealth()<=0||H->IsHidden()||FVector::DistSquared(Eye,H->GetActorLocation())>FMath::Square(30000.f)) continue;
  const float Half=H->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
  const FVector Head=H->GetActorLocation()+FVector(0,0,Half+100); FVector2D Anchor;
  if(!CultHUDProjection::Project(C,Head,Anchor)||!Safe.IsInside(Anchor)) continue;
  FCollisionQueryParams Q(SCENE_QUERY_STAT(HeroPlateSight),false,P); Q.AddIgnoredActor(H); FHitResult Hit;
  // Only visible bodies qualify. A remembered combat focus never reveals a
  // hidden hero; either its centre or head must have a real unobstructed ray.
  if(P->GetWorld()->LineTraceSingleByChannel(Hit,Eye,H->GetActorLocation(),ECC_Visibility,Q)&&P->GetWorld()->LineTraceSingleByChannel(Hit,Eye,Head,ECC_Visibility,Q)) continue;
  const bool Friendly=F.Team==P->GetFaction(),Focus=P->CombatFocus()==H;
  const float Injury=1-FMath::Clamp(H->GetHealth()/H->GetMaxHealth(),0.f,1.f);
  const float Central=1-FMath::Clamp(float(FVector2D::Distance(Anchor,FVector2D(C->SizeX*.5f,C->SizeY*.5f)))/C->SizeX,0.f,1.f);
  Candidates.Add({H,F.Name.ToUpper(),Anchor,(Focus?1000.f:0.f)+Injury*(Friendly?120.f:60.f)+Central*40+(Friendly?0.f:15.f),Friendly,Focus});
 }
 Candidates.Sort([](const auto& A,const auto& B){return A.Priority!=B.Priority?A.Priority>B.Priority:A.Hero->GetUniqueID()<B.Hero->GetUniqueID();});
 struct FDrawPlate { const FCandidate* Candidate; FString Name; FBox2D Bounds; FVector2D TextSize; };
 TArray<FDrawPlate,TInlineAllocator<4>> Cards;
 TArray<FBox2D,TInlineAllocator<4>> Placed; const int Limit=C->SizeY<500?2:4;
 for(const auto& Candidate:Candidates) {
  if(Placed.Num()>=Limit) break;
  FString Name=Candidate.Name; const float Pad=7*S,Mark=14*S,MaxWidth=150*S;
  while(!Name.IsEmpty()&&Measure->Measure(Name,Font).X>MaxWidth) { Name.LeftChopInline(1); }
  if(Name!=Candidate.Name) { while(!Name.IsEmpty()&&Measure->Measure(Name+TEXT("..."),Font).X>MaxWidth) Name.LeftChopInline(1); Name+=TEXT("..."); }
  const auto TextSize=Measure->Measure(Name,Font); const float Width=FMath::Max(100*S,float(TextSize.X)+2*Pad+Mark),Height=TextSize.Y+14*S;
  bool Fits=false; FBox2D Bounds;
  for(int Row=0;Row<7&&!Fits;++Row) {
   // Prefer short lateral leaders over stacking high above another hero.
   // Head-centred side choices fit compact views with contextual RMB instructions.
   const float ShiftX=Row>=1&&Row<=4?(Row%2?-1.f:1.f)*(Width*.5f+40*S):0.f;
   const float ShiftY=Row>=5?(Row-4)*(Height+4*S):Row>=3?-(Height*.5f+8*S):0.f;
   const FVector2D Min(Candidate.Anchor.X-Width*.5f+ShiftX,Candidate.Anchor.Y-Height-8*S-ShiftY); Bounds=FBox2D(Min,Min+FVector2D(Width,Height));
   if(!Safe.IsInside(Bounds.Min)||!Safe.IsInside(Bounds.Max)) continue;
   bool Blocked=false; for(const auto& B:Reserved) if(B.bIsValid&&B.ExpandBy(4*S).Intersect(Bounds)) { Blocked=true; break; }
   if(!Blocked) for(const auto& B:Placed) if(B.ExpandBy(4*S).Intersect(Bounds)) { Blocked=true; break; }
   Fits=!Blocked;
  }
  if(!Fits) continue;
  Placed.Add(Bounds); Cards.Add({&Candidate,MoveTemp(Name),Bounds,TextSize});
 }
 // Leaders all sit beneath all cards, including cards placed earlier by focus
 // priority. A later candidate must never draw a line across another name.
 for(const auto& Card:Cards) {
  const auto& Candidate=*Card.Candidate; const auto& Bounds=Card.Bounds; const auto Accent=Candidate.Friendly?CultUI::Teal:CultUI::Danger;
  HUD->DrawLine(FMath::Clamp(Candidate.Anchor.X,Bounds.Min.X,Bounds.Max.X),FMath::Clamp(Candidate.Anchor.Y,Bounds.Min.Y,Bounds.Max.Y),Candidate.Anchor.X,Candidate.Anchor.Y,Accent.CopyWithNewOpacity(.6f),S);
 }
 for(const auto& Card:Cards) {
  const auto& Candidate=*Card.Candidate; const auto& Bounds=Card.Bounds; const auto& Name=Card.Name; const auto TextSize=Card.TextSize;
  const float Width=Bounds.GetSize().X,Height=Bounds.GetSize().Y,Pad=7*S,Mark=14*S; const auto Accent=Candidate.Friendly?CultUI::Teal:CultUI::Danger;
  const float X=Bounds.Min.X,Y=Bounds.Min.Y,Health=Candidate.Hero->GetHealth(),Max=FMath::Max(1.f,Candidate.Hero->GetMaxHealth());
  HUD->DrawRect(CultUI::Panel,X,Y,Width,Height); HUD->DrawRect(Candidate.Focused?CultUI::Gold:Accent,X,Y,Width,FMath::Max(1.f,S));
  const float GX=X+Pad+4*S,GY=Y+TextSize.Y*.5f+3*S;
  if(Candidate.Friendly) { HUD->DrawLine(GX-3*S,GY,GX+3*S,GY,Accent,1.5f*S); HUD->DrawLine(GX,GY-3*S,GX,GY+3*S,Accent,1.5f*S); }
  else for(int I=0;I<4;++I) { const FVector2D D[]={FVector2D(0,-4),FVector2D(4,0),FVector2D(0,4),FVector2D(-4,0)}; HUD->DrawLine(GX+D[I].X*S,GY+D[I].Y*S,GX+D[(I+1)%4].X*S,GY+D[(I+1)%4].Y*S,Accent,S); }
  CultCanvasLabel(C,Name,CultUI::Bone,X+Pad+Mark,Y+3*S,11*S);
  const float BarY=Bounds.Max.Y-7*S,BarW=Width-2*Pad;
  HUD->DrawRect(FLinearColor(.12f,.17f,.20f),X+Pad,BarY,BarW,4*S); HUD->DrawRect(Accent,X+Pad,BarY,BarW*FMath::Clamp(Health/Max,0.f,1.f),4*S);
  for(int I=1;I<5;++I) HUD->DrawRect(CultUI::Panel,X+Pad+BarW*I/5.f,BarY,FMath::Max(1.f,S),4*S);
#if WITH_DEV_AUTOMATION_TESTS
  if(Capture) Captured.Add({Candidate.Hero,Name,Bounds,Candidate.Anchor,Health,Max,Candidate.Friendly,Candidate.Focused});
#endif
 }
}
