#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interaction/Interactable.h"
#include "LootContainer.generated.h"

class UInventoryComponent;
class ULootContainerDefinition;
class UInteractionComponent;
class UStaticMeshComponent;

/** Replicated world inventory generated once by the Server on first interaction. */
UCLASS()
class MYPROJ2_API ALootContainer : public AActor, public IInteractable
{
	GENERATED_BODY()

public:
	ALootContainer();

	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	virtual bool IsAvailableForInteraction(const ACharacter* RequestingCharacter) const override;
	virtual FText GetInteractionPrompt(const ACharacter* RequestingCharacter) const override;
	virtual void ExecuteInteraction(ACharacter* RequestingCharacter, UInteractionComponent* RequestingComponent) override;

	bool AuthorityEnsureGenerated();
	bool IsGenerated() const { return bGenerated; }

	UInventoryComponent* GetInventoryComponent() const { return Inventory; }

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Loot")
	TObjectPtr<UStaticMeshComponent> ContainerMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Loot")
	TObjectPtr<UInventoryComponent> Inventory;

	/** Must be unique per placed instance. OnConstruction creates one for a new placed actor. */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Loot")
	FGuid ContainerId;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Loot")
	TObjectPtr<ULootContainerDefinition> Definition;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Loot")
	int32 GenerationVersion = 1;

protected:
	virtual void OnConstruction(const FTransform& Transform) override;

	UFUNCTION()
	void OnRep_Generated();

	UFUNCTION()
	void OnRep_OpenState();

	UPROPERTY(ReplicatedUsing = OnRep_Generated, BlueprintReadOnly, Category = "Loot")
	bool bGenerated = false;

	UPROPERTY(ReplicatedUsing = OnRep_OpenState, BlueprintReadOnly, Category = "Loot")
	bool bHasBeenOpened = false;
};
