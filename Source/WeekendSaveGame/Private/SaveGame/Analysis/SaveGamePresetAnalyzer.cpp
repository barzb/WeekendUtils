///////////////////////////////////////////////////////////////////////////////////////
/// Copyright (C) by Benjamin Barz and contributors. See file: CREDITS.md
///
/// This file is part of the WeekendUtils UE5 Plugin.
///
/// Distributed under the MIT License. See file: LICENSE.md
///
///////////////////////////////////////////////////////////////////////////////////////

#include "SaveGame/Analysis/SaveGamePresetAnalyzer.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameService/GameServiceLocator.h"
#include "Misc/AutomationTest.h"
#include "SaveGame/SaveGameService.h"
#include "SaveGame/Analysis/SaveGameRecordTypes.h"
#include "SaveGame/Settings/SaveGameServiceSettings.h"

#if WITH_EDITOR
#include "SourceControlHelpers.h"
#endif

DEFINE_LOG_CATEGORY(LogSaveGamePresetAnalyzer);

///////////////////////////////////////////////////////////////////////////////////////
/// USaveGamePresetAnalyzer

void USaveGamePresetAnalyzer::PostInitProperties()
{
	Super::PostInitProperties();

	if (bAutoRegisterWithSaveGameSettings)
	{
		RegisterWithSaveGameSettings();
	}
}

void USaveGamePresetAnalyzer::RegisterWithSaveGameSettings() const
{
	if (!HasAnyFlags(RF_ClassDefaultObject))
		return;

	const UClass* ThisClass = GetClass();
	if (ThisClass->HasAnyClassFlags(CLASS_Abstract | CLASS_Hidden | CLASS_NewerVersionExists | CLASS_Deprecated))
		return;

	USaveGameServiceSettings* Settings = GetMutableDefault<USaveGameServiceSettings>();
	const TSoftClassPtr<USaveGamePresetAnalyzer> ThisClassPtr(ThisClass);
	for (const FSaveGamePresetAnalyzerEntry& Entry : Settings->PresetAnalyzers)
	{
		if (Entry.AnalyzerClass == ThisClassPtr)
			return;
	}

	FSaveGamePresetAnalyzerEntry& NewEntry = Settings->PresetAnalyzers.AddDefaulted_GetRef();
	NewEntry.AnalyzerClass = ThisClassPtr;
	NewEntry.bIsEnabled = true;

#if WITH_EDITOR
	USourceControlHelpers::CheckOutOrAddFile(Settings->GetDefaultConfigFilename());
	Settings->TryUpdateDefaultConfigFile();
#endif
}

void USaveGamePresetAnalyzer::CompareExistingReportAgainstNewRecords(const UObject* Tester, const FSaveGameAnalysisReport& ExpectedReport, const TArray<FSaveGameRecord>& ActualRecords)
{
	for (const FSaveGameRecord& ActualRecord : ActualRecords)
	{
		const TOptional<FSaveGameRecordMismatch> Mismatch = CompareRecords(Tester, ActualRecord, ExpectedReport);
#if WITH_EDITOR
		if (Mismatch.IsSet())
		{
			ExpectedReport.AddMismatchRecord(*Mismatch);
		}
		else
		{
			// Remove any outdated mismatches from previous test runs:
			ExpectedReport.RemoveMismatchRecord(ActualRecord);
		}
#endif
	}
}

TOptional<FSaveGameRecordMismatch> USaveGamePresetAnalyzer::CompareRecords(const UObject* Tester, const FSaveGameRecord& ActualRecord, const FSaveGameRecord& ExpectedRecord, const ESaveGameRecordVerbosity Verbosity)
{
	const FString SuccessMessage = FString::Printf(TEXT("Test succeeded: [%s] %s"),
		*ActualRecord.UniqueIdentifier.ToString(), *ActualRecord.Description.ToString());
	const FString FailureMessage = FString::Printf(TEXT("Test failed: [%s] Actual: %s | Expected: %s"),
		*ActualRecord.UniqueIdentifier.ToString(), *ActualRecord.Description.ToString(), *ExpectedRecord.Description.ToString());

	if (ExpectedRecord.IsIdenticalTo(ActualRecord))
	{
		AssertInfo(Tester, SuccessMessage);
		return {};
	}

	switch (Verbosity)
	{
	case ESaveGameRecordVerbosity::Error:
	{
		AssertError(Tester, FailureMessage);
		return FSaveGameRecordMismatch(ExpectedRecord.UniqueIdentifier, ActualRecord, ExpectedRecord);
	}

	case ESaveGameRecordVerbosity::Warning:
	{
		AssertWarning(Tester, FailureMessage);
		return FSaveGameRecordMismatch(ExpectedRecord.UniqueIdentifier, ActualRecord, ExpectedRecord);
	}

	case ESaveGameRecordVerbosity::Info:
	{
		AssertInfo(Tester, FailureMessage);
		return FSaveGameRecordMismatch(ExpectedRecord.UniqueIdentifier, ActualRecord, ExpectedRecord);
	}

	default: return {};
	}
}

