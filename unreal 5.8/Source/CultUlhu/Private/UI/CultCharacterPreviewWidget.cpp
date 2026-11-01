#include "UI/CultCharacterPreviewWidget.h"
#include "CultCharacterModel.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Image.h"
#include "Components/MeshComponent.h"
#include "Components/TextBlock.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/PointLightComponent.h"
#include "Engine/TextureRenderTarget2D.h"
#include "UI/CultUIStyle.h"

TSharedRef<SWidget> UCultCharacterPreviewWidget::RebuildWidget() {
 if(!Image_) {
  WidgetTree=NewObject<UWidgetTree>(this); auto* Overlay=NewObject<UOverlay>(this);
  Image_=NewObject<UImage>(this); Overlay->AddChildToOverlay(Image_)->SetHorizontalAlignment(HAlign_Fill);
  Status_=CultUI::Label(this,TEXT("MODEL PENDING"),11,CultUI::Muted);
  auto* StatusSlot=Overlay->AddChildToOverlay(Status_); StatusSlot->SetVerticalAlignment(VAlign_Bottom); StatusSlot->SetPadding(FMargin(6));
  WidgetTree->RootWidget=Overlay;
 }
 return Super::RebuildWidget();
}
void UCultCharacterPreviewWidget::ClearStage() {
 if(IsValid(Model_)) Model_->Destroy(); Model_=nullptr;
 if(IsValid(Stage_)) Stage_->Destroy(); Stage_=nullptr; Capture_=nullptr;
}
void UCultCharacterPreviewWidget::ShowCharacter(const FString& Id) {
 TakeWidget(); ClearStage(); if(!GetWorld()) return;
 Model_=GetWorld()->SpawnActor<ACultCharacterModel>();
 if(!Model_||!Model_->Initialize(Id,true)) {
  ClearStage(); Image_->SetVisibility(ESlateVisibility::Hidden);
  Status_->SetText(FText::FromString(TEXT("APPROVED MODEL NOT YET AVAILABLE"))); return;
 }
 // Isolated off-map studio, one capture for the selected entity only.
 const FVector Origin(0,0,1000000); Model_->SetActorLocation(Origin); Model_->SetActorRotation(FRotator(0,-35,0));
 Stage_=GetWorld()->SpawnActor<AActor>(); auto* Root=NewObject<USceneComponent>(Stage_); Stage_->SetRootComponent(Root); Root->RegisterComponent(); Stage_->SetActorLocation(Origin);
 for(int I=0;I<2;++I) {
  auto* Light=NewObject<UPointLightComponent>(Stage_); Light->SetupAttachment(Root); Light->RegisterComponent();
  Light->SetRelativeLocation(I?FVector(100,180,190):FVector(180,-170,240));
  Light->SetIntensity(I?25000.f:50000.f); Light->SetAttenuationRadius(1500); Light->SetCastShadows(false);
  Light->SetLightColor(I?FLinearColor(.25f,.7f,.75f):FLinearColor(1.f,.85f,.65f));
 }
 Target_=NewObject<UTextureRenderTarget2D>(this); Target_->RenderTargetFormat=RTF_RGBA8; Target_->ClearColor=FLinearColor(.008f,.018f,.022f,1);
 Target_->InitAutoFormat(512,256); Target_->UpdateResourceImmediate(true);
 Capture_=NewObject<USceneCaptureComponent2D>(Stage_); Capture_->SetupAttachment(Root); Capture_->RegisterComponent();
 Capture_->TextureTarget=Target_; Capture_->CaptureSource=SCS_FinalColorLDR; Capture_->bCaptureEveryFrame=false; Capture_->bCaptureOnMovement=false;
 Capture_->PrimitiveRenderMode=ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList; Capture_->ShowOnlyActorComponents(Model_);
 Capture_->SetRelativeLocation(FVector(410,0,Id==TEXT("mi_go")?100:120)); Capture_->SetRelativeRotation(FRotator(-4,180,0));
 Capture_->ProjectionType=ECameraProjectionMode::Orthographic; Capture_->OrthoWidth=460;
 {
  FBox Bounds(ForceInit); for(UMeshComponent* Part:Model_->Parts()) { Part->UpdateBounds(); Bounds+=Part->Bounds.GetBox(); }
  if(Bounds.IsValid) Capture_->OrthoWidth=FMath::Max(460.f,float(Bounds.GetSize().Y)*1.15f);
 }
 Capture_->ShowFlags.SetAtmosphere(false); Capture_->ShowFlags.SetFog(false); Capture_->ShowFlags.SetMotionBlur(false);
 Capture_->PostProcessSettings.bOverride_AutoExposureMethod=true; Capture_->PostProcessSettings.AutoExposureMethod=AEM_Manual;
 Capture_->PostProcessSettings.bOverride_AutoExposureBias=true; Capture_->PostProcessSettings.AutoExposureBias=8;
 Image_->SetBrushFromTextureDynamic(nullptr); FSlateBrush Brush; Brush.SetResourceObject(Target_); Brush.ImageSize=FVector2D(512,256); Image_->SetBrush(Brush);
 Image_->SetVisibility(ESlateVisibility::HitTestInvisible);
 Status_->SetText(FText::FromString(ACultCharacterModel::HasSourceAnimation(Id)?TEXT("ORIGINAL SOURCE ANIMATION"):TEXT("STATIC SOURCE / RIGGING PENDING")));
 if(Id==TEXT("sghllor")) Status_->SetText(FText::FromString(TEXT("ORIGINAL LIGHT / ANIMATED MIST")));
 Status_->SetToolTipText(FText::FromString(ACultCharacterModel::SourceCredit(Id)));
 Capture_->CaptureScene(); CaptureTime_=0;
}
void UCultCharacterPreviewWidget::NativeTick(const FGeometry& Geometry,float DeltaSeconds) {
 Super::NativeTick(Geometry,DeltaSeconds); CaptureTime_+=DeltaSeconds;
 if(Capture_&&CaptureTime_>=1.f/15) { CaptureTime_=0; Capture_->CaptureScene(); }
}
void UCultCharacterPreviewWidget::NativeDestruct() { ClearStage(); Super::NativeDestruct(); }
