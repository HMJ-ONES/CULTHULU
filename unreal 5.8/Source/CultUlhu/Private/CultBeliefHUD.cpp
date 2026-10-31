#include "CultPlayHUD.h"
#include "CultHUDSpace.h"
#include "UI/CultUIStyle.h"
#include "Framework/Application/SlateApplication.h"
#include "Fonts/FontMeasure.h"
#include "Rendering/SlateRenderer.h"
#include "CultMatchDirector.h"


#include "CultWorldDirector.h"
#include "CultUlhuPlayerPawn.h"
#include "CultPopulation.h"
#include "CultBeliefCatalog.h"
#include "beliefs/BeliefSystem.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "GameFramework/PlayerController.h"

void ACultPlayHUD::ToggleFaith()
{
 if(ACultMatchDirector::IsMatch(GetWorld())) return;
 bFaith_=!bFaith_; CloseCommands(); InspectionHover_.Reset();
 if(auto* PC=GetOwningPlayerController()) {
  PC->bShowMouseCursor=bFaith_||PC->IsInputKeyDown(EKeys::LeftAlt);
  if(PC->bShowMouseCursor) { FInputModeGameAndUI Mode; Mode.SetHideCursorDuringCapture(false); PC->SetInputMode(Mode); }
  else { FInputModeGameOnly Mode; Mode.SetConsumeCaptureMouseDown(false); PC->SetInputMode(Mode); }
 }
}
void ACultPlayHUD::FaithClick(float X,float Y)
{
 if(ACultMatchDirector::IsMatch(GetWorld())) return;
 auto* D=ACultWorldDirector::Find(GetWorld()); auto* P=Cast<ACultUlhuPlayerPawn>(GetOwningPawn()); if(!D||!P) return;
 for(int I=0;I<FaithButtons_.Num();++I) if(FaithButtons_[I].IsInside(FVector2D(X,Y))) {
  FString Key=FaithActions_[I];
  if(Key.StartsWith(TEXT("select:"))) { SelectedBelief_=FCString::Atoi(*Key.Mid(7)); FaithScroll_=0; bFaithOutcomes_=false; bFaithDetails_=false; return; }
  if(Key==TEXT("rules")||Key==TEXT("outcomes")) { bFaithOutcomes_=Key==TEXT("outcomes"); bFaithDetails_=Key==TEXT("rules")?!bFaithDetails_:false; FaithScroll_=0; return; }
  if(Key==TEXT("close")) { ToggleFaith(); return; }
  if(GetWorld()->GetNetMode()==NM_Client) { P->Notify(TEXT("The host manages shared cult beliefs and entity rites. You can still command cultists with ALT.")); return; }
  if(Key.StartsWith(TEXT("adopt:"))) { const int Slot=FCString::Atoi(*Key.Mid(6)); auto& Active=D->Beliefs().active(); const int Out=Slot>=0&&Slot<int(Active.size())?int(Active[Slot]):-1; P->Notify(D->RequestBelief(SelectedBelief_,Out)?TEXT("Belief adoption requested; watch the follower adoption timer."):TEXT("Cannot adopt: already active/pending, or all three slots are reserved. Choose a replacement.")); return; }
  return;
 }
}
void ACultPlayHUD::DrawFaith()
{
 if(ACultMatchDirector::IsMatch(GetWorld())) return;
 auto* D=ACultWorldDirector::Find(GetWorld()); auto* P=Cast<ACultUlhuPlayerPawn>(GetOwningPawn()); auto* PC=GetOwningPlayerController(); if(!D||!P||!PC) return;
 const float Scale=FMath::Clamp(FMath::Min(Canvas->SizeX/1600.f,Canvas->SizeY/900.f),1.f,2.f);
 const float W=Canvas->SizeX/Scale,H=Canvas->SizeY/Scale,M=FMath::Clamp(W*.02f,14.f,28.f),Inner=W-2*M;
 auto Rect=[&](FLinearColor C,float X,float Y,float RW,float RH) { DrawRect(C,X*Scale,Y*Scale,RW*Scale,RH*Scale); };
 Rect(CultUI::Background,M,M,Inner,H-2*M);
 auto Text=[&](const FString& V,float X,float Y,float Size,FLinearColor Color,bool Head=false) { CultUI::DrawText(Canvas,V,Color,X*Scale,Y*Scale,Size*Scale,Head); };
 auto Measure=FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
 auto Fit=[&](FString V,float Width) { if(Measure->Measure(V,CultUI::Font(10*Scale)).X<=Width*Scale) return V; while(!V.IsEmpty()&&Measure->Measure(V+TEXT("..."),CultUI::Font(10*Scale)).X>Width*Scale) V.LeftChopInline(1); return V+TEXT("..."); };
 FaithButtons_.Reset(); FaithActions_.Reset(); FaithGaugeBeliefs_.Reset(); FVector2D Mouse(-1,-1); CultHUDSpace::Pointer(PC,Mouse); const float MX=Mouse.X/Scale,MY=Mouse.Y/Scale;
 auto Button=[&](FString Key,FString Label,float X,float Y,float BW,float BH,bool Selected=false,bool Inactive=false) {
  const FBox2D Box(FVector2D(X,Y),FVector2D(X+BW,Y+BH)); FaithButtons_.Add(FBox2D(Box.Min*Scale,Box.Max*Scale)); FaithActions_.Add(Key); const bool Hover=Box.IsInside(FVector2D(MX,MY));
  Rect(Selected||Hover?FLinearColor(.04f,.115f,.108f):CultUI::Panel,X,Y,BW,BH);
  Rect(Hover?CultUI::Gold:Selected?CultUI::Teal:CultUI::Line,X,Y,BW,Selected?2:1);
  Text(Fit(Label,BW-14),X+7,Y+3,10,Selected?CultUI::Teal:Inactive?CultUI::Muted:CultUI::Bone);
 };
 Text(TEXT("BELIEFS"),M+10,M+1,22,CultUI::Gold,true);
 Button(TEXT("rules"),bFaithDetails_?TEXT("LESS"):TEXT("RULES"),W-M-246,M+2,66,26,bFaithDetails_);
 Button(TEXT("outcomes"),TEXT("OUTCOMES"),W-M-174,M+2,94,26,bFaithOutcomes_);
 Button(TEXT("close"),TEXT("CLOSE"),W-M-74,M+2,64,26);
 Text(TEXT("CLICK TO CHOOSE  /  B CLOSE  /  ALT COMMANDS FOLLOWERS"),M+10,M+35,10,CultUI::Muted);
 Text(TEXT("Beliefs take time to change."),M+10,M+54,10,CultUI::Bone);
 const float BodyY=M+80,FooterY=H-M-36,BodyH=FMath::Max(90.f,FooterY-BodyY-10),LeftW=FMath::Clamp(Inner*.38f,220.f,340.f),Gap=6;
 const int Cols=BodyH<300?2:1,Rows=12/Cols; const float CellW=(LeftW-(Cols-1)*Gap)/Cols,RowH=FMath::Min(36.f,BodyH/Rows);
 for(int B=0;B<12;++B) {
  auto Belief=static_cast<cultulhu::Belief>(B); const bool Active=D->Beliefs().isActive(Belief); const float X=M+(B%Cols)*(CellW+Gap),Y=BodyY+(B/Cols)*RowH;
  FString Name=(Active?TEXT("* "):TEXT(""))+FString(UTF8_TO_TCHAR(cultulhu::beliefName(Belief)));
  const double Pending=D->Beliefs().adoptionRemaining(Belief);
  if(Pending>=0) Name+=FString::Printf(TEXT("  %.0fs"),Pending);
  else if(Cols==1) Name+=Active?FString::Printf(TEXT("  %.0f%%"),D->BeliefExertion(B)):TEXT("  / inactive");
  Button(FString::Printf(TEXT("select:%d"),B),Name,X,Y,CellW,RowH-3,B==SelectedBelief_&&!bFaithOutcomes_,!Active&&Pending<0);
  if(Active) { FaithGaugeBeliefs_.Add(B); Rect(CultUI::Teal,X,Y+RowH-2,CellW*FMath::Clamp(D->BeliefExertion(B)/100,0.f,1.f),1); }

 }
 const float DetailX=M+LeftW+14,DetailW=W-M-DetailX;
 Rect(CultUI::Panel,DetailX,BodyY,DetailW,BodyH);
 TArray<FString> Paras,Lines;
 if(bFaithOutcomes_) { Paras.Add(TEXT("RECENT REAL OUTCOMES")); Paras.Append(D->FaithJournal()); }
 else if(bFaithDetails_) D->BeliefReport(SelectedBelief_).ParseIntoArrayLines(Paras,false);
 else {
  static const TCHAR* Benefits[]={
   TEXT("Gain strength by torturing captives."),TEXT("Gain strength through fear, raids and destruction."),
   TEXT("Breed captured creatures into hybrids."),TEXT("Adopt beliefs faster. Gain strength through destruction."),
   TEXT("Gain strength through completed sacrifices."),TEXT("Convert civilians. Unlock mass conversion."),
   TEXT("Gain strength by killing enemy cultists at war."),TEXT("Stronger healing. Surviving structures repair over time."),
   TEXT("Capture civilians with single-use traps and spread rumors."),TEXT("Stronger sorcerers and resistance to crowd control. Gain strength through necromancy."),
   TEXT("Stronger melee. Gain strength by killing civilians."),TEXT("Gain strength from resting followers and dream conversions.")};
  static const TCHAR* Costs[]={
   TEXT("Each captive rewards once. Killing your own cultists costs power."),TEXT("Fear fades during inactivity."),
   TEXT("Requires two captured beasts at a temple. Offspring may turn feral."),TEXT("Discipline and imprisonment conflict with Chaos."),
   TEXT("A 30-second rite consumes a real victim. Interruption loses progress."),TEXT("Mass conversion has a long cooldown. Rival conversion is a risk."),
   TEXT("Damage to your own base costs power."),TEXT("Other spells become weaker."),
   TEXT("Traps must capture real civilians before rewards apply."),TEXT("Necromancy takes 45 seconds. Damage can interrupt the rite."),
   TEXT("Power drains after prolonged inactivity."),TEXT("Nightmares can drive followers mad.")};
  Paras.Add(UTF8_TO_TCHAR(cultulhu::beliefName(static_cast<cultulhu::Belief>(SelectedBelief_))));
  Paras.Add(TEXT("")); Paras.Add(Benefits[SelectedBelief_]); Paras.Add(TEXT("")); Paras.Add(Costs[SelectedBelief_]);
  if(!D->Beliefs().isActive(static_cast<cultulhu::Belief>(SelectedBelief_))) { Paras.Add(TEXT("")); Paras.Add(TEXT("INACTIVE / choose a slot below to adopt.")); }
 }

 for(const auto& Para:Paras) {
  TArray<FString> Words; Para.ParseIntoArrayWS(Words); FString Line;
  for(const auto& Word:Words) { const FString Candidate=Line.IsEmpty()?Word:Line+TEXT(" ")+Word; if(!Line.IsEmpty()&&Measure->Measure(Candidate,CultUI::Font(11*Scale)).X>(DetailW-20)*Scale) { Lines.Add(Line); Line=Word; } else Line=Candidate; }
  Lines.Add(Line);
 }
 const int Visible=FMath::Max(1,FMath::FloorToInt((BodyH-32)/18)); FaithScroll_=FMath::Clamp(FaithScroll_,0,FMath::Max(0,Lines.Num()-Visible));
 for(int I=0;I<Visible&&I+FaithScroll_<Lines.Num();++I) Text(Lines[I+FaithScroll_],DetailX+10,BodyY+7+I*18,11,I==0?CultUI::Teal:CultUI::Bone);
 if(bFaithDetails_||bFaithOutcomes_) Text(FString::Printf(TEXT("%d-%d / %d   |   WHEEL TO SCROLL"),FaithScroll_+1,FMath::Min(Lines.Num(),FaithScroll_+Visible),Lines.Num()),DetailX+10,BodyY+BodyH-20,10,CultUI::Muted);
 const auto& Active=D->Beliefs().active();
 const bool Chosen=D->Beliefs().isActive(static_cast<cultulhu::Belief>(SelectedBelief_))||D->Beliefs().adoptionRemaining(static_cast<cultulhu::Belief>(SelectedBelief_))>=0;
 if(Chosen) Text(TEXT("Active or changing / select another belief to replace a slot."),M+8,FooterY+4,10,CultUI::Muted);
 else {
  const int Count=int(Active.size())+(Active.size()<3?1:0); const float AdoptW=(Inner-(Count-1)*Gap)/FMath::Max(1,Count); int Slot=0;
  if(Active.size()<3) { Button(TEXT("adopt:-1"),TEXT("Adopt into free slot"),M,FooterY,AdoptW,28); ++Slot; }
  for(int I=0;I<int(Active.size());++I) Button(FString::Printf(TEXT("adopt:%d"),I),FString(TEXT("Replace "))+UTF8_TO_TCHAR(cultulhu::beliefName(Active[I])),M+(Slot+I)*(AdoptW+Gap),FooterY,AdoptW,28);
 }
}
