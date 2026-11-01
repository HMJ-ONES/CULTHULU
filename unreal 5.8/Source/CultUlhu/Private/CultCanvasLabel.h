#pragma once
#include "Engine/Canvas.h"
#include "UI/CultUIStyle.h"
inline void CultCanvasLabel(UCanvas* Canvas,const FString& Text,FLinearColor Color,float X,float Y,float Points) { CultUI::DrawText(Canvas,Text,Color,X,Y,Points,Points>=24); }
