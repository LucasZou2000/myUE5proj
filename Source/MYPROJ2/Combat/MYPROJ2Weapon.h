#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MYPROJ2Weapon.generated.h"

class UStaticMeshComponent;

/**
 * Visible weapon actor. Owned by the Character; spawned by CombatComponent.
 * M2 scope is intentionally minimal:
 *   - Visible mesh only.
 *   - No ammo rendering, no reload animation, no inventory.
 *   - No special attachment rules; attaches to the Character's mesh socket.
 */
UCLASS()
class MYPROJ2_API AMYPROJ2Weapon : public AActor
{
	GENERATED_BODY()

public:

	AMYPROJ2Weapon();

	/** Attaches the weapon mesh to the owning Character. Safe to call on Server. */
	UFUNCTION(BlueprintCallable, Category = "Weapon")
	void AttachToOwner();

	/** World-space location of the muzzle, for fire feedback FX. */
	UFUNCTION(BlueprintPure, Category = "Weapon")
	FVector GetMuzzleLocation() const;

protected:

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon")
	TObjectPtr<UStaticMeshComponent> Mesh;

	/** Socket name on the Character's skeletal mesh to attach to. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon")
	FName AttachSocketName = TEXT("hand_r");
};
