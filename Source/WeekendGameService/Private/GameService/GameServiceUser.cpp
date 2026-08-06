///////////////////////////////////////////////////////////////////////////////////////
/// Copyright (C) by Benjamin Barz and contributors. See file: CREDITS.md
///
/// This file is part of the WeekendUtils UE5 Plugin.
///
/// Distributed under the MIT License. See file: LICENSE.md
///
///////////////////////////////////////////////////////////////////////////////////////

#include "GameService/GameServiceUser.h"

#include "WeekendGameService.h"
#include "GameService/GameServiceBase.h"
#include "GameService/GameServiceManager.h"
#include "GameService/WorldGameServiceRunner.h"

#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "Subsystems/EngineSubsystem.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "Subsystems/WorldSubsystem.h"

FGameServiceUserConfig::FGameServiceUserConfig(const UObject& GameServiceUserObject): UserObject(MakeWeakObjectPtr(&GameServiceUserObject))
{
	check(IsValid(&GameServiceUserObject));
}

FGameServiceUserConfig::FGameServiceUserConfig(const UObject* GameServiceUserObject): UserObject(MakeWeakObjectPtr(GameServiceUserObject))
{
	check(IsValid(GameServiceUserObject));
}

const UObject* FGameServiceUserConfig::GetWorldContext(const UObject* OverrideWorldContext) const
{
	return IsValid(OverrideWorldContext) ? OverrideWorldContext : UserObject.Get();
}

UWorld* FGameServiceUserConfig::GetWorld(const UObject* OverrideWorldContext) const
{
	const UObject* WorldContext = GetWorldContext(OverrideWorldContext);
	return GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull);
}

TArray<FGameServiceUser::FGameServiceClass> FGameServiceUser::GetServiceClassDependencies() const
{
	const FGameServiceUserConfig Config = ConfigureGameServiceUser();
	return Config.ServiceDependencies;
}

TArray<TSubclassOf<USubsystem>> FGameServiceUser::GetSubsystemClassDependencies() const
{
	const FGameServiceUserConfig Config = ConfigureGameServiceUser();
	return Config.SubsystemDependencies;
}

TArray<TSubclassOf<USubsystem>> FGameServiceUser::GetOptionalSubsystemClassDependencies() const
{
	const FGameServiceUserConfig Config = ConfigureGameServiceUser();
	return Config.OptionalSubsystemDependencies;
}

bool FGameServiceUser::AreAllDependenciesReady(const UObject* OptionalWorldContext) const
{
	return (AreServiceDependenciesReady(OptionalWorldContext) && AreSubsystemDependenciesReady(OptionalWorldContext));
}

bool FGameServiceUser::AreServiceDependenciesReady(const UObject* OptionalWorldContext) const
{
	const FGameServiceUserConfig Config = ConfigureGameServiceUser();
	const UGameServiceManager* ServiceManager = UGameServiceManager::FindInstance(Config.GetWorldContext(OptionalWorldContext));
	if (!IsValid(ServiceManager))
		return false;

	for (const FGameServiceClass& ServiceClass : Config.ServiceDependencies)
	{
		if (!ServiceManager->IsServiceRunning(ServiceClass))
			return false;
	}
	return true;
}

bool FGameServiceUser::AreSubsystemDependenciesReady(const UObject* OptionalWorldContext) const
{
	const FGameServiceUserConfig Config = ConfigureGameServiceUser();
	for (const TSubclassOf<USubsystem>& SubsystemClass : Config.SubsystemDependencies)
	{
		TWeakObjectPtr<const USubsystem> SubsystemInstance = FindSubsystemDependency(*SubsystemClass, OptionalWorldContext);
		if (!SubsystemInstance.IsValid())
			return false;
	}
	for (const TSubclassOf<USubsystem>& SubsystemClass : Config.OptionalSubsystemDependencies)
	{
		TWeakObjectPtr<const USubsystem> SubsystemInstance = FindSubsystemDependency(*SubsystemClass, OptionalWorldContext);
		if (!SubsystemInstance.IsValid())
		{
			// Optional subsystems might not be available in the current environment, skip those,
			// because we consider them "ready":
			if (SubsystemClass->GetDefaultObject<USubsystem>()->ShouldCreateSubsystem(nullptr))
				return false;
		}
	}
	return true;
}

