///////////////////////////////////////////////////////////////////////////////////////
/// Copyright (C) by Benjamin Barz and contributors. See file: CREDITS.md
///
/// This file is part of the WeekendUtils UE5 Plugin.
///
/// Distributed under the MIT License. See file: LICENSE.md
///
///////////////////////////////////////////////////////////////////////////////////////

#include "SaveGame/SaveGamePreset.h"

#include "AssetRegistry/IAssetRegistry.h"
#include "GameFramework/SaveGame.h"
#include "Misc/PackageName.h"
#include "GameService/GameServiceLocator.h"
#include "Misc/MessageDialog.h"
#include "SaveGame/ModularSaveGame.h"
#include "SaveGame/SaveGameHeader.h"
#include "SaveGame/SaveGameService.h"
#include "SaveGame/Analysis/SaveGamePresetAnalyzer.h"
#include "SaveGame/Settings/SaveGameServiceSettings.h"
#include "UObject/UObjectIterator.h"

#if WITH_EDITORONLY_DATA
#include "EditorUtilityLibrary.h"
#include "IAutomationControllerModule.h"
#include "ISessionFrontendModule.h"
#include "AutomationTest/AutomationTestUtils.h"
#include "Misc/DataValidation.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogSaveGamePreset, Log, All);

///////////////////////////////////////////////////////////////////////////////////////

namespace
{
	void DiscoverPresets()
	{
		if (IAssetRegistry* AssetRegistry = IAssetRegistry::Get())
		{
			const USaveGameServiceSettings* Settings = GetDefault<USaveGameServiceSettings>();
			const FString DefaultFolder = Settings->DefaultSaveGamePresetFolder.Path;
			if (FPackageName::GetPackageMountPoint(DefaultFolder).IsNone())
			{
				UE_LOG(LogSaveGamePreset, Log, TEXT("SaveGamePreset folder '%s' is not mounted (yet). Skipping preset discovery."), *DefaultFolder);
				return;
			}
			AssetRegistry->ScanPathsSynchronous({DefaultFolder});

			TArray<FAssetData> SaveGamePresetAssets;
			AssetRegistry->GetAssetsByClass(USaveGamePreset::StaticClass()->GetClassPathName(), OUT SaveGamePresetAssets, true);
			for (const FAssetData& AssetData : SaveGamePresetAssets)
			{
				LoadPackage(nullptr, *AssetData.PackageName.ToString(), LOAD_Quiet);
			}
		}
	}
}

///////////////////////////////////////////////////////////////////////////////////////

USaveGamePreset::USaveGamePreset()
{
	HeaderData = FInstancedStruct::Make<FSimpleSaveGameHeaderData>();
}

void USaveGamePreset::OpenSaveGamePresetsFolder()
{
#if WITH_EDITOR
	const FString PresetsFolder = GetDefault<USaveGameServiceSettings>()->DefaultSaveGamePresetFolder.Path;
	UEditorUtilityLibrary::SyncBrowserToFolders({PresetsFolder});
#else
	unimplemented();
#endif
}

TSet<const USaveGamePreset*> USaveGamePreset::CollectSaveGamePresets()
{
	TSet<FSlotName> OccupiedSlotNames = {};
	TSet<const USaveGamePreset*> Result = {};

	DiscoverPresets();
	for (const USaveGamePreset* Preset : TObjectRange<USaveGamePreset>())
	{
		if (!Preset || Preset->GetClass() == USaveGamePreset::StaticClass())
			continue; // Invalid or abstract class asset.
		if (!Preset->GetPresetSaveGame())
			continue;
#if !WITH_EDITOR
		if (Preset->bIsEditorOnly)
			continue;
#endif

#if (UE_BUILD_SHIPPING || UE_BUILD_TEST)
		if (Preset->bIsDeveloperOnly)
			continue;
#endif

		if (OccupiedSlotNames.Contains(Preset->PresetName))
		{
			UE_LOG(LogSaveGamePreset, Error, TEXT("Multiple SaveGamePreset assets use the same SlotName: %s"), *Preset->PresetName);
			continue;
		}

		OccupiedSlotNames.Add(Preset->PresetName);
		Result.Add(Preset);
	}

	return Result;
}

