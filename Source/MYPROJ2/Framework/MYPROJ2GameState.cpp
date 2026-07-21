// Copyright Epic Games, Inc. All Rights Reserved.

#include "Framework/MYPROJ2GameState.h"
#include "Net/UnrealNetwork.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"

AMYPROJ2GameState::AMYPROJ2GameState()
{
}

void AMYPROJ2GameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AMYPROJ2GameState, RaidPhase);
	DOREPLIFETIME(AMYPROJ2GameState, PhaseEndServerTime);
	DOREPLIFETIME(AMYPROJ2GameState, LootValueMultiplier);
	DOREPLIFETIME(AMYPROJ2GameState, RaidSeed);
}

void AMYPROJ2GameState::AuthoritySetLootValueMultiplier(float NewMultiplier)
{
	if (!HasAuthority())
	{
		UE_LOG(LogMYPROJ2Net, Warning, TEXT("AuthoritySetLootValueMultiplier called on non-authority. Ignored."));
		return;
	}

	LootValueMultiplier = FMath::Max(0.0f, NewMultiplier);
}

void AMYPROJ2GameState::AuthoritySetRaidSeed(int32 NewRaidSeed)
{
	if (!HasAuthority())
	{
		UE_LOG(LogMYPROJ2Net, Warning, TEXT("AuthoritySetRaidSeed called on non-authority. Ignored."));
		return;
	}
	RaidSeed = NewRaidSeed;
}

void AMYPROJ2GameState::AuthoritySetRaidPhase(ERaidPhase NewPhase, double NewPhaseEndServerTime)
{
	if (!HasAuthority())
	{
		UE_LOG(LogMYPROJ2Net, Warning, TEXT("AuthoritySetRaidPhase called on non-authority. Ignored."));
		return;
	}

	const ERaidPhase Previous = RaidPhase;
	RaidPhase = NewPhase;
	PhaseEndServerTime = NewPhaseEndServerTime;

	// Run the OnRep locally on the Server so host-side presentation reacts identically to clients.
	OnRep_RaidPhase(Previous);
}

void AMYPROJ2GameState::OnRep_RaidPhase(ERaidPhase PreviousPhase)
{
	if (PreviousPhase == RaidPhase)
	{
		return;
	}

	UE_LOG(LogMYPROJ2Net, Log, TEXT("RaidPhase changed: %d -> %d (PhaseEndServerTime=%.3f)"),
		static_cast<int32>(PreviousPhase), static_cast<int32>(RaidPhase), PhaseEndServerTime);
}
