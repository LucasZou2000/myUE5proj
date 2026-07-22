#include "Base/BaseStashWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/Spacer.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Kismet/GameplayStatics.h"
#include "MYPROJ2PlayerController.h"
#include "Persistence/ExtractionSaveGame.h"
#include "Persistence/ProfileSubsystem.h"

namespace
{
	UTextBlock* MakeText(UWidgetTree* Tree, const FText& Text, int32 FontSize, const FLinearColor& Color)
	{
		UTextBlock* Label = Tree->ConstructWidget<UTextBlock>();
		Label->SetText(Text);
		Label->SetColorAndOpacity(FSlateColor(Color));
		FSlateFontInfo Font = Label->GetFont();
		Font.Size = FontSize;
		Label->SetFont(Font);
		return Label;
	}

	UButton* MakeButton(UWidgetTree* Tree, const FText& Text)
	{
		UButton* Button = Tree->ConstructWidget<UButton>();
		Button->SetContent(MakeText(Tree, Text, 14, FLinearColor::White));
		return Button;
	}
}

void UBaseStashWidget::InitializeForController(AMYPROJ2PlayerController* InController)
{
	OwningRaidController = InController;
	if (IsConstructed())
	{
		Refresh();
	}
}

void UBaseStashWidget::NativeConstruct()
{
	Super::NativeConstruct();
	Refresh();
}

TSharedRef<SWidget> UBaseStashWidget::RebuildWidget()
{
	// UUserWidget builds its Slate root before NativeConstruct. Building the
	// WidgetTree here prevents a native-only widget from becoming an SNullWidget.
	BuildWidgetTree();
	return Super::RebuildWidget();
}

void UBaseStashWidget::BuildWidgetTree()
{
	if (!WidgetTree || WidgetTree->RootWidget)
	{
		return;
	}
	UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>();
	WidgetTree->RootWidget = Root;
	UBorder* Backdrop = WidgetTree->ConstructWidget<UBorder>();
	Backdrop->SetBrushColor(FLinearColor(0.015f, 0.02f, 0.025f, 0.82f));
	Root->AddChildToOverlay(Backdrop);

	UBorder* Panel = WidgetTree->ConstructWidget<UBorder>();
	Panel->SetBrushColor(FLinearColor(0.07f, 0.08f, 0.075f, 1.0f));
	Panel->SetPadding(FMargin(24.0f));
	UOverlaySlot* PanelSlot = Root->AddChildToOverlay(Panel);
	PanelSlot->SetHorizontalAlignment(HAlign_Center);
	PanelSlot->SetVerticalAlignment(VAlign_Center);

	UVerticalBox* Layout = WidgetTree->ConstructWidget<UVerticalBox>();
	Panel->SetContent(Layout);
	Layout->AddChildToVerticalBox(MakeText(WidgetTree, FText::FromString(TEXT("BASE STASH")), 22, FLinearColor(0.88f, 0.92f, 0.85f)));
	USpacer* HeaderGap = WidgetTree->ConstructWidget<USpacer>();
	HeaderGap->SetSize(FVector2D(1.0f, 14.0f));
	Layout->AddChildToVerticalBox(HeaderGap);

	UHorizontalBox* Summaries = WidgetTree->ConstructWidget<UHorizontalBox>();
	Layout->AddChildToVerticalBox(Summaries);
	UVerticalBox* StashColumn = WidgetTree->ConstructWidget<UVerticalBox>();
	Summaries->AddChildToHorizontalBox(StashColumn)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	StashColumn->AddChildToVerticalBox(MakeText(WidgetTree, FText::FromString(TEXT("WAREHOUSE 5 x 20")), 14, FLinearColor(0.65f, 0.78f, 0.68f)));
	StashSummaryText = MakeText(WidgetTree, FText::GetEmpty(), 13, FLinearColor::White);
	StashColumn->AddChildToVerticalBox(StashSummaryText);
	USpacer* Middle = WidgetTree->ConstructWidget<USpacer>();
	Middle->SetSize(FVector2D(36.0f, 1.0f));
	Summaries->AddChildToHorizontalBox(Middle);
	UVerticalBox* LoadoutColumn = WidgetTree->ConstructWidget<UVerticalBox>();
	Summaries->AddChildToHorizontalBox(LoadoutColumn)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	LoadoutColumn->AddChildToVerticalBox(MakeText(WidgetTree, FText::FromString(TEXT("RAID LOADOUT")), 14, FLinearColor(0.70f, 0.72f, 0.82f)));
	LoadoutSummaryText = MakeText(WidgetTree, FText::GetEmpty(), 13, FLinearColor::White);
	LoadoutColumn->AddChildToVerticalBox(LoadoutSummaryText);

	USpacer* ActionGap = WidgetTree->ConstructWidget<USpacer>();
	ActionGap->SetSize(FVector2D(1.0f, 18.0f));
	Layout->AddChildToVerticalBox(ActionGap);
	UHorizontalBox* Actions = WidgetTree->ConstructWidget<UHorizontalBox>();
	Layout->AddChildToVerticalBox(Actions);
	UButton* Withdraw = MakeButton(WidgetTree, FText::FromString(TEXT("TAKE ALL")));
	Withdraw->OnClicked.AddDynamic(this, &UBaseStashWidget::HandleWithdrawAll);
	Actions->AddChildToHorizontalBox(Withdraw);
	UButton* Deposit = MakeButton(WidgetTree, FText::FromString(TEXT("STORE ALL")));
	Deposit->OnClicked.AddDynamic(this, &UBaseStashWidget::HandleDepositAll);
	Actions->AddChildToHorizontalBox(Deposit);
	UButton* EnterRaid = MakeButton(WidgetTree, FText::FromString(TEXT("ENTER RAID")));
	EnterRaid->OnClicked.AddDynamic(this, &UBaseStashWidget::HandleEnterRaid);
	Actions->AddChildToHorizontalBox(EnterRaid);
	UButton* Close = MakeButton(WidgetTree, FText::FromString(TEXT("X")));
	Close->OnClicked.AddDynamic(this, &UBaseStashWidget::HandleClose);
	Actions->AddChildToHorizontalBox(Close);

	StatusText = MakeText(WidgetTree, FText::GetEmpty(), 12, FLinearColor(0.95f, 0.78f, 0.48f));
	Layout->AddChildToVerticalBox(StatusText);
}

