// CULT-ULHU: Tab stats-overlay widget.
//
// Data comes from the core StatsPanel::gather() (src/ui/StatsPanel.h):
// cult counts by state, total kills, 12 belief exertion gauges, power,
// insurrection risk, and the per-player KDA table in multiplayer.
// This widget only presents; refresh is driven by the Tab toggle.
//
// Layout is authored in the Widget Blueprint; see Docs/UmgWidgetSpecs.md.
// VERIFY IN EDITOR: BindWidget names below match the Blueprint exactly.
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"

#include "ui/StatsPanel.h"

#include "CultStatsPanelWidget.generated.h"

class UTextBlock;
class UProgressBar;
class UVerticalBox;

UCLASS(Blueprintable)
class CULTULHUCORE_API UCultStatsPanelWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// Toggled by the Tab input action (see Docs/EnhancedInputMapping.md).
	UFUNCTION(BlueprintCallable, Category = "CultUlhu|Stats")
	void SetOverlayVisible(bool bVisible);

	// Pulls fresh StatsData from the core and refreshes all bound widgets.
	// Called on show and at ~4 Hz while visible.
	UFUNCTION(BlueprintCallable, Category = "CultUlhu|Stats")
	void Refresh();

protected:
	virtual void NativeConstruct() override;

	// VERIFY IN EDITOR: these BindWidget names must exist in the Blueprint.
	UPROPERTY(meta = (BindWidget))
	UTextBlock* PowerText = nullptr;

	UPROPERTY(meta = (BindWidget))
	UTextBlock* KillsText = nullptr;

	UPROPERTY(meta = (BindWidget))
	UProgressBar* InsurrectionBar = nullptr;

	// 12 belief gauges, created dynamically (one per Belief).
	UPROPERTY(meta = (BindWidget))
	UVerticalBox* BeliefGaugeList = nullptr;

	// KDA rows, created dynamically in multiplayer.
	UPROPERTY(meta = (BindWidget))
	UVerticalBox* KdaList = nullptr;

private:
	void RefreshBeliefGauges(const cultulhu::StatsData& Data);
	void RefreshKda(const cultulhu::StatsData& Data);
};
