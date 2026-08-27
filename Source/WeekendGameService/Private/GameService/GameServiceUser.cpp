///////////////////////////////////////////////////////////////////////////////////////
/// Copyright (C) by Benjamin Barz and contributors. See file: CREDITS.md
///
/// This file is part of the WeekendUtils UE5 Plugin.
///
/// Distributed under the MIT License. See file: LICENSE.md
///
///////////////////////////////////////////////////////////////////////////////////////

#include "GameService/GameServiceUser.h"

#include "TimerManager.h"
#include "WeekendGameService.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "GameService/GameServiceBase.h"
#include "GameService/GameServiceManager.h"
#include "GameService/GameServiceUtils.h"
#include "GameService/WorldGameServiceRunner.h"

#include "Subsystems/EngineSubsystem.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "Subsystems/SubsystemCollection.h"
#include "Subsystems/WorldSubsystem.h"

namespace
{
	/**
	 * Reports developers who use the @FGameServiceUser API on an object that can never reach a world - a class default
	 * object, or an archetype like an instanced subobject of a config asset - without passing an OverrideWorldContext.
	 * Such usage can never succeed at runtime, in any build configuration, at any point in time, so it is reported as a
	 * hard programming error rather than degraded gracefully.
	 *
	 * Objects that merely lost their world are deliberately not reported: @UGameInstance::Shutdown() clears the world
	 * context while still alive subsystems keep ticking, and world teardown leaves service users alive for a while. The
	 * optional parts of the API resolve those cases to a graceful "not found" instead - see @UGameServiceManager::FindInstance().
	 */
	void CheckWorldContextIsUsableWithoutOverride(const UObject* WorldContext)
	{
#if DO_CHECK
		if (!IsValid(WorldContext) || !WorldContext->IsTemplate(RF_ClassDefaultObject | RF_ArchetypeObject))
			return;

		// (i) Commandlets - the cook above all - construct and inspect class default objects and archetypes while no
		// game world exists at all, so the usage above cannot be distinguished from normal operation and must never be
		// fatal there. The same applies while the engine is already shutting down.
		if (IsRunningCommandlet() || IsEngineExitRequested())
			return;

		checkf(false, TEXT("GameServiceUser %s is a CDO or archetype and can never access a world, so it cannot resolve game services through its own world context. ")
					  TEXT("Pass a world-bound OverrideWorldContext to the FGameServiceUser API when accessing services from such objects."), *GetPathNameSafe(WorldContext));
#endif
	}
}

// (i) A user object that is already being destroyed is a legal input here: service users are allowed to unbind and
// unregister from services during their own BeginDestroy() - see @FGameServiceUserConfig::GetWorldContext().
// Only a null user object is a programming error.
FGameServiceUserConfig::FGameServiceUserConfig(const UObject& GameServiceUserObject): UserObject(MakeWeakObjectPtr(&GameServiceUserObject))
{
}

FGameServiceUserConfig::FGameServiceUserConfig(const UObject* GameServiceUserObject): UserObject(MakeWeakObjectPtr(GameServiceUserObject))
{
	ensureAlwaysMsgf(GameServiceUserObject != nullptr, TEXT("FGameServiceUserConfig() created with null UserObject!"));
}

const UObject* FGameServiceUserConfig::GetWorldContext(const UObject* OverrideWorldContext) const
{
	if (OverrideWorldContext != nullptr)
	{
		ensureAlwaysMsgf(OverrideWorldContext->GetWorld(), TEXT("OverrideWorldContext cannot access any world - is CDO or transient? (%s)"), *GetPathNameSafe(OverrideWorldContext));
		return OverrideWorldContext;
	}

	// (i) GetEvenIfUnreachable: a user object that unbinds from services during its own destruction must still resolve
	// its world context, so lookups can reach the service manager while the world is alive. Once the world itself is
	// gone, lookups degrade to a graceful "not found" - see @UGameServiceManager::FindInstance().
	const UObject* WorldContext = UserObject.GetEvenIfUnreachable();
	CheckWorldContextIsUsableWithoutOverride(WorldContext);
	return WorldContext;
}

