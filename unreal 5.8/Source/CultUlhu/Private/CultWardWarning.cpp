#include "CultWardWarning.h"
#include "CultSkillAttack.h"
#include "CultMatchUnit.h"
#include "CultUlhuPlayerPawn.h"
#include "CultPlayHUD.h"
#include "EngineUtils.h"
#include "Engine/Canvas.h"
#include "UI/CultUIStyle.h"
#include "Framework/Application/SlateApplication.h"
#include "Fonts/FontMeasure.h"
#include "Rendering/SlateRenderer.h"

FCultWardWarning CultWardWarning::Observe(ACultUlhuPlayerPawn* P) {
 FCultWardWarning Result;
 if(!IsValid(P)||P->GetHealth()<=0||P->IsDashing()) return Result;
 float Earliest=TNumericLimits<float>::Max();
 for(TActorIterator<ACultSkillAttack> It(P->GetWorld());It;++It) {
  auto* Ward=Cast<ACultMatchUnit>(It->GetOwner());
  if(It->IsActorBeingDestroyed()||!It->WardLocked||It->WardTarget.Get()!=P||!Ward||!Ward->IsStructure()||!ACultSkillAttack::Alive(Ward)||!ACultSkillAttack::Enemy(Ward,P)) continue;
  const float Left=It->WardReleaseRemaining();
  if(Result.IsVisible()&&Left>=Earliest) continue;
  Earliest=Left; Result.Shot=*It; Result.ReleaseIn=Left; Result.Incoming=Left<=0; Result.Source=Ward->GetActorLocation();
 }
 return Result;
}
FBox2D CultWardWarning::Bounds(FVector2D Canvas,float S,FVector2D TextSize) {
 const FVector2D Size(FMath::Min(Canvas.X-24*S,FMath::Max(280*S,TextSize.X+70*S)),FMath::Max(58*S,TextSize.Y+20*S));
 const FVector2D Top((Canvas.X-Size.X)*.5f,FMath::Max(12*S,Canvas.Y*.5f-158*S));
 return FBox2D(Top,Top+Size);
}
FCultWardWarningPanel CultWardWarning::Panel(const FCultWardWarning& Warning,float Cooldown,UCanvas* C,bool DockTop,float Stamina) {
 FCultWardWarningPanel Result; if(!Warning.IsVisible()||!C) return Result;
 const float S=FMath::Clamp(FMath::Min(C->SizeX/1600.f,C->SizeY/900.f),.55f,2.f);
 Result.Title=Warning.Incoming?TEXT("WARD SHOT INCOMING"):TEXT("WARD LOCKED ON YOU");
 Result.Detail=Cooldown>.05f?FString::Printf(TEXT("DASH RECHARGING  %.1fs"),Cooldown):Stamina<18?TEXT("LOW STAMINA | DASH UNAVAILABLE"):Warning.Incoming?TEXT("TAP SHIFT TO BREAK LOCK"):FString::Printf(TEXT("DASH AT RELEASE  %.1fs"),Warning.ReleaseIn);
 const auto Measure=FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
 const auto Title=Measure->Measure(Result.Title,CultUI::Font(15*S,true)),Detail=Measure->Measure(Result.Detail,CultUI::Font(11*S));
 Result.TitleHeight=Title.Y;
 Result.Bounds=Bounds(FVector2D(C->SizeX,C->SizeY),S,FVector2D(FMath::Max(Title.X,Detail.X),Title.Y+Detail.Y+4*S));
 if(DockTop) { const auto Size=Result.Bounds.GetSize(); Result.Bounds=FBox2D(FVector2D(Result.Bounds.Min.X,12*S),FVector2D(Result.Bounds.Max.X,12*S+Size.Y)); }
 return Result;
}
void CultWardWarning::Draw(ACultPlayHUD* HUD,ACultUlhuPlayerPawn* P,UCanvas* C,bool DockTop) {
 const auto Warning=Observe(P); if(!Warning.IsVisible()) return;
 const float S=FMath::Clamp(FMath::Min(C->SizeX/1600.f,C->SizeY/900.f),.55f,2.f);
 const auto Data=P->CombatHUD(); const auto View=Panel(Warning,Data.DashCooldown,C,DockTop,Data.Stamina); const auto& B=View.Bounds;
 const FLinearColor Coral(1.f,.3f,.18f);
 HUD->DrawRect(CultUI::Panel,B.Min.X,B.Min.Y,B.GetSize().X,B.GetSize().Y); HUD->DrawRect(Coral,B.Min.X,B.Min.Y,3*S,B.GetSize().Y);
 CultUI::DrawText(C,View.Title,Coral,B.Min.X+48*S,B.Min.Y+8*S,15*S,true);
 CultUI::DrawText(C,View.Detail,CultUI::Bone,B.Min.X+48*S,B.Min.Y+12*S+View.TitleHeight,11*S);
 const FVector Local=(Warning.Source-P->GetActorLocation()).GetSafeNormal2D().RotateAngleAxis(-P->AimRotation().Yaw,FVector::UpVector);
 const FVector2D D(Local.Y,-Local.X),Across(-D.Y,D.X),Tip=FVector2D(B.Min.X+26*S,B.GetCenter().Y)+D*11*S;
 for(int Sign:{-1,1}) { const auto End=Tip-D*12*S+Across*Sign*7*S; HUD->DrawLine(Tip.X,Tip.Y,End.X,End.Y,Coral,2*S); }
}
