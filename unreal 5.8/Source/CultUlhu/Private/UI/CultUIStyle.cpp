#include "UI/CultUIStyle.h"
#include "Components/Button.h"
#include "Fonts/CompositeFont.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Misc/Paths.h"
#include "Engine/Canvas.h"
#include "Engine/Font.h"
#include "CanvasItem.h"
#include "UObject/StrongObjectPtr.h"
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
}
}
