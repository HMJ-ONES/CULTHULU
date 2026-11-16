#include "CultPlayHUD.h"
#include "CultUlhuPlayerPawn.h"
#include "CultSkillAttack.h"
#include "CultCanvasLabel.h"
#include "CultHUDProjection.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "CultWardWarning.h"
#include "CultMatchDirector.h"
#include "UI/CultUIStyle.h"

void DrawCultCombatFeel(ACultPlayHUD* HUD,ACultUlhuPlayerPawn* P,UCanvas* C)
{
 auto* PC=HUD->GetOwningPlayerController(); if(!PC) return;
 const float S=FMath::Clamp(FMath::Min(C->SizeX/1600.f,C->SizeY/900.f),.55f,2.f);
 const float X=C->SizeX*.5f,Y=C->SizeY*.5f;
 const FLinearColor Coral(1.f,.3f,.18f),Gold(1.f,.75f,.3f),Blue(.15f,.85f,1.f);
 const auto* Match=ACultMatchDirector::Find(P->GetWorld());
 if(!Match||!Match->IsPaused()) {
  const auto Warning=CultWardWarning::Observe(P);
  if(Warning.IsVisible()) {
   const auto Panel=CultWardWarning::Panel(Warning,P->CombatHUD().DashCooldown,C); const auto& Box=Panel.Bounds;
   HUD->DrawRect(CultUI::Panel,Box.Min.X,Box.Min.Y,Box.GetSize().X,Box.GetSize().Y);
   HUD->DrawRect(Coral,Box.Min.X,Box.Min.Y,3*S,Box.GetSize().Y);
   CultUI::DrawText(C,Panel.Title,Coral,Box.Min.X+48*S,Box.Min.Y+8*S,15*S,true);
   CultUI::DrawText(C,Panel.Detail,CultUI::Bone,Box.Min.X+48*S,Box.Min.Y+12*S+Panel.TitleHeight,11*S);
   const FVector Local=(Warning.Source-P->GetActorLocation()).GetSafeNormal2D().RotateAngleAxis(-P->AimRotation().Yaw,FVector::UpVector);
   const FVector2D D(Local.Y,-Local.X),Side(-D.Y,D.X),Tip=FVector2D(Box.Min.X+26*S,Box.GetCenter().Y)+D*11*S;
   for(int Sign:{-1,1}) { const auto End=Tip-D*12*S+Side*Sign*7*S; HUD->DrawLine(Tip.X,Tip.Y,End.X,End.Y,Coral,2*S); }
  }
 }
 const float Hurt=P->HurtFeedbackTime()/.32f;
 if(Hurt>0) {
  const FLinearColor Edge(1.f,.1f,.08f,Hurt*.38f);
  HUD->DrawRect(Edge,0,0,C->SizeX,7*S); HUD->DrawRect(Edge,0,C->SizeY-7*S,C->SizeX,7*S);
  HUD->DrawRect(Edge,0,0,7*S,C->SizeY); HUD->DrawRect(Edge,C->SizeX-7*S,0,7*S,C->SizeY);
  if(P->HasIncomingDirection()) {
   const FVector To=(P->IncomingOrigin()-P->GetActorLocation()).GetSafeNormal2D();
   const FVector Local=To.RotateAngleAxis(-P->AimRotation().Yaw,FVector::UpVector);
   const FVector2D D(Local.Y,-Local.X),Side(-D.Y,D.X),Tip=FVector2D(X,Y)+D*96*S;
   for(int Sign:{-1,1}) { const FVector2D End=Tip-D*14*S+Side*Sign*8*S; HUD->DrawLine(Tip.X,Tip.Y,End.X,End.Y,Coral,3*S); }
  }
 }
 for(const auto& R:P->ImpactTexts()) {
  FVector2D V; if(!CultHUDProjection::Project(C,R.Point,V)||V.X<20||V.X>C->SizeX-90||V.Y<30||V.Y>C->SizeY-100) continue;
  const float Fade=FMath::Clamp((.85f-R.Age)/.25f,0.f,1.f);
  auto Color=R.Killed?Gold:FLinearColor::White; Color.A=Fade;
  const float TextY=V.Y-28*S-R.Age*48*S;
  if(TextY+24*S>HUD->CombatContentTop()-8*S) continue;
  CultCanvasLabel(C,R.Amount>=1?FString::Printf(TEXT("%.0f"),R.Amount):TEXT("<1"),Color,V.X+12*S,TextY,(R.Killed?20.f:16.f)*S);
 }
 if(P->KillConfirmTime()>0) CultCanvasLabel(C,TEXT("ELIMINATED"),Gold,X-55*S,Y+32*S,14*S);
 if(P->EvadeConfirmTime()>0) CultCanvasLabel(C,TEXT("DODGED"),Blue,X-34*S,Y+56*S,14*S);
 const float Recovery=FMath::Clamp(P->MeleeRecovery()/.62f,0.f,1.f);
 HUD->DrawRect(FLinearColor(.02f,.05f,.09f,.8f),X-45*S,C->SizeY-104*S,90*S,3*S);
 HUD->DrawRect(Recovery>0?FLinearColor(.55f,.65f,.7f):Blue,X-45*S,C->SizeY-104*S,90*S*(1.f-Recovery),3*S);

 // World geometry shows the real volume. Keep only two small timed labels here.
 TArray<ACultSkillAttack*> Threats;
 for(TActorIterator<ACultSkillAttack> It(P->GetWorld());It;++It) {
  if(!It->IsWindingUp()||!ACultSkillAttack::Alive(It->GetOwner())) continue;
  if(It->Shape==ACultSkillAttack::EShape::Bolt||It->Shape==ACultSkillAttack::EShape::Beam) continue;
  if(It->Shape==ACultSkillAttack::EShape::Sweep&&!ACultSkillAttack::Enemy(P,It->GetOwner())) continue;
  if(FVector::Dist2D(It->GetActorLocation(),P->GetActorLocation())>It->Reach+P->GetSimpleCollisionRadius()+4000) continue;
  if(!ACultSkillAttack::Clear(P->GetWorld(),PC->PlayerCameraManager->GetCameraLocation(),It->GetActorLocation(),P,It->GetOwner())) continue;
  Threats.Add(*It);
 }
 Threats.Sort([P](const ACultSkillAttack& A,const ACultSkillAttack& B){
  const bool CloseA=ACultSkillAttack::Enemy(P,A.GetOwner())&&FVector::Dist2D(A.GetActorLocation(),P->GetActorLocation())<A.Reach+P->GetSimpleCollisionRadius()+500;
  const bool CloseB=ACultSkillAttack::Enemy(P,B.GetOwner())&&FVector::Dist2D(B.GetActorLocation(),P->GetActorLocation())<B.Reach+P->GetSimpleCollisionRadius()+500;
  return CloseA!=CloseB?CloseA:A.TimeUntilActive()<B.TimeUntilActive();
 });
 int Drawn=0;
 for(auto* A:Threats) {
  FVector2D Label;
  if(!CultHUDProjection::Project(C,A->GetActorLocation()+FVector(0,0,120),Label)||Label.X<70*S||Label.X>C->SizeX-120*S||Label.Y<100*S||Label.Y>C->SizeY-180*S) continue;
  if(Label.Y+4*S>HUD->CombatContentTop()-8*S) continue;
  if(++Drawn>2) break;
  const bool Enemy=ACultSkillAttack::Enemy(P,A->GetOwner());
  const float Progress=A->WindupProgress(); auto Color=Enemy?Coral:Blue; Color.A=.55f+Progress*.45f;
  const TCHAR* Kind=A->Shape==ACultSkillAttack::EShape::Sweep?TEXT("SWING"):A->Shape==ACultSkillAttack::EShape::Sphere?TEXT("AURA"):A->Shape==ACultSkillAttack::EShape::Cone?TEXT("CONE"):TEXT("IMPACT");
  CultCanvasLabel(C,FString::Printf(TEXT("%s  %.1fs"),Kind,A->TimeUntilActive()),Color,Label.X-45*S,Label.Y-18*S,12*S);
  HUD->DrawRect(FLinearColor(.015f,.025f,.035f,.85f),Label.X-45*S,Label.Y,90*S,3*S);
  HUD->DrawRect(Color,Label.X-45*S,Label.Y,90*S*Progress,3*S);
 }
}