TSet<USaveGamePreset::FSlotName> USaveGamePreset::CollectSaveGamePresetNames()
{
	TSet<FSlotName> Result = {};

	DiscoverPresets();
	for (const USaveGamePreset* Preset : TObjectRange<USaveGamePreset>())
	{
		if (!Preset || !Preset->GetPresetSaveGame())
			continue;

#if !WITH_EDITOR
		if (Preset->bIsEditorOnly)
			continue;
#endif

#if (UE_BUILD_SHIPPING || UE_BUILD_TEST)
		if (Preset->bIsDeveloperOnly)
			continue;
#endif

		Result.Add(Preset->PresetName);
	}

	return Result;
}

const USaveGamePreset* USaveGamePreset::FindSaveGamePreset(const FSlotName& PresetName)
{
	DiscoverPresets();
	for (const USaveGamePreset* Preset : TObjectRange<USaveGamePreset>())
	{
		if (Preset && Preset->PresetName == PresetName)
			return Preset;
	}

	return nullptr;
}

const USaveGamePreset* USaveGamePreset::FindCurrentlyLoadedGamePreset(const UObject& WorldContextObject)
{
	const USaveGameService* SaveGameService = UGameServiceLocator::FindService<USaveGameService>(&WorldContextObject);
	if (!SaveGameService)
		return nullptr;

	const TOptional<FString> SlotName = SaveGameService->GetCurrentSaveGame().GetSlotLastRestoredFrom();
	if (!SlotName.IsSet())
		return nullptr;

	return FindSaveGamePreset(*SlotName);
}

void USaveGamePreset::RestoreAsCurrentSaveGame(USaveGameService& SaveGameService) const
{
	// Create a new save game instance, using the instanced one configured in the preset as template:
	USaveGame* SaveGameObject = CreateSaveGameObject(SaveGameService);
	SaveGameService.RestoreAsCurrentSaveGame(*SaveGameObject, PresetName);
}

void USaveGamePreset::RestoreAsAndTravelIntoCurrentSaveGame(USaveGameService& SaveGameService) const
{
	// Create a new save game instance, using the instanced one configured in the preset as template:
	USaveGame* SaveGameObject = CreateSaveGameObject(SaveGameService);
	SaveGameService.RestoreAsAndTravelIntoCurrentSaveGame(*SaveGameObject, PresetName);
}

#if WITH_EDITOR

FSaveGameAnalysisReport USaveGamePreset::CreateAnalysisReport(const USaveGame* InSaveGame, const FInstancedStruct* InHeaderData, const TOptional<FString>& InSourceFileName)
{
	FSaveGameAnalysisReport Report;
	Report.AnalyzedAtUtc = FDateTime::UtcNow();
	Report.SourceFileName = InSourceFileName;

	const UModularSaveGame* ModularSaveGame = Cast<UModularSaveGame>(InSaveGame);
	if (!ModularSaveGame)
		return Report;

	const USaveGameServiceSettings* Settings = GetDefault<USaveGameServiceSettings>();
	if (!GIsAutomationTesting) // Automation tests configure PresetAnalyzers explicitly and must not be polluted by re-registered CDOs.
	{
		for (const USaveGamePresetAnalyzer* AnalyzerInstance : TObjectRange<USaveGamePresetAnalyzer>(RF_NoFlags))
		{
			AnalyzerInstance->RegisterWithSaveGameSettings(); // Make sure all analyzers are registered.
		}
	}

	TArray<FSaveGameRecord> Records;
	for (const FSaveGamePresetAnalyzerEntry& AnalyzerEntry : Settings->PresetAnalyzers)
	{
		if (!AnalyzerEntry.bIsEnabled)
			continue;

		const TSubclassOf<USaveGamePresetAnalyzer> AnalyzerClass = AnalyzerEntry.AnalyzerClass.LoadSynchronous();
		if (!AnalyzerClass)
			continue;

		// Run Analyzer:
		const USaveGamePresetAnalyzer* Analyzer = GetDefault<USaveGamePresetAnalyzer>(AnalyzerClass);
		Analyzer->AnalyzeSaveGame(*ModularSaveGame, InHeaderData, OUT Records);
	}

	Report.GeneratedRecords = Records;
	Report.UpdateGeneratedDescription();
	UE_LOG(LogSaveGamePreset, Log, TEXT("Analysis produced %d records from %d analyzers."), Report.GeneratedRecords.Num(), Settings->PresetAnalyzers.Num());

	return Report;
}

