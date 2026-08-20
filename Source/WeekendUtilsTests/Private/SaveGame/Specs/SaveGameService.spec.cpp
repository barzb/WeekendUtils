///////////////////////////////////////////////////////////////////////////////////////
/// Copyright (C) by Benjamin Barz and contributors. See file: CREDITS.md
///
/// This file is part of the WeekendUtils UE5 Plugin.
///
/// Distributed under the MIT License. See file: LICENSE.md
///
///////////////////////////////////////////////////////////////////////////////////////

#include "Misc/Build.h"

#if WITH_AUTOMATION_WORKER

#include "AutomationTest/AutomationSpecMacros.h"
#include "AutomationTest/AutomationTestWorld.h"
#include "GameService/GameServiceManager.h"
#include "SaveGame/Mocks/MockSaveGameSerializer.h"
#include "SaveGame/SaveGameService.h"

#define SPEC_TEST_CATEGORY "WeekendUtils.SaveGame"

using namespace WeekendUtils;

WE_BEGIN_DEFINE_SPEC(SaveGameService)
	TSharedPtr<FScopedAutomationTestWorld> TestWorld;
	TObjectPtr<USaveGameService> SaveGameService;
	TObjectPtr<UMockSaveGameSerializer> SaveGameSerializer;
	static inline FString TestSlotName = "Test";
	static inline int32 UserIndex = 0;
WE_END_DEFINE_SPEC(SaveGameService)
{
	BeforeEach([this]
	{
		TestWorld = MakeShared<FScopedAutomationTestWorld>(SpecTestWorldName);
		TestWorld->InitializeGame();

		SaveGameService = &UGameServiceManager::SummonInstance(TestWorld->AsPtr()).StartService<USaveGameService>(TestWorld->AsRef());
		SaveGameSerializer = SaveGameService->GetSaveGameSerializer<UMockSaveGameSerializer>();
	});

	AfterEach([this]
	{
		UGameServiceManager::SummonInstance(TestWorld->AsPtr()).ShutdownAllServices();
		SaveGameService = nullptr;
		TestWorld.Reset();
	});

	Describe("RequestAutosave", [this]
	{
		It("should create a SaveGame file for the current game in the Autosave slot.", [this]
		{
			if (!TestNotNull("MockSaveGameSerializer", SaveGameSerializer.Get()))
				return;

			const FString AutosaveSlotName = SaveGameService->GetAutosaveSlotName();
			TestFalse("Autosave file exists before autosaving", SaveGameSerializer->DoesSaveGameExist(AutosaveSlotName, UserIndex));

			SaveGameService->RequestAutosave("Test");
			TestTrue("Autosave file exists after autosaving", SaveGameSerializer->DoesSaveGameExist(AutosaveSlotName, UserIndex));
		});

		It("should call the async Callback after the game was saved.", [this]
		{
			if (!TestNotNull("MockSaveGameSerializer", SaveGameSerializer.Get()))
				return;

			USaveGame* SaveGamePassedByCallback = nullptr;
			TSharedRef<bool> bWasCallbackCalledWithSuccess = MakeShared<bool>(false);
			SaveGameService->RequestAutosave("Test", USaveGameService::FOnSaveLoadCompleted::CreateLambda([&](USaveGame* SavedGame, bool bSuccess)
			{
				SaveGamePassedByCallback = SavedGame;
				*bWasCallbackCalledWithSuccess = bSuccess;
			}));

			TestNotNull("SaveGamePassedByCallback", SaveGamePassedByCallback);
			TestTrue("bWasCallbackCalledWithSuccess", *bWasCallbackCalledWithSuccess);
		});
	});

	Describe("RequestSaveCurrentSaveGameToSlot", [this]
	{
		It("should create a SaveGame file for the current game in the desired slot.", [this]
		{
			if (!TestNotNull("MockSaveGameSerializer", SaveGameSerializer.Get()))
				return;

			TestFalse("Save file exists before saving", SaveGameSerializer->DoesSaveGameExist(TestSlotName, UserIndex));

			SaveGameService->RequestSaveCurrentSaveGameToSlot("Test", TestSlotName);
			TestTrue("Save file exists after saving", SaveGameSerializer->DoesSaveGameExist(TestSlotName, UserIndex));
		});

		It("should call the async Callback after the game was saved.", [this]
		{
			if (!TestNotNull("MockSaveGameSerializer", SaveGameSerializer.Get()))
				return;

			USaveGame* SaveGamePassedByCallback = nullptr;
			TSharedRef<bool> bWasCallbackCalledWithSuccess = MakeShared<bool>(false);
			SaveGameService->RequestSaveCurrentSaveGameToSlot("Test", TestSlotName, USaveGameService::FOnSaveLoadCompleted::CreateLambda([&](USaveGame* SavedGame, bool bSuccess)
			{
				SaveGamePassedByCallback = SavedGame;
				*bWasCallbackCalledWithSuccess = bSuccess;
			}));

			TestNotNull("SaveGamePassedByCallback", SaveGamePassedByCallback);
			TestTrue("bWasCallbackCalledWithSuccess", *bWasCallbackCalledWithSuccess);
		});

		It("should cache the saved SaveGame", [this]
		{
			if (!TestNotNull("MockSaveGameSerializer", SaveGameSerializer.Get()))
				return;

			SaveGameService->RequestSaveCurrentSaveGameToSlot("Test", TestSlotName);

			const USaveGame* CachedSaveGame = SaveGameService->GetCachedSaveGameSnapshotAtSlot(TestSlotName);
			TestNotNull("CachedSaveGame", CachedSaveGame);
		});
	});
}

#undef SPEC_TEST_CATEGORY
#endif WITH_AUTOMATION_WORKER
