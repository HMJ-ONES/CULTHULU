#include "CultPlayHUD.h"
#include "UI/CultUIStyle.h"
#include "CultSkillAttack.h"
#include "CultMatchDirector.h"
#include "CultMatchUnit.h"
#include "CultDestructible.h"
#include "CultUlhuPlayerPawn.h"
#include "CultFeralBeast.h"
#include "CultPopulation.h"
#include "CultWorldDirector.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "CanvasItem.h"
#include "Engine/Font.h"
#include "UObject/StrongObjectPtr.h"
#include "Styling/CoreStyle.h"
#include "Framework/Application/SlateApplication.h"
#include "Fonts/FontMeasure.h"
#include "Rendering/SlateRenderer.h"
#include "GameFramework/PlayerController.h"

void DrawCultCombatFeel(ACultPlayHUD*,ACultUlhuPlayerPawn*,UCanvas*);

// A restrained hero-shooter hierarchy: vital state left, ability state right,
// open center, faction details available on demand instead of covering play.
void DrawCultCombat(ACultPlayHUD* HUD,ACultUlhuPlayerPawn* P,UCanvas* C) {
 const float S=FMath::Clamp(FMath::Min(C->SizeX/1600.f,C->SizeY/900.f),.55f,2.f), W=C->SizeX,H=C->SizeY;
 auto* Match=ACultMatchDirector::Find(P->GetWorld());
 const auto Data=P->CombatHUD();
 const FLinearColor Ink=CultUI::Panel,White=CultUI::Bone,Blue=CultUI::Teal,Gold=CultUI::Gold,Grey=CultUI::Muted;
 auto Rect=[&](FLinearColor Color,float X,float Y,float A,float B){HUD->DrawRect(Color,X,Y,A,B);};
 auto FontText=[&](FString T,FLinearColor Color,float X,float Y,float Points,bool Strong) { CultUI::DrawText(C,T,Color,X,Y,Points*S,Strong); };
 auto Text=[&](FString T,FLinearColor Color,float X,float Y,float Size=1.f){FontText(T,Color,X,Y,14.f*Size,false);};
 const float X=42*S,Y=H-130*S;
 Rect(Ink,X,Y,320*S,90*S); Rect(Gold,X,Y,3*S,90*S); Rect(CultUI::Line,X+3*S,Y,317*S,1*S);
 FString Name=Data.Name; int Comma; if(Name.FindChar(',',Comma)) Name.LeftInline(Comma); Name=Name.ToUpper();
 Text(Name,White,X+16*S,Y+9*S,1.05f);
 FontText(FString::Printf(TEXT("%.0f"),FMath::Max(0.f,Data.Health)),White,X+16*S,Y+28*S,32,true);
 Text(FString::Printf(TEXT("/ %.0f"),Data.MaxHealth),Grey,X+115*S,Y+43*S,.9f);
 Rect(FLinearColor(.16f,.22f,.29f),X+16*S,Y+69*S,286*S,9*S); Rect(Data.Health/Data.MaxHealth<.25f?FLinearColor(1,.23f,.22f):White,X+16*S,Y+69*S,286*S*FMath::Clamp(Data.Health/Data.MaxHealth,0.f,1.f),9*S);
 Rect(FLinearColor(.1f,.17f,.22f),X,Y+96*S,320*S,4*S); Rect(Blue,X,Y+96*S,320*S*FMath::Clamp(Data.Stamina/Data.MaxStamina,0.f,1.f),4*S);
 Text(TEXT("STAMINA"),Grey,X,Y+104*S,.65f);
 const float Card=148*S,Gap=8*S,Start=W-42*S-3*Card-2*Gap,CardY=H-160*S;
 const auto Measure=FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
 const auto AbilityFont=CultUI::Font(14.f*.84f*S);
 auto FitName=[&](const FString& Name) {
  TArray<FString> Words; Name.ParseIntoArrayWS(Words); FString Line,Result; int Lines=1;
  const float Width=Card-16*S;
  for(const auto& Word:Words) {
   const FString Candidate=Line.IsEmpty()?Word:Line+TEXT(" ")+Word;
   if(!Line.IsEmpty()&&Measure->Measure(Candidate,AbilityFont).X>Width) {
    if(Lines==2) { Line+=TEXT(" ")+Word; continue; }
    Result=Line+TEXT("\n"); Line=Word; ++Lines;
   } else Line=Candidate;
  }
  if(Measure->Measure(Line,AbilityFont).X>Width) {
   while(!Line.IsEmpty()&&Measure->Measure(Line+TEXT("..."),AbilityFont).X>Width) Line.LeftChopInline(1);
   Line+=TEXT("...");
  }
  return Result+Line;
 };
 const TCHAR* Keys[]={TEXT("Q"),TEXT("F"),TEXT("R")};
 for(int I=0;I<3;++I) {
  const float A=Start+I*(Card+Gap); bool Ready=Data.Cooldowns[I]<=0;
  Rect(Ink,A,CardY,Card,120*S); Rect(Ready?Blue:CultUI::Line,A,CardY,Card,1*S); Rect(CultUI::Line,A,CardY+120*S,Card,1*S);
  FontText(Ready?Keys[I]:FString::Printf(TEXT("%.1f"),Data.Cooldowns[I]),Ready?White:Grey,A+14*S,CardY+12*S,26,true);
  Text(FitName(Data.AbilityNames[I]),White,A+8*S,CardY+51*S,.84f);
  FString State=Ready?TEXT("READY"):FString(Keys[I])+TEXT("  COOLDOWN");
  if(Match&&Match->Mode()==ECultPlayMode::Dotgoo) if(auto* F=Match->Fighter(P)) { if(I==2&&F->Level<6) State=TEXT("LEVEL 6"); else if(I<2&&F->Level<3&&F->FirstPick!=I) State=F->FirstPick<0?TEXT("PICK THIS"):TEXT("LEVEL 3"); }
  Text(State,Ready?Blue:Grey,A+8*S,CardY+100*S,.66f);
 }
 const float Mid=W*.5f, CY=H*.5f;
 for(int I:{-1,1}) { HUD->DrawLine(Mid+I*6*S,CY,Mid+I*13*S,CY,White,1.5f*S); HUD->DrawLine(Mid,CY+I*6*S,Mid,CY+I*13*S,White,1.5f*S); }
 if(P->HitConfirmTime()>0) for(int I:{-1,1}) for(int J:{-1,1}) HUD->DrawLine(Mid+I*13*S,CY+J*13*S,Mid+I*20*S,CY+J*20*S,Gold,2*S);
 Rect(Ink,Mid-64*S,H-77*S,128*S,32*S); Text(Data.DashCooldown>0?FString::Printf(TEXT("DASH  %.1fs"),Data.DashCooldown):Data.Stamina<18?TEXT("LOW STAMINA"):TEXT("TAP SHIFT DASH"),Data.DashCooldown>0||Data.Stamina<18?Grey:Blue,Mid-50*S,H-70*S,.83f);
 if(!Data.RmbHint.IsEmpty()) { Rect(Ink,Mid-250*S,H-148*S,500*S,28*S); Text(Data.RmbHint,Blue,Mid-238*S,H-141*S,.72f); }
 Text(TEXT("HOLD SHIFT  SPRINT"),Grey,Mid-60*S,H-38*S,.62f);
 Text(Match?Match->Mode()==ECultPlayMode::Domination?TEXT("DOMINATION"):TEXT("GREAT OLD ONES"):TEXT("THE DARK WILDS"),White,42*S,28*S,1.1f);
 if(!Match) if(auto* D=ACultWorldDirector::Find(P->GetWorld())) Text(FString::Printf(TEXT("POWER %.0f     CULT STRENGTH x%.2f"),D->FaithPower(),D->CombatMultiplier(0)),Blue,42*S,51*S,.72f);
 const FString Hints=Match?TEXT("TAB SCORE   G RECALL   ALT RALLY   V VIEW"):TEXT("B  BELIEFS   ALT  ORDERS   TAB  DETAILS   V  VIEW");
 const auto HintFont=CultUI::Font(14.f*.70f*S);
 Text(Hints,Grey,W-42*S-Measure->Measure(Hints,HintFont).X,32*S,.70f);
 DrawCultCombatFeel(HUD,P,C);
 const auto Pick=P->CameraAim();
 AActor* Aimed=Pick.GetActor(); if(!ACultSkillAttack::Alive(Aimed)) Aimed=P->CombatFocus();
 FString Target; float HP=0,Max=1;
 if(auto* Hero=Cast<ACultUlhuPlayerPawn>(Aimed)) { Target=Hero->CombatHUD().Name; HP=Hero->GetHealth(); Max=Hero->GetMaxHealth(); }
 if(auto* Unit=Cast<ACultMatchUnit>(Aimed)) { Target=Unit->Title(); if(Match&&Match->UnitProtected(Unit)) Target+=TEXT(" | PROTECTED"); HP=Unit->Health; Max=Unit->MaxHealth; }
 if(auto* B=Cast<ACultFeralBeast>(Aimed)) if(!B->IsDead()) { Target=B->Species(); Target.ReplaceInline(TEXT("_"),TEXT(" ")); HP=B->GetHealth(); Max=B->GetMaxHealth(); }
 if(auto* N=Cast<ACultPopulation>(Aimed)) { Target=ACultWorldDirector::CultName(N->GetFaction())+TEXT(" | ")+N->GetActivity(); HP=N->GetHealth(); Max=N->IsCultist()?100:1; }
 if(auto* B=Cast<ACultDestructible>(Aimed)) if(!B->IsDestroyed()) { Target=B->ActorHasTag(TEXT("CultTemple"))?TEXT("TEMPLE"):TEXT("STRUCTURE"); HP=B->GetHealth(); Max=B->GetMaxHealth(); }
 if(auto* B=Cast<ACultFeralBeast>(Aimed)) if(!B->IsDead()) {
  if(B->StaggerRemaining()>0) Text(TEXT("STAGGERED"),Blue,Mid-65*S,126*S,.85f);
  else if(B->RecoveryRemaining()>0) Text(TEXT("RECOVERING - ATTACK NOW"),Gold,Mid-100*S,126*S,.8f);
 }
 if(!Target.IsEmpty()) { const float TargetY=Match?CY-70*S:90*S; Text(Target.ToUpper(),White,Mid-120*S,TargetY,.85f); Rect(Ink,Mid-120*S,TargetY+23*S,240*S,6*S); Rect(Gold,Mid-120*S,TargetY+23*S,240*S*FMath::Clamp(HP/Max,0.f,1.f),6*S); }
 if(!P->GetNotice().IsEmpty()) {
  TArray<FString> Words; P->GetNotice().ParseIntoArrayWS(Words); FString Message,NoticeLine; const auto NoticeFont=CultUI::Font(14.f*.85f*S);
  for(const auto& Word:Words) {
   const FString Candidate=NoticeLine.IsEmpty()?Word:NoticeLine+TEXT(" ")+Word;
   if(!NoticeLine.IsEmpty()&&Measure->Measure(Candidate,NoticeFont).X>576*S) { Message+=NoticeLine+TEXT("\n"); NoticeLine=Word; } else NoticeLine=Candidate;
  }
  Message+=NoticeLine; const float NoticeHeight=float(Measure->Measure(Message,NoticeFont).Y)+16*S,NoticeY=H-170*S-NoticeHeight;
  Rect(Ink,Mid-300*S,NoticeY,600*S,NoticeHeight); Text(Message,Gold,Mid-288*S,NoticeY+8*S,.85f);
 }
 Text(TEXT("LMB / RMB  |  SPACE  |  E"),Grey,Start,CardY+126*S,.70f);
}