void FGameServiceUser::WaitForDependencies(FOnWaitingFinished Callback, const UObject* WorldContext)
{
	const FGameServiceUserConfig Config = ConfigureGameServiceUser();
	WorldContext = Config.GetWorldContext(WorldContext);
	checkf(!WorldContext->HasAnyFlags(RF_ClassDefaultObject), TEXT("WaitForDependencies() was used with a CDO object. Please pass a world-bound context object."));

	if (!AreServiceDependenciesReady(WorldContext))
	{
		UWorld* ServiceWorld = Config.GetWorld(WorldContext);
		if (!IsValid(ServiceWorld))
		{
			UE_LOG(LogGameService, Warning, TEXT("%s cannot wait for dependencies: no valid world for the given context."), *GetNameSafe(Config.GetUserObject()));
			return;
		}

		// Start all service dependencies, which just happens:
		UGameServiceManager& ServiceManager = UGameServiceManager::SummonInstance(WorldContext);
		for (const FGameServiceClass& ServiceClass : Config.ServiceDependencies)
		{
			const bool bWasDependencyStarted = ServiceManager.TryStartService(*ServiceWorld, ServiceClass).IsSet();
			ensureMsgf(bWasDependencyStarted, TEXT("%s is waiting for dependency %s, which could not be started. Is %s properly configured?"),
				*GetNameSafe(Config.GetUserObject()), *ServiceClass->GetName(), *ServiceClass->GetName());
		}
	}

	if (AreAllDependenciesReady(WorldContext))
	{
		Callback.ExecuteIfBound();
		return;
	}

	// Wait for subsystem dependencies, which are started whenever:
	PendingDependencyWaitCallbacks.Add(Callback);
	PollPendingDependencyWaitCallbacks(WorldContext);
}

void FGameServiceUser::WaitForDependencies(TFunction<void()> Callback, const UObject* OptionalWorldContext)
{
	const FGameServiceUserConfig Config = ConfigureGameServiceUser();
	WaitForDependencies(FOnWaitingFinished::CreateWeakLambda(Config.GetUserObject(), Callback), OptionalWorldContext);
}

void FGameServiceUser::InitializeWorldSubsystemDependencies_Internal(FSubsystemCollectionBase& SubsystemCollection)
{
	const FGameServiceUserConfig Config = ConfigureGameServiceUser();
	SubsystemCollection.InitializeDependency<UWorldGameServiceRunner>();
	for (const TSubclassOf<USubsystem>& SubsystemDependency : Config.SubsystemDependencies)
	{
		if (SubsystemDependency->IsChildOf<UWorldSubsystem>())
		{
			SubsystemCollection.InitializeDependency(SubsystemDependency);
		}
	}
	for (const TSubclassOf<USubsystem>& SubsystemDependency : Config.OptionalSubsystemDependencies)
	{
		if (SubsystemDependency->IsChildOf<UWorldSubsystem>())
		{
			SubsystemCollection.InitializeDependency(SubsystemDependency);
		}
	}
}

UObject* FGameServiceUser::UseGameService_Internal(const TSubclassOf<UObject>& ServiceClass, const UObject* WorldContext) const
{
	return &UseGameService(ServiceClass, WorldContext);
}

UObject* FGameServiceUser::FindOptionalGameService_Internal(const FGameServiceClass& ServiceClass, const UObject* WorldContext) const
{
	return FindOptionalGameService(ServiceClass, WorldContext).Get();
}

UGameServiceBase& FGameServiceUser::UseGameService(const FGameServiceClass& ServiceClass, const UObject* WorldContext) const
{
	if (UGameServiceBase* CachedService = CachedServiceDependencies.Find<UGameServiceBase>(ServiceClass))
		return *CachedService;

	const FGameServiceUserConfig Config = ConfigureGameServiceUser();
	WorldContext = Config.GetWorldContext(WorldContext);
	checkf(!WorldContext->HasAnyFlags(RF_ClassDefaultObject), TEXT("UseGameService() was used with a CDO object. Please pass a world-bound context object."));

	CheckGameServiceDependencies();

	// (!) ServiceDependencies must be registered for GameServiceUsers.
	ensureMsgf(Config.ServiceDependencies.Contains(ServiceClass),
		TEXT("UseGameService<%s>() was called, but service is not registered as dependency."),
		*ServiceClass->GetName());

	// (!) World subsystem service users should call InitializeWorldSubsystemDependencies() before accessing
	// game services to ensure the used services were configured correctly before.
	if (Config.GetUserObject()->IsA<UWorldSubsystem>())
	{
		const UWorldGameServiceRunner* ServiceRunner = Config.GetWorld(WorldContext)->GetSubsystem<UWorldGameServiceRunner>();
		const bool bServiceRunnerIsInitialized = (IsValid(ServiceRunner) && ServiceRunner->IsInitialized());
		ensureMsgf(bServiceRunnerIsInitialized,
			TEXT("%s is a UWorldSubsystem trying to use game services before the UWorldGameServiceRunner was initialized. ")
			TEXT("Please call InitializeWorldSubsystemDependencies() before using services."),
			*Config.GetUserObject()->GetClass()->GetName());
	}

	UGameServiceManager& ServiceManager = UGameServiceManager::SummonInstance(WorldContext);

	TOptional<FGameServiceInstanceClass> ServiceInstanceClass = ServiceManager.DetermineServiceInstanceClass(ServiceClass);
	checkf(ServiceInstanceClass.IsSet(), TEXT("No appropriate service instance class found for %s"), *GetNameSafe(ServiceClass));
	// If the above check triggers, then our service dependency was probably configured via interface, but nobody told the service manager
	// which UGameService class should be instanced for the interface. Check the UGameServiceConfig for the currently running map.

	UGameServiceBase& StartedService = ServiceManager.StartService(*Config.GetWorld(WorldContext), ServiceClass, *ServiceInstanceClass);
	CachedServiceDependencies.Add(ServiceClass, &StartedService);
	return StartedService;
}

