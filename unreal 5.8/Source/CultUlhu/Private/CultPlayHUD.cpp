#include "CultUlhuPlayerController.h"
#include "CultPlayHUD.h"
#include "CultCanvasLabel.h"
#include "CultHUDSpace.h"
#include "Camera/PlayerCameraManager.h"
#include "UI/CultUIStyle.h"
#include "CultUlhuPlayerPawn.h"
#include "CultUlhuGameInstance.h"
#include "CultCanonicalKit.h"
#include "CultMatchDirector.h"
#include "CultPopulation.h"
#include "CultWorldDirector.h"
#include "CultFeralBeast.h"
#include "Engine/Canvas.h"
#include "EngineUtils.h"
#include "Engine/Engine.h"
#include "GameFramework/PlayerController.h"
#include "ui/CommandMenu.h"

void DrawCultCombat(ACultPlayHUD*,ACultUlhuPlayerPawn*,UCanvas*);

void ACultPlayHUD::CommandClick()
{
 if(auto* M=ACultMatchDirector::Find(GetWorld())) { if(auto* P=Cast<ACultUlhuPlayerPawn>(GetOwningPawn())) { if(M->IsOver()||M->IsPaused()) M->ClickResult(); else M->Ping(P); } return; }
 auto* PC=GetOwningPlayerController(); auto* P=Cast<ACultUlhuPlayerPawn>(GetOwningPawn()); if(!PC||!P) return;
 FVector2D Pointer; if(!CultHUDSpace::Pointer(PC,Pointer)) return;
 const float X=Pointer.X,Y=Pointer.Y;
 if(bRegionMap_) { RegionMapClick(Pointer); return; }
 if(bFaith_) { FaithClick(X,Y); return; }
 if(auto* D=ACultWorldDirector::Find(GetWorld())) if(D->BattlePrepared()&&RegionMapHint_.IsInside(Pointer)) { ToggleRegionMap(); return; }
 if(bCommands_) { for(int I=0;I<Buttons_.Num();++I) if(Buttons_[I].IsInside(FVector2D(X,Y))) { if(Enabled_[I]) { if(auto* D=ACultWorldDirector::Find(GetWorld())) { if(GetWorld()->GetNetMode()==NM_Client) { if(auto* C=Cast<ACultUlhuPlayerController>(PlayerOwner)) C->ServerCultOrder(Actions_[I],CommandTarget_.Get(),CommandPoint_); } else P->Notify(D->Execute(Actions_[I],CommandTarget_.Get(),CommandPoint_)); } CloseCommands(); } return; } CloseCommands(); return; }
 SelectionStart_=FVector2D(X,Y); bSelecting_=true;
}
void ACultPlayHUD::CommandRelease()
{
 if(!bSelecting_) return; bSelecting_=false;
 auto* PC=GetOwningPlayerController(); auto* D=ACultWorldDirector::Find(GetWorld()); if(!PC||!D) return;
 FVector2D Pointer; if(!CultHUDSpace::Pointer(PC,Pointer,true)) return; const float X=Pointer.X,Y=Pointer.Y;
 AActor* SelectedEnemy=nullptr;
 if(FVector2D::Distance(SelectionStart_,FVector2D(X,Y))>12) {
  FBox2D Rect(ForceInit); Rect+=SelectionStart_; Rect+=FVector2D(X,Y);
  TArray<ACultPopulation*> Units;
  for(TActorIterator<ACultPopulation> It(GetWorld());It;++It) {
   FVector2D Screen; if(PC->ProjectWorldLocationToScreen(It->GetActorLocation(),Screen,true)&&Rect.IsInside(Screen)&&PC->LineOfSightTo(*It,PC->PlayerCameraManager?PC->PlayerCameraManager->GetCameraLocation():FVector::ZeroVector)) {
    if(It->IsCultist()&&It->GetFaction()==0) Units.Add(*It); else if(It->GetHealth()>0) SelectedEnemy=*It;
   }
  }
  if(!Units.IsEmpty()) { D->SelectFollowers(Units); if(GetWorld()->GetNetMode()==NM_Client) if(auto* C=Cast<ACultUlhuPlayerController>(PlayerOwner)) C->ServerCultSelection(Units); SelectedEnemy=nullptr; }
  if(auto* P=Cast<ACultUlhuPlayerPawn>(GetOwningPawn())) P->Notify(FString::Printf(TEXT("%d followers selected. Choose Build or Prepare for battle."),D->SelectedCount()));
 }
 OpenCommands(X,Y,SelectedEnemy);
}
void ACultPlayHUD::OpenCommands(float X,float Y,AActor* SelectedEnemy)
{
 auto* P=Cast<ACultUlhuPlayerPawn>(GetOwningPawn()); auto* D=ACultWorldDirector::Find(GetWorld()); if(!P||!D) return;
 const auto Hit=P->CameraAim(25000,true); CommandPoint_=Hit.bBlockingHit?Hit.ImpactPoint:Hit.TraceEnd; CommandTarget_=Hit.GetActor();
 if(SelectedEnemy) { CommandTarget_=SelectedEnemy; CommandPoint_=SelectedEnemy->GetActorLocation(); }
 if(FVector2D::Distance(SelectionStart_,FVector2D(X,Y))<=12) if(auto* Own=Cast<ACultPopulation>(CommandTarget_.Get())) if(Own->IsCultist()&&Own->GetFaction()==0) {
  TArray<ACultPopulation*> Units;
  for(TActorIterator<ACultPopulation> It(GetWorld());It;++It) if(FVector::Dist2D(Own->GetActorLocation(),It->GetActorLocation())<1000&&GetOwningPlayerController()->LineOfSightTo(*It,GetOwningPlayerController()->PlayerCameraManager?GetOwningPlayerController()->PlayerCameraManager->GetCameraLocation():FVector::ZeroVector)) Units.Add(*It);
  D->SelectFollowers(Units); if(GetWorld()->GetNetMode()==NM_Client) if(auto* C=Cast<ACultUlhuPlayerController>(PlayerOwner)) C->ServerCultSelection(Units);
 }
 cultulhu::MenuContext C; C.pos={float(CommandPoint_.X/100),float(CommandPoint_.Z/100),float(CommandPoint_.Y/100)};
 if(auto* Unit=Cast<ACultPopulation>(CommandTarget_.Get())) C.kind=Unit->IsCaptive()?cultulhu::MenuContext::Kind::Captive:Unit->GetFaction()==0?cultulhu::MenuContext::Kind::OwnCultist:cultulhu::MenuContext::Kind::Enemy;
 else if(Cast<ACultFeralBeast>(CommandTarget_.Get())||Cast<ACultUlhuPlayerPawn>(CommandTarget_.Get())) C.kind=cultulhu::MenuContext::Kind::Enemy;
 else if(CommandTarget_.Get()&&CommandTarget_.Get()->ActorHasTag(TEXT("CultTemple"))) C.kind=cultulhu::MenuContext::Kind::Altar;
 cultulhu::GameStateSummary Summary; for(TActorIterator<ACultPopulation> It(GetWorld());It;++It) { if(It->GetFaction()==0) ++Summary.commandableCultists; if(It->IsCaptive()) ++Summary.captivesHeld; }
 cultulhu::CommandMenu Model; const auto Menu=Model.generateMenu(C,Summary);
 Actions_.Reset(); Labels_.Reset(); Enabled_.Reset(); Buttons_.Reset();
 for(const auto& B:Menu) {
  // Faith actions use the same follower-aware dispatch, wherever they appear.
  using A=cultulhu::MenuAction;
  if(B.action==A::RitualSacrifice||B.action==A::SacrificeOrder||B.action==A::RitualNecromancy||B.action==A::RitualConvert) continue;
  Actions_.Add(static_cast<int32>(B.action)); Labels_.Add(UTF8_TO_TCHAR(B.label.c_str())); Enabled_.Add(B.enabled);
 }
 auto Add=[&](int A,const FString& Label,bool On=true) { Actions_.Add(A); Labels_.Add(Label); Enabled_.Add(On); };
 Add(ACultWorldDirector::Preach,TEXT("Preach / recruit nearby"));
 Add(ACultWorldDirector::PrepareBattle,D->BattlePrepared()?TEXT("Disengage battle preparation"):TEXT("Prepare for battle"),D->BattlePrepared()||D->SelectedCount()>0);
 Add(ACultWorldDirector::DisengageBattle,TEXT("Disengage selected group"),D->SelectedCount()>0);
 Add(ACultWorldDirector::GlobalRetreat,TEXT("GLOBAL RETREAT"));
 Add(ACultWorldDirector::ClearSelection,TEXT("Clear selection"));
 auto Extra=[&](cultulhu::MenuAction A,const TCHAR* Label) { if(!Actions_.Contains(int(A))) Add(int(A),Label); };
 Extra(cultulhu::MenuAction::BuildAt,TEXT("Build with selected crew"));
 Extra(cultulhu::MenuAction::RaidAt,TEXT("Raze city at target"));
 Add(ACultWorldDirector::RestFollowers,TEXT("Rest selected followers"));
 Add(ACultWorldDirector::MassConvert,TEXT("Mass convert (entity)"));
 Add(ACultWorldDirector::PlaceTrap,TEXT("Place capture trap"));
 Add(ACultWorldDirector::TortureCaptive,TEXT("Torture captive"));
 Add(ACultWorldDirector::SacrificeCaptive,TEXT("Sacrifice at temple"));
 Add(ACultWorldDirector::PerformNecromancy,TEXT("Necromancy at temple"));
 Add(ACultWorldDirector::BreedCreatures,TEXT("Breed captured creatures"));
 Add(ACultWorldDirector::ImprisonCultist,TEXT("Imprison cultist"));
 Add(ACultWorldDirector::PunishCultist,TEXT("Punish infringer"));
 CommandScreen_=FVector2D(X,Y); bCommands_=true;
}
void ACultPlayHUD::DrawHUD()
{
 Super::DrawHUD(); InspectionCards_.Reset(); auto* P=Cast<ACultUlhuPlayerPawn>(GetOwningPawn()); if(!P||!Canvas) return;
 const float S=FMath::Clamp(FMath::Min(Canvas->SizeX/1600.f,Canvas->SizeY/900.f),.6f,2.f); UFont* Font=GEngine->GetMediumFont();
 DrawCultCombat(this,P,Canvas);
 if(auto* M=ACultMatchDirector::Find(GetWorld())) { M->Draw(this,Canvas,P); return; }

 if(bRegionMap_) { InspectionHover_.Reset(); DrawRegionMap(); return; }
 if(bFaith_) { InspectionHover_.Reset(); DrawFaith(); return; }
 RegionMapHint_=FBox2D(ForceInit);
 if(auto* D=ACultWorldDirector::Find(GetWorld())) if(D->BattlePrepared()) {
  const float Width=280*S,Y=FMath::Max(135*S,CombatBottomTop_-32*S);
  RegionMapHint_=FBox2D(FVector2D(Canvas->SizeX-Width-20*S,Y),FVector2D(Canvas->SizeX-20*S,Y+26*S));
  DrawRect(CultUI::Panel,RegionMapHint_.Min.X,Y,Width,26*S);
  CultCanvasLabel(Canvas,FString::Printf(TEXT("M  MAP / SEND %d PREPARED"),D->PreparedCount()),CultUI::Gold,RegionMapHint_.Min.X+8*S,Y+4*S,13*S);
 }
 if(P->StatsVisible()) DrawSociety();
 DrawSelection();
 DrawPopulationInspection();
 if(bCommands_) {
  const int Cols=Canvas->SizeY<500||Canvas->SizeX>=1000?3:2;
  const int Rows=FMath::DivideAndRoundUp(Actions_.Num(),Cols);
  const float PanelW=FMath::Min(Canvas->SizeX-32.f,1000*S),Width=(PanelW-(Cols-1)*8*S)/Cols;
  const float Height=FMath::Min(42*S,(Canvas->SizeY-155*S-32)/FMath::Max(1,Rows)-6*S),PanelH=Rows*(Height+6*S);
  const FVector2D Center(FMath::Clamp(CommandScreen_.X,16.f,FMath::Max(16.f,Canvas->SizeX-PanelW-16)),FMath::Clamp(CommandScreen_.Y,120*S,FMath::Max(120*S,Canvas->SizeY-PanelH-16)));
  DrawRect(CultUI::Background,Center.X-8*S,Center.Y-8*S,PanelW+16*S,PanelH+16*S);
  Buttons_.Reset();
  for(int I=0;I<Actions_.Num();++I) {
   const FVector2D Min=Center+FVector2D((I%Cols)*(Width+8*S),(I/Cols)*(Height+6*S));
   const FBox2D R(Min,Min+FVector2D(Width,Height)); Buttons_.Add(R);
   FVector2D Mouse; const bool Hover=Enabled_[I]&&CultHUDSpace::Pointer(GetOwningPlayerController(),Mouse)&&R.IsInside(Mouse);
   DrawRect(Hover?FLinearColor(.05f,.14f,.12f):CultUI::Panel,R.Min.X,R.Min.Y,R.GetSize().X,R.GetSize().Y);
   DrawRect(Hover?CultUI::Gold:CultUI::Line,R.Min.X,R.Min.Y,R.GetSize().X,1*S);
   CultCanvasLabel(Canvas,Labels_[I],Enabled_[I]?CultUI::Bone:CultUI::Muted,R.Min.X+8*S,R.Min.Y+4*S,FMath::Max(10.f,14.f*S));
  }

 }
}
