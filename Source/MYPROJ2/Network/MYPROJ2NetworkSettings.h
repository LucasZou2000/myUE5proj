// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "MYPROJ2NetworkSettings.generated.h"

/**
 * Centralized network tuning values. Developer settings only; not replicated.
 * All network tolerances live here instead of scattered literals.
 */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "MYPROJ2 Network Settings"))
class MYPROJ2_API UMYPROJ2NetworkSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:

	UMYPROJ2NetworkSettings();

	/** Client aim update send rate cap (Hz). */
	UPROPERTY(Config, EditAnywhere, Category = "Network|Aim", meta = (ClampMin = "1.0", ClampMax = "60.0"))
	float AimSendRateHz = 15.0f;

	/** Max distance from character to interaction target (cm). */
	UPROPERTY(Config, EditAnywhere, Category = "Network|Interaction", meta = (ClampMin = "0.0"))
	float MaxInteractDistance = 250.0f;

	/** Max tolerated distance between client-supplied fire origin and server-derived origin (cm). */
	UPROPERTY(Config, EditAnywhere, Category = "Network|Combat", meta = (ClampMin = "0.0"))
	float MaxFireOriginError = 150.0f;

	/** Max tolerated angular error between client aim direction and server-derived direction (degrees). */
	UPROPERTY(Config, EditAnywhere, Category = "Network|Combat", meta = (ClampMin = "0.0", ClampMax = "180.0"))
	float MaxAimErrorDegrees = 35.0f;

	static const UMYPROJ2NetworkSettings* Get();
};
