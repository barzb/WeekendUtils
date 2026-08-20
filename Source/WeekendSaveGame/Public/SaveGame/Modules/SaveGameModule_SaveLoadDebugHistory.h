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

#include "SaveGameModule_SaveLoadDebugHistory.generated.h"

class USaveGameService;

/**
 * Module for @UModularSaveGame that stores a history of executed operations of the @USaveGameService.
 * The history is only saved, not restored, since the process of restoring would alter the history.
 */
UCLASS(DisplayName = "Save/Load Debug History")
class WEEKENDSAVEGAME_API USaveGameModule_SaveLoadDebugHistory : public USaveGameModule
{
	GENERATED_BODY()

public:
	USaveGameModule_SaveLoadDebugHistory()
	{
		DefaultModuleName = "SaveLoadDebugHistory";
		ModuleVersion = 0;
	}

	UPROPERTY(Transient)
	TObjectPtr<USaveGameService> SaveGameService = nullptr;

	UPROPERTY(SaveGame, VisibleAnywhere, Category = "SaveGame")
	TArray<FString> DebugHistory = {};

protected:
	// - USaveGameModule
	virtual void PreSaveModule() override;
	// --
};
