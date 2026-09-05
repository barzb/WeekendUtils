///////////////////////////////////////////////////////////////////////////////////////
/// Copyright (C) by Benjamin Barz and contributors. See file: CREDITS.md
///
/// This file is part of the WeekendUtils UE5 Plugin.
///
/// Distributed under the MIT License. See file: LICENSE.md
///
///////////////////////////////////////////////////////////////////////////////////////

#include "GameService/WorldGameServiceRunner.h"

#include "WeekendGameService.h"
#include "Engine/World.h"
#include "GameService/GameModeServiceConfigBase.h"
#include "GameService/GameServiceBase.h"
#include "GameService/GameServiceManager.h"
#include "GameService/GameServiceWorldBootstrapper.h"
#include "GameService/WorldServiceConfigBase.h"
#include "Subsystems/SubsystemCollection.h"

namespace Internal
{
	static TStrongObjectPtr<UGameServiceConfig> GConfigInstanceForNextWorld = nullptr;
	static TOptional<bool> GAutoStartServicesForNextWorld = {}; 
}

bool UWorldGameServiceRunner::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	switch (WorldType)
	{
	// Only create in game worlds:
	case EWorldType::Game:
	case EWorldType::PIE:
		return true;

	case EWorldType::None:
	case EWorldType::Editor:
	case EWorldType::EditorPreview:
	case EWorldType::GamePreview:
	case EWorldType::GameRPC:
	case EWorldType::Inactive:
	default:
		return false;
	}
}

bool UWorldGameServiceRunner::ShouldCreateSubsystem(UObject* Outer) const
{
	// Do not start services for the dummy world that gets initialized on engine startup, see UWorld::CreateWorld()
	return Super::ShouldCreateSubsystem(Outer) &&
		(GIsAutomationTesting || Outer->GetName() != "Untitled");
}

void UWorldGameServiceRunner::Initialize(FSubsystemCollectionBase& Collection)
{
	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("UWorldGameServiceRunner.Initialize"), STAT_WorldGameServiceRunner_Initialize, STATGROUP_GameService);

	Super::Initialize(Collection);

	RegisterAutoServiceConfigs();

	// Initialize all other WorldSubsystems that any configured service depends on first:
	for (const TSubclassOf<UWorldSubsystem>& WorldSubsystemDependency : GatherWorldSubsystemDependencies())
	{
		Collection.InitializeDependency(WorldSubsystemDependency);
	}

	if (bAutoStartServicesForNextWorld)
	{
		RunWorldBootstrappers();
		StartRegisteredServices();
	}
}

void UWorldGameServiceRunner::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	TickRunningServices(DeltaTime);
}

void UWorldGameServiceRunner::TickRunningServices(float DeltaTime)
{
	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("UWorldGameServiceRunner.TickRunningServices"), STAT_WorldGameServiceRunner_TickRunningServices, STATGROUP_GameService);

	for (UGameServiceBase* RunningService : UGameServiceManager::SummonInstance(this).GetAllStartedServiceInstances())
	{
		if (!IsValid(RunningService) || !RunningService->IsTickable())
			continue;

		RunningService->TickService(DeltaTime);
	}
}

TStatId UWorldGameServiceRunner::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(STAT_WorldGameServiceStarter_Tick, STATGROUP_Tickables);
}

void UWorldGameServiceRunner::Deinitialize()
{
	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("UWorldGameServiceRunner.Deinitialize"), STAT_WorldGameServiceRunner_Deinitialize, STATGROUP_GameService);

	// Clean up the ServiceManager, so the service runner in the next world can start anew:
	if (UGameServiceManager* ServiceManager = UGameServiceManager::FindInstance(this); IsValid(ServiceManager))
	{
		ServiceManager->ShutdownAllServicesWithLifetime(EGameServiceLifetime::ShutdownWithWorld);
		ServiceManager->ClearServiceRegister(EGameServiceLifetime::ShutdownWithWorld);
	}

	Super::Deinitialize();
}

void UWorldGameServiceRunner::SetServiceConfigForNextWorld(UGameServiceConfig& ServiceConfig)
{
	Internal::GConfigInstanceForNextWorld = TStrongObjectPtr(&ServiceConfig);
	Internal::GAutoStartServicesForNextWorld.Reset();
	UE_LOG(LogGameService, Log, TEXT("Setting %s as GameServiceConfig for the next world to start."), *ServiceConfig.GetName());
}

void UWorldGameServiceRunner::SetServiceConfigForNextWorld(UGameServiceConfig& ServiceConfig, bool bAutoStartServices)
{
	Internal::GConfigInstanceForNextWorld = TStrongObjectPtr(&ServiceConfig);
	Internal::GAutoStartServicesForNextWorld = bAutoStartServices;
	UE_LOG(LogGameService, Log, TEXT("Setting %s as GameServiceConfig for the next world to start."), *ServiceConfig.GetName());
}

