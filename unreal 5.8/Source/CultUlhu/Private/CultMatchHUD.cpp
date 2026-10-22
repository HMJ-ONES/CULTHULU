#include "CultMatchDirector.h"
#include "CultHUDSpace.h"
#include "CultMatchState.h"
#include "CultMobaRoute.h"
#include "CultJungleCamp.h"
#include "CultFeralBeast.h"
#include "CultPlayHUD.h"
#include "CultCanvasLabel.h"
#include "UI/CultUIStyle.h"
#include "CultUlhuPlayerPawn.h"
#include "CultMatchUnit.h"
#include "Engine/Canvas.h"
#include "GameFramework/PlayerController.h"
#include "EngineUtils.h"
#include "Engine/StaticMeshActor.h"
#include "Components/StaticMeshComponent.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/SlateRenderer.h"
#include "Fonts/FontMeasure.h"

void ACultMatchDirector::Draw(ACultPlayHUD* HUD,UCanvas* C,ACultUlhuPlayerPawn* P) {
 if(!State_||!C||!P) return;
 const float S=FMath::Clamp(FMath::Min(C->SizeX/1600.f,C->SizeY/900.f),.65f,2.f),W=C->SizeX,H=C->SizeY,M=W*.5f;
 const FLinearColor Blue=CultUI::Teal,Orange=CultUI::Danger,White=CultUI::Bone,Gold=CultUI::Gold,Ink=CultUI::Panel,Muted=CultUI::Muted;
 auto Text=[&](const FString& T,float X,float Y,float Size,FLinearColor Col) { CultCanvasLabel(C,T,Col,X,Y,Size*S); };
 auto Panel=[&](float X,float Y,float A,float B) { HUD->DrawRect(Ink,X,Y,A,B); };
 const auto Measure=FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
 auto Fit=[&](FString T,float Width,float Points) {
  const auto Font=CultUI::Font(Points*S); if(Measure->Measure(T,Font).X<=Width) return T;
  while(!T.IsEmpty()&&Measure->Measure(T+TEXT("..."),Font).X>Width) T.LeftChopInline(1);
  return T+TEXT("...");
 };
 auto Wrap=[&](const FString& T,float Width,float Points) {
  const auto Font=CultUI::Font(Points*S); TArray<FString> Words; T.ParseIntoArrayWS(Words); FString Result,Line;
  for(const auto& Word:Words) {
   const FString Candidate=Line.IsEmpty()?Word:Line+TEXT(" ")+Word;
   if(!Line.IsEmpty()&&Measure->Measure(Candidate,Font).X>Width) { Result+=Line+TEXT("\n"); Line=Fit(Word,Width,Points); }
   else Line=Fit(Candidate,Width,Points);
  }
  return Result+Line;
 };
 // Reserve the actual ability/RMB/notice bounds, including on small displays.
 const float Map=FMath::Clamp(HUD->CombatContentTop()-88*S-48*S,80*S,205*S),MX=W-Map-24*S,MY=88*S;
 Panel(M-205*S,12*S,410*S,64*S);
 float Left=State_->Capture?Score(0):CoreHealth(0),Right=State_->Capture?Score(1):CoreHealth(1);
 Text(FString::Printf(TEXT("%.0f"),Left),M-175*S,18*S,27,Blue); Text(FString::Printf(TEXT("%.0f"),Right),M+110*S,18*S,27,Orange);
 float Limit=State_->Capture?600:1200; int Remaining=FMath::CeilToInt(FMath::Max(0.f,Limit-Elapsed()));
 Text(Overtime()?TEXT("OVERTIME"):FString::Printf(TEXT("%02d:%02d"),Remaining/60,Remaining%60),M-42*S,17*S,20,White);
 FString Status=State_->Capture?Contested()?TEXT("CONTESTED - SCORING PAUSED"):Holder()==0?TEXT("YOUR SEAL  +1 / SECOND"):Holder()==1?TEXT("ENEMY SEAL  +1 / SECOND"):TEXT("CAPTURE THE CENTRAL SEAL"):FString::Printf(TEXT("TOWERS  %d : %d  |  WAVE IN %.0fs"),TowersAlive(0),TowersAlive(1),FMath::CeilToFloat(State_->NextWave));
 Text(Status,M-174*S,51*S,12,Contested()?Gold:Muted);
 const float CopyWidth=FMath::Min(740*S,MX-58*S),CopyX=34*S,Pad=8*S;
 const FString Help=Wrap(Hint(P),CopyWidth,12);
 const auto HelpSize=Measure->Measure(Help,CultUI::Font(12*S)); const float HelpHeight=HelpSize.Y;
 float CopyBottom=94*S+HelpHeight+Pad;
 Panel(24*S,87*S,HelpSize.X+20*S,CopyBottom-87*S); Text(Help,CopyX,94*S,12,Gold);
 if(auto* F=Fighter(P)) {
  if(State_->Moba) {
   FString Progress=FString::Printf(TEXT("LEVEL %d   XP %d / 50   POWER x%.2f   INVOKES %d"),F->Level,F->XP%50,DamageMultiplier(P),F->Invokes);
   if(H<500) Progress+=TEXT("  |  ")+RelicStatus();
   Progress=Fit(Progress,CopyWidth,13);
   const auto ProgressSize=Measure->Measure(Progress,CultUI::Font(13*S)); Panel(24*S,CopyBottom+Pad-3*S,ProgressSize.X+20*S,ProgressSize.Y+6*S);
   Text(Progress,CopyX,CopyBottom+Pad,13,Blue); CopyBottom+=Measure->Measure(Progress,CultUI::Font(13*S)).Y+Pad;
  }
  if(F->Respawn>0) { Panel(M-190*S,H*.45f,380*S,75*S); Text(TEXT("VESSEL DEFEATED"),M-125*S,H*.45f+10*S,22,Orange); Text(FString::Printf(TEXT("Returning to your temple in %.0f"),FMath::CeilToFloat(F->Respawn)),M-150*S,H*.45f+42*S,16,White); }
 }
 if(State_->Moba&&H>=500) {
  const FString Relics=Fit(RelicStatus(),CopyWidth,11); const auto RelicSize=Measure->Measure(Relics,CultUI::Font(11*S));
  Panel(24*S,CopyBottom+Pad-3*S,RelicSize.X+20*S,RelicSize.Y+6*S); Text(Relics,CopyX,CopyBottom+Pad,11,Muted);
  CopyBottom+=Measure->Measure(Relics,CultUI::Font(11*S)).Y+Pad;
 }
 FBox2D ObjectiveAlert(ForceInit);
 if(!Announcement().IsEmpty()&&!IsOver()) {
  const float MaxWidth=FMath::Min(680*S,MX-48*S),AY=FMath::Max(155*S,CopyBottom+(H<500?Pad:2*Pad));
  const FString Alert=Wrap(Announcement(),MaxWidth-2*Pad,12); const auto AlertSize=Measure->Measure(Alert,CultUI::Font(12*S));
  const float Width=AlertSize.X+2*Pad,Height=AlertSize.Y+2*Pad,AX=FMath::Clamp(M-Width*.5f,24*S,MX-24*S-Width);
  Panel(AX,AY,Width,Height); HUD->DrawRect(Gold,AX,AY,2*S,Height); Text(Alert,AX+Pad,AY+Pad,12,White);
  ObjectiveAlert=FBox2D(FVector2D(AX,AY),FVector2D(AX+Width,AY+Height));
 }
 // Always-visible tactical map. The same coordinates drive movement and objectives.
 Panel(MX-8*S,MY-8*S,Map+16*S,Map+40*S);
 auto Point=[&](FVector V) { return FVector2D(MX+(V.X/(State_->Moba?80000.f:64000.f)+.5f)*Map,MY+(.5f-V.Y/(State_->Moba?46000.f:60000.f))*Map); };
 // Draw the physical layout, not a separate painted approximation. Raised decks
 // and solid cover remain legible behind objective and team markers.
 for(TActorIterator<AStaticMeshActor> It(GetWorld());It;++It) {
  if(!It->ActorHasTag(TEXT("MatchArena"))||It->ActorHasTag(TEXT("CultMatchFloor"))) continue;
  auto* Mesh=It->GetStaticMeshComponent(); if(Mesh->GetCollisionEnabled()==ECollisionEnabled::NoCollision) continue;
  const auto Bounds=Mesh->Bounds.GetBox(); if(Bounds.Max.Z<0) continue; auto A=Point(Bounds.Min),B=Point(Bounds.Max);
  const float X=FMath::Clamp(float(A.X),MX,MX+Map),Y=FMath::Clamp(float(B.Y),MY,MY+Map);
  const float MapRight=FMath::Clamp(float(B.X),MX,MX+Map),MapBottom=FMath::Clamp(float(A.Y),MY,MY+Map);
  HUD->DrawRect(It->ActorHasTag(TEXT("CultMatchWalkSurface"))?FLinearColor(.19f,.29f,.31f):FLinearColor(.10f,.16f,.18f),X,Y,MapRight-X,MapBottom-Y);
 }
 if(State_->Moba) for(int Lane=0;Lane<3;++Lane) {
  for(int I=0;I<FCultMobaRoute::Segments;++I) {
   const auto A=Point(FCultMobaRoute::Knot(Lane,I)),B=Point(FCultMobaRoute::Knot(Lane,I+1));
   HUD->DrawLine(A.X,A.Y,B.X,B.Y,FLinearColor(.19f,.29f,.32f),3*S);
  }
 } else { auto A=Point(Objective()); const auto E=CaptureHalfExtent(); const float RX=E.X/64000.f*Map,RY=E.Y/60000.f*Map; HUD->DrawRect(Holder()==0?Blue:Holder()==1?Orange:Gold,A.X-RX,A.Y-RY,RX*2,RY*2); }
 if(!State_->Moba) {
  // Dashed underground line and portal brackets distinguish this from the seal.
  const FLinearColor Tunnel(.25f,.65f,.68f);
  for(int Y=-23000;Y<24000;Y+=2200) { const auto A=Point(FVector(0,Y,0)),B=Point(FVector(0,Y+1000,0)); HUD->DrawLine(A.X,A.Y,B.X,B.Y,Tunnel,1.2f*S); }
  for(int Side:{-1,1}) for(int X=-14000;X<14000;X+=2200) { const auto A=Point(FVector(X,Side*12500,0)),B=Point(FVector(X+1000,Side*12500,0)); HUD->DrawLine(A.X,A.Y,B.X,B.Y,Tunnel,1.2f*S); }
  for(int Side:{-1,1}) { const auto A=Point(FVector(-3500,Side*24000,0)),B=Point(FVector(3500,Side*24000,0)); HUD->DrawLine(A.X,A.Y,B.X,B.Y,Tunnel,2*S); }
 }
 for(auto& U:State_->Units) if(U.IsValid()&&U->Health>0) { auto A=Point(U->GetActorLocation()); const float Size=U->IsStructure()?6*S:2*S; HUD->DrawRect(U->Team==0?Blue:Orange,A.X-Size*.5f,A.Y-Size*.5f,Size,Size); }
 for(const auto& F:State_->Fighters) if(F.Pawn.IsValid()&&F.Respawn<=0) { auto A=Point(F.Pawn->FeetLocation()); const float Size=F.Pawn.Get()==P?8*S:5*S; HUD->DrawRect(F.Pawn.Get()==P?White:F.Team==0?Blue:Orange,A.X-Size*.5f,A.Y-Size*.5f,Size,Size); }
 if(State_->Moba) for(const auto& R:State_->Relics) if(R.Claim<0&&State_->Time>=R.Spawn) { auto A=Point(R.Point); HUD->DrawRect(Gold,A.X-3*S,A.Y-3*S,6*S,6*S); }
 if(State_->Moba) for(TActorIterator<ACultJungleCamp> It(GetWorld());It;++It) {
  const auto A=Point(It->GetActorLocation()); const bool Alive=It->Monster()&&!It->Monster()->IsDead();
  const auto Color=It->Resetting()?Orange:Alive?Gold:Muted; const float R=5*S;
  HUD->DrawLine(A.X-R,A.Y,A.X,A.Y-R,Color,1.3f*S); HUD->DrawLine(A.X,A.Y-R,A.X+R,A.Y,Color,1.3f*S);
  HUD->DrawLine(A.X+R,A.Y,A.X,A.Y+R,Color,1.3f*S); HUD->DrawLine(A.X,A.Y+R,A.X-R,A.Y,Color,1.3f*S);
  if(!Alive) Text(FString::Printf(TEXT("%.0f"),FMath::CeilToFloat(It->RespawnRemaining())),A.X+R,A.Y,8,Muted);
 }
 const float FeedRow=Measure->Measure(TEXT("YOU > ENEMY"),CultUI::Font(12*S)).Y+12*S,FeedY=MY+Map+44*S;
 const int Rows=FMath::Clamp(FMath::FloorToInt((HUD->CombatContentTop()-12*S-FeedY)/FeedRow),0,H<500?1:3);
 const bool Compact=Rows==0&&!State_->Eliminations.IsEmpty();
 if(!Compact) { Text(TEXT("ALLIES"),MX,MY+Map+8*S,11,Blue); Text(TEXT("ENEMIES"),MX+Map*.52f,MY+Map+8*S,11,Orange); }
 if(!IsOver()&&!IsPaused()&&!P->StatsVisible()) {
  const int Count=FMath::Min(State_->Eliminations.Num(),Compact?1:Rows);
  for(int I=0;I<Count;++I) {
   const auto& E=State_->Eliminations[State_->Eliminations.Num()-1-I]; const float Y=Compact?MY+Map+5*S:FeedY+I*FeedRow;
   const float Width=Map+16*S,Points=Compact?10:12,Inset=6*S;
   const float NameWidth=(Width-2*Inset-Measure->Measure(TEXT(" > "),CultUI::Font(Points*S)).X)*.5f;
   const FString A=Fit(E.Attacker.Contains(TEXT("(YOU)"))?TEXT("YOU"):E.Attacker,NameWidth,Points);
   const FString V=Fit(E.Victim.Contains(TEXT("(YOU)"))?TEXT("YOU"):E.Victim,NameWidth,Points);
   auto Color=E.Local?Gold:White; Color.A=FMath::Clamp(E.Remaining,0.f,1.f);
   Panel(MX-8*S,Y,Width,FeedRow-2*S); HUD->DrawRect(E.AttackerTeam<0?Muted:E.AttackerTeam==P->GetFaction()?Blue:Orange,MX-8*S,Y,2*S,FeedRow-2*S);
   Text(A+TEXT(" > ")+V,MX-8*S+Inset,Y+4*S,Points,Color);
   HUD->DrawRect(E.VictimTeam==P->GetFaction()?Blue:Orange,MX+Map+6*S,Y,2*S,FeedRow-2*S);
  }
 }
 if(auto* PC=HUD->GetOwningPlayerController()) {
  FVector2D Screen; FVector Goal=State_->PingTime>0?State_->PingPoint:State_->Capture?Objective():Base(1);
  if(PC->ProjectWorldLocationToScreen(Goal+FVector(0,0,500),Screen,true)&&Screen.Y>210*S&&Screen.Y< H-240*S) {
   const FString Label=State_->PingTime>0?TEXT("RALLY"):State_->Capture?TEXT("CONTROL SEAL"):CoreOpen(1)?TEXT("ENEMY GOD - EXPOSED"):LaneWardsCleared(1)?TEXT("DESTROY NEXUS WARD"):TEXT("PUSH ALL THREE LANES");
   const auto Size=Measure->Measure(Label,CultUI::Font(13*S)); const float LabelX=FMath::Clamp(float(Screen.X)-float(Size.X)*.5f,24*S,MX-24*S-float(Size.X));
   float LabelY=Screen.Y;
   if(ObjectiveAlert.bIsValid&&FBox2D(FVector2D(LabelX-6*S,LabelY-4*S),FVector2D(LabelX+Size.X+6*S,LabelY+Size.Y+4*S)).Intersect(ObjectiveAlert.ExpandBy(4*S))) LabelY=ObjectiveAlert.Max.Y+8*S;
   Panel(LabelX-6*S,LabelY-4*S,Size.X+12*S,Size.Y+8*S); Text(Label,LabelX,LabelY,13,Gold);
  }
 }
 if(P->StatsVisible()||IsOver()||IsPaused()) {
  const float PW=760*S,PH=440*S,X=M-PW*.5f,Y=H*.5f-PH*.5f;
  if(IsOver()||IsPaused()) HUD->DrawRect(FLinearColor(.008f,.015f,.025f,.8f),0,0,W,H);
  Panel(X,Y,PW,PH);
  Text(IsPaused()?TEXT("PAUSED"):IsOver()?Winner()<0?TEXT("DRAW"):Winner()==0?TEXT("VICTORY"):TEXT("DEFEAT"):TEXT("MATCH SCOREBOARD"),X+24*S,Y+18*S,30,IsOver()&&Winner()!=0?Orange:Gold);
  Text(TEXT("ENTITY"),X+24*S,Y+68*S,14,Muted); Text(TEXT("K / D / A"),X+440*S,Y+68*S,14,Muted); Text(TEXT("LEVEL"),X+600*S,Y+68*S,14,Muted);
  for(int I=0;I<State_->Fighters.Num();++I) {
   const auto& F=State_->Fighters[I]; const float Row=Y+(99+I*25)*S;
   Text(F.Name.Left(32),X+24*S,Row,13,F.Team==0?Blue:Orange);
   Text(FString::Printf(TEXT("%d / %d / %d"),F.Kills,F.Deaths,F.Assists),X+440*S,Row,13,White);
   Text(FString::Printf(TEXT("%d%s"),F.Level,F.Respawn>0?TEXT("  REFORMING"):TEXT("")),X+600*S,Row,13,Muted);
  }
  if(IsOver()||IsPaused()) {
   State_->ResultButtons.Reset(); const int Count=IsPaused()?3:2; const float BW=IsPaused()?230:330,Stride=IsPaused()?240:355;
   for(int I=0;I<Count;++I) {
    FVector2D Min(X+(24+I*Stride)*S,Y+373*S),Max=Min+FVector2D(BW*S,45*S); State_->ResultButtons.Add(FBox2D(Min,Max));
    FVector2D Mouse; const bool Hover=CultHUDSpace::Pointer(HUD->GetOwningPlayerController(),Mouse)&&FBox2D(Min,Max).IsInside(Mouse);
    HUD->DrawRect(Hover?FLinearColor(.05f,.14f,.12f):Ink,Min.X,Min.Y,Max.X-Min.X,Max.Y-Min.Y);
    HUD->DrawRect(Hover?Gold:I==0?Blue:CultUI::Line,Min.X,Min.Y,Max.X-Min.X,1*S);
    FString Label=IsPaused()?(I==0?TEXT("RESUME [ESC]"):I==1?TEXT("RESTART [ENTER]"):TEXT("MODE SELECT")):(I==0?TEXT("REMATCH [ENTER]"):TEXT("MODE SELECT [ESC]"));
    Text(Label,Min.X+18*S,Min.Y+14*S,15,White);
   }
  } else Text(TEXT("TAB close | G recall | ALT + click rally | ESC pause"),X+24*S,Y+394*S,13,Muted);
 }
}
