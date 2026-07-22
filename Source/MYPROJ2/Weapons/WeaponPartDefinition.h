#pragma once

#include "CoreMinimal.h"
#include "Items/ItemDefinition.h"
#include "Weapons/WeaponBuildTypes.h"
#include "WeaponPartDefinition.generated.h"

UCLASS(BlueprintType)
class MYPROJ2_API UWeaponPartDefinition : public UItemDefinition
{
	GENERATED_BODY()

public:
	UWeaponPartDefinition();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon Part")
	EWeaponPartSlot Slot = EWeaponPartSlot::Barrel;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon Part")
	FGameplayTagQuery CompatibleWeaponQuery;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon Part")
	FWeaponStatModifier Modifier;
};