TOptional<FSaveGameRecordMismatch> USaveGamePresetAnalyzer::CompareRecords(const UObject* Tester, const FSaveGameRecord& ActualRecord, const FSaveGameAnalysisReport& ReportOfExpectedRecords)
{
	const TArray<FSaveGameRecord> ExpectedRecords = ReportOfExpectedRecords.GetRecords();
	const FSaveGameRecord* ExpectedRecord = ExpectedRecords.FindByKey(ActualRecord);
	if (!ExpectedRecord) // Missing record!
	{
		const FString Message = FString::Printf(TEXT("[%s] (Info) Missing expected record for %s: %s"),
			*GetNameSafe(Tester), *ReportOfExpectedRecords.SourceFileName.Get(GetNameSafe(Tester)), *ActualRecord.UniqueIdentifier.ToString());
		switch (ActualRecord.MismatchVerbosity)
		{
		case ESaveGameRecordVerbosity::Error:
			AssertError(Tester, Message);
			break;
		case ESaveGameRecordVerbosity::Warning:
			AssertWarning(Tester, Message);
			break;
		case ESaveGameRecordVerbosity::Info:
			AssertInfo(Tester, Message);
			break;
		}
		return CreateMissingRecordMismatch(ActualRecord);
	}

	TOptional<FSaveGameRecordMismatch> Mismatch = CompareRecords(Tester, ActualRecord, *ExpectedRecord, ActualRecord.MismatchVerbosity);
	if (!Mismatch.IsSet() || !ReportOfExpectedRecords.OverrideRecords.Contains(ActualRecord))
		return Mismatch; // No mismatch or mismatch to GeneratedRecord.

	// In case of error, make sure the mismatch references the (original) generated record, not the (also mismatching) OverrideRecord:
	if (const FSaveGameRecord* GeneratedRecord = ReportOfExpectedRecords.GeneratedRecords.FindByKey(ActualRecord))
	{
		Mismatch->ActualRecord = *GeneratedRecord;
	}
	return Mismatch;
}

FSaveGameRecordMismatch USaveGamePresetAnalyzer::CreateMissingRecordMismatch(const FSaveGameRecord& ActualRecord)
{
	FSaveGameRecordMismatch Mismatch{ActualRecord.UniqueIdentifier, ActualRecord, ActualRecord};
	Mismatch.ExpectedRecord.ExpectedValue.InitializeAs(Mismatch.ExpectedRecord.ExpectedValue.GetScriptStruct(), nullptr); // Default struct value.
	Mismatch.ExpectedRecord.Description = INVTEXT("(missing)");
	return Mismatch;
}

void USaveGamePresetAnalyzer::AssertError(const UObject* Tester, const FString& Error)
{
	const FString Message = FString::Printf(TEXT("[%s] (Error) %s"), *GetNameSafe(Tester), *Error);
	if (FAutomationTestBase* CurrentTest = GIsAutomationTesting ? FAutomationTestFramework::Get().GetCurrentTest() : nullptr)
	{
		CurrentTest->AddError(*Message);
	}
	else
	{
		UE_LOG(LogSaveGamePresetAnalyzer, Error, TEXT("%s"), *Message);
	}
}

void USaveGamePresetAnalyzer::AssertWarning(const UObject* Tester, const FString& Warning)
{
	const FString Message = FString::Printf(TEXT("[%s] (Warning) %s"), *GetNameSafe(Tester), *Warning);
	if (FAutomationTestBase* CurrentTest = GIsAutomationTesting ? FAutomationTestFramework::Get().GetCurrentTest() : nullptr)
	{
		CurrentTest->AddWarning(*Message);
	}
	else
	{
		UE_LOG(LogSaveGamePresetAnalyzer, Warning, TEXT("%s"), *Message);
	}
}

