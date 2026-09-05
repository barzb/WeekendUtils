///////////////////////////////////////////////////////////////////////////////////////
/// Copyright (C) by Benjamin Barz and contributors. See file: CREDITS.md
///
/// This file is part of the WeekendUtils UE5 Plugin.
///
/// Distributed under the MIT License. See file: LICENSE.md
///
///////////////////////////////////////////////////////////////////////////////////////

#include "Misc/Build.h"

#if WITH_AUTOMATION_WORKER && WITH_EDITOR

#include "AutomationTest/AutomationSpecMacros.h"
#include "AutomationTest/AutomationTestWorld.h"
#include "GameplayTagContainer.h"
#include "GameplayTagsManager.h"
#include "Engine/World.h"
#include "SaveGame/Analysis/SaveGameAnalysisReport.h"
#include "SaveGame/ModularSaveGame.h"
#include "SaveGame/SaveGameObjectPreset.h"
#include "SaveGame/SaveGamePreset.h"
#include "SaveGame/Settings/SaveGameServiceSettings.h"
#include "SaveGame/Mocks/MockSaveGamePresetAnalyzer.h"

#define SPEC_TEST_CATEGORY "WeekendUtils.SaveGame"

using namespace WeekendUtils;

///////////////////////////////////////////////////////////////////////////////////////

namespace
{
	struct FTestTags : FGameplayTagNativeAdder
	{
		FGameplayTag Root;
		FGameplayTag Tag1;
		FGameplayTag Tag2;

		virtual void AddTags() override
		{
			UGameplayTagsManager& Manager = UGameplayTagsManager::Get();
			Root = Manager.AddNativeGameplayTag(TEXT("Test.WeekendUtils.SaveGamePresetAnalysis"));
			Tag1 = Manager.AddNativeGameplayTag(TEXT("Test.WeekendUtils.SaveGamePresetAnalysis.Tag1"));
			Tag2 = Manager.AddNativeGameplayTag(TEXT("Test.WeekendUtils.SaveGamePresetAnalysis.Tag2"));
		}
	};
	FTestTags GTestTags;
}

///////////////////////////////////////////////////////////////////////////////////////

WE_BEGIN_DEFINE_SPEC(SaveGamePresetAnalysis)
	TSharedPtr<FScopedAutomationTestWorld> TestWorld;
	TObjectPtr<UModularSaveGame> TestSaveGame;
	TObjectPtr<USaveGameObjectPreset> TestPreset;
	TArray<FSaveGamePresetAnalyzerEntry> OriginalAnalyzers;
