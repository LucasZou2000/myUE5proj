// Copyright Epic Games, Inc. All Rights Reserved.

#include "MYPROJ2GameMode.h"
#include "Framework/MYPROJ2GameState.h"
#include "Framework/MYPROJ2PlayerState.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"

AMYPROJ2GameMode::AMYPROJ2GameMode()
{
	GameStateClass = AMYPROJ2GameState::StaticClass();
	PlayerStateClass = AMYPROJ2PlayerState::StaticClass();
}

void AMYPROJ2GameMode::BeginPlay()
{
	Super::BeginPlay();

	// Initial phase: Lobby with no scheduled end.
	if (AMYPROJ2GameState* GS = GetGameState<AMYPROJ2GameState>())
	{
		GS->AuthoritySetRaidPhase(ERaidPhase::Lobby, 0.0);
		// One seed per hosted raid. Containers combine it with their stable IDs.
		GS->AuthoritySetRaidSeed(static_cast<int32>(GetTypeHash(FGuid::NewGuid())));
		GS->AuthoritySetLootValueMultiplier(1.0f);
	}
}

void AMYPROJ2GameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);

	if (!NewPlayer)
	{
		return;
	}

	AMYPROJ2PlayerState* PS = NewPlayer->GetPlayerState<AMYPROJ2PlayerState>();
	if (PS && !PS->GetRaidPlayerId().IsValid())
	{
		PS->AuthorityAssignRaidPlayerId(FGuid::NewGuid());
	}

	UE_LOG(LogMYPROJ2Net, Log, TEXT("PostLogin: %s (RaidPlayerId=%s) joined. Players=%d"),
		NewPlayer ? *NewPlayer->GetName() : TEXT("<null>"),
		PS ? *PS->GetRaidPlayerId().ToString() : TEXT("<none>"),
		GetNumPlayers());
}

void AMYPROJ2GameMode::Logout(AController* Exiting)
{
	const APlayerController* PC = Cast<APlayerController>(Exiting);
	const AMYPROJ2PlayerState* PS = PC ? PC->GetPlayerState<AMYPROJ2PlayerState>() : nullptr;

	UE_LOG(LogMYPROJ2Net, Log, TEXT("Logout: %s (RaidPlayerId=%s) left."),
		Exiting ? *Exiting->GetName() : TEXT("<null>"),
		PS ? *PS->GetRaidPlayerId().ToString() : TEXT("<none>"));

	Super::Logout(Exiting);
}

void AMYPROJ2GameMode::SetRaidPhase(ERaidPhase NewPhase, double NewPhaseEndServerTime)
{
	// GameMode only exists on Server, so this is inherently authoritative.
	if (AMYPROJ2GameState* GS = GetGameState<AMYPROJ2GameState>())
	{
		GS->AuthoritySetRaidPhase(NewPhase, NewPhaseEndServerTime);
	}
}
