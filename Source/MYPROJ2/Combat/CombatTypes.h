#pragma once

#include "CoreMinimal.h"
#include "Engine/HitResult.h"
#include "CombatTypes.generated.h"

/**
 * Types shared by Weapon fire, CombatComponent validation, and HealthComponent.
 * Rule per m2-combat.md:
 *   - FFiringContext travels Server → owning Client (Montage) and Server → all (multicast FX).
 *   - FFireRejectionInfo travels Server → owning Client only.
 *   - FFireStartInfo travels owning Client → Server only.
 *   - Server never sends a Transform back for correction. Local fire origin is trusted
 *     within MaxFireOriginTolerance; montage relies on the aim yaw pipeline from M1.
 */

UENUM(BlueprintType)
enum class EFireRejectReason : uint8
{
	None                 UMETA(DisplayName = "None"),
	NoWeapon             UMETA(DisplayName = "No Weapon"),
	NoAmmo               UMETA(DisplayName = "No Ammo"),
	OutOfRange           UMETA(DisplayName = "Out Of Range"),
	RateLimited          UMETA(DisplayName = "Rate Limited"),
	OriginMismatch       UMETA(DisplayName = "Origin Mismatch"),
	InvalidFireDirection UMETA(DisplayName = "Invalid Fire Direction")
};

/** Client → Server. What the shooter actually saw at trigger time. */
USTRUCT(BlueprintType)
struct MYPROJ2_API FFireStartInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Combat")
	FVector FireOrigin = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Combat")
	FVector FireDirection = FVector::ForwardVector;

	UPROPERTY(BlueprintReadOnly, Category = "Combat")
	FVector MuzzleSocket = FVector::ZeroVector;
};

/** Server → owning Client. Authoritative explanation for a rejected trigger. */
USTRUCT(BlueprintType)
struct MYPROJ2_API FFireRejectionInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Combat")
	EFireRejectReason Reason = EFireRejectReason::None;

	UPROPERTY(BlueprintReadOnly, Category = "Combat")
	float ServerTime = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Combat")
	FVector ExpectedOrigin = FVector::ZeroVector;
};

/** Server → owning Client (montage) + Server → all (multicast FX). */
USTRUCT(BlueprintType)
struct MYPROJ2_API FFiringContext
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Combat")
	FVector FireOrigin = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Combat")
	FVector FireDirection = FVector::ForwardVector;

	UPROPERTY(BlueprintReadOnly, Category = "Combat")
	FVector MuzzleSocket = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Combat")
	float FireRate = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Combat")
	int32 AmmoAfterShot = 0;
};