void USaveGamePresetAnalyzer::AssertInfo(const UObject* Tester, const FString& Info)
{
	const FString Message = FString::Printf(TEXT("[%s] (Info) %s"), *GetNameSafe(Tester), *Info);
	if (FAutomationTestBase* CurrentTest = GIsAutomationTesting ? FAutomationTestFramework::Get().GetCurrentTest() : nullptr)
	{
		CurrentTest->AddInfo(*Message);
	}
	else
	{
		UE_LOG(LogSaveGamePresetAnalyzer, Log, TEXT("%s"), *Message);
	}
}

///////////////////////////////////////////////////////////////////////////////////////
/// USaveGamePresetAnalyzer_BlueprintBase

void USaveGamePresetAnalyzer_BlueprintBase::AnalyzeSaveGame(const USaveGame& SaveGame, const FInstancedStruct* HeaderData, TArray<FSaveGameRecord>& InOutRecords) const
{
	AnalyzeSaveGame_Blueprint(&SaveGame, IN OUT InOutRecords);
}

void USaveGamePresetAnalyzer_BlueprintBase::AssertExistingReportAgainstNewBlueprintAnalysis(
	const UObject* Tester, const FSaveGameAnalysisReport& ExpectedReport, TSubclassOf<USaveGamePresetAnalyzer_BlueprintBase> AnalyzerClass)
{
	const USaveGameService* SaveGameService = UGameServiceLocator::FindService<USaveGameService>(Tester);
	const TOptional<FString> SlotName = SaveGameService ? SaveGameService->GetCurrentSaveGame().GetSlotLastRestoredFrom() : TOptional<FString>{};
	if (!ensureAlways(SlotName.IsSet()))
		return;
	const USaveGamePreset* SaveGamePreset = USaveGamePreset::FindSaveGamePreset(*SlotName);
	if (!ensureAlways(SaveGamePreset))
		return;

	USaveGamePresetAnalyzer_BlueprintBase* TempAnalyzer = NewObject<USaveGamePresetAnalyzer_BlueprintBase>(Tester->GetWorld(), AnalyzerClass.Get());
	TempAnalyzer->CachedExpectedReport = &ExpectedReport;
	TempAnalyzer->CachedTester = Tester;
	TempAnalyzer->CompareAgainstExistingReport_Blueprint(SaveGamePreset, SaveGamePreset->AnalysisReport);
	TempAnalyzer->CachedExpectedReport = nullptr;
	TempAnalyzer->CachedTester = nullptr;
	TempAnalyzer = nullptr;

	GEngine->ForceGarbageCollection();
}

bool USaveGamePresetAnalyzer_BlueprintBase::FindSaveGameRecordByTag(FGameplayTag ExactRecordTag, FSaveGameRecord& OutRecord) const
{
	if (!CachedExpectedReport)
		return false;

	TOptional<FSaveGameRecord> FoundRecord = CachedExpectedReport->FindRecordByExactTag(ExactRecordTag);
	if (!FoundRecord.IsSet())
		return false;

	OutRecord = *FoundRecord;
	return true;
}

bool USaveGamePresetAnalyzer_BlueprintBase::FindSaveGameRecordById(FName UniqueId, FSaveGameRecord& OutRecord) const
{
	if (!CachedExpectedReport)
		return false;

	TOptional<FSaveGameRecord> FoundRecord = CachedExpectedReport->FindRecordByUniqueId(UniqueId);
	if (!FoundRecord.IsSet())
		return false;

	OutRecord = *FoundRecord;
	return true;
}

ECommonAvailability USaveGamePresetAnalyzer_BlueprintBase::FindOrAssertExpectedValue(FGameplayTag ExactRecordTag, FInstancedStruct& OutExpectedValue)
{
	FSaveGameRecord FoundRecord;
	if (!FindSaveGameRecordByTag(ExactRecordTag, OUT FoundRecord))
	{
		AssertMissingRecord(ExactRecordTag);
		return ECommonAvailability::Unavailable;
	}

	OutExpectedValue = FoundRecord.ExpectedValue;
	return ECommonAvailability::Available;
}

