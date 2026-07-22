#pragma once

#include "CoreMinimal.h"
#include "Items/ItemDefinition.h"
#include "Weapons/WeaponBuildTypes.h"
#include "WeaponData.generated.h"

/**
 * Data-only weapon descriptor. M2 ships exactly one instance: DA_Weapon_TestRifle.
 * Per m2-combat.md Step 3:
 *   - No inventory, no pickup, no equip switching in M2.
 *   - Ammo, Damage, Range, FireRate consumed by CombatComponent.
 *   - WeaponClass is the AMYPROJ2Weapon subclass to spawn on the Character.
 */
UCLASS(BlueprintType)
class MYPROJ2_API UWeaponData : public UItemDefinition
{
	GENERATED_BODY()

public:
	UWeaponData();

	/** Weapon actor class spawned on the owning Character at BeginPlay. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon")
	TSubclassOf<class AMYPROJ2Weapon> WeaponClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon")
	FWeaponStatBlock BaseStats;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon")
	FGameplayTagContainer CompatibilityTags;

	/** Keeps an existing M2 asset behavior-identical until its BaseStats are explicitly migrated. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Migration")
	bool bUseLegacyM2Stats = true;

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

	virtual FPrimaryAssetId GetPrimaryAssetId() const override
	{
		return FPrimaryAssetId(TEXT("Weapon"), ItemId);
	}

	FWeaponStatBlock GetEffectiveBaseStats() const;
};
