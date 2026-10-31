#include "CultUlhuPlayGameMode.h"
#include "CultMatchSpawn.h"
// CULT-ULHU play game mode implementation.
#include "CultActorLabels.h"
#include "CultWildsPresentation.h"
#include "CultSceneLighting.h"

#include "EngineUtils.h"
#include "GameFramework/PlayerStart.h"

#include "CultUlhuGameInstance.h"
#include "CultUlhuPlayerPawn.h"
#include "CultUlhuPlayerController.h"
#include "CultPlayHUD.h"
#include "TimerManager.h"
#include "CultWorldDirector.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "CultLivingLandscape.h"
#include "CultMatchDirector.h"
#include "CultFeralBeast.h"
#include "Components/StaticMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMeshActor.h"
#include "Components/TextRenderComponent.h"
#include "Components/LightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Engine/ReflectionCapture.h"
#include "Engine/SkyLight.h"
#include "Engine/TextureCube.h"

void PopulateCultWilds(UWorld* World);

ACultUlhuPlayGameMode::ACultUlhuPlayGameMode()
{
	PrimaryActorTick.bCanEverTick = false; // World director owns the scoped core simulation.
 DefaultPawnClass = ACultUlhuPlayerPawn::StaticClass();
	PlayerControllerClass = ACultUlhuPlayerController::StaticClass();
	HUDClass = ACultPlayHUD::StaticClass();
}

void ACultUlhuPlayGameMode::InitGame(const FString& MapName,const FString& Options,FString& ErrorMessage)
{
 Super::InitGame(MapName,Options,ErrorMessage);
 if(!MapName.Contains(TEXT("L_DarkWilds"))) return;
 // A fixed sky environment keeps moving creatures readable in the vessel's shadow.
 // Capturing the runtime blockout can otherwise produce nearly black ambient light.
 // Runtime replacement keeps the authored level and all imported art recoverable.
 for(TActorIterator<AActor> It(GetWorld());It;++It) {
  if(It->IsA<AReflectionCapture>()) { It->Destroy(); continue; }
  if(It->IsA<ACultFeralBeast>()) { It->Destroy(); continue; }
  TArray<UStaticMeshComponent*> Components; It->GetComponents(Components);
  if(Components.Num()>0||It->IsA<AStaticMeshActor>()) { It->Tags.AddUnique(TEXT("CultLegacyGeometry")); It->SetActorEnableCollision(false); }
  for(auto* C:Components) { C->SetVisibility(false); C->SetCollisionEnabled(ECollisionEnabled::NoCollision); }
  TArray<UTextRenderComponent*> Labels; It->GetComponents(Labels); for(auto* Label:Labels) Label->SetVisibility(false);
  TArray<ULightComponent*> Lights; It->GetComponents(Lights); for(auto* Light:Lights) Light->SetMobility(EComponentMobility::Movable);
  if(It->IsA<APlayerStart>()) It->SetActorLocation(FVector(0,0,2000));
 }
 CultSceneLighting::Ambient(GetWorld(),.10f,FLinearColor(.72f,.82f,1.f));
 if(auto* GI=Cast<UCultUlhuGameInstance>(GetGameInstance())) if(GI->SelectedMode!=ECultPlayMode::FreeRoam) { ACultMatchDirector::BuildArena(GetWorld(),GI->SelectedMode); return; }
 CultWildsPresentation::Apply(GetWorld());
 auto* Land=GetWorld()->SpawnActor<ACultLivingLandscape>(); auto* GI=Cast<UCultUlhuGameInstance>(GetGameInstance()); Land->Generate(GI?GI->FreeRoamSeed:FMath::Rand());
}

void ACultUlhuPlayGameMode::BeginPlay()
{
	Super::BeginPlay();

	UCultUlhuGameInstance* GI = Cast<UCultUlhuGameInstance>(GetGameInstance());
	if (!GI) return;

	FString Map = GetWorld()->GetName();
	if (!Map.Contains(TEXT("L_MainMenu")) && !GI->bPlayChosen &&
		!FParse::Param(FCommandLine::Get(), TEXT("CultGameplayTest")))
	{
		GetWorldTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this, [this]() {
			UGameplayStatics::OpenLevel(this, FName(TEXT("/Game/Maps/L_MainMenu")));
		}));
		return;
	}
	if (Map.Contains(TEXT("L_MainMenu")))
	{
		// Menu map: UI only, no pawn needed.
		GI->ShowHomeScreen();
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("[CultUlhu] mode=%s character=%s"),
		*UCultUlhuGameInstance::ModeDisplayName(GI->SelectedMode),
		*GI->SelectedCharacterId);
	if (Map.Contains(TEXT("L_DarkWilds")))
		GetWorldTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this, [this]() {
   // InitGame precedes component/actor initialization; old serialized collision can return on registration.
   // Reapply the replacement after level initialization, before launching either mode's simulation.
   for(TActorIterator<AActor> It(GetWorld());It;++It) if(It->ActorHasTag(TEXT("CultLegacyGeometry"))) {
    It->SetActorEnableCollision(false); TArray<UStaticMeshComponent*> Parts; It->GetComponents(Parts);
    for(auto* C:Parts) { C->SetVisibility(false); C->SetCollisionEnabled(ECollisionEnabled::NoCollision); }
   }
   GetWorld()->SpawnActor<ACultWorldDirector>(); if(ACultMatchDirector::IsMatch(GetWorld())) { GetWorld()->SpawnActor<ACultMatchDirector>(); if(auto* G=Cast<UCultUlhuGameInstance>(GetGameInstance())) if(G->HostedRoomLobby) G->ShowHostedRoomLobby(); } else { PopulateCultWilds(GetWorld()); if(auto* G=Cast<UCultUlhuGameInstance>(GetGameInstance())) G->BeginFreeRoamSession(); } }));
}