ECommonAvailability USaveGamePresetAnalyzer_BlueprintBase::FindOrAssertExpectedValue_Bool(FGameplayTag ExactRecordTag, bool& OutActualValue)
{
	FSaveGameRecord FoundRecord;
	if (!FindSaveGameRecordByTag(ExactRecordTag, OUT FoundRecord))
	{
		AssertMissingRecord(ExactRecordTag);
		return ECommonAvailability::Unavailable;
	}

	const FSaveGameRecord_BoolValue* ExpectedRecord = FoundRecord.ExpectedValue.GetPtr<FSaveGameRecord_BoolValue>();
	if (!ExpectedRecord)
	{
		AssertWrongTypeRecord(ExactRecordTag, GetNameSafe(FoundRecord.ExpectedValue.GetScriptStruct()), "FSaveGameRecord_BoolValue");
		return ECommonAvailability::Unavailable;
	}

	OutActualValue = ExpectedRecord->bValue;
	return ECommonAvailability::Available;
}

ECommonAvailability USaveGamePresetAnalyzer_BlueprintBase::FindOrAssertExpectedValue_Int32(FGameplayTag ExactRecordTag, int32& OutActualValue)
{
	FSaveGameRecord FoundRecord;
	if (!FindSaveGameRecordByTag(ExactRecordTag, OUT FoundRecord))
	{
		AssertMissingRecord(ExactRecordTag);
		return ECommonAvailability::Unavailable;
	}

	const FSaveGameRecord_IntValue* ExpectedRecord = FoundRecord.ExpectedValue.GetPtr<FSaveGameRecord_IntValue>();
	if (!ExpectedRecord)
	{
		AssertWrongTypeRecord(ExactRecordTag, GetNameSafe(FoundRecord.ExpectedValue.GetScriptStruct()), "FSaveGameRecord_IntValue");
		return ECommonAvailability::Unavailable;
	}

	OutActualValue = ExpectedRecord->Value;
	return ECommonAvailability::Available;
}

ECommonAvailability USaveGamePresetAnalyzer_BlueprintBase::FindOrAssertExpectedValue_Float(FGameplayTag ExactRecordTag, float& OutActualValue)
{
	FSaveGameRecord FoundRecord;
	if (!FindSaveGameRecordByTag(ExactRecordTag, OUT FoundRecord))
	{
		AssertMissingRecord(ExactRecordTag);
		return ECommonAvailability::Unavailable;
	}

	const FSaveGameRecord_FloatValue* ExpectedRecord = FoundRecord.ExpectedValue.GetPtr<FSaveGameRecord_FloatValue>();
	if (!ExpectedRecord)
	{
		AssertWrongTypeRecord(ExactRecordTag, GetNameSafe(FoundRecord.ExpectedValue.GetScriptStruct()), "FSaveGameRecord_FloatValue");
		return ECommonAvailability::Unavailable;
	}

	OutActualValue = ExpectedRecord->Value;
	return ECommonAvailability::Available;
}

ECommonAvailability USaveGamePresetAnalyzer_BlueprintBase::FindOrAssertExpectedValue_String(FGameplayTag ExactRecordTag, FString& OutActualValue)
{
	FSaveGameRecord FoundRecord;
	if (!FindSaveGameRecordByTag(ExactRecordTag, OUT FoundRecord))
	{
		AssertMissingRecord(ExactRecordTag);
		return ECommonAvailability::Unavailable;
	}

	const FSaveGameRecord_StringValue* ExpectedRecord = FoundRecord.ExpectedValue.GetPtr<FSaveGameRecord_StringValue>();
	if (!ExpectedRecord)
	{
		AssertWrongTypeRecord(ExactRecordTag, GetNameSafe(FoundRecord.ExpectedValue.GetScriptStruct()), "FSaveGameRecord_StringValue");
		return ECommonAvailability::Unavailable;
	}

	OutActualValue = ExpectedRecord->Value;
	return ECommonAvailability::Available;
}

