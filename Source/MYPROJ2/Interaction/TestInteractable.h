// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interaction/Interactable.h"
#include "TestInteractable.generated.h"

class UStaticMeshComponent;

/**
 * Minimal test interactable for M1 validation. Toggles bActivated on interaction;
 * the change is replicated and visualised via material colour flip on the static mesh.
 */
UCLASS(Blueprintable)
class MYPROJ2_API ATestInteractable : public AActor, public IInteractable
{
	GENERATED_BODY()

public:

	ATestInteractable();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	//~ IInteractable
	virtual bool IsAvailableForInteraction(const ACharacter* RequestingCharacter) const override;
	virtual FText GetInteractionPrompt(const ACharacter* RequestingCharacter) const override;
	virtual void ExecuteInteraction(ACharacter* RequestingCharacter, UInteractionComponent* RequestingComponent) override;
	//~ End IInteractable

protected:

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(ReplicatedUsing = OnRep_Activated, BlueprintReadOnly, Category = "Interaction")
	bool bActivated = false;

	UFUNCTION()
	void OnRep_Activated();

	void ApplyVisualState();
};
