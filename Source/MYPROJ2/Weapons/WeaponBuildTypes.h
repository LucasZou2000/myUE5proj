#pragma once

#include "CoreMinimal.h"
#include "WeaponBuildTypes.generated.h"

UENUM(BlueprintType)
enum class EWeaponPartSlot : uint8
{
	Barrel,
	Stock,
	Magazine,
	Optic
};

UENUM(BlueprintType)
enum class EWeaponAssemblyRejectReason : uint8
{
	None,
	InvalidRequest,
	StaleRequest,
	NotAllowed,
	Dead,
	NoWeapon,
	MissingItem,
	InvalidDefinition,
	WrongSlot,
	Incompatible,
	SlotOccupied,
	SlotEmpty,
	NoInventorySpace
};

USTRUCT(BlueprintType)
struct MYPROJ2_API FWeaponStatBlock
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon") float Damage = 15.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon") float FireIntervalSeconds = 0.2f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon") float RangeCm = 5000.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon") float SpreadDegrees = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon") float NoiseRadiusCm = 1200.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon") int32 MagazineSize = 30;
};

USTRUCT(BlueprintType)
struct MYPROJ2_API FWeaponStatDelta
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon") float Damage = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon") float FireIntervalSeconds = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon") float RangeCm = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon") float SpreadDegrees = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon") float NoiseRadiusCm = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon") int32 MagazineSize = 0;
};

USTRUCT(BlueprintType)
struct MYPROJ2_API FWeaponStatScale
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon") float Damage = 1.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon") float FireIntervalSeconds = 1.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon") float RangeCm = 1.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon") float SpreadDegrees = 1.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon") float NoiseRadiusCm = 1.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon") float MagazineSize = 1.0f;
};

USTRUCT(BlueprintType)
struct MYPROJ2_API FWeaponStatModifier
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon") FWeaponStatDelta Additive;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon") FWeaponStatScale Multiplicative;
};

USTRUCT(BlueprintType)
struct MYPROJ2_API FInstalledWeaponPart
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Weapon") EWeaponPartSlot Slot = EWeaponPartSlot::Barrel;
	UPROPERTY(BlueprintReadOnly, Category = "Weapon") FGuid SourceInstanceId;
	UPROPERTY(BlueprintReadOnly, Category = "Weapon") FPrimaryAssetId PartDefinitionId;
};
