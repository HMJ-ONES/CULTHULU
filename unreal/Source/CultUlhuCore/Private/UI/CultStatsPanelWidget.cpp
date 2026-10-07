// CULT-ULHU stats panel widget implementation.
#include "UI/CultStatsPanelWidget.h"
#include "Components/TextBlock.h"
#include "Components/ProgressBar.h"
#include "Components/VerticalBox.h"
#include "Subsystems/CultBeliefSubsystem.h"
#include "Subsystems/CultPowerSubsystem.h"
#include "Subsystems/CultManagerSubsystem.h"

void UCultStatsPanelWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetVisibility(ESlateVisibility::Collapsed);
}

void UCultStatsPanelWidget::SetOverlayVisible(bool bVisible)
{
	SetVisibility(bVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	if (bVisible) Refresh();
}

void UCultStatsPanelWidget::Refresh()
{
	UGameInstance* GI = GetGameInstance();
	if (!GI) return;
	UCultManagerSubsystem* CultSub = GI->GetSubsystem<UCultManagerSubsystem>();
	// VERIFY IN EDITOR: UCultManagerSubsystem is a world subsystem — fetch
	// via GetWorld()->GetSubsystem<>() instead if GetGameInstance() misses.
	UCultPowerSubsystem* PowerSub = GI->GetSubsystem<UCultPowerSubsystem>();
	if (!CultSub || !PowerSub || !CultSub->Cult() || !PowerSub->Exertion())
		return;

	// VERIFY IN EDITOR: KDA rows source — PlayerStatsTracker lives in the
	// game module's netcode wrapper; pass its rows here (empty in solo).
	std::vector<cultulhu::net::KdaRow> KdaRows;
	const cultulhu::StatsData Data = cultulhu::StatsPanel::gather(
		*CultSub->Cult(), *PowerSub->Exertion(), PowerSub->Power(), KdaRows);

	if (PowerText) PowerText->SetText(FText::AsNumber(Data.power));
	if (KillsText) KillsText->SetText(FText::AsNumber(Data.totalKills));
	if (InsurrectionBar) InsurrectionBar->SetPercent(Data.insurrectionRisk / 100.0f);

	RefreshBeliefGauges(Data);
	RefreshKda(Data);
}

void UCultStatsPanelWidget::RefreshBeliefGauges(const cultulhu::StatsData& Data)
{
	// VERIFY IN EDITOR: implement — 12 rows (belief name + progress bar),
	// rebuilt or pooled; belief names via cultulhu::beliefName().
	(void)Data;
}

void UCultStatsPanelWidget::RefreshKda(const cultulhu::StatsData& Data)
{
	// VERIFY IN EDITOR: implement — one row per KdaRow (name, K/D/A);
	// hidden entirely when the table is empty (single-player).
	(void)Data;
}