TWeakObjectPtr<UGameServiceBase> FGameServiceUser::FindOptionalGameService(const FGameServiceClass& ServiceClass, const UObject* WorldContext) const
{
	if (UGameServiceBase* CachedService = CachedServiceDependencies.Find<UGameServiceBase>(ServiceClass))
		return MakeWeakObjectPtr(CachedService);

	const FGameServiceUserConfig Config = ConfigureGameServiceUser();
	WorldContext = Config.GetWorldContext(WorldContext);
	checkf(!WorldContext->HasAnyFlags(RF_ClassDefaultObject), TEXT("FindOptionalGameService() was used with a CDO object. Please pass a world-bound context object."));

	const UGameServiceManager* ServiceManager = UGameServiceManager::FindInstance(WorldContext);
	const TWeakObjectPtr<UGameServiceBase> ServiceInstance = (IsValid(ServiceManager) ? MakeWeakObjectPtr(ServiceManager->FindStartedServiceInstance(ServiceClass)) : nullptr);
	return ServiceInstance;
}

TWeakObjectPtr<USubsystem> FGameServiceUser::FindSubsystemDependency(const TSubclassOf<USubsystem>& SubsystemClass, const UObject* WorldContext) const
{
	if (USubsystem* CachedSubsystem = CachedSubsystemDependencies.Find<USubsystem>(SubsystemClass))
		return MakeWeakObjectPtr(CachedSubsystem);

	const FGameServiceUserConfig Config = ConfigureGameServiceUser();
	WorldContext = Config.GetWorldContext(WorldContext);
	checkf(!WorldContext->HasAnyFlags(RF_ClassDefaultObject), TEXT("FindSubsystemDependency() was with for a CDO object. Please pass a world-bound context object."));

	// (!) SubsystemDependencies should be registered for GameServiceUsers, but not as strictly as ServiceDependencies, so only log a warning:
	const bool bWarnAboutMissingConfig = (!Config.SubsystemDependencies.Contains(SubsystemClass) && !Config.OptionalSubsystemDependencies.Contains(SubsystemClass));
	UE_CLOG(bWarnAboutMissingConfig, LogGameService, Warning, TEXT("ServiceUser %s accesses %s, which was never configured as SubsystemDependency"),
		*GetNameSafe(Config.GetUserObject()), *GetNameSafe(SubsystemClass));

	auto Sanitize = [SubsystemClass](USubsystem* Subsystem) -> TWeakObjectPtr<USubsystem>
	{
		// (i) This looks stupid, but when you use GetSubsystemBase() and no instance of the desired subsystem class
		// was ever created (i.e. because of ShouldCreateSubsystem() returns false in some environments), then the API
		// just gives you the first subsystem it can find, regardless of inheritance. WTF.
		return (IsValid(Subsystem) && Subsystem->GetClass() == SubsystemClass) ? MakeWeakObjectPtr(Subsystem) : nullptr;
	};

	auto Cache = [this, SubsystemClass](TWeakObjectPtr<USubsystem> Subsystem) -> TWeakObjectPtr<USubsystem>
	{
		if (Subsystem.IsValid())
		{
			CachedSubsystemDependencies.Add(SubsystemClass, Subsystem.Get());
		}
		return Subsystem;
	};

	// [ENGINE]
	if (SubsystemClass->IsChildOf<UEngineSubsystem>())
	{
		return Cache(Sanitize(GEngine ? GEngine->GetEngineSubsystemBase(*SubsystemClass) : nullptr));
	}

	const UWorld* ServiceWorld = Config.GetWorld(WorldContext);
	if (!IsValid(ServiceWorld))
		return nullptr;

	// [WORLD]
	if (SubsystemClass->IsChildOf<UWorldSubsystem>())
	{
		return Cache(Sanitize(ServiceWorld->GetSubsystemBase(*SubsystemClass)));
	}

	// [GAME INSTANCE]
	if (SubsystemClass->IsChildOf<UGameInstanceSubsystem>())
	{
		const UGameInstance* GameInstance = ServiceWorld->GetGameInstance();
		return Cache(Sanitize(IsValid(GameInstance) ? GameInstance->GetSubsystemBase(*SubsystemClass) : nullptr));
	}

	// [LOCAL PLAYER]
	if (SubsystemClass->IsChildOf<ULocalPlayerSubsystem>())
	{
		const APlayerController* FirstPlayerController = ServiceWorld->GetFirstPlayerController();
		const ULocalPlayer* FirstLocalPlayer = IsValid(FirstPlayerController) ? FirstPlayerController->GetLocalPlayer() : nullptr;
		return Cache(Sanitize(IsValid(FirstLocalPlayer) ? FirstLocalPlayer->GetSubsystemBase(*SubsystemClass) : nullptr));
	}

	// (!) EditorSubsystems are not supported for game services.
	checkf(false, TEXT("GameServiceUser can only find subsystem dependencies of supported classes, not %s"), *GetNameSafe(SubsystemClass));
	return nullptr;
}

