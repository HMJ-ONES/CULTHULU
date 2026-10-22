#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CultCharacterPreviewWidget.generated.h"
class UImage;
class UTextBlock;
class UTextureRenderTarget2D;
class USceneCaptureComponent2D;
class ACultCharacterModel;
UCLASS()
class CULTULHU_API UCultCharacterPreviewWidget : public UUserWidget {
 GENERATED_BODY()
public:
 virtual TSharedRef<SWidget> RebuildWidget() override;
 void ShowCharacter(const FString& Id);
 virtual void NativeTick(const FGeometry& Geometry,float DeltaSeconds) override;
 virtual void NativeDestruct() override;
private:
 friend struct FCultCharacterModelTestAccess;
 UPROPERTY() UImage* Image_=nullptr;
 UPROPERTY() UTextBlock* Status_=nullptr;
 UPROPERTY() UTextureRenderTarget2D* Target_=nullptr;
 UPROPERTY() AActor* Stage_=nullptr;
 UPROPERTY() ACultCharacterModel* Model_=nullptr;
 UPROPERTY() USceneCaptureComponent2D* Capture_=nullptr;
 float CaptureTime_=0;
 void ClearStage();
};
