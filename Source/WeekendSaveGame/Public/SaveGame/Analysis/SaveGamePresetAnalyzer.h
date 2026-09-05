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
#include "SaveGame/Analysis/SaveGameAnalysisReport.h"
#include "Utils/CommonAvailabilityEnum.h"

#include "SaveGamePresetAnalyzer.generated.h"

class USaveGamePreset;
class USaveGame;

WEEKENDSAVEGAME_API DECLARE_LOG_CATEGORY_EXTERN(LogSaveGamePresetAnalyzer, Log, All);

///////////////////////////////////////////////////////////////////////////////////////

/**
 * Reads deserialized savegame module data and produces structured records.
 * Also verifies those records against live game state during tests.
 * Projects subclass this once per concern (missions, inventory, ...).
 * Registered via USaveGameServiceSettings::PresetAnalyzers.
 */
UCLASS(Abstract, NotBlueprintable, EditInlineNew)
class WEEKENDSAVEGAME_API USaveGamePresetAnalyzer : public UObject
{
	GENERATED_BODY()

public:
	// - UObject
	virtual void PostInitProperties() override;
	virtual bool IsEditorOnly() const override { return true; }
	// --

	/**
	 * Analyze the deserialized savegame and produce records.
	 * Called during import and when updating the analysis of an existing preset.
	 * @param SaveGame - The deserialized savegame to analyze.
	 * @param HeaderData - The header data associated with the savegame.
	 * @param InOutRecords - Output array to append discovered records to.
	 */
	virtual void AnalyzeSaveGame(const USaveGame& SaveGame, const FInstancedStruct* HeaderData, TArray<FSaveGameRecord>& InOutRecords) const
		PURE_VIRTUAL(USaveGamePresetAnalyzer::AnalyzeSaveGame, );

	/** Adds a non-abstract analyzer class to the GameServiceSettings. See PostInitProperties. */
	void RegisterWithSaveGameSettings() const;

	/** Compares two sets of records, asserts in case of mismatches, and populates the MismatchedRecords of the ExpectedReport. */
	static void CompareExistingReportAgainstNewRecords(const UObject* Tester, const FSaveGameAnalysisReport& ExpectedReport, const TArray<FSaveGameRecord>& ActualRecords);

protected:
	UPROPERTY(EditDefaultsOnly, Category = "SaveGame")
	bool bAutoRegisterWithSaveGameSettings = true;

	static TOptional<FSaveGameRecordMismatch> CompareRecords(const UObject* Tester, const FSaveGameRecord& ActualRecord, const FSaveGameRecord& ExpectedRecord, ESaveGameRecordVerbosity Verbosity = ESaveGameRecordVerbosity::Error);
	static TOptional<FSaveGameRecordMismatch> CompareRecords(const UObject* Tester, const FSaveGameRecord& ActualRecord, const FSaveGameAnalysisReport& ReportOfExpectedRecords);
	static FSaveGameRecordMismatch CreateMissingRecordMismatch(const FSaveGameRecord& ActualRecord);
	static void AssertError(const UObject* Tester, const FString& Error);
	static void AssertWarning(const UObject* Tester, const FString& Warning);
	static void AssertInfo(const UObject* Tester, const FString& Info);
};

///////////////////////////////////////////////////////////////////////////////////////

/**
 * Blueprint base class for USaveGamePresetAnalyzer.
 */
UCLASS(Abstract, Blueprintable)
class WEEKENDSAVEGAME_API USaveGamePresetAnalyzer_BlueprintBase : public USaveGamePresetAnalyzer
{
	GENERATED_BODY()

public:
	// - USaveGamePresetAnalyzer
	virtual void AnalyzeSaveGame(const USaveGame& SaveGame, const FInstancedStruct* HeaderData, TArray<FSaveGameRecord>& InOutRecords) const override;
	// --

	/**
	 * Asserts (or logs) that the analysis of a savegame report produced by a specified analyzer matches an expected report.
	 * Used to compare results between an existing analysis and a new analyzer run.
	 *
	 * @param Tester Who is running this test. Context object.
	 * @param ExpectedReport The expected savegame analysis report to compare against.
	 * @param AnalyzerClass The class of the savegame preset analyzer to use for generating the new analysis.
	 */
	UFUNCTION(BlueprintCallable, Category = "SaveGameAnalyzer", meta = (DefaultToSelf = "Tester"))
	static void AssertExistingReportAgainstNewBlueprintAnalysis(const UObject* Tester, const FSaveGameAnalysisReport& ExpectedReport, TSubclassOf<USaveGamePresetAnalyzer_BlueprintBase> AnalyzerClass);

protected:
	/** Implement in Blueprint to generate the offline analysis of a SaveGame which will be hardcoded into the respective SaveGamePreset. */
	UFUNCTION(BlueprintImplementableEvent, Category = "SaveGameAnalyzer", DisplayName = "AnalyzeSaveGame", meta = (ForceAsFunction))
	void AnalyzeSaveGame_Blueprint(const USaveGame* SaveGame, TArray<FSaveGameRecord>& InOutRecords) const;

