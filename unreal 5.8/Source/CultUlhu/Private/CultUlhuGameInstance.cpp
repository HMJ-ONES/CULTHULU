#include "CultUlhuGameInstance.h"
#include "CultFreeRoamSave.h"
#include "UI/CultFreeRoamMenuWidget.h"
#include "TimerManager.h"
#include "Engine/Engine.h"
#include "CultUlhuPlayerPawn.h"
// CULT-ULHU game instance implementation.
#include "CultActorLabels.h"

#include "Blueprint/UserWidget.h"
#include "Camera/CameraActor.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"

#include "UI/CultHomeScreenWidget.h"
#include "UI/CultCharacterSelectWidget.h"
#include "CultCanonicalKit.h"
#include "UI/CultBeliefSelectWidget.h"

const TArray<FCultRosterEntry>& UCultUlhuGameInstance::Roster()
{
	// id, display name, one-line flavor, canonical height (m).
	static const TArray<FCultRosterEntry> R = [] {
		TArray<FCultRosterEntry> A;
		auto E = [&A](const TCHAR* Id, const TCHAR* Name, const TCHAR* Flavor, float H) {
			if(!CultCanonicalKit::IsReleased(Id)) return;
			FCultRosterEntry E; E.Id = Id; E.DisplayName = Name; E.Flavor = Flavor; E.HeightM = H;
			A.Add(E);
		};
		E(TEXT("bast"),           TEXT("Bast, Queen of the Cats of Ulthar"),   TEXT("The cats remember every kindness. And every debt."), 25.0f);
		E(TEXT("bokrug"),         TEXT("Bokrug, the Great Water Lizard"),      TEXT("Sarnath fell in a single night. He remembers."),      28.0f);
		E(TEXT("cthulhu_avatar"), TEXT("Cthulhu, the Dreaming God"),           TEXT("The dreaming god walks. Cities drown in his shadow."), 30.0f);
		E(TEXT("dagon"),          TEXT("Dagon, the Deep Father"),              TEXT("The tide brings his children. It always does."),        28.0f);
		E(TEXT("elder_mind"),     TEXT("The Elder Mind, Protector of the Yith"), TEXT("It has read tomorrow. It disapproves of your plans."), 30.0f);
		E(TEXT("ghatanothoa"),    TEXT("Ghatanothoa, Lord of the Volcano"),    TEXT("Look upon him and be still. Forever is optional."),     28.0f);
		E(TEXT("hastur"),         TEXT("Hastur, the King in Yellow"),          TEXT("Have you seen the Yellow Sign? You have now."),         22.0f);
		E(TEXT("howling_eye"),    TEXT("The Howling Eye, Heart of the Polyp Scourge"), TEXT("The storm never stops screaming. Neither will you."), 30.0f);
		E(TEXT("hypnos"),         TEXT("Hypnos, Lord of Sleep"),               TEXT("He owns the world behind your eyes. Visit often."),     22.0f);
		E(TEXT("mi_go"),          TEXT("The Mi-Go, Fungi from Yuggoth"),       TEXT("Surgery is just diplomacy with sharper tools."),        20.0f);
		E(TEXT("mother_hydra"),   TEXT("Mother Hydra, the Deep Mother"),       TEXT("She does not fight her wars. She mothers them."),       26.0f);
		E(TEXT("nodens"),         TEXT("Nodens, Lord of the Great Abyss"),     TEXT("The abyss does not chase. It simply arrives."),         25.0f);
		E(TEXT("nug_and_yeb"),    TEXT("Nug and Yeb, the Twin Blasphemies"),   TEXT("Two hungers, one shadow. Do not count them twice."),    22.0f);
		E(TEXT("nyarlathotep"),   TEXT("Nyarlathotep, the Crawling Chaos"),    TEXT("A thousand forms, and every one of them is lying."),    20.0f);
		E(TEXT("rhan_tegoth"),    TEXT("Rhan-Tegoth, Terror of the Museum"),   TEXT("The exhibit walked out. The building did not survive."), 24.0f);
		E(TEXT("sghllor"),        TEXT("Sghllor, the Colour Out of Space"),    TEXT("It fell from the sky and the farm was never right."),   30.0f);
		E(TEXT("shoggoths"),      TEXT("The Shoggoths, the Shapeless Ones"),   TEXT("Tekeli-li! The mass remembers every shape it hated."),  25.0f);
		E(TEXT("shub_niggurath"), TEXT("Shub-Niggurath, the Black Goat"),      TEXT("The woods are full of her children. So is everywhere."), 28.0f);
		E(TEXT("yig"),            TEXT("Yig, Father of Serpents"),             TEXT("He does not hate. He remembers. That is worse."),       24.0f);
		E(TEXT("yog_sothoth"),    TEXT("Yog-Sothoth, the All-in-One"),         TEXT("The gate is open. The gate was always open."),          30.0f);
		return A;
	}();
	return R;
}

