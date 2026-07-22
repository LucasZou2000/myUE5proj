#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BaseStashWidget.generated.h"

class AMYPROJ2PlayerController;
class UTextBlock;

/** Minimal local base screen for M5 preparation. It delegates all state changes to UProfileSubsystem. */
UCLASS()
class MYPROJ2_API UBaseStashWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void InitializeForController(AMYPROJ2PlayerController* InController);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;

private:
	void BuildWidgetTree();
	void Refresh();

	UFUNCTION()
	void HandleWithdrawAll();

	UFUNCTION()
	void HandleDepositAll();

	UFUNCTION()
	void HandleEnterRaid();

	UFUNCTION()
	void HandleClose();

	UPROPERTY()
	TObjectPtr<AMYPROJ2PlayerController> OwningRaidController;

	UPROPERTY()
	TObjectPtr<UTextBlock> StashSummaryText;

	UPROPERTY()
	TObjectPtr<UTextBlock> LoadoutSummaryText;

	UPROPERTY()
	TObjectPtr<UTextBlock> StatusText;
};