FSaveGameAnalysisReport USaveGamePreset::CreateAnalysisReport(const USaveGame* InSaveGame, const TSubclassOf<USaveGamePresetAnalyzer>& AnalyzerClass, const FInstancedStruct* InHeaderData)
{
	FSaveGameAnalysisReport Report;
	Report.AnalyzedAtUtc = FDateTime::UtcNow();

	const UModularSaveGame* ModularSaveGame = Cast<UModularSaveGame>(InSaveGame);
	if (!ModularSaveGame)
		return Report;

	const USaveGameServiceSettings* Settings = GetDefault<USaveGameServiceSettings>();

	TArray<FSaveGameRecord> Records;
	for (const FSaveGamePresetAnalyzerEntry& AnalyzerEntry : Settings->PresetAnalyzers)
	{
		if (!AnalyzerEntry.bIsEnabled)
			continue;

		if (AnalyzerEntry.AnalyzerClass.LoadSynchronous() != AnalyzerClass)
			continue;

		// Run Analyzer:
		const USaveGamePresetAnalyzer* Analyzer = GetDefault<USaveGamePresetAnalyzer>(AnalyzerClass);
		Analyzer->AnalyzeSaveGame(*ModularSaveGame, InHeaderData, OUT Records);
		break;
	}

	Report.GeneratedRecords = Records;
	Report.UpdateGeneratedDescription();
	UE_LOG(LogSaveGamePreset, Log, TEXT("Analysis produced %d records from %d analyzers."), Report.GeneratedRecords.Num(), Settings->PresetAnalyzers.Num());

	return Report;
}

void USaveGamePreset::UpdateAnalysis()
{
	const TArray OverwriteRecords = AnalysisReport.OverrideRecords;

	AnalysisReport = CreateAnalysisReport(GetPresetSaveGame(), &HeaderData, AnalysisReport.SourceFileName);

	if (OverwriteRecords.Num() > 0)
	{
		AnalysisReport.OverrideRecords = OverwriteRecords;
		AnalysisReport.UpdateGeneratedDescription();
	}

	MarkPackageDirty();
}

void USaveGamePreset::UpdateAnalysisDescription()
{
	AnalysisReport.UpdateGeneratedDescription();
	MarkPackageDirty();
}