FString UCultUlhuGameInstance::ModeDisplayName(ECultPlayMode Mode)
{
	switch (Mode)
	{
	case ECultPlayMode::FreeRoam:  return TEXT("Free Roam");
	case ECultPlayMode::Domination: return TEXT("Domination — 5v5 capture");
	case ECultPlayMode::Dotgoo:    return TEXT("Defense of the Great Old Ones — 5v5");
	}
	return TEXT("Free Roam");
}

FString UCultUlhuGameInstance::ModeStartLabel(ECultPlayMode Mode)
{
	// Matches the WILD_PlayerStart_* labels placed by BuildDarkWilds.py.
	switch (Mode)
	{
	case ECultPlayMode::FreeRoam:  return TEXT("WILD_PlayerStart_CavernRim");
	case ECultPlayMode::Domination: return TEXT("WILD_PlayerStart_Plateau");
	case ECultPlayMode::Dotgoo:    return TEXT("WILD_PlayerStart_SwampEdge");
	}
	return TEXT("WILD_PlayerStart_Plateau");
}

void UCultUlhuGameInstance::ShowWidget(UUserWidget* Widget, bool bUIOnly)
{
	if (!Widget) return;
	Widget->SetIsFocusable(true);
	ClearMenuWidgets();
	MenuWidgets_.Add(Widget);
	Widget->AddToViewport(100);
	if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
	{
		if (bUIOnly)
		{
			FInputModeUIOnly Mode;
			Mode.SetWidgetToFocus(Widget->TakeWidget());
			PC->SetInputMode(Mode);
			PC->bShowMouseCursor = true;
		}
		else
		{
			PC->SetInputMode(FInputModeGameOnly());
			PC->bShowMouseCursor = false;
		}
	}
}

void UCultUlhuGameInstance::ClearMenuWidgets()
{
	for (UUserWidget* W : MenuWidgets_)
		if (W) W->RemoveFromParent();
	MenuWidgets_.Empty();
}

void UCultUlhuGameInstance::ShowHomeScreen()
{
	if (UWorld* W = GetWorld())
	{
		FString Map = W->GetName();
		if (!Map.Contains(TEXT("L_MainMenu")))
			return; // only auto-show on the menu map
	}
	if (UCultHomeScreenWidget* Widget =
		CreateWidget<UCultHomeScreenWidget>(this, UCultHomeScreenWidget::StaticClass()))
	{
		ShowWidget(Widget, true);
	}

	// Menu vista: if the map provides a MENU_VistaCamera (built by
	// BuildMainMenu.py), look through it so the menu floats over a live
	// eldritch vista instead of a black void. Harmless if absent.
	if (UWorld* W = GetWorld())
	{
		if (APlayerController* PC = W->GetFirstPlayerController())
		{
			for (TActorIterator<ACameraActor> It(W); It; ++It)
			{
				if (CultActorLabels::Anchor(*It,TEXT("MENU_VistaCamera")))
				{
					PC->SetViewTargetWithBlend(*It, 0.0f);
					break;
				}
			}
		}
	}
}

void UCultUlhuGameInstance::OpenCharacterSelect(ECultPlayMode Mode)
{
 if(!CultCanonicalKit::IsReleased(SelectedCharacterId)) SelectedCharacterId=TEXT("cthulhu_avatar");
	InitialBeliefs_.Reset();
	SelectedMode = Mode;
	if (UCultCharacterSelectWidget* Widget =
		CreateWidget<UCultCharacterSelectWidget>(this, UCultCharacterSelectWidget::StaticClass()))
	{
		Widget->SetMode(Mode);
		ShowWidget(Widget, true);
		Widget->RebuildGrid();
	}
}

bool UCultUlhuGameInstance::SetInitialBeliefs(const TArray<int32>& Choices)
{
 if(SelectedMode!=ECultPlayMode::FreeRoam) { InitialBeliefs_.Reset(); return false; }
 TSet<int32> Unique; for(int32 B:Choices) { if(B<0||B>=12) return false; Unique.Add(B); }
 if(Unique.Num()!=3||Choices.Num()!=3) return false;
 InitialBeliefs_=Choices; return true;
}
void UCultUlhuGameInstance::OpenBeliefSelect()
{
 if(SelectedMode!=ECultPlayMode::FreeRoam) { StartPlay(); return; }
 InitialBeliefs_.Reset();
 if(auto* Widget=CreateWidget<UCultBeliefSelectWidget>(this,UCultBeliefSelectWidget::StaticClass())) ShowWidget(Widget,true);
}
void UCultUlhuGameInstance::StartPlay()
{
 if(!CultCanonicalKit::IsReleased(SelectedCharacterId)) { OpenCharacterSelect(SelectedMode); return; }
 if(SelectedMode==ECultPlayMode::FreeRoam&&InitialBeliefs_.Num()!=3) { OpenBeliefSelect(); return; }
 if(SelectedMode!=ECultPlayMode::FreeRoam) InitialBeliefs_.Reset();
 cultulhu::CharacterDef Definition; if(!CultCanonicalKit::Load(SelectedCharacterId,Definition)) return;
	bPlayChosen = true;
 FreeRoamReady=false; FreeRoamMenuOpen=false; PendingFreeRoam_.Reset();
 if(SelectedMode==ECultPlayMode::FreeRoam) FreeRoamSeed=FMath::Rand();
	ClearMenuWidgets();
	if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
	{
		PC->SetInputMode(FInputModeGameOnly());
		PC->bShowMouseCursor = false;
	}
	// The Dark Wilds map is built by unreal/Tools/BuildDarkWilds.py.
	UGameplayStatics::OpenLevel(GetWorld(), FName(TEXT("/Game/Maps/L_DarkWilds")));
}

