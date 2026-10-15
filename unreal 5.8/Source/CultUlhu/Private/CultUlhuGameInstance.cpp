// CULT-ULHU game instance implementation.
#include "CultUlhuGameInstance.h"

#include "Blueprint/UserWidget.h"
#include "Camera/CameraActor.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"

#include "UI/CultHomeScreenWidget.h"
#include "UI/CultCharacterSelectWidget.h"

const TArray<FCultRosterEntry>& UCultUlhuGameInstance::Roster()
{
	// id, display name, one-line flavor, canonical height (m).
	static const TArray<FCultRosterEntry> R = [] {
		TArray<FCultRosterEntry> A;
		auto E = [&A](const TCHAR* Id, const TCHAR* Name, const TCHAR* Flavor, float H) {
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
				if (It->GetActorLabel() == TEXT("MENU_VistaCamera"))
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
	SelectedMode = Mode;
	if (UCultCharacterSelectWidget* Widget =
		CreateWidget<UCultCharacterSelectWidget>(this, UCultCharacterSelectWidget::StaticClass()))
	{
		Widget->SetMode(Mode);
		ShowWidget(Widget, true);
		Widget->RebuildGrid();
	}
}

void UCultUlhuGameInstance::StartPlay()
{
	ClearMenuWidgets();
	if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
	{
		PC->SetInputMode(FInputModeGameOnly());
		PC->bShowMouseCursor = false;
	}
	// The Dark Wilds map is built by unreal 5.8/Tools/BuildDarkWilds.py.
	UGameplayStatics::OpenLevel(GetWorld(), FName(TEXT("/Game/Maps/L_DarkWilds")));
}

void UCultUlhuGameInstance::ReturnToMenu()
{
	ClearMenuWidgets();
	if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
	{
		PC->SetInputMode(FInputModeUIOnly());
		PC->bShowMouseCursor = true;
	}
	UGameplayStatics::OpenLevel(GetWorld(), FName(TEXT("/Game/Maps/L_MainMenu")));
}