ECommonAvailability USaveGamePresetAnalyzer_BlueprintBase::FindOrAssertExpectedValue_StringList(FGameplayTag ExactRecordTag, TArray<FString>& OutActualValues)
{
	FSaveGameRecord FoundRecord;
	if (!FindSaveGameRecordByTag(ExactRecordTag, OUT FoundRecord))
	{
		AssertMissingRecord(ExactRecordTag);
		return ECommonAvailability::Unavailable;
	}

	const FSaveGameRecord_StringListValue* ExpectedRecord = FoundRecord.ExpectedValue.GetPtr<FSaveGameRecord_StringListValue>();
	if (!ExpectedRecord)
	{
		AssertWrongTypeRecord(ExactRecordTag, GetNameSafe(FoundRecord.ExpectedValue.GetScriptStruct()), "FSaveGameRecord_StringListValue");
		return ECommonAvailability::Unavailable;
	}

	OutActualValues = ExpectedRecord->Values;
	return ECommonAvailability::Available;
}

void USaveGamePresetAnalyzer_BlueprintBase::CompareRecords(const FSaveGameRecord& ActualRecord, const FSaveGameRecord& ExpectedRecord)
{
	const TOptional<FSaveGameRecordMismatch> Mismatch = Super::CompareRecords(CachedTester.Get(), ActualRecord, ExpectedRecord);
#if WITH_EDITOR
	if (CachedExpectedReport == nullptr)
		return;

	if (Mismatch.IsSet())
	{
		CachedExpectedReport->AddMismatchRecord(*Mismatch);
	}
	else
	{
		// Remove any outdated mismatches from previous test runs:
		CachedExpectedReport->RemoveMismatchRecord(ActualRecord);
	}
#endif
}

void USaveGamePresetAnalyzer_BlueprintBase::AssertMismatchError(FString Error, const FSaveGameRecord& ActualRecord, const FSaveGameRecord& ExpectedRecord)
{
	AssertError(CachedTester.Get(), Error);
#if WITH_EDITOR
	if (CachedExpectedReport != nullptr)
	{
		CachedExpectedReport->AddMismatchRecord({ExpectedRecord.UniqueIdentifier, ActualRecord, ExpectedRecord});
	}
#endif
}

void USaveGamePresetAnalyzer_BlueprintBase::AssertMismatchWarning(FString Warning, const FSaveGameRecord& ActualRecord, const FSaveGameRecord& ExpectedRecord)
{
	AssertWarning(CachedTester.Get(), Warning);
#if WITH_EDITOR
	if (CachedExpectedReport != nullptr)
	{
		CachedExpectedReport->AddMismatchRecord({ExpectedRecord.UniqueIdentifier, ActualRecord, ExpectedRecord});
	}
#endif
}

void USaveGamePresetAnalyzer_BlueprintBase::AssertMismatchInfo(FString Info, const FSaveGameRecord& ActualRecord, const FSaveGameRecord& ExpectedRecord)
{
	AssertInfo(CachedTester.Get(), Info);
#if WITH_EDITOR
	if (CachedExpectedReport != nullptr)
	{
		CachedExpectedReport->AddMismatchRecord({ExpectedRecord.UniqueIdentifier, ActualRecord, ExpectedRecord});
	}
#endif
}

void USaveGamePresetAnalyzer_BlueprintBase::AssertSimpleError(FString Error)
{
	AssertError(CachedTester.Get(), Error);
}

void USaveGamePresetAnalyzer_BlueprintBase::AssertSimpleWarning(FString Warning)
{
	AssertWarning(CachedTester.Get(), Warning);
}

void USaveGamePresetAnalyzer_BlueprintBase::AssertSimpleInfo(FString Info)
{
	AssertInfo(CachedTester.Get(), Info);
}

void USaveGamePresetAnalyzer_BlueprintBase::AssertMissingRecord(const FGameplayTag& MissingRecordTag)
{
	AssertSimpleInfo(FString::Printf(TEXT("Expected SaveGameRecord \"%s\" does not exist."), *MissingRecordTag.ToString()));
}

void USaveGamePresetAnalyzer_BlueprintBase::AssertWrongTypeRecord(const FGameplayTag& MissingRecordTag, const FString& ActualType, const FString& ExpectedType)
{
	AssertSimpleError(FString::Printf(TEXT("Expected SaveGameRecord \"%s\" is of wrong type. Expected: %s, Actual: %s"), *MissingRecordTag.ToString(), *ExpectedType, *ActualType));
}
