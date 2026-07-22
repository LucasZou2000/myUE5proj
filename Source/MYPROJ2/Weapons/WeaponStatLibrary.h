#pragma once

#include "CoreMinimal.h"
#include "Weapons/WeaponBuildTypes.h"
#include "WeaponStatLibrary.generated.h"

class UWeaponPartDefinition;

UCLASS()
class MYPROJ2_API UWeaponStatLibrary : public UObject
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "Weapon")
	static FWeaponStatBlock BuildStats(const FWeaponStatBlock& BaseStats,
		const TArray<UWeaponPartDefinition*>& Parts);

	static FWeaponStatBlock BuildStatsFromModifiers(const FWeaponStatBlock& BaseStats,
		const TArray<FWeaponStatModifier>& Modifiers);
};
