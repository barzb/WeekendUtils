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
#include "SaveGame/Analysis/SaveGamePresetAnalyzer.h"

#include "MockSaveGamePresetAnalyzer.generated.h"

/**
 * Mock analyzer for automation tests. Uses static fields so the CDO can be configured by tests
 * before ProduceAnalysisReport (which uses GetDefault) is called.
 */
UCLASS(Hidden, NotBlueprintable, NotBlueprintType, HideDropdown, ClassGroup = "Tests")
class WEEKENDUTILSTESTS_API UMockSaveGamePresetAnalyzer : public USaveGamePresetAnalyzer
{
	GENERATED_BODY()

public:
	static inline TArray<FSaveGameRecord> MockRecordsToReturn = {};
	static inline bool bMockVerificationShouldPass = true;
	static inline FText MockFailureReason = FText::GetEmpty();

	UMockSaveGamePresetAnalyzer()
	{
		bAutoRegisterWithSaveGameSettings = false;
	}

	static void ResetMockState()
	{
		MockRecordsToReturn.Empty();
		bMockVerificationShouldPass = true;
		MockFailureReason = FText::GetEmpty();
	}

	// - USaveGamePresetAnalyzer
	virtual void AnalyzeSaveGame(const USaveGame& SaveGame, const FInstancedStruct* HeaderData, TArray<FSaveGameRecord>& OutRecords) const override
	{
		OutRecords.Append(MockRecordsToReturn);
	}
	// --
};
