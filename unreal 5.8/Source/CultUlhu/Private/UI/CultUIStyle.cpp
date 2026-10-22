#include "UI/CultUIStyle.h"
#include "Components/Button.h"
#include "Fonts/CompositeFont.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Misc/Paths.h"
#include "Engine/Canvas.h"
#include "Engine/Font.h"
#include "CanvasItem.h"
#include "UObject/StrongObjectPtr.h"
#include "CultHUDTextProbe.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Framework/Application/SlateApplication.h"
#include "Rendering/SlateRenderer.h"
#include "Fonts/FontMeasure.h"
#endif
namespace CultUI {
FSlateFontInfo Font(float Size,bool Heading,bool Medium) {
 static const TSharedPtr<const FCompositeFont> Family=[] {
  auto F=MakeShared<FCompositeFont>();
  for(const auto& E:{TPair<FName,FString>(TEXT("Body"),TEXT("Roboto-Regular.ttf")),TPair<FName,FString>(TEXT("Medium"),TEXT("Roboto-Medium.ttf")),TPair<FName,FString>(TEXT("Display"),TEXT("Roboto-BoldCondensed.ttf"))})
   F->DefaultTypeface.Fonts.Emplace(E.Key,FPaths::ConvertRelativePathToFull(FPaths::ProjectContentDir()/TEXT("UI/Fonts")/E.Value),EFontHinting::Default,EFontLoadingPolicy::LazyLoad);
  return F;
 }();
 return FSlateFontInfo(Family,FMath::Max(10.f,Size),Heading?TEXT("Display"):Medium?TEXT("Medium"):TEXT("Body"));
}
UTextBlock* Label(UObject* Outer,const FString& Value,int Size,FLinearColor Color,bool Heading,ETextJustify::Type Align) {
 auto* T=NewObject<UTextBlock>(Outer); T->SetText(FText::FromString(Value)); T->SetFont(Font(Size,Heading)); T->SetColorAndOpacity(FSlateColor(Color)); T->SetJustification(Align); return T;
}
void Button(UButton* B,bool Primary,bool Selected) {
 const FLinearColor Fill=Primary?FLinearColor(.075f,.19f,.175f):Selected?FLinearColor(.04f,.115f,.108f):Panel;
 FButtonStyle S; S.Normal=FSlateRoundedBoxBrush(Fill,2.f,Selected||Primary?Teal:Line,Selected?2.f:1.f);
 S.Hovered=FSlateRoundedBoxBrush(FLinearColor(.055f,.145f,.137f),2.f,Gold,1.f);
 S.Pressed=FSlateRoundedBoxBrush(FLinearColor(.025f,.08f,.075f),2.f,Teal,1.f);
 S.Disabled=FSlateRoundedBoxBrush(FLinearColor(.02f,.03f,.034f),2.f,FLinearColor(.07f,.1f,.1f),1.f);
 S.NormalPadding=FMargin(16,11); S.PressedPadding=FMargin(16,12,16,10);
 B->SetStyle(S); B->SetColorAndOpacity(Bone); B->SetClickMethod(EButtonClickMethod::DownAndUp);
}
void DrawText(UCanvas* C,const FString& Value,FLinearColor Color,float X,float Y,float Size,bool Heading) {
 static TStrongObjectPtr<UFont> Cache(LoadObject<UFont>(nullptr,TEXT("/Engine/EngineFonts/Roboto.Roboto")));
 FCanvasTextItem T(FVector2D(X,Y),FText::FromString(Value),Font(Size,Heading),Color); T.Font=Cache.Get(); C->DrawItem(T);
#if WITH_DEV_AUTOMATION_TESTS
 if(CultHUDTextProbe::Enabled) {
  auto& Frame=GIsHighResScreenshot?CultHUDTextProbe::CaptureFrame:CultHUDTextProbe::Frame;
  auto& Samples=GIsHighResScreenshot?CultHUDTextProbe::Captured:CultHUDTextProbe::Samples;
  if(Frame!=GFrameCounter) { Frame=GFrameCounter; Samples.Reset(); }
  if(Samples.Num()<128) {
   const float DPI=C->Canvas->GetDPIScale();
   const auto Measure=FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
   Samples.Add({Value,FVector2D(X,Y),T.DrawnSize,Measure->Measure(Value,Font(Size,Heading)),Measure->Measure(Value,Font(Size,Heading),DPI),FVector2D(C->SizeX,C->SizeY),Size,DPI,int32(Cache->FontCacheType)});
  }
 }
#endif
}
}
