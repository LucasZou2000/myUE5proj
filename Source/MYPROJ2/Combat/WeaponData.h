#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "WeaponData.generated.h"

/**
 * Data-only weapon descriptor. M2 ships exactly one instance: DA_Weapon_TestRifle.
 * Per m2-combat.md Step 3:
 *   - No inventory, no pickup, no equip switching in M2.
 *   - Ammo, Damage, Range, FireRate consumed by CombatComponent.
 *   - WeaponClass is the AMYPROJ2Weapon subclass to spawn on the Character.
 */
UCLASS(BlueprintType)
class MYPROJ2_API UWeaponData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:

	/** Human-readable name for HUD/debug. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon")
	FText DisplayName;

	/** Weapon actor class spawned on the owning Character at BeginPlay. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon")
	TSubclassOf<class AMYPROJ2Weapon> WeaponClass;

	/** Shots per minute. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat", meta = (ClampMin = "1.0"))
	float FireRate = 300.f;

	/** Per-shot damage. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat", meta = (ClampMin = "0.0"))
	float Damage = 15.f;

	/** Maximum trace distance (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat", meta = (ClampMin = "0.0"))
	float Range = 5000.f;

	/** Total ammo for M2 (no reload). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat", meta = (ClampMin = "0"))
	int32 Ammo = 30;
};