void UCultUlhuGameInstance::ReturnToMenu()
{
	bPlayChosen = false; FreeRoamReady=false; FreeRoamMenuOpen=false;
 UGameplayStatics::SetGamePaused(this,false);
 GetWorld()->GetTimerManager().ClearTimer(AutosaveTimer_);
	ClearMenuWidgets();
	if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
	{
		PC->SetInputMode(FInputModeUIOnly());
		PC->bShowMouseCursor = true;
	}
	UGameplayStatics::OpenLevel(GetWorld(), FName(TEXT("/Game/Maps/L_MainMenu")));
}

bool UCultUlhuGameInstance::HasFreeRoamSave() const { return CultFreeRoamSave::Exists(FreeRoamSaveSlot); }
void UCultUlhuGameInstance::ToggleFreeRoamMenu() {
 if(SelectedMode!=ECultPlayMode::FreeRoam&&FreeRoamReady) return;
 if(FreeRoamMenuOpen) { ResumeFreeRoam(); return; }
 if(auto* P=Cast<ACultUlhuPlayerPawn>(UGameplayStatics::GetPlayerPawn(this,0))) { P->RoutePlayerInput(ECultPlayerInput::Heavy,false); P->RoutePlayerInput(ECultPlayerInput::Primary,false); P->StopJumping(); P->SetSprinting(false); }
 FreeRoamMenuOpen=true;
 if(FreeRoamReady&&GetWorld()->GetNetMode()==NM_Standalone) UGameplayStatics::SetGamePaused(this,true);
 auto* Widget=CreateWidget<UCultFreeRoamMenuWidget>(this,UCultFreeRoamMenuWidget::StaticClass()); ShowWidget(Widget,true);
}
void UCultUlhuGameInstance::ResumeFreeRoam() {
 if(!FreeRoamReady) { FreeRoamMenuOpen=false; ShowHomeScreen(); return; }
 FreeRoamMenuOpen=false; ClearMenuWidgets(); UGameplayStatics::SetGamePaused(this,false);
 if(auto* PC=GetWorld()->GetFirstPlayerController()) { PC->SetInputMode(FInputModeGameOnly()); PC->bShowMouseCursor=false; PC->ResetIgnoreMoveInput(); PC->ResetIgnoreLookInput(); }
}
bool UCultUlhuGameInstance::SaveFreeRoam() {
 TArray<uint8> Data; FString Error;
 if(!FreeRoamReady||!CultFreeRoamSave::Capture(GetWorld(),Data,Error)||!CultFreeRoamSave::Write(Data,Error,FreeRoamSaveSlot)) { SaveStatus=Error.IsEmpty()?TEXT("The world is not ready to save."):Error; return false; }
 SaveStatus=TEXT("World saved  /  ")+FDateTime::Now().ToString(TEXT("%H:%M:%S"));
 UE_LOG(LogTemp,Display,TEXT("[CultSave] %s (%d bytes)"),*SaveStatus,Data.Num()); return true;
}
bool UCultUlhuGameInstance::ContinueFreeRoam() {
 if(GetWorld()->GetNetMode()==NM_Client) { SaveStatus=TEXT("The host controls world loading."); return false; }
 if(GetWorld()->GetNetMode()==NM_ListenServer) { SaveStatus=TEXT("Save and return to the menu before loading a co-op world."); return false; }
 TArray<uint8> Data; FString Error; int32 Seed; FString Character; TArray<int32> Beliefs;
 if(!CultFreeRoamSave::Read(Data,Error,FreeRoamSaveSlot)||!CultFreeRoamSave::Inspect(Data,Seed,Character,Beliefs,Error)) { SaveStatus=Error; return false; }
 PendingFreeRoam_=MoveTemp(Data); FreeRoamSeed=Seed; SelectedCharacterId=Character; InitialBeliefs_=Beliefs; SelectedMode=ECultPlayMode::FreeRoam;
 bPlayChosen=true; FreeRoamReady=false; FreeRoamMenuOpen=false; ClearMenuWidgets(); UGameplayStatics::SetGamePaused(this,false);
 UGameplayStatics::OpenLevel(this,TEXT("/Game/Maps/L_DarkWilds")); return true;
}
void UCultUlhuGameInstance::FinishFreeRoamLoad() {
 if(!PendingFreeRoam_.IsEmpty()) { FString Error; if(!CultFreeRoamSave::Restore(GetWorld(),PendingFreeRoam_,Error)) { SaveStatus=TEXT("Load failed: ")+Error; FreeRoamReady=false; PendingFreeRoam_.Reset(); ToggleFreeRoamMenu(); return; } PendingFreeRoam_.Reset(); SaveStatus=TEXT("Saved world restored."); }
 FreeRoamReady=true; FreeRoamMenuOpen=false;
 GetWorld()->GetTimerManager().SetTimer(AutosaveTimer_,FTimerDelegate::CreateWeakLambda(this,[this]() { if(FreeRoamReady&&GetWorld()->GetNetMode()!=NM_Client) SaveFreeRoam(); }),60.f,true);
}
void UCultUlhuGameInstance::BeginFreeRoamSession() { FinishFreeRoamLoad(); }
void UCultUlhuGameInstance::ExitFreeRoam() {
 if(GetWorld()->GetNetMode()!=NM_Client&&!SaveFreeRoam()) return;
 ReturnToMenu();
}
bool UCultUlhuGameInstance::HostFreeRoam() {
 if(!FreeRoamReady||GetWorld()->GetNetMode()!=NM_Standalone) { SaveStatus=TEXT("Enter or continue a world before hosting."); return false; }
 if(!SaveFreeRoam()) return false;
 FString Error; if(!CultFreeRoamSave::Read(PendingFreeRoam_,Error,FreeRoamSaveSlot)) { SaveStatus=Error; return false; }
 FreeRoamReady=false; FreeRoamMenuOpen=false; ClearMenuWidgets(); UGameplayStatics::SetGamePaused(this,false);
 UGameplayStatics::OpenLevel(this,TEXT("/Game/Maps/L_DarkWilds"),true,TEXT("listen")); return true;
}
bool UCultUlhuGameInstance::JoinFreeRoam(const FString& Input) {
 FString Address=Input.TrimStartAndEnd();
 // Direct IP/hostnames only: never accept travel options or console commands from an edit box.
 if(Address.IsEmpty()||Address.Len()>253) { SaveStatus=TEXT("Enter the host's LAN, VPN or public IP address."); return false; }
 for(TCHAR C:Address) if(!FChar::IsAlnum(C)&&C!=TEXT('.')&&C!=TEXT(':')&&C!=TEXT('-')) { SaveStatus=TEXT("Use a hostname or IP, optionally followed by :7777."); return false; }
 if(FreeRoamReady&&GetWorld()->GetNetMode()!=NM_Client&&!SaveFreeRoam()) return false;
 if(auto* PC=GetWorld()->GetFirstPlayerController()) {
  SelectedMode=ECultPlayMode::FreeRoam; bPlayChosen=true; FreeRoamMenuOpen=false; FreeRoamReady=false; ClearMenuWidgets(); UGameplayStatics::SetGamePaused(this,false);
  PC->ClientTravel(Address,TRAVEL_Absolute); return true;
 }
 SaveStatus=TEXT("No local player is available to join."); return false;
}

#if WITH_DEV_AUTOMATION_TESTS
void StartCultCoopProbe(UCultUlhuGameInstance* Game);
void StartCultPresentationProbe(UCultUlhuGameInstance* Game);
void StartCultCreepWaveProbe(UCultUlhuGameInstance* Game);
#endif
void UCultUlhuGameInstance::Init() { Super::Init();
#if WITH_DEV_AUTOMATION_TESTS
 StartCultCoopProbe(this);
 StartCultPresentationProbe(this);
 StartCultCreepWaveProbe(this);
#endif
 if(GEngine) GEngine->OnNetworkFailure().AddUObject(this,&UCultUlhuGameInstance::NetworkFailure); }
void UCultUlhuGameInstance::Shutdown() { if(GEngine) GEngine->OnNetworkFailure().RemoveAll(this); Super::Shutdown(); }
void UCultUlhuGameInstance::NetworkFailure(UWorld* World,UNetDriver* Driver,ENetworkFailure::Type Failure,const FString& Reason) {
 if(SelectedMode!=ECultPlayMode::FreeRoam||!bPlayChosen) return;
 SaveStatus=TEXT("Co-op connection ended: ")+Reason+TEXT(". Your local saved world is available with Continue."); ReturnToMenu();
}