void USaveGamePreset::ReviewMismatchedRecords()
{
	int32 NumAffectedRecords = 0, NumResolvedErrors = 0;
	TOptional<EAppReturnType::Type> Result = {};

	for (int32 i = 0; i < AnalysisReport.MismatchedRecords.Num(); i++)
	{
		// Break out of the loop if Cancel/NoAll is selected, see switch-case below.
		if (Result.IsSet() && *Result == EAppReturnType::Cancel)
			break;

		const FSaveGameRecordMismatch Mismatch = AnalysisReport.MismatchedRecords[i];
		++NumAffectedRecords;

		// Previous result will be kept if YesAll is selected, see switch-case below.
		if (!Result.IsSet())
		{
			const EAppMsgCategory AppCategory = Mismatch.ExpectedRecord.MismatchVerbosity == ESaveGameRecordVerbosity::Error
				? EAppMsgCategory::Error : Mismatch.ExpectedRecord.MismatchVerbosity == ESaveGameRecordVerbosity::Warning
					? EAppMsgCategory::Warning : EAppMsgCategory::Info;
			Result = FMessageDialog::Open(
				AppCategory, EAppMsgType::YesNoYesAllNoAllCancel,
				FText::FromString(FString::Printf(TEXT("%s in Record \"%s\"\n\n> Expected:\n%s\n\n> Actual:\n%s\n\nAdd exception to Override Records?"),
					*WeekendUtils::EnumToString(Mismatch.ExpectedRecord.MismatchVerbosity),
					*Mismatch.ExpectedRecord.UniqueIdentifier.ToString(),
					*Mismatch.ExpectedRecord.Description.ToString(),
					*Mismatch.ActualRecord.Description.ToString())),
				FText::FromString(*FString::Printf(TEXT("Review Test Mismatch [%d/%d]"), i + 1, AnalysisReport.MismatchedRecords.Num())));
		}

		switch (*Result)
		{
		case EAppReturnType::No:
			Result.Reset();
			break;

		case EAppReturnType::Yes:
			Result.Reset(); // Fall-through..
		case EAppReturnType::YesAll:
			AnalysisReport.AddOverrideForActualRecord(Mismatch.ActualRecord);
			AnalysisReport.MismatchedRecords.RemoveAt(i--); // Remove and adjust iterator.
			++NumResolvedErrors;
			break;

		default:
		case EAppReturnType::NoAll:
		case EAppReturnType::Cancel:
			Result = EAppReturnType::Cancel;
			break;
		}
	}

	if (NumResolvedErrors > 0)
	{
		UpdateAnalysisDescription();
	}

	if (NumAffectedRecords == 0)
	{
		FMessageDialog::Open(EAppMsgType::Ok,
			FText::FromString("No Errors have been recorded. Run relevant AutomationTests, then review asset again."),
			FText::FromString("Nothing to review"));
	}
}

void USaveGamePreset::OpenTestsInAutomationFrontend() const
{
	IAutomationControllerModule& AutomationModule = FModuleManager::LoadModuleChecked<IAutomationControllerModule>(TEXT("AutomationController"));
	const IAutomationControllerManagerPtr Controller = AutomationModule.GetAutomationController();

	// Collect all test names, filter for tests that end with this preset's name:
	TArray<FString> AllTestNames;
	Controller->GetFilteredTestNames(OUT AllTestNames);
	const FString Suffix = TEXT(".") + PresetName;
	const TArray<FString> MatchingTests = AllTestNames.FilterByPredicate([&Suffix](const FString& TestName)
	{
		return TestName.EndsWith(Suffix);
	});

	WeekendUtils::OpenTestsInAutomationTestFrontend(MatchingTests);
}

EDataValidationResult USaveGamePreset::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);
	if (PresetName.IsEmpty())
	{
		Context.AddError(INVTEXT("SlotName is not set. Should be a unique name across all other presets!"));
		Result = CombineDataValidationResults(Result, EDataValidationResult::Invalid);
	}
	const USaveGame* SaveGameObject = GetPresetSaveGame();
	if (!SaveGameObject)
	{
		Context.AddError(INVTEXT("SaveGame is invalid."));
		Result = CombineDataValidationResults(Result, EDataValidationResult::Invalid);
	}
	else
	{
		Result = CombineDataValidationResults(Result, SaveGameObject->IsDataValid(Context));
	}
	return Result;
}
#endif

FPrimaryAssetId USaveGamePreset::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(USaveGamePreset::StaticClass()->GetFName(), GetFName());
}

bool USaveGamePreset::NeedsLoadForClient() const
{
#if UE_BUILD_SHIPPING
	if (bIsDeveloperOnly)
		return false;
#endif
#if !WITH_EDITOR
	if (bIsEditorOnly)
		return false;
#endif
	return Super::NeedsLoadForClient();
}

bool USaveGamePreset::NeedsLoadForServer() const
{
#if UE_BUILD_SHIPPING
	if (bIsDeveloperOnly)
		return false;
#endif
#if !WITH_EDITOR
	if (bIsEditorOnly)
		return false;
#endif
	return Super::NeedsLoadForServer();
}
