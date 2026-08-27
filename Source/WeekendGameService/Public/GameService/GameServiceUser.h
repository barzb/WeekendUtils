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
#include "Engine/TimerHandle.h"
#include "GameService/GameServiceUtils.h"
#include "UObject/WeakInterfacePtr.h"

class FSubsystemCollectionBase;
class UGameServiceBase;
class USubsystem;
class UWorldSubsystem;

/**
 * Configuration struct for a @FGameServiceUser derived classes.
 * Example usage of ConfigureGameServiceUser() implementation:
 *		return FGameServiceUserConfig(this)
 *			.AddServiceDependency<USomeService>()
 *			.AddServiceDependency<USomeOtherService>()
 *			.AddSubsystemDependency<USomeSubsystem>();
 *	Grand-child classes should consider calling Super::ConfigureGameServiceUser() to inherit the dependency configured by their parents.
 *	@note that GameServiceUsers need access to a World via the passed WorldContextObject.
 */
struct WEEKENDGAMESERVICE_API FGameServiceUserConfig
{
	FGameServiceUserConfig(const UObject& GameServiceUserObject);
	FGameServiceUserConfig(const UObject* GameServiceUserObject);

	/** Utility function for chaining multiple ServiceDependencies.Add<T>() calls on the same config object. */
	template <typename T> FGameServiceUserConfig& AddServiceDependency() { ServiceDependencies.Add<T>(); return *this; }
	/** Utility function for chaining multiple SubsystemDependencies.Add<T>() calls on the same config object. */
	template <typename T> FGameServiceUserConfig& AddSubsystemDependencies() { SubsystemDependencies.Add<T>(); return *this; }
	/** Utility function for chaining multiple OptionalSubsystemDependencies.Add<T>() calls on the same config object. */
	template <typename T> FGameServiceUserConfig& AddOptionalSubsystemDependencies() { OptionalSubsystemDependencies.Add<T>(); return *this; }

	FORCEINLINE const UObject* GetUserObject() const { return UserObject.Get(); }
	const UObject* GetWorldContext(const UObject* OverrideWorldContext) const;
	UWorld* GetWorld(const UObject* OverrideWorldContext) const;

	/**
	 * Dependency config container for game services. Call ServiceDependencies.Add<T>() in the constructor of inherited classes.
	 * @remark Supported game service types: @UGameServiceBase, @UInterface
	 */
	FGameServiceDependencies ServiceDependencies;

	/**
	 * Dependency config container for subsystem dependencies. Call SubsystemDependencies.Add<T>() in the constructor of inherited classes.
	 * @remark Supported subsystem types: @UWorldSubsytem, @UEngineSubsytem, @UGameInstanceSubsystem, @LocalPlayerSubsystem
	 */
	FSubsystemDependencies SubsystemDependencies;

	/**
	 * Dependency config container for optional subsystem dependencies. Call OptionalSubsystemDependencies.Add<T>() in the constructor
	 * of inherited classes. When the subsystem class is not available in the current environment due to its @ShouldCreateSubsystem
	 * implementation, it will be skipped when waiting for dependencies.
	 * @remark Supported subsystem types: @UWorldSubsytem, @UEngineSubsytem, @UGameInstanceSubsystem, @LocalPlayerSubsystem
	 */
	FSubsystemDependencies OptionalSubsystemDependencies;

private:
	FGameServiceUserConfig() = delete;
	TWeakObjectPtr<const UObject> UserObject = nullptr;
};

/**
 * Grants classes inheriting from FGameServiceUser utilities to configure GameService and Subsystem dependencies
 * and the possibility to directly access those dependencies in their runtime code.
 *
 * Intended usage:
 * - Inherit your @UClass from @FGameServiceUser
 * - Configure dependencies in your class constructor:
 *     @example: ServiceDependencies.Add<USomeGameService>();
 *     @example: SubsystemDependencies.Add<USomeSubsystem>();
 * - Access services directly in your runtime code (requires a fully created world):
 *     @example: USomeGameService& MyDependency = UseGameService<USomeGameService>();
 *     @example: TObjectPtr<USomeGameService> MyDependency = UseGameServiceAsPtr<USomeGameService>();
 *     @example: TWeakObjectPtr<USomeGameService> MyDependency = UseGameServiceAsWeakPtr<USomeGameService>();
 *     @example: TWeakObjectPtr<USomeOtherService> NiceToHaveService = FindOptionalGameService<USomeOtherService>();
 *     @note: Interfaced services use the interface version of storage pointers instead: @TScriptInterface, @TWeakInterfacePtr
 * - Access subsystem dependencies in your runtime code:
 *     @example: TWeakObjectPtr<USomeSubsystem> PotentiallyUnavailableSubsystem = FindSubsystemDependency<USomeSubsystem>();
 * - When relying on subsystems or async services whose initialization order might overlap with that of your class, use this:
 *     @example: WaitForDependencies(FOnWaitingFinished::CreateUObject(this, &ThisClass::ExecuteCodeRelyingOnAsyncDependencies));
 *
 * @remark Inheriting classes must implement @ConfigureGameServiceUser().
 * @remark The world context configured by @ConfigureGameServiceUser() is used by default. Objects that cannot reach a world
 *         themselves - class default objects, archetypes, instanced subobjects of assets - must pass a world-bound
 *         @OptionalWorldContext instead, otherwise the API reports the misuse.
 * @remark The optional parts of the API remain safe to use while the user object is being destroyed, so service users
 *         can unbind and unregister from services in their own BeginDestroy(). Lookups still resolve while the world
 *         is alive and gracefully return "not found" once it is gone. Do not trigger new service logic from there.
 */
