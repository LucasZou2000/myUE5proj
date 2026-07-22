// Copyright Epic Games, Inc. All Rights Reserved.

#include "MYPROJ2GameMode.h"
#include "Framework/MYPROJ2GameState.h"
#include "Framework/MYPROJ2PlayerState.h"
#include "Inventory/InventoryComponent.h"
#include "MYPROJ2PlayerController.h"
#include "Persistence/ProfileSubsystem.h"
#include "Persistence/ProfileSaveTypes.h"
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

	// M5's base/profile flow is explicitly local. A future multiplayer deploy
	// exchange must carry a Server-validated payload instead of reading a client save.
	if (GetNetMode() == NM_Standalone)
	{
		if (AMYPROJ2PlayerController* RaidController = Cast<AMYPROJ2PlayerController>(NewPlayer))
		{
			if (UProfileSubsystem* Profile = GetGameInstance()->GetSubsystem<UProfileSubsystem>())
			{
				int64 CurrentCurrency = 0;
				if (Profile->LoadCurrentInventory(RaidController->GetInventoryComponent(), CurrentCurrency))
				{
					RaidController->AuthoritySetCarriedCurrency(CurrentCurrency);
					UE_LOG(LogMYPROJ2Net, Log, TEXT("Loaded local current carry from profile: currency=%lld."), CurrentCurrency);
				}
			}
		}
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

void AMYPROJ2GameMode::AuthorityExtractPlayer(AMYPROJ2PlayerController* PlayerController)
{
	if (!PlayerController)
	{
		return;
	}
	AMYPROJ2PlayerState* PlayerState = PlayerController->GetPlayerState<AMYPROJ2PlayerState>();
	if (PlayerState && PlayerState->GetLifeState() != EPlayerLifeState::Alive)
	{
		return;
	}
	FRaidSettlementPayload Settlement;
	Settlement.bExtracted = true;
	Settlement.CarriedCurrency = PlayerController->GetCarriedCurrency();
	for (const FReplicatedInventoryEntry& Entry : PlayerController->GetInventoryComponent()->GetEntries())
	{
		Settlement.Items.Add(Entry.Item);
	}
	PlayerController->GetInventoryComponent()->AuthorityClearAll();
	PlayerController->AuthoritySetCarriedCurrency(0);
	if (PlayerState)
	{
		PlayerState->AuthoritySetLifeState(EPlayerLifeState::Extracted);
	}
	PlayerController->ClientFinalizeRaidSettlement(Settlement);
}

void AMYPROJ2GameMode::AuthorityHandlePlayerDeath(AMYPROJ2PlayerController* PlayerController)
{
	if (!PlayerController)
	{
		return;
	}
	AMYPROJ2PlayerState* PlayerState = PlayerController->GetPlayerState<AMYPROJ2PlayerState>();
	if (PlayerState && PlayerState->GetLifeState() != EPlayerLifeState::Alive)
	{
		return;
	}
	PlayerController->GetInventoryComponent()->AuthorityClearAll();
	PlayerController->AuthoritySetCarriedCurrency(0);
	if (PlayerState)
	{
		PlayerState->AuthoritySetLifeState(EPlayerLifeState::Dead);
	}
	FRaidSettlementPayload Settlement;
	Settlement.bExtracted = false;
	PlayerController->ClientFinalizeRaidSettlement(Settlement);
}