UWorld* FGameServiceUserConfig::GetWorld(const UObject* OverrideWorldContext) const
{
	// (i) A missing world is not fatal here: the optional parts of the API rely on resolving it to a graceful "not found", while the parts that require a world check for one themselves.
	const UObject* WorldContext = GetWorldContext(OverrideWorldContext);
	return GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull);
}

FGameServiceDependencies FGameServiceUser::GetServiceClassDependencies() const
{
	const FGameServiceUserConfig Config = ConfigureGameServiceUser();
	return Config.ServiceDependencies;
}

FSubsystemDependencies FGameServiceUser::GetSubsystemClassDependencies() const
{
	const FGameServiceUserConfig Config = ConfigureGameServiceUser();
	return Config.SubsystemDependencies;
}

FSubsystemDependencies FGameServiceUser::GetOptionalSubsystemClassDependencies() const
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
	const UObject* WorldContext = Config.GetWorldContext(OptionalWorldContext);
	const UGameServiceManager* ServiceManager = UGameServiceManager::FindInstance(WorldContext);
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

void FGameServiceUser::WaitForDependencies(FOnWaitingFinished Callback, const UObject* OptionalWorldContext)
{
	const FGameServiceUserConfig Config = ConfigureGameServiceUser();
	const UObject* WorldContext = Config.GetWorldContext(OptionalWorldContext);
	checkf(!WorldContext->HasAnyFlags(RF_ClassDefaultObject), TEXT("WaitForDependencies() was used with a CDO object. Please pass a world-bound context object."));

	if (!AreServiceDependenciesReady(WorldContext))
	{
		// Start all service dependencies, which just happens:
		UGameServiceManager& ServiceManager = UGameServiceManager::SummonInstance(WorldContext);
		UWorld& ServiceWorld = *Config.GetWorld(WorldContext);
		for (const FGameServiceClass& ServiceClass : Config.ServiceDependencies)
		{
			const bool bWasDependencyStarted = ServiceManager.TryStartService(ServiceWorld, ServiceClass).IsSet();
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

UObject* FGameServiceUser::UseGameService_Internal(const TSubclassOf<UObject>& ServiceClass, const UObject* OptionalWorldContext) const
{
	return &UseGameService(ServiceClass, OptionalWorldContext);
}

UObject* FGameServiceUser::FindOptionalGameService_Internal(const FGameServiceClass& ServiceClass, const UObject* OptionalWorldContext) const
{
	return FindOptionalGameService(ServiceClass, OptionalWorldContext).Get();
}

UGameServiceBase& FGameServiceUser::UseGameService(const FGameServiceClass& ServiceClass, const UObject* OptionalWorldContext) const
{
	if (UGameServiceBase* CachedService = CachedServiceDependencies.Find<UGameServiceBase>(ServiceClass))
		return *CachedService;

	const FGameServiceUserConfig Config = ConfigureGameServiceUser();
	const UObject* WorldContext = Config.GetWorldContext(OptionalWorldContext);
	checkf(!WorldContext->HasAnyFlags(RF_ClassDefaultObject), TEXT("UseGameService() was used with a CDO object. Please pass a world-bound context object."));
	UWorld* World = Config.GetWorld(WorldContext);

	CheckGameServiceDependencies();

	// (!) ServiceDependencies must be registered for GameServiceUsers.
	ensureMsgf(Config.ServiceDependencies.Contains(ServiceClass),
		TEXT("UseGameService<%s>() was called, but service is not registered as dependency."),
		*ServiceClass->GetName());

	// (!) World subsystem service users should call InitializeWorldSubsystemDependencies() before accessing
	// game services to ensure the used services were configured correctly before.
	if (WorldContext->IsA<UWorldSubsystem>())
	{
		const UWorldGameServiceRunner* ServiceRunner = World->GetSubsystem<UWorldGameServiceRunner>();
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

	UGameServiceBase& StartedService = ServiceManager.StartService(*World, ServiceClass, *ServiceInstanceClass);
	CachedServiceDependencies.Add(ServiceClass, &StartedService);
	return StartedService;
}

TWeakObjectPtr<UGameServiceBase> FGameServiceUser::FindOptionalGameService(const FGameServiceClass& ServiceClass, const UObject* OptionalWorldContext) const
{
	if (UGameServiceBase* CachedService = CachedServiceDependencies.Find<UGameServiceBase>(ServiceClass))
		return MakeWeakObjectPtr(CachedService);

	const FGameServiceUserConfig Config = ConfigureGameServiceUser();
	const UObject* WorldContext = Config.GetWorldContext(OptionalWorldContext);
	const UGameServiceManager* ServiceManager = UGameServiceManager::FindInstance(WorldContext);
	const TWeakObjectPtr<UGameServiceBase> ServiceInstance = (IsValid(ServiceManager) ? MakeWeakObjectPtr(ServiceManager->FindStartedServiceInstance(ServiceClass)) : nullptr);
	return ServiceInstance;
}

TWeakObjectPtr<USubsystem> FGameServiceUser::FindSubsystemDependency(const TSubclassOf<USubsystem>& SubsystemClass, const UObject* OptionalWorldContext) const
{
	if (USubsystem* CachedSubsystem = CachedSubsystemDependencies.Find<USubsystem>(SubsystemClass))
		return MakeWeakObjectPtr(CachedSubsystem);

	const FGameServiceUserConfig Config = ConfigureGameServiceUser();
	const UObject* WorldContext = Config.GetWorldContext(OptionalWorldContext);

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

	// (i) Every branch below needs a world, and losing it is a legal "not found" case here - i.e. while the game
	// instance is shutting down but its subsystems are still ticking.
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
		const ULocalPlayer* FirstLocalPlayer = ServiceWorld->GetFirstPlayerController()->GetLocalPlayer();
		return Cache(Sanitize(IsValid(FirstLocalPlayer) ? FirstLocalPlayer->GetSubsystemBase(*SubsystemClass) : nullptr));
	}

	// (!) EditorSubsystems are not supported for game services.
	checkf(false, TEXT("GameServiceUser can only find subsystem dependencies of supported classes, not %s"), *GetNameSafe(SubsystemClass));
	return nullptr;
}

bool FGameServiceUser::IsGameServiceRegistered(const FGameServiceClass& ServiceClass, const UObject* OptionalWorldContext) const
{
	const FGameServiceUserConfig Config = ConfigureGameServiceUser();
	const UObject* WorldContext = Config.GetWorldContext(OptionalWorldContext);
	const UGameServiceManager* ServiceManager = UGameServiceManager::FindInstance(WorldContext);
	return IsValid(ServiceManager) && ServiceManager->IsServiceRegistered(ServiceClass);
}

void FGameServiceUser::PollPendingDependencyWaitCallbacks(const UObject* OptionalWorldContext)
{
	const FGameServiceUserConfig Config = ConfigureGameServiceUser();
	if (!IsValid(Config.GetUserObject()) || !IsValid(Config.GetWorld(OptionalWorldContext)))
		return; // User died while waiting.

	const UObject* WorldContext = Config.GetWorldContext(OptionalWorldContext);

	// Notify waiting objects when dependencies are ready:
	if (AreAllDependenciesReady(WorldContext))
	{
		while (PendingDependencyWaitCallbacks.Num() > 0)
		{
			PendingDependencyWaitCallbacks.Pop().ExecuteIfBound();
		}
		return;
	}

	// Keep polling:
	FTimerManager& Timer = Config.GetWorld(WorldContext)->GetTimerManager();
	PendingDependencyWaitTimerHandle = Timer.SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(Config.GetUserObject(), [this, WorldContext]()
	{
		PollPendingDependencyWaitCallbacks(WorldContext);
	}));
}

void FGameServiceUser::StopWaitingForDependencies(const UObject* OptionalWorldContext)
{
	const FGameServiceUserConfig Config = ConfigureGameServiceUser();
	if (!IsValid(Config.GetUserObject()) || !IsValid(Config.GetWorld(OptionalWorldContext)))
		return;

	const UObject* WorldContext = Config.GetWorldContext(OptionalWorldContext);

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

