#include "CultWardWarning.h"
#include "CultSkillAttack.h"
#include "CultMatchUnit.h"
#include "CultUlhuPlayerPawn.h"
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
FCultWardWarningPanel CultWardWarning::Panel(const FCultWardWarning& Warning,float Cooldown,UCanvas* C) {
 FCultWardWarningPanel Result; if(!Warning.IsVisible()||!C) return Result;
 const float S=FMath::Clamp(FMath::Min(C->SizeX/1600.f,C->SizeY/900.f),.55f,2.f);
 Result.Title=Warning.Incoming?TEXT("WARD SHOT INCOMING"):TEXT("WARD LOCKED ON YOU");
 Result.Detail=Cooldown>.05f?FString::Printf(TEXT("DASH RECHARGING  %.1fs"),Cooldown):Warning.Incoming?TEXT("TAP SHIFT TO BREAK LOCK"):FString::Printf(TEXT("DASH AT RELEASE  %.1fs"),Warning.ReleaseIn);
 const auto Measure=FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
 const auto Title=Measure->Measure(Result.Title,CultUI::Font(15*S,true)),Detail=Measure->Measure(Result.Detail,CultUI::Font(11*S));
 Result.TitleHeight=Title.Y;
 Result.Bounds=Bounds(FVector2D(C->SizeX,C->SizeY),S,FVector2D(FMath::Max(Title.X,Detail.X),Title.Y+Detail.Y+4*S));
 return Result;
}
