// CULT-ULHU character select implementation.
#include "UI/CultCharacterSelectWidget.h"

#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/UniformGridPanel.h"
#include "Components/ScrollBox.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/Border.h"

#include "UI/CultCharacterCardWidget.h"
#include "CultUlhuGameInstance.h"

namespace
{
	const FLinearColor kBack(0.015f, 0.01f, 0.03f, 1.0f);
	const FLinearColor kGold(0.85f, 0.68f, 0.35f, 1.0f);
	const FLinearColor kBone(0.82f, 0.80f, 0.74f, 1.0f);
	const FLinearColor kDim(0.55f, 0.52f, 0.60f, 1.0f);
	const FLinearColor kGo(0.16f, 0.34f, 0.18f, 1.0f);
	const FLinearColor kGoHover(0.24f, 0.48f, 0.26f, 1.0f);

	UTextBlock* MakeText(const FString& Str, float Size, const FLinearColor& Color)
	{
		UTextBlock* T = NewObject<UTextBlock>();
		T->SetText(FText::FromString(Str));
		FSlateFontInfo F = T->GetFont();
		F.Size = static_cast<int32>(Size);
		T->SetFont(F);
		T->SetColorAndOpacity(FSlateColor(Color));
		T->SetJustification(ETextJustify::Center);
		return T;
	}

	UButton* MakeButton(const FString& Label, float Size, const FLinearColor& Normal,
	                    const FLinearColor& Hover)
	{
		UButton* B = NewObject<UButton>();
		FButtonStyle S = B->GetStyle();
		S.Normal.TintColor = FSlateColor(Normal);
		S.Hovered.TintColor = FSlateColor(Hover);
		S.Pressed.TintColor = FSlateColor(Hover * 1.4f);
		B->SetStyle(S);
		UTextBlock* T = MakeText(Label, Size, kBone);
		B->AddChild(T);
		B->SetClickMethod(EButtonClickMethod::DownAndUp);
		return B;
	}
}

void UCultCharacterSelectWidget::NativeConstruct()
{
	Super::NativeConstruct();
	BuildLayout();
}

void UCultCharacterSelectWidget::BuildLayout()
{
	Root_ = NewObject<UCanvasPanel>(this);
	if (UBorder* Back = NewObject<UBorder>(this))
	{
		Back->SetBrushColor(kBack);
		if (UCanvasPanelSlot* S = Root_->AddChildToCanvas(Back))
		{
			S->SetAnchors(FAnchors(0, 0, 1, 1));
			S->SetOffsets(FMargin(0));
		}
	}

	UVerticalBox* Col = NewObject<UVerticalBox>(this);
	if (UCanvasPanelSlot* S = Root_->AddChildToCanvas(Col))
	{
		S->SetAnchors(FAnchors(0.5f, 0.5f, 0.5f, 0.5f));
		S->SetAlignment(FVector2D(0.5f, 0.5f));
		S->SetAutoSize(true);
	}

	TitleText_ = MakeText(TEXT("CHOOSE YOUR ENTITY"), 44, kGold);
	if (UVerticalBoxSlot* S = Col->AddChildToVerticalBox(TitleText_)) S->SetPadding(FMargin(0, 0, 0, 6));

	DetailText_ = MakeText(TEXT(""), 18, kDim);
	if (UVerticalBoxSlot* S = Col->AddChildToVerticalBox(DetailText_)) S->SetPadding(FMargin(0, 0, 0, 12));

	UScrollBox* Scroll = NewObject<UScrollBox>(this);
	Grid_ = NewObject<UUniformGridPanel>(this);
	Grid_->SetSlotPadding(FMargin(8));
	Scroll->AddChild(Grid_);
	Scroll->SetConsumeMouseWheel(EConsumeMouseWheel::WhenScrollingPossible);
	if (UVerticalBoxSlot* S = Col->AddChildToVerticalBox(Scroll))
	{
		S->SetPadding(FMargin(0, 0, 0, 16));
	}

	UHorizontalBox* Row = NewObject<UHorizontalBox>(this);
	BackButton_ = MakeButton(TEXT("BACK"), 22,
		FLinearColor(0.14f, 0.08f, 0.10f, 1.0f), FLinearColor(0.26f, 0.14f, 0.18f, 1.0f));
	BackButton_->OnClicked.AddDynamic(this, &UCultCharacterSelectWidget::OnBackClicked);
	PlayButton_ = MakeButton(TEXT("ENTER THE WILDS"), 26, kGo, kGoHover);
	PlayButton_->OnClicked.AddDynamic(this, &UCultCharacterSelectWidget::OnPlayClicked);
	if (UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(BackButton_)) S->SetPadding(FMargin(0, 0, 24, 0));
	Row->AddChildToHorizontalBox(PlayButton_);
	Col->AddChildToVerticalBox(Row);

	if (!WidgetTree)
		WidgetTree = NewObject<UWidgetTree>(this, TEXT("WidgetTree"));
	WidgetTree->RootWidget = Root_;
}

void UCultCharacterSelectWidget::RebuildGrid()
{
	if (!Grid_) BuildLayout();
	Grid_->ClearChildren();
	Cards_.Empty();

	const TArray<FCultRosterEntry>& R = UCultUlhuGameInstance::Roster();
	const int32 Cols = 5;
	for (int32 i = 0; i < R.Num(); ++i)
	{
		UCultCharacterCardWidget* Card = CreateWidget<UCultCharacterCardWidget>(
			this, UCultCharacterCardWidget::StaticClass());
		Card->Setup(i, R[i].DisplayName, R[i].Flavor, this);
		Grid_->AddChildToUniformGrid(Card, i / Cols, i % Cols);
		Cards_.Add(Card);
	}
	RefreshSelection();
}

void UCultCharacterSelectWidget::SelectCharacter(int32 Index)
{
	const TArray<FCultRosterEntry>& R = UCultUlhuGameInstance::Roster();
	if (Index < 0 || Index >= R.Num()) return;
	SelectedIndex_ = Index;
	if (auto* GI = Cast<UCultUlhuGameInstance>(GetGameInstance()))
		GI->SelectedCharacterId = R[Index].Id;
	RefreshSelection();
}

void UCultCharacterSelectWidget::RefreshSelection()
{
	const TArray<FCultRosterEntry>& R = UCultUlhuGameInstance::Roster();
	for (int32 i = 0; i < Cards_.Num(); ++i)
		Cards_[i]->SetSelected(i == SelectedIndex_);
	if (DetailText_ && R.IsValidIndex(SelectedIndex_))
	{
		const FCultRosterEntry& E = R[SelectedIndex_];
		DetailText_->SetText(FText::FromString(
			FString::Printf(TEXT("%s  —  %.0f m of apocalypse"),
				*UCultUlhuGameInstance::ModeDisplayName(Mode_), E.HeightM)));
	}
	if (TitleText_)
		TitleText_->SetText(FText::FromString(
			FString::Printf(TEXT("CHOOSE YOUR ENTITY — %s"),
				*UCultUlhuGameInstance::ModeDisplayName(Mode_).ToUpper())));
}

void UCultCharacterSelectWidget::OnBackClicked()
{
	if (auto* GI = Cast<UCultUlhuGameInstance>(GetGameInstance())) GI->ShowHomeScreen();
}

void UCultCharacterSelectWidget::OnPlayClicked()
{
	if (auto* GI = Cast<UCultUlhuGameInstance>(GetGameInstance())) GI->StartPlay();
}
