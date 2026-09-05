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
#include "Engine/DeveloperSettings.h"
#include "SaveGame/SaveGameFilePreset.h"
#include "SaveGame/Analysis/SaveGamePresetAnalyzer.h"
#include "SaveGame/Mocks/MockableSaveLoadBehavior.h"
#include "SaveGame/SaveLoadBehavior.h"

#include "SaveGameServiceSettings.generated.h"

/** Entry in the analyzer list. Autopopulated by analyzer CDOs via PostInitProperties. */
USTRUCT(BlueprintType)
struct WEEKENDSAVEGAME_API FSaveGamePresetAnalyzerEntry
{
	GENERATED_BODY()

public:
	UPROPERTY(VisibleAnywhere, NoClear, Category = "Analyzer")
	TSoftClassPtr<USaveGamePresetAnalyzer> AnalyzerClass = nullptr;

	UPROPERTY(EditAnywhere, Category = "Analyzer")
	bool bIsEnabled = true;
};

/**
 * Project settings for the @USaveGameService and its surrounding API.
 */
UCLASS(Config = "Game", DefaultConfig, meta = (DisplayName = "Savegame Service"))
class WEEKENDSAVEGAME_API USaveGameServiceSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	/** Behavior that defines which and when save games will be saved & loaded for the game. */
	UPROPERTY(Config, EditDefaultsOnly, NoClear, Category = "Behavior")
	TSoftClassPtr<USaveLoadBehavior> SaveLoadBehavior = UDefaultSaveLoadBehavior::StaticClass();

#if WITH_EDITORONLY_DATA
	/** Behavior that defines which and when save games will be saved & loaded for Play In Standalone (Editor). */
	UPROPERTY(Config, EditDefaultsOnly, NoClear, Category = "Behavior")
	TSoftClassPtr<USaveLoadBehavior> PlayInStandaloneSaveLoadBehavior = UDefaultSaveLoadBehavior::StaticClass();

	/** Behavior that defines which and when save games will be saved & loaded for Play In Editor. */
	UPROPERTY(Config, EditDefaultsOnly, NoClear, Category = "Behavior")
	TSoftClassPtr<USaveLoadBehavior> PlayInEditorSaveLoadBehavior = UDefaultPlayInEditorSaveLoadBehavior::StaticClass();
#endif

	/** Behavior that defines which and when save games will be saved & loaded for automation tests. */
	UPROPERTY(Config, EditDefaultsOnly, NoClear, Category = "Behavior")
	TSoftClassPtr<UMockableSaveLoadBehavior> AutomationTestSaveLoadBehavior = UMockableSaveLoadBehavior::StaticClass();

	/** When enabled, saving to a SaveGame is always allowed by default (if not locked manually through game logic). */
	UPROPERTY(Config, EditDefaultsOnly, Category = "Behavior")
	bool bAlwaysAllowSaving = true;

	/** Allow saving to a SaveGame while playing in these maps. */
	UPROPERTY(Config, EditDefaultsOnly, Category = "Behavior", meta = (EditCondition = "!bAlwaysAllowSaving", AllowedClasses = "/Script/Engine.World"))
	TSet<FSoftObjectPath> MapsWhereSavingIsAllowed = {};

	/** Allow saving to a SaveGame while playing in these GameModes. */
	UPROPERTY(Config, EditDefaultsOnly, Category = "Behavior", meta = (EditCondition = "!bAlwaysAllowSaving", MetaClass = "/Script/Engine.GameModeBase"))
	TSet<FSoftClassPath> GameModesWhereSavingIsAllowed = {};

	/** How many save/load events to keep in the debug history of the SaveGameService. History may be saved into the save game. */
	UPROPERTY(Config, EditDefaultsOnly, Category = "Behavior", AdvancedDisplay)
	uint8 DebugHistoryEntriesToKeep = 16;

	/** Name of the SaveGame slot to save to while playing in editor (see @UDefaultPlayInEditorSaveLoadBehavior). */
	UPROPERTY(Config, EditAnywhere, Category = "PlayInEditor")
	FString DefaultPlayInEditorSaveGameSlotName = "PlayInEditor";

	/** Where the project stores @USaveGamePreset assets. */
	UPROPERTY(Config, EditDefaultsOnly, Category = "Presets", meta = (ContentDir))
	FDirectoryPath DefaultSaveGamePresetFolder = FDirectoryPath("/Game/Savegames/");

	/** Default prefix that is given to @USaveGamePreset assets when created via factory. */
	UPROPERTY(Config, EditDefaultsOnly, Category = "Presets")
	FString DefaultSaveGamePresetAssetPrefix = "DA_SaveGame_";

	/** File-based preset class to create during .sav file import. Override to use a project-specific subclass. */
	UPROPERTY(Config, EditDefaultsOnly, NoClear, Category = "Presets")
	TSoftClassPtr<USaveGameFilePreset> DefaultFilePresetClass = USaveGameFilePreset::StaticClass();

	/** Analyzer classes to run when creating/importing presets. Auto-populated; toggle bIsEnabled to disable. */
	UPROPERTY(Config, EditDefaultsOnly, Category = "Presets", meta = (TitleProperty = "{AnalyzerClass}: {bIsEnabled}"))
	TArray<FSaveGamePresetAnalyzerEntry> PresetAnalyzers = {};

	/**
	 * Maps used for automated preset verification tests.
	 * Each map should contain a @ASaveGamePresetFunctionalTest actor and the relevant game actors.
	 * During tests, each preset is restored then each map is loaded to verify records against live state.
	 */
	UPROPERTY(Config, EditDefaultsOnly, Category = "Testing", meta = (AllowedClasses = "/Script/Engine.World"))
	TArray<FSoftObjectPath> PresetVerificationTestMaps = {};
};
