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
#include "Templates/SubclassOf.h"
#include "UObject/Object.h"

#include "SaveLoadBehavior.generated.h"

class USaveGame;
class USaveGameSerializer;
class USaveGameService;
struct FCurrentSaveGame;

WEEKENDSAVEGAME_API DECLARE_LOG_CATEGORY_EXTERN(LogSaveLoadBehavior, Verbose, All);

///////////////////////////////////////////////////////////////////////////////////////

/**
 * Polymorphic sub-object of @USaveGameService that extracts certain project specific
 * save/load behaviours from the service implementation.
 * The chosen behavior classes depend on the current build environment and can be
 * configured in @USaveGameServiceSettings of the ProjectSettings.
 */
UCLASS(Abstract)
class WEEKENDSAVEGAME_API USaveLoadBehavior : public UObject
{
	GENERATED_BODY()

public:
	using FSlotName = FString;

	/** Called right after the SaveLoadBehavior was created by the @USaveGameService. */
	virtual void Initialize(USaveGameService& SaveGameService) {}

	/** Called as part of the @USaveGameService start, shortly after @Initialize(). */
	virtual void HandleGameStart(USaveGameService& SaveGameService) {}

	/** Called as part of the @USaveGameService shutdown. */
	virtual void HandleGameExit(USaveGameService& SaveGameService) {}

	/** Called right before the current SaveGame is being saved to file. */
	virtual void HandleBeforeSaveGameSaved(const FCurrentSaveGame& SaveGame, USaveGameService* SaveGameService);

	/** Called right after the current SaveGame was saved to file. */
	virtual void HandleAfterSaveGameSaved(const FCurrentSaveGame& SaveGame, USaveGameService* SaveGameService) {}

	/** Called right after the current SaveGame was restored. */
	virtual void HandleAfterSaveGameRestored(const FCurrentSaveGame& SaveGame, USaveGameService* SaveGameService);

	/** Called after the current level has been changed. */
	virtual void HandleLevelChanged(USaveGameService& SaveGameService, UWorld* World) {}

	virtual FSlotName GetAutosaveSlotName(const FCurrentSaveGame& CurrentSaveGame) const { return "Autosave"; }

	/** @returns whether the current SaveGame can be continued (loaded). */
	virtual bool CanContinueCurrentSaveGame(const FCurrentSaveGame& CurrentSaveGame) const;

	virtual TSet<FSlotName> GetSaveSlotNamesAllowedForSaving(const FCurrentSaveGame& CurrentSaveGame) const PURE_VIRTUAL(GetSaveSlotNamesAllowedForSaving, return {};);
	virtual TSet<FSlotName> GetSaveSlotNamesAllowedForLoading(const FCurrentSaveGame& CurrentSaveGame) const PURE_VIRTUAL(GetSaveSlotNamesAllowedForLoading, return {};);

	/** Factory method called by @USaveGameService to create new SaveGame objects. Default = @UModularSaveGame. */
	virtual USaveGame& CreateNewSavegameObject(USaveGameService& SaveGameService) const;

	/** Called by @USaveGameService to create copies of SaveGames as snapshots. Default = @UModularSaveGame. */
	virtual USaveGame& DuplicateSaveGameObject(USaveGameService& SaveGameService, const USaveGame& SaveGameToCopy) const;

	/** @returns the class that defines how the save game data will be read-from and written-to a physical file. */
	virtual TSubclassOf<USaveGameSerializer> GetSaveGameSerializerClass() const;

	/** @returns timestamp of the last time given SaveGame was saved. This can return nothing if the information doesn't exist. */
	virtual TOptional<FDateTime> FindTimeOfLastSaveFromSaveGame(const USaveGame& SaveGame) const;

	/** Attempts to travel into the level (hopefully) saved in given SaveGame. @returns whether this was successful. */
	virtual bool TryTravelToSavedLevel(const FCurrentSaveGame& SaveGame);

protected:
	/** Possibility for derived behaviors to make an options-string that is passed in TryTravelToSavedLevel(). */
	virtual FString MakeTravelOptions(const FCurrentSaveGame& SaveGame) const { return FString(); }
};

///////////////////////////////////////////////////////////////////////////////////////

/**
 * Default implementation of a regular save/load behaviour with 8 slots.
 * Preloads all available SaveGames asynchronously at game start and set the most recently
 * saved one as current SaveGame, but does not travel into the saved level.
 */
UCLASS()
class WEEKENDSAVEGAME_API UDefaultSaveLoadBehavior : public USaveLoadBehavior
{
	GENERATED_BODY()

public:
	UDefaultSaveLoadBehavior();

	// - USaveGameBehavior
	virtual void HandleGameStart(USaveGameService& SaveGameService) override;
	virtual TSet<FSlotName> GetSaveSlotNamesAllowedForSaving(const FCurrentSaveGame& CurrentSaveGame) const override;
	virtual TSet<FSlotName> GetSaveSlotNamesAllowedForLoading(const FCurrentSaveGame& CurrentSaveGame) const override;
	// --

protected:
	UPROPERTY(EditDefaultsOnly, Category = "SaveGame")
	TSet<FString> SaveSlotNames;

	virtual void HandlePreloadCompleted(USaveGameService& SaveGameService, TArray<USaveGame*> PreloadedSaveGames, TArray<FSlotName> PreloadedSlotNames);
};

///////////////////////////////////////////////////////////////////////////////////////

/**
 * Default implementation of a Play-In-Editor save/load behavior.
 * By default, SaveGames are saved into the slot named "PlayInEditor".
 * The initial loading of a SaveGame is always synchronous, to ensure data availability.
 * Offers utilities for loading from @USaveGamePreset assets (Editor Widget -> EW_PlayInEditorSaveGame).
 */
UCLASS()
class WEEKENDSAVEGAME_API UDefaultPlayInEditorSaveLoadBehavior : public USaveLoadBehavior
{
	GENERATED_BODY()

public:
	// - USaveGameBehavior
	virtual void HandleGameStart(USaveGameService& SaveGameService) override;
	virtual TSet<FSlotName> GetSaveSlotNamesAllowedForSaving(const FCurrentSaveGame& CurrentSaveGame) const override;
	virtual TSet<FSlotName> GetSaveSlotNamesAllowedForLoading(const FCurrentSaveGame& CurrentSaveGame) const override;
	virtual FSlotName GetAutosaveSlotName(const FCurrentSaveGame& CurrentSaveGame) const override;
	// - UObject
	virtual bool IsEditorOnly() const override { return true; }
	// --

	virtual FSlotName GetPlayInEditorSlotName() const;
};
