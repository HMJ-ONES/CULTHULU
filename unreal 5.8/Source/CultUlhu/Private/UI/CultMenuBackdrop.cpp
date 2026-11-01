#include "UI/CultMenuBackdrop.h"
#include "UI/CultUIStyle.h"
#include "Widgets/SLeafWidget.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
class SCultMenuBackdrop:public SLeafWidget {
public:
 SLATE_BEGIN_ARGS(SCultMenuBackdrop) {} SLATE_END_ARGS()
 void Construct(const FArguments&) { SetVisibility(EVisibility::HitTestInvisible); }
 FVector2D ComputeDesiredSize(float) const override { return FVector2D(1600,900); }
 int32 OnPaint(const FPaintArgs&,const FGeometry& G,const FSlateRect&,FSlateWindowElementList& Out,int32 Layer,const FWidgetStyle&,bool) const override {
  const FVector2D D=G.GetLocalSize(); const float S=FMath::Min(D.X/1600.,D.Y/900.);
  const auto* White=FCoreStyle::Get().GetBrush("WhiteBrush");
  FSlateDrawElement::MakeBox(Out,Layer,G.ToPaintGeometry(),White,ESlateDrawEffect::None,CultUI::Background);
  const FVector2D C(D.X*.77,D.Y*.49); const float R=265*S;
  for(int I=18;I>0;--I) { float A=I/18.f; FSlateDrawElement::MakeBox(Out,Layer+1,G.ToPaintGeometry(FVector2D(R*2*A,R*2*A),FSlateLayoutTransform(C-FVector2D(R*A))),White,ESlateDrawEffect::None,FLinearColor(.025f,.09f,.08f,.017f)); }
  auto Lines=[&](const TArray<FVector2D>& P,FLinearColor Col,float Width=1) { FSlateDrawElement::MakeLines(Out,Layer+2,G.ToPaintGeometry(),P,ESlateDrawEffect::None,Col,true,Width*S); };
  for(int Ring=0;Ring<3;++Ring) for(int Arc=0;Arc<3;++Arc) { TArray<FVector2D> P; for(int I=0;I<=48;++I) { float A=FMath::DegreesToRadians(Arc*120+8+I*104.f/48); P.Add(C+FVector2D(FMath::Cos(A),FMath::Sin(A))*R*(1-Ring*.17)); } Lines(P,FLinearColor(.16f,.34f,.3f,.28f-Ring*.05f)); }
  for(int I=0;I<12;++I) { float A=I*PI/6; FVector2D V(FMath::Cos(A),FMath::Sin(A)); Lines({C+V*R*1.04,C+V*R*1.08},FLinearColor(.55f,.45f,.28f,.4f)); }
  TArray<FVector2D> Sigil; for(int I=0;I<=5;++I) { float A=I*4*PI/5-PI/2; Sigil.Add(C+FVector2D(FMath::Cos(A),FMath::Sin(A))*R*.58); } Lines(Sigil,FLinearColor(.2f,.48f,.4f,.38f));
  Lines({C+FVector2D(-62,-4)*S,C+FVector2D(0,-31)*S,C+FVector2D(62,-4)*S,C+FVector2D(0,24)*S,C+FVector2D(-62,-4)*S},FLinearColor(.65f,.53f,.32f,.55f));
  Lines({C+FVector2D(0,-18)*S,C+FVector2D(0,12)*S},CultUI::Teal,2);
  Lines({FVector2D(54*S,D.Y-53*S),FVector2D(D.X-54*S,D.Y-53*S)},FLinearColor(.15f,.25f,.24f,.45f));
  return Layer+2;
 }
};
TSharedRef<SWidget> UCultMenuBackdrop::RebuildWidget() { return SNew(SCultMenuBackdrop); }
