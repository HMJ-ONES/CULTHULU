#include "CultPlayHUD.h"
#include "CultRegionMap.h"
#include "CultHUDSpace.h"
#include "CultCanvasLabel.h"
#include "CultWorldDirector.h"
#include "CultMatchDirector.h"
#include "CultPopulation.h"
#include "CultFeralBeast.h"
#include "CultDestructible.h"
#include "CultUlhuPlayerPawn.h"
#include "CultUlhuPlayerController.h"
#include "CultUlhuGameInstance.h"
#include "Engine/Canvas.h"
#include "EngineUtils.h"
#include "Framework/Application/SlateApplication.h"
#include "Fonts/FontMeasure.h"
#include "Rendering/SlateRenderer.h"

void ACultPlayHUD::ToggleRegionMap()
{
 if(ACultMatchDirector::IsMatch(GetWorld())) return;
 if(auto* G=Cast<UCultUlhuGameInstance>(GetGameInstance())) if(G->FreeRoamMenuOpen) return;
 bRegionMap_=!bRegionMap_; bFaith_=false; CloseCommands(); InspectionHover_.Reset();
 if(auto* PC=GetOwningPlayerController()) {
  PC->bShowMouseCursor=bRegionMap_||PC->IsInputKeyDown(EKeys::LeftAlt);
  if(PC->bShowMouseCursor) { FInputModeGameAndUI Mode; Mode.SetHideCursorDuringCapture(false); PC->SetInputMode(Mode); }
  else { FInputModeGameOnly Mode; Mode.SetConsumeCaptureMouseDown(false); PC->SetInputMode(Mode); }
 }
}
void ACultPlayHUD::RegionMapClick(FVector2D Pointer)
{
 if(RegionMapBounds_.IsInside(Pointer)) { MapRegion_=CultRegionMap::Index(CultRegionMap::Unproject(Pointer,RegionMapBounds_)); return; }
 for(int I=0;I<RegionMapButtons_.Num();++I) if(RegionMapButtons_[I].IsInside(Pointer)) {
  if(I==3) { ToggleRegionMap(); return; }
  auto* D=ACultWorldDirector::Find(GetWorld()); auto* P=Cast<ACultUlhuPlayerPawn>(GetOwningPawn()); if(!D||!P) return;
  if(MapRegion_<0) { P->Notify(TEXT("Click a region on the map first.")); return; }
  if(!D->BattlePrepared()||D->PreparedCount()<=0) { P->Notify(TEXT("ALT + select followers, then Prepare for battle before sending an expedition.")); return; }
  const int Action=ACultWorldDirector::ExploreRegion+I; const FVector Point=CultRegionMap::Center(MapRegion_);
  if(GetWorld()->GetNetMode()==NM_Client) { if(auto* PC=Cast<ACultUlhuPlayerController>(GetOwningPlayerController())) PC->ServerCultOrder(Action,nullptr,Point); }
  else P->Notify(D->Execute(Action,nullptr,Point));
  return;
 }
}
void ACultPlayHUD::DrawRegionMap()
{
 auto* D=ACultWorldDirector::Find(GetWorld()); auto* PC=GetOwningPlayerController(); if(!D||!PC||!Canvas) return;
 const float S=FMath::Clamp(FMath::Min(Canvas->SizeX/1280.f,Canvas->SizeY/720.f),.65f,1.6f);
 const float Margin=16*S,Top=60*S,Bottom=Canvas->SizeY-20*S,Side=FMath::Min(Bottom-Top,Canvas->SizeX*.60f);
 DrawRect(CultUI::Background,Margin,Margin,Canvas->SizeX-2*Margin,Canvas->SizeY-2*Margin);
 CultCanvasLabel(Canvas,TEXT("THE DARK WILDS / EXPEDITIONS"),CultUI::Gold,Margin+10*S,Margin+4*S,22*S);
 RegionMapBounds_=FBox2D(FVector2D(Margin+10*S,Top),FVector2D(Margin+10*S+Side,Top+Side));
 auto Label=[&](FString Text,FLinearColor Color,float X,float Y,float Size=13) { CultCanvasLabel(Canvas,Text,Color,X,Y,Size*S); };
 auto* Land=ACultLivingLandscape::Find(GetWorld());
 // Cache the actual terrain tint. Opening the map never allocates a texture.
 static TWeakObjectPtr<ACultLivingLandscape> CachedLand; static int CachedSeed=0; static TArray<FLinearColor> Tints;
 if(Tints.Num()!=1296||CachedLand.Get()!=Land||(Land&&CachedSeed!=Land->WorldSeed)) {
  CachedLand=Land; CachedSeed=Land?Land->WorldSeed:0; Tints.SetNum(1296);
  for(int Y=0;Y<36;++Y) for(int X=0;X<36;++X) {
   const FVector P((X/36.f*2-1)*ACultLivingLandscape::Extent,(1-Y/36.f*2)*ACultLivingLandscape::Extent,0);
   const float H=Land?Land->Height(P.X,P.Y):0;
   Tints[Y*36+X]=FMath::Lerp(FLinearColor(.025f,.085f,.072f),FLinearColor(.22f,.23f,.24f),FMath::Clamp(H/23000,0.f,1.f));
  }
 }
 for(int Y=0;Y<36;++Y) for(int X=0;X<36;++X) {
  const float Tile=Side/36; const FVector2D Min=RegionMapBounds_.Min+FVector2D(X*Tile,Y*Tile);
  DrawRect(Tints[Y*36+X],Min.X,Min.Y,Tile+1,Tile+1);
 }
 FVector2D Mouse(-1,-1); CultHUDSpace::Pointer(PC,Mouse);
 const int Hover=RegionMapBounds_.IsInside(Mouse)?CultRegionMap::Index(CultRegionMap::Unproject(Mouse,RegionMapBounds_)):INDEX_NONE;
 for(int R=0;R<36;++R) {
  const auto C=CultRegionMap::Project(CultRegionMap::Center(R),RegionMapBounds_); const float Cell=Side/6;
  if(R==MapRegion_||R==Hover) DrawRect(FLinearColor(.1f,.7f,.6f,R==MapRegion_?.28f:.12f),C.X-Cell/2,C.Y-Cell/2,Cell,Cell);
  Label(CultRegionMap::Name(R),R==MapRegion_?CultUI::Gold:CultUI::Muted,C.X-Cell/2+4*S,C.Y-Cell/2+3*S,11);
 }
 for(int I=0;I<=6;++I) {
  const float V=I*Side/6; DrawLine(RegionMapBounds_.Min.X+V,Top,RegionMapBounds_.Min.X+V,Top+Side,CultUI::Line,1);
  DrawLine(RegionMapBounds_.Min.X,Top+V,RegionMapBounds_.Max.X,Top+V,CultUI::Line,1);
 }
 int Buildings=0,Monsters=0,Troops=0;
 auto Dot=[&](FVector P,FLinearColor Color,float Radius) { if(CultRegionMap::Index(P)<0) return; const auto C=CultRegionMap::Project(P,RegionMapBounds_); DrawRect(Color,C.X-Radius*S,C.Y-Radius*S,Radius*2*S,Radius*2*S); };
 for(TActorIterator<ACultDestructible> It(GetWorld());It;++It) if(!It->IsDestroyed()&&(It->ActorHasTag(TEXT("LivingCity"))||It->ActorHasTag(TEXT("FaithAltar")))) {
  Dot(It->GetActorLocation(),It->ActorHasTag(TEXT("FaithAltar"))?CultUI::Teal:CultUI::Gold,2);
  if(It->ActorHasTag(TEXT("LivingCity"))&&CultRegionMap::Index(It->GetActorLocation())==MapRegion_) ++Buildings;
 }
 for(TActorIterator<ACultFeralBeast> It(GetWorld());It;++It) if(!It->IsDead()&&!It->IsCaptured()) { Dot(It->GetActorLocation(),CultUI::Danger,2); if(CultRegionMap::Index(It->GetActorLocation())==MapRegion_) ++Monsters; }
 for(TActorIterator<ACultPopulation> It(GetWorld());It;++It) if(It->IsCultist()&&It->GetFaction()==0&&It->GetHealth()>0) { Dot(It->GetActorLocation(),D->IsSelected(*It)?CultUI::Gold:CultUI::Teal,1.5f); if(CultRegionMap::Index(It->GetActorLocation())==MapRegion_) ++Troops; }
 for(const auto& P:ACultLivingLandscape::CaveSites()) { const auto C=CultRegionMap::Project(P,RegionMapBounds_); Label(TEXT("C"),CultUI::Bone,C.X,C.Y,12); }
 if(auto* P=GetOwningPawn()) Dot(P->GetActorLocation(),FLinearColor::White,4);
 const float X=RegionMapBounds_.Max.X+18*S,W=Canvas->SizeX-Margin-X-10*S;
 float Y=Top;
 Label(MapRegion_>=0?TEXT("REGION ")+CultRegionMap::Name(MapRegion_):TEXT("SELECT A REGION"),CultUI::Gold,X,Y,20); Y+=34*S;
 Label(FString::Printf(TEXT("%d troops prepared"),D->PreparedCount()),CultUI::Bone,X,Y); Y+=28*S;
 if(MapRegion_>=0) {
  Label(FString::Printf(TEXT("%d city structures"),Buildings),CultUI::Gold,X,Y); Y+=23*S;
  Label(FString::Printf(TEXT("%d wild monsters"),Monsters),CultUI::Danger,X,Y); Y+=23*S;
  Label(FString::Printf(TEXT("%d friendly followers"),Troops),CultUI::Teal,X,Y); Y+=29*S;
 }
 RegionMapButtons_.Reset(); const TCHAR* Names[]={TEXT("EXPLORE REGION"),TEXT("HUNT MONSTERS"),TEXT("RAID CITY"),TEXT("CLOSE / M / ESC")};
 for(int I=0;I<4;++I) {
  const FBox2D B(FVector2D(X,Y),FVector2D(X+W,Y+32*S)); RegionMapButtons_.Add(B);
  const bool Enabled=I==3||(MapRegion_>=0&&D->BattlePrepared()&&D->PreparedCount()>0&&(I!=2||Buildings>0));
  DrawRect(Enabled&&B.IsInside(Mouse)?FLinearColor(.05f,.16f,.14f):CultUI::Panel,X,Y,W,32*S);
  Label(Names[I],Enabled?CultUI::Bone:CultUI::Muted,X+7*S,Y+7*S,12); Y+=39*S;
 }
 if(auto* Player=Cast<ACultUlhuPlayerPawn>(GetOwningPawn())) if(!Player->GetNotice().IsEmpty()) {
  Y+=6*S; const auto Measure=FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
  TArray<FString> Words; Player->GetNotice().ParseIntoArrayWS(Words); FString Line;
  for(const auto& Word:Words) {
   const FString Next=Line.IsEmpty()?Word:Line+TEXT(" ")+Word;
   if(!Line.IsEmpty()&&Measure->Measure(Next,CultUI::Font(11*S)).X>W) { Label(Line,CultUI::Gold,X,Y,11); Y+=18*S; Line=Word; if(Y+18*S>Bottom) { Line.Empty(); break; } }
   else Line=Next;
  }
  if(!Line.IsEmpty()&&Y+18*S<=Bottom) { Label(Line,CultUI::Gold,X,Y,11); Y+=23*S; }
 }
 if(Y+72*S<Bottom) {
  Label(TEXT("Gold: cities   Red: monsters"),CultUI::Muted,X,Y,11); Y+=20*S;
  Label(TEXT("Teal: troops / temples   C: cave"),CultUI::Muted,X,Y,11); Y+=26*S;
  Label(TEXT("Troops march; the world stays live."),CultUI::Bone,X,Y,11); Y+=20*S;
  Label(TEXT("ALT: select another group to send."),CultUI::Muted,X,Y,11);
 }
}