	/**
	 * Implement in Blueprint to assert the comparison between the SaveGamePreset analysis (ExpectedReport) vs. a new runtime analysis (ActualReport).
	 * Call AssertMismatchError/Warning/Info() or AssertSimpleError/Warning/Info() to report mismatches.
	 * !!! Does not support asynchronous execution. Comparison must be performed in this function.
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "SaveGameAnalyzer", DisplayName = "AssertAgainstExistingRepor", meta = (ForceAsFunction))
	void CompareAgainstExistingReport_Blueprint(const USaveGamePreset* SaveGamePreset, const FSaveGameAnalysisReport& ExpectedReport);

	UFUNCTION(BlueprintCallable, Category = "SaveGameAnalyzer")
	bool FindSaveGameRecordByTag(FGameplayTag ExactRecordTag, FSaveGameRecord& OutRecord) const;
	UFUNCTION(BlueprintCallable, Category = "SaveGameAnalyzer")
	bool FindSaveGameRecordById(FName UniqueId, FSaveGameRecord& OutRecord) const;

	UFUNCTION(BlueprintCallable, BlueprintPure = False, Category = "SaveGameAnalyzer", meta = (ExpandEnumAsExecs = "ReturnValue", Categories = "SaveGame.Analysis"))
	ECommonAvailability FindOrAssertExpectedValue(FGameplayTag ExactRecordTag, FInstancedStruct& OutExpectedValue);
	UFUNCTION(BlueprintCallable, BlueprintPure = False, Category = "SaveGameAnalyzer", meta = (ExpandEnumAsExecs = "ReturnValue", Categories = "SaveGame.Analysis"))
	ECommonAvailability FindOrAssertExpectedValue_Bool(FGameplayTag ExactRecordTag, bool& OutActualValue);
	UFUNCTION(BlueprintCallable, BlueprintPure = False, Category = "SaveGameAnalyzer", meta = (ExpandEnumAsExecs = "ReturnValue", Categories = "SaveGame.Analysis"))
	ECommonAvailability FindOrAssertExpectedValue_Int32(FGameplayTag ExactRecordTag, int32& OutActualValue);
	UFUNCTION(BlueprintCallable, BlueprintPure = False, Category = "SaveGameAnalyzer", meta = (ExpandEnumAsExecs = "ReturnValue", Categories = "SaveGame.Analysis"))
	ECommonAvailability FindOrAssertExpectedValue_Float(FGameplayTag ExactRecordTag, float& OutActualValue);
	UFUNCTION(BlueprintCallable, BlueprintPure = False, Category = "SaveGameAnalyzer", meta = (ExpandEnumAsExecs = "ReturnValue", Categories = "SaveGame.Analysis"))
	ECommonAvailability FindOrAssertExpectedValue_String(FGameplayTag ExactRecordTag, FString& OutActualValue);
	UFUNCTION(BlueprintCallable, BlueprintPure = False, Category = "SaveGameAnalyzer", meta = (ExpandEnumAsExecs = "ReturnValue", Categories = "SaveGame.Analysis"))
	ECommonAvailability FindOrAssertExpectedValue_StringList(FGameplayTag ExactRecordTag, TArray<FString>& OutActualValues);

	UFUNCTION(BlueprintCallable, Category = "SaveGameAnalyzer")
	void CompareRecords(const FSaveGameRecord& ActualRecord, const FSaveGameRecord& ExpectedRecord);
	UFUNCTION(BlueprintCallable, Category = "SaveGameAnalyzer")
	void AssertMismatchError(FString Error, const FSaveGameRecord& ActualRecord, const FSaveGameRecord& ExpectedRecord);
	UFUNCTION(BlueprintCallable, Category = "SaveGameAnalyzer")
	void AssertMismatchWarning(FString Warning, const FSaveGameRecord& ActualRecord, const FSaveGameRecord& ExpectedRecord);
	UFUNCTION(BlueprintCallable, Category = "SaveGameAnalyzer")
	void AssertMismatchInfo(FString Info, const FSaveGameRecord& ActualRecord, const FSaveGameRecord& ExpectedRecord);

	UFUNCTION(BlueprintCallable, Category = "SaveGameAnalyzer")
	void AssertSimpleError(FString Error);
	UFUNCTION(BlueprintCallable, Category = "SaveGameAnalyzer")
	void AssertSimpleWarning(FString Warning);
	UFUNCTION(BlueprintCallable, Category = "SaveGameAnalyzer")
	void AssertSimpleInfo(FString Info);

	void AssertMissingRecord(const FGameplayTag& MissingRecordTag);
	void AssertWrongTypeRecord(const FGameplayTag& MissingRecordTag, const FString& ActualType, const FString& ExpectedType);

private:
	/** Temporarily cached pointers that are only valid while @AssertAgainstExistingReport_Blueprint is executed (synchronously). */
	const FSaveGameAnalysisReport* CachedExpectedReport = nullptr;
	TWeakObjectPtr<const UObject> CachedTester = nullptr;
};