bool FGameServiceUser::IsGameServiceRegistered(const FGameServiceClass& ServiceClass, const UObject* WorldContext) const
{
	const FGameServiceUserConfig Config = ConfigureGameServiceUser();
	WorldContext = Config.GetWorldContext(WorldContext);
	checkf(!WorldContext->HasAnyFlags(RF_ClassDefaultObject), TEXT("IsGameServiceRegistered() was used with a CDO object. Please pass a world-bound context object."));

	const UGameServiceManager* ServiceManager = UGameServiceManager::FindInstance(WorldContext);
	return IsValid(ServiceManager) && ServiceManager->IsServiceRegistered(ServiceClass);
}

void FGameServiceUser::PollPendingDependencyWaitCallbacks(const UObject* WorldContext)
{
	const FGameServiceUserConfig Config = ConfigureGameServiceUser();
	if (!IsValid(Config.GetUserObject()) || !IsValid(Config.GetWorld(WorldContext)))
		return; // User died while waiting.

	WorldContext = Config.GetWorldContext(WorldContext);
	checkf(!WorldContext->HasAnyFlags(RF_ClassDefaultObject), TEXT("PollPendingDependencyWaitCallbacks() was used with a CDO object. Please pass a world-bound context object."));

	// Notify waiting objects when dependencies are ready:
	if (AreAllDependenciesReady(WorldContext))
	{
		while (PendingDependencyWaitCallbacks.Num() > 0)
		{
			PendingDependencyWaitCallbacks.Pop().ExecuteIfBound();
		}
		return;
	}

	// Keep polling. The context is captured weakly, because it can die while the service user lives on:
	const TWeakObjectPtr<const UObject> WeakWorldContext = MakeWeakObjectPtr(WorldContext);
	FTimerManager& Timer = Config.GetWorld(WorldContext)->GetTimerManager();
	PendingDependencyWaitTimerHandle = Timer.SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(Config.GetUserObject(), [this, WeakWorldContext]()
	{
		if (!WeakWorldContext.IsValid())
			return;

		PollPendingDependencyWaitCallbacks(WeakWorldContext.Get());
	}));
}

void FGameServiceUser::StopWaitingForDependencies(const UObject* WorldContext)
{
	const FGameServiceUserConfig Config = ConfigureGameServiceUser();
	if (!IsValid(Config.GetUserObject()) || !IsValid(Config.GetWorld(WorldContext)))
		return;

	WorldContext = Config.GetWorldContext(WorldContext);
	checkf(!WorldContext->HasAnyFlags(RF_ClassDefaultObject), TEXT("StopWaitingForDependencies() was used with a CDO object. Please pass a world-bound context object."));

	if (PendingDependencyWaitTimerHandle.IsSet())
	{
		Config.GetWorld(WorldContext)->GetTimerManager().ClearTimer(*PendingDependencyWaitTimerHandle);
		PendingDependencyWaitTimerHandle.Reset();
	}

	PendingDependencyWaitCallbacks.Empty();
}

void FGameServiceUser::InvalidateCachedDependencies() const
{
	CachedServiceDependencies.Empty();
	CachedSubsystemDependencies.Empty();
}

