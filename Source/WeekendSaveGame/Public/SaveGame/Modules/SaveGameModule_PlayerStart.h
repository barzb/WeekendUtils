///////////////////////////////////////////////////////////////////////////////////////
/// Copyright (C) by Benjamin Barz and contributors. See file: CREDITS.md
///
/// This file is part of the WeekendUtils UE5 Plugin.
///
/// Distributed under the MIT License. See file: LICENSE.md
///
///////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "CoreMinimal.h"
#include "SaveGame/SaveGameModule.h"

#include "SaveGameModule_PlayerStart.generated.h"

/**
 * Saves the PlayerStartTag of an @APlayerStart actor.
 * The projects @USaveLoadBehaviour could read this tag in @MakeTravelOptions() which will allow
 * the travel process to start the player directly at the desired PlayerStart.
 */
UCLASS(DisplayName = "Player Start")
class WEEKENDSAVEGAME_API USaveGameModule_PlayerStart : public USaveGameModule
{
	GENERATED_BODY()

public:
	USaveGameModule_PlayerStart()
	{
		DefaultModuleName = "PlayerStart";
		ModuleVersion = 0;
	}

	UPROPERTY(SaveGame, EditAnywhere, Category = "SaveGame")
	FString PlayerStartTag = FString();

	UPROPERTY(SaveGame, VisibleAnywhere, Category = "SaveGame")
	FTransform WorldCoordinates = FTransform();

	void AppendAsTravelPortalOption(FString& Options) const
	{
		// This black-magic is required to set the "Portal" of the FUrl that eventually ends up in AGameModeBase::FindPlayerStart:
		Options += "#" + PlayerStartTag;
	}
};
