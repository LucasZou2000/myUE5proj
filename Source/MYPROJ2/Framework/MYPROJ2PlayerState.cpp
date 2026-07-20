// Copyright Epic Games, Inc. All Rights Reserved.

#include "Framework/MYPROJ2PlayerState.h"
#include "Net/UnrealNetwork.h"

AMYPROJ2PlayerState::AMYPROJ2PlayerState()
{
}

void AMYPROJ2PlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AMYPROJ2PlayerState, RaidPlayerId);
	DOREPLIFETIME(AMYPROJ2PlayerState, LifeState);
}

void AMYPROJ2PlayerState::BeginPlay()
{
	Super::BeginPlay();

	// Server-side safety: if BeginPlay runs before GameMode assigns an ID, generate one.
	if (HasAuthority() && !RaidPlayerId.IsValid())
	{
		RaidPlayerId = FGuid::NewGuid();
	}
}

void AMYPROJ2PlayerState::AuthorityAssignRaidPlayerId(const FGuid& NewId)
{
	if (!HasAuthority())
	{
		return;
	}
	RaidPlayerId = NewId;
}

void AMYPROJ2PlayerState::AuthoritySetLifeState(EPlayerLifeState NewState)
{
	if (!HasAuthority())
	{
		return;
	}

	const EPlayerLifeState Previous = LifeState;
	LifeState = NewState;

	// Mirror OnRep locally so host presentation matches client behaviour.
	OnRep_LifeState(Previous);
}

void AMYPROJ2PlayerState::OnRep_LifeState(EPlayerLifeState PreviousState)
{
	if (PreviousState == LifeState)
	{
		return;
	}

	UE_LOG(LogMYPROJ2Net, Log, TEXT("PlayerState %s LifeState: %d -> %d"),
		*GetPlayerName(), static_cast<int32>(PreviousState), static_cast<int32>(LifeState));
}
