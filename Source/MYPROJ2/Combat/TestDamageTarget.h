#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TestDamageTarget.generated.h"

class UStaticMeshComponent;
class UHealthComponent;

/**
 * Test damageable actor for M2 acceptance. Stationary cube that:
 *   - Carries a HealthComponent.
 *   - Blocks the Visibility trace channel so hitscan finds it.
 *   - Tints based on Health: >50% green, >0% yellow, ==0 red.
 * BP_TestDamageTarget subclasses this for placement; C++ keeps logic simple.
 */
UCLASS()
class MYPROJ2_API ATestDamageTarget : public AActor
{
	GENERATED_BODY()

public:

	ATestDamageTarget();

protected:

	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Target")
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Target")
	TObjectPtr<UHealthComponent> HealthComponent;

private:

	UFUNCTION()
	void OnHealthChanged();

	void ApplyVisualState();

	FTimerHandle HealthPollTimer;
};
