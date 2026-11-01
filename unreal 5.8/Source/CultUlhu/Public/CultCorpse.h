#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CultCorpse.generated.h"
UCLASS()
class CULTULHU_API ACultCorpse : public AActor {
 GENERATED_BODY()
public:
 ACultCorpse();
 static ACultCorpse* Leave(AActor* Body);
};