void UWorldGameServiceRunner::RegisterAutoServiceConfigs()
{
	UGameServiceManager& GameServiceManager = UGameServiceManager::SummonInstance(this);

	// By default, don't automatically start services in tests as they should be started on-demand:
	bAutoStartServicesForNextWorld = Internal::GAutoStartServicesForNextWorld.Get(!GIsAutomationTesting);
	Internal::GAutoStartServicesForNextWorld.Reset();

	if (Internal::GConfigInstanceForNextWorld.IsValid())
	{
		GameServiceManager.RegisterServices(*Internal::GConfigInstanceForNextWorld);
		Internal::GConfigInstanceForNextWorld.Reset();
	}

	// Configs by GameMode:
	if (const UGameModeServiceConfigBase* AutoConfig = UGameModeServiceConfigBase::FindConfigForWorld(*GetWorld()))
	{
		UE_LOG(LogGameService, Log, TEXT("Matching GameModeServiceConfig (%s) found for current world (%s)."),
			*GetNameSafe(AutoConfig->GetClass()), *GetNameSafe(GetWorld()));

		GameServiceManager.RegisterServices(*AutoConfig);
	}

	// Configs by World name:
	if (const UWorldServiceConfigBase* AutoConfig = UWorldServiceConfigBase::FindConfigForWorld(*GetWorld()))
	{
		UE_LOG(LogGameService, Log, TEXT("Matching WorldServiceConfig (%s) found for current world (%s)."),
			*GetNameSafe(AutoConfig->GetClass()), *GetNameSafe(GetWorld()));

		GameServiceManager.RegisterServices(*AutoConfig);
	}
}

void UWorldGameServiceRunner::RunWorldBootstrappers()
{
	// Configs by GameMode:
	if (const UGameModeServiceConfigBase* AutoConfig = UGameModeServiceConfigBase::FindConfigForWorld(*GetWorld()))
	{
		UE_LOG(LogGameService, Log, TEXT("Matching GameModeServiceConfig (%s) found for current world (%s)."),
			*GetNameSafe(AutoConfig->GetClass()), *GetNameSafe(GetWorld()));

		for (const TSubclassOf<UGameServiceWorldBootstrapper>& BootstrapperClass : AutoConfig->GetWorldBootstrappers())
		{
			UGameServiceWorldBootstrapper* BootstrapperInstance = NewObject<UGameServiceWorldBootstrapper>(GetWorld(), BootstrapperClass);
			BootstrapperInstance->BootstrapOuterWorld(GetWorldRef());
		}
	}

	// Configs by World name:
	if (const UWorldServiceConfigBase* AutoConfig = UWorldServiceConfigBase::FindConfigForWorld(*GetWorld()))
	{
		UE_LOG(LogGameService, Log, TEXT("Matching WorldServiceConfig (%s) found for current world (%s)."),
			*GetNameSafe(AutoConfig->GetClass()), *GetNameSafe(GetWorld()));

		for (const TSubclassOf<UGameServiceWorldBootstrapper>& BootstrapperClass : AutoConfig->GetWorldBootstrappers())
		{
			UGameServiceWorldBootstrapper* BootstrapperInstance = NewObject<UGameServiceWorldBootstrapper>(GetWorld(), BootstrapperClass);
			BootstrapperInstance->BootstrapOuterWorld(GetWorldRef());
		}
	}
}

void UWorldGameServiceRunner::StartRegisteredServices()
{
	UE_LOG(LogGameService, Verbose, TEXT("WorldGameServiceRunner will now start registered game services."));
	UGameServiceManager::SummonInstance(this).StartRegisteredServices(*GetWorld());
}

TArray<TSubclassOf<UWorldSubsystem>> UWorldGameServiceRunner::GatherWorldSubsystemDependencies() const
{
	TArray<TSubclassOf<UWorldSubsystem>> Result;

	TArray<FGameServiceInstanceClass> ServiceInstanceClasses = UGameServiceManager::SummonInstance(this).GetAllRegisteredServiceInstanceClasses();
	for (const FGameServiceInstanceClass& ServiceClass : ServiceInstanceClasses)
	{
		const auto GameServiceCDO = ServiceClass->GetDefaultObject<UGameServiceBase>();
		TArray<TSubclassOf<USubsystem>> AllSubsystemDependencies = GameServiceCDO->GetSubsystemClassDependencies();
		AllSubsystemDependencies.Append(GameServiceCDO->GetOptionalSubsystemClassDependencies());
		for (const TSubclassOf<USubsystem>& SubsystemDependency : AllSubsystemDependencies)
		{
			if (!SubsystemDependency->IsChildOf<UWorldSubsystem>())
				continue;

			Result.AddUnique(*SubsystemDependency);
		}
	}

	return Result;
}
