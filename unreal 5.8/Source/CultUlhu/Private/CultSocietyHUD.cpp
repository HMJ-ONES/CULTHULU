#include "CultPlayHUD.h"
#include "CultWorldDirector.h"
#include "CultUlhuGameInstance.h"
#include "CultCanonicalKit.h"
#include "UI/CultUIStyle.h"
#include "Engine/Canvas.h"
#include "Framework/Application/SlateApplication.h"
#include "Fonts/FontMeasure.h"
#include "Rendering/SlateRenderer.h"
void ACultPlayHUD::DrawSociety() {
 auto* D=ACultWorldDirector::Find(GetWorld()); if(!D||!Canvas) return;
 const float W=Canvas->SizeX; auto Measure=FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
 auto Fit=[&](FString Value) { if(Measure->Measure(Value,CultUI::Font(11)).X<=W-64) return Value; while(!Value.IsEmpty()&&Measure->Measure(Value+TEXT("..."),CultUI::Font(11)).X>W-64) Value.LeftChopInline(1); return Value+TEXT("..."); };
 DrawRect(CultUI::Background,16,16,W-32,148);
 CultUI::DrawText(Canvas,FString::Printf(TEXT("LIVING CULTS  /  STRENGTH x%.2f  /  B FOR BELIEFS AND OUTCOMES"),D->CombatMultiplier(0)),CultUI::Gold,28,26,14,true);
 const FLinearColor Colors[]={CultUI::Teal,CultUI::Danger,FLinearColor(.66f,.55f,.8f)};
 for(int F=0;F<3;++F) CultUI::DrawText(Canvas,Fit(D->ActivitySummary(F)),Colors[F],28,54+F*23,11);
 CultUI::DrawText(Canvas,Fit(D->RecentContribution()),CultUI::Muted,28,127,11);
 if(auto* GI=Cast<UCultUlhuGameInstance>(GetGameInstance())) {
  DrawRect(CultUI::Background,16,180,W-32,FMath::Min(180.f,float(Canvas->SizeY)-196));
  CultUI::DrawText(Canvas,TEXT("YOUR MOVESET"),CultUI::Gold,28,190,14,true);
  TArray<FString> Lines; CultCanonicalKit::Summary(GI->SelectedCharacterId).ParseIntoArrayLines(Lines);
  for(int I=0;I<Lines.Num();++I) CultUI::DrawText(Canvas,Fit(Lines[I]),CultUI::Bone,28,216+I*17,11);
 }
}
