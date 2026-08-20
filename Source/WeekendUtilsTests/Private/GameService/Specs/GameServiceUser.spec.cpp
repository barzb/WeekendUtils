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
#include "AutomationTest/Mocks/SubsystemMocks.h"
#include "GameService/GameServiceConfig.h"
#include "GameService/GameServiceManager.h"
#include "GameService/GameServiceUser.h"
#include "GameService/Mocks/GameServiceMocks.h"
#include "GameService/Mocks/GameServiceUserMocks.h"

#define SPEC_TEST_CATEGORY "WeekendUtils.GameService"

using namespace WeekendUtils;
using namespace Mocks;

WE_BEGIN_DEFINE_SPEC(GameServiceUser)
	TSharedPtr<FScopedAutomationTestWorld> TestWorld;
	TObjectPtr<UGameServiceUserMock> ServiceUser;
WE_END_DEFINE_SPEC(GameServiceUser)
{
	BeforeEach([this]
	{
		TestWorld = MakeShared<FScopedAutomationTestWorld>(SpecTestWorldName);
		TestWorld->InitializeGame();

		UGameServiceManager::SummonInstance(TestWorld->AsPtr()).ClearServiceRegister(EGameServiceLifetime::ShutdownWithWorld);
		UGameServiceConfig::CreateForWorld(TestWorld->AsRef(), [](UGameServiceConfig& Config)
		{
			Config.SetPriority(100);
			Config.AddService<UVoidService>();
			Config.AddService<IMockGameServiceInterface, UInterfacedService>();
		});

		// Note that this "mock" is not mocking the unit under test, just wrapping it within a UObject.
		ServiceUser = NewObject<UGameServiceUserMock>(TestWorld->AsPtr());
	});

	AfterEach([this]
	{
		ServiceUser->StopWaitingForDependencies();
		ServiceUser = nullptr;
		TestWorld.Reset();
	});

	Describe("UseGameService", [this]
	{
		It("should make sure the used service was started before returning it", [this]
		{
			ServiceUser->ServiceDependencies.Add<UVoidService>();
			const UVoidService& Service = ServiceUser->UseGameService<UVoidService>();
			TestTrue("Service.bWasStarted", Service.bWasStarted);
		});
	});

	Describe("UseGameServiceAsPtr", [this]
	{
		It("should return a valid pointer", [this]
		{
			ServiceUser->ServiceDependencies.Add<UVoidService>();
			ServiceUser->ServiceDependencies.Add<IMockGameServiceInterface>();

			const TObjectPtr<UVoidService> ObjectService = ServiceUser->UseGameServiceAsPtr<UVoidService>();
			TestTrue("IsValid(ObjectService)", IsValid(ObjectService.Get()));

			const TScriptInterface<IMockGameServiceInterface> InterfaceService = ServiceUser->UseGameServiceAsInterface<IMockGameServiceInterface>();
			TestTrue("IsValid(InterfaceService)", IsValid(InterfaceService.GetObject()));
		});
	});

	Describe("FindOptionalGameService", [this]
	{
		It("should return nullptr when the used service wasn't started before", [this]
		{
			ServiceUser->ServiceDependencies.Add<UVoidService>();
			ServiceUser->ServiceDependencies.Add<IMockGameServiceInterface>();

			const TWeakObjectPtr<UVoidService> OptionalObjectService = ServiceUser->FindOptionalGameService<UVoidService>();
			TestFalse("IsValid(OptionalObjectService)", OptionalObjectService.IsValid());

			const TWeakInterfacePtr<IMockGameServiceInterface> OptionalInterfaceService = ServiceUser->FindOptionalGameService<IMockGameServiceInterface>();
			TestFalse("IsValid(OptionalInterfaceService)", OptionalInterfaceService.IsValid());
		});

		It("should return a valid pointer to the existing service if service was already started", [this]
		{
			ServiceUser->ServiceDependencies.Add<UVoidService>();
			ServiceUser->ServiceDependencies.Add<IMockGameServiceInterface>();

			ServiceUser->UseGameService<UVoidService>(); // Starts the service.
			const TWeakObjectPtr<UVoidService> OptionalObjectService = ServiceUser->FindOptionalGameService<UVoidService>();
			TestTrue("IsValid(OptionalObjectService)", IsValid(OptionalObjectService.Get()));

			ServiceUser->UseGameServiceAsInterface<IMockGameServiceInterface>(); // Starts the service.
			const TWeakInterfacePtr<IMockGameServiceInterface> OptionalInterfaceService = ServiceUser->FindOptionalGameService<IMockGameServiceInterface>();
			TestTrue("IsValid(OptionalInterfaceService)", OptionalInterfaceService.IsValid());
		});
	});

	Describe("FindSubsystemDependency", [this]
	{
		It("should be able to find a UEngineSubsystem", [this]
		{
			ServiceUser->SubsystemDependencies.Add<UEngineSubsystemMock>();
			const TWeakObjectPtr<UEngineSubsystemMock> EngineSubsystem = ServiceUser->FindSubsystemDependency<UEngineSubsystemMock>();
			TestTrue("IsValid(EngineSubsystem)", EngineSubsystem.IsValid());
		});

		It("should be able to find a UWorldSubsystem", [this]
		{
			ServiceUser->SubsystemDependencies.Add<UWorldSubsystemMock>();
			const TWeakObjectPtr<UWorldSubsystemMock> WorldSubsystem = ServiceUser->FindSubsystemDependency<UWorldSubsystemMock>();
			TestTrue("IsValid(WorldSubsystem)", WorldSubsystem.IsValid());
		});

		It("should be able to find a UGameInstanceSubsystem", [this]
		{
			ServiceUser->SubsystemDependencies.Add<UGameInstanceSubsystemMock>();
			const TWeakObjectPtr<UGameInstanceSubsystemMock> GameInstanceSubsystem = ServiceUser->FindSubsystemDependency<UGameInstanceSubsystemMock>();
			TestTrue("IsValid(GameInstanceSubsystem)", GameInstanceSubsystem.IsValid());
		});

		It("should be able to find a ULocalPlayerSubsystem", [this]
		{
			ServiceUser->SubsystemDependencies.Add<ULocalPlayerSubsystemMock>();
			const TWeakObjectPtr<ULocalPlayerSubsystemMock> LocalPlayerSubsystem = ServiceUser->FindSubsystemDependency<ULocalPlayerSubsystemMock>();
			TestTrue("IsValid(LocalPlayerSubsystem)", LocalPlayerSubsystem.IsValid());
		});
	});

	Describe("WaitForDependencies", [this]
	{
		It("should immediately execute the callback when all dependencies are already running", [this]
		{
			ServiceUser->ServiceDependencies.Add<UVoidService>();
			ServiceUser->UseGameService<UVoidService>(); // Starts the service.

			bool bWasCallbackExecuted = false;
			ServiceUser->WaitForDependencies(UGameServiceUserMock::FOnWaitingFinished::CreateLambda([this, &bWasCallbackExecuted]
			{
				bWasCallbackExecuted = true;
				const TWeakObjectPtr<UVoidService> DependencyService = ServiceUser->FindOptionalGameService<UVoidService>();
				TestTrue("IsValid(DependencyService)", DependencyService.IsValid());
			}));

			TestTrue("bWasCallbackExecuted", bWasCallbackExecuted);
		});

		It("should immediately execute the callback when NOT all dependencies are running, BUT they could be started", [this]
		{
			ServiceUser->ServiceDependencies.Add<UVoidService>(); // Started automatically.

			bool bWasCallbackExecuted = false;
			ServiceUser->WaitForDependencies(UGameServiceUserMock::FOnWaitingFinished::CreateLambda([this, &bWasCallbackExecuted]
			{
				bWasCallbackExecuted = true;
			}));

			TestTrue("bWasCallbackExecuted", bWasCallbackExecuted);
		});

		It("should NOT immediately execute the callback when NOT all dependencies are running", [this]
		{
			ServiceUser->ServiceDependencies.Add<UVoidService>(); // Started automatically.
			ServiceUser->SubsystemDependencies.Add<UWorldSubsystem>(); // Abstract class can never be created.

			bool bWasCallbackExecuted = false;
			ServiceUser->WaitForDependencies(UGameServiceUserMock::FOnWaitingFinished::CreateLambda([this, &bWasCallbackExecuted]
			{
				bWasCallbackExecuted = true;
			}));

			TestFalse("bWasCallbackExecuted", bWasCallbackExecuted);
		});

		It("should execute the callback after all dependencies are running", [this]
		{
			ServiceUser->ServiceDependencies.Add<UVoidService>(); // Started automatically.
			ServiceUser->SubsystemDependencies.Add<UWorldSubsystem>(); // Abstract class can never be created.

			bool bWasCallbackExecuted = false;
			ServiceUser->WaitForDependencies(UGameServiceUserMock::FOnWaitingFinished::CreateLambda([this, &bWasCallbackExecuted]
			{
				bWasCallbackExecuted = true;
				const TWeakObjectPtr<UVoidService> DependencyService = ServiceUser->FindOptionalGameService<UVoidService>();
				TestTrue("IsValid(DependencyService)", DependencyService.IsValid());
			}));

			// Service not yet started:
			ServiceUser->SimulateTick();
			TestFalse("bWasCallbackExecuted", bWasCallbackExecuted);

			// Now it should trigger:
			ServiceUser->SubsystemDependencies.Remove(UWorldSubsystem::StaticClass()); // Remove the impossible dependency.
			ServiceUser->SimulateTick();
			TestTrue("bWasCallbackExecuted", bWasCallbackExecuted);
		});
	});
}

#undef SPEC_TEST_CATEGORY
#endif WITH_AUTOMATION_WORKER