void UBaseStashWidget::Refresh()
{
	UProfileSubsystem* ProfileSubsystem = GetGameInstance() ? GetGameInstance()->GetSubsystem<UProfileSubsystem>() : nullptr;
	const UExtractionSaveGame* Profile = ProfileSubsystem ? ProfileSubsystem->GetProfile() : nullptr;
	if (!Profile || !StashSummaryText || !LoadoutSummaryText)
	{
		return;
	}
	StashSummaryText->SetText(FText::FromString(FString::Printf(TEXT("Currency: %lld\n%s"),
		Profile->StashCurrency, *ProfileSubsystem->GetInventorySummary(Profile->Stash))));
	LoadoutSummaryText->SetText(FText::FromString(FString::Printf(TEXT("Currency: %lld\n%s"),
		Profile->PreparedLoadout.Currency, *ProfileSubsystem->GetInventorySummary(Profile->PreparedLoadout.Inventory))));
}

void UBaseStashWidget::HandleWithdrawAll()
{
	UProfileSubsystem* Profile = GetGameInstance() ? GetGameInstance()->GetSubsystem<UProfileSubsystem>() : nullptr;
	const bool bSuccess = Profile && Profile->MoveAllStashToLoadout();
	StatusText->SetText(FText::FromString(bSuccess ? TEXT("All warehouse contents moved to loadout.") : TEXT("Could not take all: loadout capacity or save failed.")));
	Refresh();
}

void UBaseStashWidget::HandleDepositAll()
{
	UProfileSubsystem* Profile = GetGameInstance() ? GetGameInstance()->GetSubsystem<UProfileSubsystem>() : nullptr;
	const bool bSuccess = Profile && Profile->MoveAllLoadoutToStash();
	StatusText->SetText(FText::FromString(bSuccess ? TEXT("All loadout contents stored.") : TEXT("Could not store all: stash capacity or save failed.")));
	Refresh();
}

void UBaseStashWidget::HandleEnterRaid()
{
	UProfileSubsystem* Profile = GetGameInstance() ? GetGameInstance()->GetSubsystem<UProfileSubsystem>() : nullptr;
	if (!Profile || !Profile->BeginRaidFromPreparation())
	{
		StatusText->SetText(FText::FromString(TEXT("Could not prepare raid loadout.")));
		return;
	}
	UGameplayStatics::OpenLevel(this, FName(TEXT("L_Test_Network")));
}

void UBaseStashWidget::HandleClose()
{
	RemoveFromParent();
}
