#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CultFirstPersonView.generated.h"

class UProceduralMeshComponent;
class ACultCharacterModel;
// Local presentation only. World skeletons and server hitboxes remain authoritative.
UCLASS()
class CULTULHU_API UCultFirstPersonView : public UActorComponent {
 GENERATED_BODY()
public:
 UCultFirstPersonView();
 virtual void TickComponent(float Dt,ELevelTick TickType,FActorComponentTickFunction* Function) override;
 bool HasVisibleAnatomy() const;
#if WITH_EDITOR
 static bool Bake(const FString& Id);
#endif
private:
 struct FVertex { FVector3f Position,Normal; FVector2f UV; uint16 Bones[8]={}; float Weights[8]={}; };
 struct FSection { int32 Part=0,Material=0; FString SourceAsset; FVector Anchor=FVector::ZeroVector; bool AnchorReady=false; int32 Side=1; TArray<FVertex> Source; TArray<int32> Triangles; TArray<FVector> Points,Normals; TArray<FVector2D> UVs; };
 TArray<FSection> Sections;
 TMap<int32,FVector> InitialAnchors;
 UPROPERTY() TArray<TObjectPtr<UProceduralMeshComponent>> Meshes;
 TWeakObjectPtr<ACultCharacterModel> Model;
 bool Loaded=false; float Clock=0,LastUpdate=-1; FRotator PreviousLook=FRotator::ZeroRotator;
 FVector Sway=FVector::ZeroVector;
 void LoadView(const FString& Id);
 static void TransferViewData(FArchive& Ar,TArray<FSection>& Data);
};
