#include "Combat/HealthComponent.h"
#include "Character/MYPROJ2CharacterBase.h"
#include "MYPROJ2GameMode.h"
#include "MYPROJ2PlayerController.h"
#include "Network/MYPROJ2NetworkTypes.h"
#include "Net/UnrealNetwork.h"

UHealthComponent::UHealthComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UHealthComponent::BeginPlay()
{
	Super::BeginPlay();

	// Server initialises the pool; clients receive the value via replication.
	if (GetOwner() && GetOwner()->HasAuthority())
	{
		Health = MaxHealth;
	}
}

void UHealthComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UHealthComponent, Health);
}

float UHealthComponent::ApplyDamage(float DamageAmount)
{
	AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority())
	{
		UE_LOG(LogMYPROJ2Combat, Warning, TEXT("ApplyDamage called on non-authority; ignoring"));
		return Health;
	}

	if (DamageAmount <= 0.f)
	{
		return Health;
	}

	const float Previous = Health;
	Health = FMath::Max(0.f, Health - DamageAmount);

	UE_LOG(LogMYPROJ2Combat, Verbose, TEXT("%s took %.1f damage: %.1f -> %.1f"),
		*GetNameSafe(Owner), DamageAmount, Previous, Health);
	if (Previous > 0.0f && Health <= 0.0f)
	{
		if (AMYPROJ2CharacterBase* Character = Cast<AMYPROJ2CharacterBase>(Owner))
		{
			if (AMYPROJ2PlayerController* RaidController = Cast<AMYPROJ2PlayerController>(Character->GetController()))
			{
				if (AMYPROJ2GameMode* RaidGameMode = Owner->GetWorld()->GetAuthGameMode<AMYPROJ2GameMode>())
				{
					RaidGameMode->AuthorityHandlePlayerDeath(RaidController);
				}
			}
		}
	}

	return Health;
}

float UHealthComponent::AuthorityHeal(float HealAmount)
{
	AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority())
	{
		UE_LOG(LogMYPROJ2Combat, Warning, TEXT("AuthorityHeal called on non-authority; ignoring"));
		return Health;
	}

	if (HealAmount <= 0.f || IsDead())
	{
		return Health;
	}

	const float Previous = Health;
	Health = FMath::Min(MaxHealth, Health + HealAmount);

	UE_LOG(LogMYPROJ2Combat, Verbose, TEXT("%s healed %.1f: %.1f -> %.1f"),
		*GetNameSafe(Owner), HealAmount, Previous, Health);

	return Health;
}

void UHealthComponent::OnRep_Health(float PreviousHealth)
{
	// Placeholder for M2 UI hook (Step 11) and future M4 death presentation.
	UE_LOG(LogMYPROJ2Combat, Verbose, TEXT("%s health rep: %.1f -> %.1f"),
		*GetNameSafe(GetOwner()), PreviousHealth, Health);
}
