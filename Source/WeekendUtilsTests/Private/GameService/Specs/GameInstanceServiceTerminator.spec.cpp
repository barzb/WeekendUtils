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

#include "GameService/GameInstanceServiceTerminator.h"
#include "GameService/GameServiceManager.h"
#include "GameService/Mocks/GameServiceMocks.h"

#define SPEC_TEST_CATEGORY "WeekendUtils.GameService"

using namespace WeekendUtils;
using namespace Mocks;

/**
 * @UGameInstanceServiceTerminator members are not accessible outside the WeekendUtils module,
 * but some of its inner workings can still be verified via tests.
 */
WE_BEGIN_DEFINE_SPEC(GameInstanceServiceTerminator)
	TSharedPtr<FScopedAutomationTestWorld> TestWorld;
	TObjectPtr<UGameInstanceSubsystem> GameInstanceServiceTerminator;
WE_END_DEFINE_SPEC(GameInstanceServiceTerminator)
{
	BeforeEach([this]
	{
		TestWorld = MakeShared<FScopedAutomationTestWorld>(SpecTestWorldName);
		TestWorld->InitializeGame();

		GameInstanceServiceTerminator = TestWorld->AsRef().GetGameInstance()->GetSubsystem<UGameInstanceServiceTerminator>();
		ensure(GameInstanceServiceTerminator);
	});

	AfterEach([this]
	{
		TestWorld.Reset();
	});

	Describe("Deinitialize", [this]
	{
		It("should shutdown all running GameServices with EGameServiceLifetime::ShutdownWithGameInstance.", [this]
		{
			UGameServiceManager& ServiceManager = UGameServiceManager::SummonInstance(TestWorld->AsPtr());
			UVoidService& GameInstanceService  = *NewObject<UVoidService>(TestWorld->AsPtr());
			GameInstanceService.Lifetime = EGameServiceLifetime::ShutdownWithGameInstance;
			ServiceManager.StartService<UVoidService>(TestWorld->AsRef(), GameInstanceService);

			GameInstanceServiceTerminator->Deinitialize();
			TestTrue("GameInstanceService.bWasShutDown ", GameInstanceService.bWasShutDown);
		});

		It("should shutdown all running GameServices with EGameServiceLifetime::ShutdownWithWorld.", [this]
		{
			UGameServiceManager& ServiceManager = UGameServiceManager::SummonInstance(TestWorld->AsPtr());
			UVoidService& WorldService  = *NewObject<UVoidService>(TestWorld->AsPtr());
			WorldService.Lifetime = EGameServiceLifetime::ShutdownWithWorld;
			ServiceManager.StartService<UVoidService>(TestWorld->AsRef(), WorldService);

			GameInstanceServiceTerminator->Deinitialize();
			TestTrue("WorldService.bWasShutDown ", WorldService.bWasShutDown);
		});
	});
}

#undef SPEC_TEST_CATEGORY
#endif WITH_AUTOMATION_WORKER
