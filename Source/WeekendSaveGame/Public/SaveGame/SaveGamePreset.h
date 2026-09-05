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
#include "Engine/DataAsset.h"
#include "SaveGame/Analysis/SaveGameAnalysisReport.h"
#include "StructUtils/InstancedStruct.h"

#include "SaveGamePreset.generated.h"

class USaveGamePresetAnalyzer;
class USaveGame;
class USaveGameService;

/**
 * Abstract base for preset assets that wrap a SaveGame for testing and quick-loading.
 * Concrete subclasses differ in how they store and produce the SaveGame object:
 * - @USaveGameObjectPreset stores a USaveGame as an instanced UObject subobject.
 * - @USaveGameFilePreset stores raw serialized bytes and deserializes on demand.
 */
UCLASS(Abstract, BlueprintType, CollapseCategories)
class WEEKENDSAVEGAME_API USaveGamePreset : public UDataAsset
{
	GENERATED_BODY()

public:
	using FSlotName = FString;

	/** Only available in editor, exclude from any cooked builds. */
	UPROPERTY(EditDefaultsOnly, Category = "SaveGame")
	bool bIsEditorOnly = false;

	/** Only available in editor and dev-builds, exclude from any release builds. */
	UPROPERTY(EditDefaultsOnly, Category = "SaveGame", meta = (EditCondition = "!bIsEditorOnly"))
	bool bIsDeveloperOnly = true;

	/** Pretends to be a SaveGame slot, so it should be unique across other presets. */
	UPROPERTY(EditDefaultsOnly, NoClear, Category = "SaveGame")
	FString PresetName = FString();

	UPROPERTY(EditDefaultsOnly, NoClear, Category = "SaveGame")
	TOptional<FString> DisplayName = {};

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SaveGame", meta = (MultiLine))
	TOptional<FText> AdditionalInformation = {};

	UPROPERTY(EditDefaultsOnly, meta = (ExcludeBaseStruct, BaseStruct = "/Script/WeekendUtils.SaveGameHeaderDataBase"), Category = "SaveGame")
	FInstancedStruct HeaderData;

	/** Analysis report generated at import time. Contains structured records for display and automated verification. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Analysis")
	FSaveGameAnalysisReport AnalysisReport = FSaveGameAnalysisReport();

	USaveGamePreset();

	UFUNCTION(BlueprintCallable, Category = "Weekend Utils|Save Game", meta = (DevelopmentOnly))
	static void OpenSaveGamePresetsFolder();

	/** Scans for available presets that match the current build environment. */
	static TSet<const USaveGamePreset*> CollectSaveGamePresets();

	/** Scans for available presets that match the current build environment. */
	static TSet<FSlotName> CollectSaveGamePresetNames();

	/** @returns and scans a particular preset. */
	static const USaveGamePreset* FindSaveGamePreset(const FSlotName& PresetName);

	/** @returns the SaveGamePreset that might be currently loaded as CurrentSaveGame in the @USaveGameService. */
	static const USaveGamePreset* FindCurrentlyLoadedGamePreset(const UObject& WorldContextObject);

	/** @returns the savegame object held by this preset. Subclasses provide different storage strategies. */
	virtual const USaveGame* GetPresetSaveGame() const
		PURE_VIRTUAL(USaveGamePreset::GetSaveGame, return nullptr;);

	/** Create a new USaveGame instance for game restoration. Subclasses differ in how the object is produced. */
	virtual USaveGame* CreateSaveGameObject(USaveGameService& SaveGameService) const
		PURE_VIRTUAL(USaveGamePreset::CreateSaveGameObject, return nullptr;);

	virtual void RestoreAsCurrentSaveGame(USaveGameService& SaveGameService) const;
	virtual void RestoreAsAndTravelIntoCurrentSaveGame(USaveGameService& SaveGameService) const;

#if WITH_EDITOR
	/** Runs all registered analyzers against a savegame and produces an analysis report. */
	static FSaveGameAnalysisReport CreateAnalysisReport(const USaveGame* InSaveGame, const FInstancedStruct* InHeaderData = nullptr, const TOptional<FString>& InSourceFileName = {});

	/** Runs a specific registered analyzer against a savegame and produces an analysis report. */
	static FSaveGameAnalysisReport CreateAnalysisReport(const USaveGame* InSaveGame, const TSubclassOf<USaveGamePresetAnalyzer>& AnalyzerClass, const FInstancedStruct* InHeaderData = nullptr);

	/** Re-runs all registered analyzers and updates the analysis report for this preset. Keeps "OverrideRecords". */
	UFUNCTION(CallInEditor, Category = "Commands", meta = (DevelopmentOnly), DisplayName = "Update Analysis")
	virtual void UpdateAnalysis();

	/** Regenerates the description of the AnalysisReport without re-running all analyzers. */
	UFUNCTION(CallInEditor, Category = "Commands", meta = (DevelopmentOnly), DisplayName = "Update Analysis Description")
	virtual void UpdateAnalysisDescription();

	/** Prompts the user with a choice about what to do with each mismatching record. Mismatches are populated (temporarily) by running relevant automation tests. */
	UFUNCTION(CallInEditor, Category = "Commands", meta = (DevelopmentOnly), DisplayName = "Review Mismatched Records")
	virtual void ReviewMismatchedRecords();

	/** Opens the Automation Frontend and selects all tests associated with this preset. */
	UFUNCTION(CallInEditor, Category = "Commands", meta = (DevelopmentOnly), DisplayName = "Open Tests")
	void OpenTestsInAutomationFrontend() const;
#endif

	// - UObject
	virtual FPrimaryAssetId GetPrimaryAssetId() const override;
	virtual bool NeedsLoadForClient() const override;
	virtual bool NeedsLoadForServer() const override;
	virtual bool IsEditorOnly() const override { return bIsEditorOnly; }
#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
	// --
};