AActor* ACultUlhuPlayGameMode::ChoosePlayerStart_Implementation(AController* Player)
{
	if (GetWorld()->GetName().Contains(TEXT("L_MainMenu")))
		return Super::ChoosePlayerStart_Implementation(Player);
	if (UCultUlhuGameInstance* GI = Cast<UCultUlhuGameInstance>(GetGameInstance()))
	{
		const FString Want = UCultUlhuGameInstance::ModeStartLabel(GI->SelectedMode);
		for (TActorIterator<APlayerStart> It(GetWorld()); It; ++It)
		{
			if (It->PlayerStartTag==FName(*Want)||CultActorLabels::Anchor(*It,FName(*Want)))
				return *It;
		}
		UE_LOG(LogTemp, Warning,
			TEXT("[CultUlhu] start '%s' not found (run BuildDarkWilds.py); using default."),
			*Want);
	}
	return Super::ChoosePlayerStart_Implementation(Player);
}

APawn* ACultUlhuPlayGameMode::SpawnDefaultPawnFor_Implementation(
	AController* NewPlayer, AActor* StartSpot)
{
	if (GetWorld() && GetWorld()->GetName().Contains(TEXT("L_MainMenu")))
		return nullptr;
 if(auto* G=Cast<UCultUlhuGameInstance>(GetGameInstance())) if(G->HostedRoomLobby&&NewPlayer&&NewPlayer->IsLocalController()) return nullptr;
	if(auto* G=Cast<UCultUlhuGameInstance>(GetGameInstance())) {
  FTransform Transform=StartSpot?StartSpot->GetActorTransform():FTransform(FVector(0,0,2500));
  if(G->SelectedMode!=ECultPlayMode::FreeRoam) Transform=FTransform(FRotator::ZeroRotator,CultMatchSpawn::Feet(G->SelectedMode,0)+FVector(0,0,2000));
  if(G->SelectedMode==ECultPlayMode::FreeRoam&&GetWorld()->GetNetMode()==NM_ListenServer) for(FConstPlayerControllerIterator It=GetWorld()->GetPlayerControllerIterator();It;++It) if(auto* Other=It->Get()) if(Other!=NewPlayer&&Other->GetPawn()) { Transform.SetLocation(Other->GetPawn()->GetActorLocation()+FVector(0,5000,0)); break; }
  auto* Body=GetWorld()->SpawnActorDeferred<ACultUlhuPlayerPawn>(ACultUlhuPlayerPawn::StaticClass(),Transform,NewPlayer,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
  if(Body) { Body->ConfigurePlayer(G->SelectedCharacterId); Body->FinishSpawning(Transform); } return Body;
 }
 return Super::SpawnDefaultPawnFor_Implementation(NewPlayer, StartSpot);
}

void ACultUlhuPlayGameMode::PreLogin(const FString& Options,const FString& Address,const FUniqueNetIdRepl& Id,FString& Error) {
 Super::PreLogin(Options,Address,Id,Error); if(!Error.IsEmpty()) return;
 if(auto* G=Cast<UCultUlhuGameInstance>(GetGameInstance())) {
  G->ValidateRoomLogin(Options,Error); int Players=0; for(FConstPlayerControllerIterator It=GetWorld()->GetPlayerControllerIterator();It;++It) if(It->Get()&&It->Get()->GetPawn()) ++Players;
  if(Players>=(G->SelectedMode==ECultPlayMode::FreeRoam?8:10)) Error=TEXT("This room is full. Ask the host for another match.");
 }
}
void ACultUlhuPlayGameMode::Logout(AController* Exiting) { if(auto* M=ACultMatchDirector::Find(GetWorld())) M->RemoveHumanPlayer(Exiting); Super::Logout(Exiting); }