class WEEKENDGAMESERVICE_API FGameServiceUser
{
public:
	using FGameServiceClass = TSubclassOf<UObject>; // => see GameServiceBase.h

	/** Must be overridden by derived class to configure ServiceDependencies and SubsystemDependencies. */
	virtual FGameServiceUserConfig ConfigureGameServiceUser() const = 0;

	/** Can be overridden by a derived class to throw exceptions for invalid dependencies. */
	virtual void CheckGameServiceDependencies() const {}

	/** @returns all game service classes that this service user depends on. */
	FGameServiceDependencies GetServiceClassDependencies() const;

	/** @returns all subsystem classes that this service user depends on. */
	FSubsystemDependencies GetSubsystemClassDependencies() const;

	/** @returns all optional subsystem classes that this service user depends on. */
	FSubsystemDependencies GetOptionalSubsystemClassDependencies() const;

	/** @returns whether all game service dependencies are running and all subsystem dependencies are available. */
	bool AreAllDependenciesReady(const UObject* OptionalWorldContext = nullptr) const;
	bool AreServiceDependenciesReady(const UObject* OptionalWorldContext = nullptr) const;
	bool AreSubsystemDependenciesReady(const UObject* OptionalWorldContext = nullptr) const;

protected:
	FGameServiceUser() = default;
	~FGameServiceUser() = default;

	/**
	 * Defers given callback until all dependencies are ready to be used. Triggers callback immediately if that is already the case.
	 * This is only necessary when depending on subsystems whose birth might be after the birth of the service user.
	 * Dependencies to other game services are always available, because they are created + started on demand when not yet running.
	 * However, async services might take a while until they are considered 'running', even when they are started on demand.
	 */
	DECLARE_DELEGATE(FOnWaitingFinished)
	void WaitForDependencies(FOnWaitingFinished Callback, const UObject* OptionalWorldContext = nullptr);
	void WaitForDependencies(TFunction<void( )> Callback, const UObject* OptionalWorldContext = nullptr);

	/**
	 * Only to be called by @UWorldSubsystem classes inheriting from @FGameServiceUser, in their @Initialize call before waiting for dependencies.
	 * This makes sure the @WorldGameServiceRunner and all configured subsystem dependencies have already been initialized.
	 */
	template<typename ServiceUserClass, typename = typename TEnableIf<TIsDerivedFrom<ServiceUserClass, UWorldSubsystem>::IsDerived>::Type>
	void InitializeWorldSubsystemDependencies(FSubsystemCollectionBase& SubsystemCollection)
	{
		InitializeWorldSubsystemDependencies_Internal(SubsystemCollection);
	}

	/**
	 * @returns another game service that is part of the ServiceDependencies config.
	 * If the desired service is not yet created or running, it will be created and started on demand.
	 * If called on a CDO, please pass a world-bound WorldContext as the optional argument.
	 */
	UGameServiceBase& UseGameService(const FGameServiceClass& ServiceClass, const UObject* OptionalWorldContext = nullptr) const;

	/** 
	 * @returns another game service as weak pointer, even when not configured as dependency, considering it might not be available. 
	 * If called on a CDO, please pass a world-bound WorldContext as the optional argument.
	 */
	TWeakObjectPtr<UGameServiceBase> FindOptionalGameService(const FGameServiceClass& ServiceClass, const UObject* OptionalWorldContext = nullptr) const;

	/** 
	 * @returns a subsystem that was configured as dependency as weak pointer, considering it might not yet be available.
	 * If called on a CDO, please pass a world-bound WorldContext as the optional argument.
	 */
	TWeakObjectPtr<USubsystem> FindSubsystemDependency(const TSubclassOf<USubsystem>& SubsystemClass, const UObject* OptionalWorldContext = nullptr) const;

	/** @returns whether a certain game service class is currently registered. */
	bool IsGameServiceRegistered(const FGameServiceClass& ServiceClass, const UObject* OptionalWorldContext = nullptr) const;

	template<typename T> typename TEnableIf<TIsDerivedFrom<T, UGameServiceBase>::IsDerived, T&>::Type
	/*(T&)*/ UseGameService(const UObject* OptionalWorldContext = nullptr) const
	{
		return *Cast<T>(UseGameService_Internal(T::StaticClass(), OptionalWorldContext));
	}