WE_END_DEFINE_SPEC(SaveGamePresetAnalysis)
{
	BeforeEach([this]
	{
		TestWorld = MakeShared<FScopedAutomationTestWorld>(SpecTestWorldName);
		TestWorld->InitializeGame();

		TestSaveGame = NewObject<UModularSaveGame>();
		TestPreset = NewObject<USaveGameObjectPreset>();
		TestPreset->PresetName = TEXT("TestPreset");

		// Save and replace analyzer settings:
		USaveGameServiceSettings* Settings = GetMutableDefault<USaveGameServiceSettings>();
		OriginalAnalyzers = Settings->PresetAnalyzers;
		Settings->PresetAnalyzers.Empty();
		FSaveGamePresetAnalyzerEntry MockEntry;
		MockEntry.AnalyzerClass = UMockSaveGamePresetAnalyzer::StaticClass();
		Settings->PresetAnalyzers.Add(MockEntry);

		UMockSaveGamePresetAnalyzer::ResetMockState();
	});

	AfterEach([this]
	{
		// Restore original analyzer settings:
		USaveGameServiceSettings* Settings = GetMutableDefault<USaveGameServiceSettings>();
		Settings->PresetAnalyzers = OriginalAnalyzers;

		UMockSaveGamePresetAnalyzer::ResetMockState();
		TestPreset = nullptr;
		TestSaveGame = nullptr;
		TestWorld.Reset();
	});

	Describe("CreateAnalysisReport", [this]
	{
		It("should produce an empty report when no analyzers return records.", [this]
		{
			UMockSaveGamePresetAnalyzer::MockRecordsToReturn.Empty();

			const FSaveGameAnalysisReport Report = USaveGamePreset::CreateAnalysisReport(TestSaveGame, &TestPreset->HeaderData, FString("test.sav"));

			TestFalse("Report has no records", Report.HasRecords());
			if (TestTrue("Source file name", Report.SourceFileName.IsSet()))
			{
				TestEqual("Source file name", *Report.SourceFileName, FString("test.sav"));
			}
			TestTrue("AnalyzedAtUtc is set", Report.AnalyzedAtUtc > FDateTime());
		});

		It("should collect records from registered analyzers.", [this]
		{
			const FGameplayTag TestTag1 = GTestTags.Tag1;
			const FGameplayTag TestTag2 = GTestTags.Tag2;

			FSaveGameRecord Record1;
			Record1.RecordTag = TestTag1;
			Record1.Description = FText::FromString("Record 1");
			Record1.MismatchVerbosity = ESaveGameRecordVerbosity::Error;
			UMockSaveGamePresetAnalyzer::MockRecordsToReturn.Add(Record1);

			FSaveGameRecord Record2;
			Record2.RecordTag = TestTag2;
			Record1.Description = FText::FromString("Record 1");
			Record2.MismatchVerbosity = ESaveGameRecordVerbosity::Info;
			UMockSaveGamePresetAnalyzer::MockRecordsToReturn.Add(Record2);

			const FSaveGameAnalysisReport Report = USaveGamePreset::CreateAnalysisReport(TestSaveGame, &TestPreset->HeaderData);

			const TArray<FSaveGameRecord> Records = Report.GetRecords();
			TestTrue("Report has records", Report.HasRecords());
			TestEqual("Record count", Records.Num(), 2);
			TestEqual("First record description", Records[0].Description.ToString(), Record1.Description.ToString());
			TestEqual("Second record Verbosity", Records[1].MismatchVerbosity, Record2.MismatchVerbosity);
		});

		It("should assemble generated description from analyzer records.", [this]
		{
			FSaveGameRecord Record1;
			Record1.RecordTag = GTestTags.Tag1;
			Record1.Description = FText::FromString("Record 1");
			UMockSaveGamePresetAnalyzer::MockRecordsToReturn.Add(Record1);

			const FSaveGameAnalysisReport Report = USaveGamePreset::CreateAnalysisReport(TestSaveGame, &TestPreset->HeaderData);

			TestFalse("Generated description is not empty", Report.GeneratedDescription.IsEmpty());

			// Records are grouped by the 3rd segment of their record tag, each contributing its description:
			const FString Description = Report.GeneratedDescription.ToString();
			TestTrue("Description contains record category", Description.Contains(TEXT("## SaveGamePresetAnalysis ##")));
			TestTrue("Description contains record description", Description.Contains(TEXT("- Record 1")));
		});

		It("should return empty report for non-modular savegames.", [this]
		{
			const FSaveGameAnalysisReport Report = USaveGamePreset::CreateAnalysisReport(nullptr, &TestPreset->HeaderData);

			TestFalse("Report has no records", Report.HasRecords());
		});
	});

	Describe("FSaveGameAnalysisReport", [this]
	{
		It("should filter records by gameplay tag.", [this]
		{
			const FGameplayTag AnalysisRoot = GTestTags.Root;
			const FGameplayTag TestTag1 = GTestTags.Tag1;
			const FGameplayTag TestTag2 = GTestTags.Tag2;

			FSaveGameRecord Record1;
			Record1.RecordTag = TestTag1;
			Record1.Description = FText::FromString("Record 1");

			FSaveGameRecord Record2;
			Record2.RecordTag = TestTag2;
			Record2.Description = FText::FromString("Record 2");

			FSaveGameAnalysisReport Report;
			Report.AddGeneratedRecord(Record1);
			Report.AddGeneratedRecord(Record2);

			const TArray<FSaveGameRecord> AllRecords = Report.FindRecordsByTag(AnalysisRoot);
			TestEqual("All records match root tag", AllRecords.Num(), 2);

			const TArray<FSaveGameRecord> Tag1Records = Report.FindRecordsByTag(TestTag1);
			TestEqual("Filtered Tag1 records", Tag1Records.Num(), 1);
		});
	});
}

#undef SPEC_TEST_CATEGORY
#endif // WITH_AUTOMATION_WORKER && WITH_EDITOR
