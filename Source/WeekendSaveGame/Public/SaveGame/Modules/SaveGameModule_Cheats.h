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
#include "Subsystems/WorldSubsystem.h"

#include "SaveGameModule_Cheats.generated.h"

/**
 * Module for @UModularSaveGame that stores cheat commands that should be executed as soon as
 * the SaveGame is restored and travelled into. @note that this module is development only!
 */
UCLASS(DisplayName = "Cheats To Execute")
class WEEKENDSAVEGAME_API USaveGameModule_Cheats : public USaveGameModule
{
	GENERATED_BODY()

public:
	USaveGameModule_Cheats()
	{
		DefaultModuleName = "ExecuteCheats";
		ModuleVersion = 0;
	}

	/** Cheat commands (with args) that will be executed as soon as the SaveGame is restored and travelled into. */
	UPROPERTY(SaveGame, EditDefaultsOnly, Category = "SaveGame")
	TSet<FString> CheatsToExecuteAfterTravel = {};
};

///////////////////////////////////////////////////////////////////////////////////////

/**
 * Subsystem that takes care of executing cheats from @USaveGameModule_Cheats. Development only!
 */
UCLASS(Hidden)
class UExecuteCheatFromSaveGameSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	// - UWorldSubsystem
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	// --
};