	template<typename T> typename TEnableIf<TIsDerivedFrom<T, UGameServiceBase>::IsDerived, TObjectPtr<T>>::Type
	/*(TObjectPtr<T>)*/ UseGameServiceAsPtr(const UObject* OptionalWorldContext = nullptr) const
	{
		return TObjectPtr<T>(Cast<T>(UseGameService_Internal(T::StaticClass(), OptionalWorldContext)));
	}

	template<typename T> typename TEnableIf<TIsIInterface<T>::Value, TScriptInterface<T>>::Type
	/*(TScriptInterface<T>)*/ UseGameServiceAsInterface(const UObject* OptionalWorldContext = nullptr) const
	{
		return TScriptInterface<T>(UseGameService_Internal(T::UClassType::StaticClass(), OptionalWorldContext));
	}

	template<typename T> typename TEnableIf<TIsDerivedFrom<T, UGameServiceBase>::IsDerived, TWeakObjectPtr<T>>::Type
	/*(TWeakObjectPtr<T>)*/ UseGameServiceAsWeakPtr(const UObject* OptionalWorldContext = nullptr) const
	{
		return TWeakObjectPtr<T>(Cast<T>(UseGameService_Internal(T::StaticClass(), OptionalWorldContext)));
	}

	template<typename T> typename TEnableIf<TIsIInterface<T>::Value, TWeakInterfacePtr<T>>::Type
	/*(TWeakInterfacePtr<T>)*/ UseGameServiceAsWeakInterface(const UObject* OptionalWorldContext = nullptr) const
	{
		return TWeakInterfacePtr<T>(UseGameService_Internal(T::UClassType::StaticClass(), OptionalWorldContext));
	}

	template<typename T> typename TEnableIf<TIsDerivedFrom<T, UGameServiceBase>::IsDerived, TWeakObjectPtr<T>>::Type
	/*(TWeakObjectPtr<T>)*/ FindOptionalGameService(const UObject* OptionalWorldContext = nullptr) const
	{
		return TWeakObjectPtr<T>(Cast<T>(FindOptionalGameService_Internal(T::StaticClass(), OptionalWorldContext)));
	}

	template<typename T> typename TEnableIf<TIsIInterface<T>::Value, TWeakInterfacePtr<T>>::Type
	/*(TWeakInterfacePtr<T>)*/ FindOptionalGameService(const UObject* OptionalWorldContext = nullptr) const
	{
		return TWeakInterfacePtr<T>(FindOptionalGameService_Internal(T::UClassType::StaticClass(), OptionalWorldContext));
	}

	template<typename T>
	TWeakObjectPtr<T> FindSubsystemDependency() const
	{
		static_assert(TIsDerivedFrom<T, USubsystem>::IsDerived);
		return TWeakObjectPtr<T>(Cast<T>(FindSubsystemDependency(T::StaticClass())));
	}

	template<typename T>
	bool IsGameServiceRegistered() const
	{
		return IsGameServiceRegistered(GameService::GetServiceUClass<T>());
	}

	/** When waiting for dependencies, this is called automatically each tick, but can also be called manually by derived class. */
	void PollPendingDependencyWaitCallbacks(const UObject* OptionalWorldContext = nullptr);

	/** When waiting for dependencies, this can be called when the wait should be canceled, i.e. when the service user is prematurely destroyed. */
	void StopWaitingForDependencies(const UObject* OptionalWorldContext = nullptr);

	/** Clears the cache of dependency instances. Should be called when the service user re-configures dependencies at runtime. */
	void InvalidateCachedDependencies() const;

private:
	TArray<FOnWaitingFinished> PendingDependencyWaitCallbacks = {};
	TOptional<FTimerHandle> PendingDependencyWaitTimerHandle = {};

	struct FCachedDependencies : private TMap<const UClass*, TWeakObjectPtr<UObject>>
	{
		using TMap::Empty;
		void Add(const UClass* Class, UObject* Dependency)
		{
			TMap::Add(Class, MakeWeakObjectPtr(Dependency));
		}
		template <typename T>
		T* Find(const UClass* Class) const
		{
			const auto* FoundDependency = TMap::Find(Class);
			return FoundDependency && FoundDependency->IsValid() ? Cast<T>(*FoundDependency) : nullptr;
		}
	};
	mutable FCachedDependencies CachedServiceDependencies = {};
	mutable FCachedDependencies CachedSubsystemDependencies = {};

	// - Internal calls for templates using only UObject, so the child class does not have to include "GameServiceBase.h" when templates are inlined
	UObject* UseGameService_Internal(const TSubclassOf<UObject>& ServiceClass, const UObject* OptionalWorldContext) const;
	UObject* FindOptionalGameService_Internal(const FGameServiceClass& ServiceClass, const UObject* OptionalWorldContext) const;
	void InitializeWorldSubsystemDependencies_Internal(FSubsystemCollectionBase& SubsystemCollection);
	// --
};
