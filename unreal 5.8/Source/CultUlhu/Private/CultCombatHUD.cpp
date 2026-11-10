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
#include "CultHUDProjection.h"
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
 const auto Area=P->CastAreaPreview();
 const auto* Fighter=Match&&Match->Mode()==ECultPlayMode::Dotgoo?Match->Fighter(P):nullptr;
 const bool Learning=Fighter&&Fighter->Level<3&&Fighter->FirstPick<0&&Match->Warmup()<=0&&!P->MatchDead()&&!HUD->Presentation().Expanded;
 const bool Guidance=!Match||HUD->Presentation().Guidance;
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
 if(Match&&Match->Mode()==ECultPlayMode::Dotgoo) if(auto* F=Match->Fighter(P)) { FontText(FString::Printf(TEXT("LV %d"),F->Level),Gold,X+260*S,Y+10*S,12,true); Rect(CultUI::Line,X,Y+122*S,320*S,2*S); Rect(Gold,X,Y+122*S,320*S*(F->XP%50)/50.f,2*S); }
 const float Card=148*S,Gap=8*S,Start=W-42*S-3*Card-2*Gap;
 const float CardPad=FMath::Max(6.f,8*S),RowGap=FMath::Max(3.f,6*S);
 const auto Measure=FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
 const auto AbilityFont=CultUI::Font(14.f*.84f*S);
 auto FitName=[&](const FString& Name) {
  TArray<FString> Words; Name.ParseIntoArrayWS(Words); FString Line,Result; int Lines=1;
  const float Width=Card-2*CardPad;
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
 const FLinearColor LearnGold(1.f,.83f,.22f);
 FString Names[3],States[3],KeyLabels[3]; bool Ready[3]; float NameHeight=0,KeyHeight=0,StateHeight=0;
 for(int I=0;I<3;++I) {
  Ready[I]=Data.Cooldowns[I]<=0; Names[I]=FitName(Data.AbilityNames[I]);
  KeyLabels[I]=Keys[I];
  States[I]=TEXT("");
  if(Fighter) { if(I==2&&Fighter->Level<6) States[I]=TEXT("LEVEL 6"); else if(I<2&&Fighter->Level<3&&Fighter->FirstPick!=I) States[I]=Fighter->FirstPick<0?TEXT("PICK THIS"):TEXT("LEVEL 3"); }
  if(Area.Slot==I) States[I]=TEXT("CONFIRM");
  if(!States[I].IsEmpty()) Ready[I]=false;
  NameHeight=FMath::Max(NameHeight,float(Measure->Measure(Names[I],AbilityFont).Y));
  KeyHeight=FMath::Max(KeyHeight,float(Measure->Measure(KeyLabels[I],CultUI::Font(20*S,true)).Y));
  if(!States[I].IsEmpty()) StateHeight=FMath::Max(StateHeight,float(Measure->Measure(States[I],CultUI::Font(14.f*.66f*S)).Y));
 }
 const FString Bindings=TEXT("LMB / RMB  |  SPACE  |  E");
 const auto BindSize=Measure->Measure(Bindings,CultUI::Font(14.f*.70f*S));
 const float BindY=H-BindSize.Y-FMath::Max(6.f,12*S),CardBottom=Guidance?BindY-FMath::Max(RowGap,CardPad+2*S):H-FMath::Max(10.f,16*S);
 const float CardHeight=FMath::Max(84*S,2*CardPad+KeyHeight+NameHeight+StateHeight+(StateHeight>0?2:1)*RowGap),CardY=CardBottom-CardHeight;
 const float KeyY=CardY+CardPad,NameY=KeyY+KeyHeight+RowGap,StateY=NameY+NameHeight+RowGap;
 HUD->CombatBottomTop_=CardY;
 if(Learning) {
  const FString Caption=TEXT("LEARN ONE ABILITY");
  const auto Size=Measure->Measure(Caption,CultUI::Font(25*S,true));
  const float CaptionX=Start+Card+Gap*.5f-Size.X*.5f, CaptionY=CardY-Size.Y-11*S;
  Rect(Ink,Start,CaptionY-3*S,2*Card+Gap,Size.Y+8*S);
  FontText(Caption,LearnGold,CaptionX,CaptionY,25,true);
  HUD->CombatBottomTop_=CaptionY-3*S;
 }
 for(int I=0;I<3;++I) {
  const float A=Start+I*(Card+Gap);
  const bool Choice=Learning&&I<2;
  const auto Accent=Choice?LearnGold:Area.Slot==I?Blue:Ready[I]?Blue:CultUI::Line;
  Rect(Ink,A,CardY,Card,CardHeight);
  if(Choice) {
   const float Glow=.17f+.07f*FMath::Sin(P->GetWorld()->GetTimeSeconds()*4.f);
   Rect(FLinearColor(LearnGold.R,LearnGold.G,LearnGold.B,Glow),A-2*S,CardY-2*S,Card+4*S,CardHeight+4*S);
  }
  Rect(Accent,A,CardY,Card,FMath::Max(1.f,Choice?3*S:S)); Rect(Choice?Accent:CultUI::Line,A,CardBottom-1,Card,1);
  FontText(KeyLabels[I],Choice?LearnGold:Ready[I]||Area.Slot==I?White:Grey,A+CardPad,KeyY,20,true);
  if(Data.Cooldowns[I]>0) { const FString CD=Data.Cooldowns[I]>=10?FString::Printf(TEXT("%.0f"),FMath::CeilToFloat(Data.Cooldowns[I])):FString::Printf(TEXT("%.1f"),Data.Cooldowns[I]); const float Width=Measure->Measure(CD,CultUI::Font(18*S,true)).X; FontText(CD,Grey,A+Card-CardPad-Width,KeyY,18,true); }
  Text(Names[I],White,A+CardPad,NameY,.84f);
  if(!States[I].IsEmpty()) Text(States[I],Gold,A+CardPad,StateY,.66f);
 }
 const float Mid=W*.5f, CY=H*.5f;
 const bool Previewing=Area.Slot!=INDEX_NONE&&!HUD->Presentation().Expanded;
 const FLinearColor AimColor=!Previewing?FLinearColor::White:Area.Valid?FLinearColor(.34f,1.f,.9f):FLinearColor(1.f,.55f,.29f);
 if(Previewing) {
  if(Area.Valid) {
   FVector2D Last=FVector2D::ZeroVector; bool LastVisible=false;
   for(int I=0;I<=48;++I) {
    const float Angle=I*(2.f*PI/48.f);
    const FVector Rim=Area.Center+FVector(FMath::Cos(Angle)*Area.Radius,FMath::Sin(Angle)*Area.Radius,0);
    FVector2D Screen;
    const bool Visible=CultHUDProjection::Project(C,Rim,Screen);
    if(Visible) {
     if(LastVisible) {
      HUD->DrawLine(Last.X,Last.Y,Screen.X,Screen.Y,FLinearColor(.01f,.05f,.06f,.92f),5*S);
      HUD->DrawLine(Last.X,Last.Y,Screen.X,Screen.Y,FLinearColor(.34f,1.f,.9f,.97f),2.3f*S);
     }
     Last=Screen;
    }
    LastVisible=Visible;
   }
  }
  FString Prompt=Area.OutOfRange?TEXT("OUT OF RANGE"):TEXT("AIM AT A SURFACE");
  if(Area.Valid) Prompt=Learning?FString::Printf(TEXT("PRESS %s AGAIN TO LEARN AND CAST"),Keys[Area.Slot]):
   FString::Printf(TEXT("PRESS %s AGAIN TO CAST"),Keys[Area.Slot]);
  const auto Size=Measure->Measure(Prompt,CultUI::Font(15*S,true));
  const float PromptX=Mid-Size.X*.5f, PromptY=CY+44*S;
  Rect(Ink,PromptX-7*S,PromptY-2*S,Size.X+14*S,Size.Y+4*S);
  FontText(Prompt,AimColor,PromptX,PromptY,15,true);
 }
 for(int I:{-1,1}) { HUD->DrawLine(Mid+I*6*S,CY,Mid+I*11*S,CY,AimColor,1.5f*S); HUD->DrawLine(Mid,CY+I*6*S,Mid,CY+I*11*S,AimColor,1.5f*S); }
 Rect(AimColor,Mid-S*.65f,CY-S*.65f,1.3f*S,1.3f*S);
 if(P->HitConfirmTime()>0) for(int I:{-1,1}) for(int J:{-1,1}) HUD->DrawLine(Mid+I*13*S,CY+J*13*S,Mid+I*20*S,CY+J*20*S,Gold,2*S);
 const FString Dash=Data.DashCooldown>0?FString::Printf(TEXT("DASH  %.1fs"),Data.DashCooldown):Data.Stamina<18?TEXT("LOW STAMINA"):(Guidance?TEXT("TAP SHIFT DASH"):TEXT("SHIFT / DASH"));
 const FString Sprint=Guidance?TEXT("HOLD SHIFT  SPRINT"):TEXT("");
 const auto DashSize=Measure->Measure(Dash,CultUI::Font(14.f*.83f*S)),SprintSize=Measure->Measure(Sprint,CultUI::Font(14.f*.62f*S));
 const float CueCenter=FMath::Min(Mid,(X+320*S+Start)*.5f),CuePad=FMath::Max(5.f,8*S);
 const float SprintY=H-SprintSize.Y-FMath::Max(6.f,12*S),DashY=Sprint.IsEmpty()?H-DashSize.Y-2*CuePad-FMath::Max(6.f,12*S):SprintY-RowGap-DashSize.Y-2*CuePad;
 const float CueWidth=FMath::Max(DashSize.X,SprintSize.X)+2*CuePad;
 Rect(Ink,CueCenter-CueWidth*.5f,DashY,CueWidth,SprintY+SprintSize.Y+CuePad-DashY);
 Text(Dash,Data.DashCooldown>0||Data.Stamina<18?Grey:Blue,CueCenter-DashSize.X*.5f,DashY+CuePad,.83f);
 if(!Sprint.IsEmpty()) Text(Sprint,Grey,CueCenter-SprintSize.X*.5f,SprintY,.62f);
 float MessageBottom=HUD->CombatBottomTop_-RowGap;
 if(!Data.RmbHint.IsEmpty()) {
  const auto RmbSize=Measure->Measure(Data.RmbHint,CultUI::Font(14.f*.72f*S)); const float RmbY=MessageBottom-RmbSize.Y-2*CardPad;
  Rect(Ink,Mid-RmbSize.X*.5f-CardPad,RmbY,RmbSize.X+2*CardPad,RmbSize.Y+2*CardPad); Text(Data.RmbHint,Blue,Mid-RmbSize.X*.5f,RmbY+CardPad,.72f);
  HUD->CombatBottomTop_=FMath::Min(HUD->CombatBottomTop_,RmbY); MessageBottom=RmbY-RowGap;
 }
 const FString ModeTitle=Match?Match->Mode()==ECultPlayMode::Domination?TEXT("DOMINATION"):TEXT("GREAT OLD ONES"):TEXT("THE DARK WILDS");
 FString PowerLine; if(!Match) if(auto* D=ACultWorldDirector::Find(P->GetWorld())) PowerLine=FString::Printf(TEXT("POWER %.0f     CULT STRENGTH x%.2f"),D->FaithPower(),D->CombatMultiplier(0));
 const auto TitleSize=Measure->Measure(ModeTitle,CultUI::Font(17*S,true));
 const auto PowerSize=PowerLine.IsEmpty()?FVector2D::ZeroVector:FVector2D(Measure->Measure(PowerLine,CultUI::Font(14.f*.72f*S)));
 const float HeaderX=42*S,HeaderY=20*S,Pad=FMath::Max(6.f,8*S),PowerY=HeaderY+TitleSize.Y+2*S;
 const float HeaderBottom=(PowerLine.IsEmpty()?HeaderY+TitleSize.Y:PowerY+PowerSize.Y)+2*S;
 const float HeaderRight=HeaderX+FMath::Max(TitleSize.X,PowerSize.X)+Pad;
 if(Guidance) Rect(Ink,HeaderX-Pad,HeaderY-Pad,HeaderRight-HeaderX+Pad,HeaderBottom-HeaderY+Pad);
 if(Guidance) Rect(Blue,HeaderX-Pad,HeaderY-Pad,FMath::Max(1.f,S),HeaderBottom-HeaderY+Pad);
 if(Guidance) FontText(ModeTitle,White,HeaderX,HeaderY,17,true);
 if(!PowerLine.IsEmpty()) Text(PowerLine,Blue,HeaderX,PowerY,.72f);
 FString Hints=Match?TEXT("TAB SCORE   G RECALL   ALT RALLY   V VIEW"):TEXT("B  BELIEFS   ALT  ORDERS   TAB  DETAILS   V  VIEW");
 const auto HintFont=CultUI::Font(14.f*.70f*S);
 float HintY=32*S;
 if(Match) {
  const float MatchScale=FMath::Clamp(FMath::Min(W/1600.f,H/900.f),.55f,2.f);
  if(W-42*S-Measure->Measure(Hints,HintFont).X-Pad<W*.5f+205*MatchScale+8*S) { Hints=TEXT("TAB SCORE   G RECALL\nALT RALLY   V VIEW"); HintY=12*S; }
 }
 const auto HintSize=Measure->Measure(Hints,HintFont); const float HintX=W-42*S-HintSize.X;
 if(!Match&&HintX-Pad<HeaderRight+8*S) HintY=HeaderBottom+Pad+4*S;
 // Opaque enough to remain legible against bright architecture, without a
 // full-width banner over the play view. ALT supplies its own contextual strip.
 const bool HintUnderSelection=!Match&&HintY>HeaderBottom&&HUD->GetOwningPlayerController()&&HUD->GetOwningPlayerController()->bShowMouseCursor;
 if(Guidance&&!HintUnderSelection) {
  Rect(Ink,HintX-Pad,HintY-Pad,HintSize.X+2*Pad,HintSize.Y+2*Pad);
  Rect(CultUI::Line,HintX-Pad,HintY+HintSize.Y+Pad-1,HintSize.X+2*Pad,1);
  Text(Hints,White,HintX,HintY,.70f);
 }
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
 if(!Target.IsEmpty()) {
  Target=Target.ToUpper(); const auto TargetFont=CultUI::Font(14.f*.85f*S);
  if(Measure->Measure(Target,TargetFont).X>240*S) { while(!Target.IsEmpty()&&Measure->Measure(Target+TEXT("..."),TargetFont).X>240*S) Target.LeftChopInline(1); Target+=TEXT("..."); }
  const float TargetY=Match?CY-70*S:90*S; Text(Target.ToUpper(),White,Mid-120*S,TargetY,.85f); Rect(Ink,Mid-120*S,TargetY+23*S,240*S,6*S); Rect(Gold,Mid-120*S,TargetY+23*S,240*S*FMath::Clamp(HP/Max,0.f,1.f),6*S); }
 if(!P->GetNotice().IsEmpty()) {
  TArray<FString> Words; P->GetNotice().ParseIntoArrayWS(Words); FString Message,NoticeLine; const auto NoticeFont=CultUI::Font(14.f*.85f*S);
  for(const auto& Word:Words) {
   const FString Candidate=NoticeLine.IsEmpty()?Word:NoticeLine+TEXT(" ")+Word;
   if(!NoticeLine.IsEmpty()&&Measure->Measure(Candidate,NoticeFont).X>576*S) { Message+=NoticeLine+TEXT("\n"); NoticeLine=Word; } else NoticeLine=Candidate;
  }
  Message+=NoticeLine; const float NoticeHeight=float(Measure->Measure(Message,NoticeFont).Y)+16*S,NoticeY=MessageBottom-NoticeHeight;
  HUD->CombatBottomTop_=FMath::Min(HUD->CombatBottomTop_,NoticeY);
  Rect(Ink,Mid-300*S,NoticeY,600*S,NoticeHeight); Text(Message,Gold,Mid-288*S,NoticeY+8*S,.85f);
 }
 if(Guidance) { Rect(Ink,Start-CardPad,BindY-CardPad,BindSize.X+2*CardPad,BindSize.Y+2*CardPad); Text(Bindings,Grey,Start,BindY,.70f); }
}
