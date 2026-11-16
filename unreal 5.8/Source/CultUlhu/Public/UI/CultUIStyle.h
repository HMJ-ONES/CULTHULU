#pragma once
#include "CoreMinimal.h"
#include "Fonts/SlateFontInfo.h"
#include "Components/TextBlock.h"
class UButton;
class UCanvas;
namespace CultUI {
inline const FLinearColor Background(.009f,.016f,.022f,1),Panel(.018f,.032f,.039f,.96f),Line(.13f,.23f,.24f,1),
 Bone(.9f,.92f,.87f,1),Muted(.47f,.59f,.59f,1),Teal(.24f,.73f,.64f,1),Gold(.77f,.63f,.4f,1),Danger(.91f,.32f,.25f,1);
FSlateFontInfo Font(float Size,bool Heading=false,bool Medium=false);
UTextBlock* Label(UObject* Outer,const FString& Value,int Size,FLinearColor Color=Bone,bool Heading=false,ETextJustify::Type Align=ETextJustify::Left);
void Button(UButton* Widget,bool Primary=false,bool Selected=false);
void DrawText(UCanvas* Canvas,const FString& Value,FLinearColor Color,float X,float Y,float Size,bool Heading=false);
}
