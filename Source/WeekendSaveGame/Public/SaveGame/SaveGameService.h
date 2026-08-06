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
#include "CurrentSaveGame.h"
#include "GameFramework/SaveGame.h"
#include "GameService/GameServiceBase.h"

#include "SaveGameService.generated.h"

class USaveGame;
class USaveGameDataProcessor;
class USaveGameSerializer;
class USaveGameServiceSettings;
class USaveLoadBehavior;

using FAsyncSaveGameHandle = FGuid;
using FAsyncLoadGameHandle = FGuid;
using FSaveLoadLockHandle = FGuid;

WEEKENDSAVEGAME_API DECLARE_LOG_CATEGORY_EXTERN(LogSaveGameService, Verbose, All); // Default = Verbose

/**
 * Central API for saving and loading SaveGames.
 * Once configured, the service starts with the first applicable world, but remains alive
 * even when the level is changed (lifetime is bound to the @UGameInstance).
 *
 * This service splits off parts of its implementation as polymorphic subobjects that can
 * be customized by each project (see @USaveLoadBehavior and @USaveGameSerializer).
 *
 * Other game service classes that need to save and restore data from the SaveGame should
 * consider deriving from @URestorableGameService.
 */
UCLASS()
class WEEKENDSAVEGAME_API USaveGameService : public UGameServiceBase
{
	GENERATED_BODY()

public:
	using FDebugContext = FString;
	using FSlotName = FString;
	enum class EStatus : uint8
	{
		Uninitialized,
		Saving,
		Loading,
		Idle
	};

	DECLARE_DELEGATE_TwoParams(FOnSaveLoadCompleted, USaveGame*, bool /*bSuccess*/)
	DECLARE_DELEGATE_TwoParams(FOnPreloadCompleted, TArray<USaveGame*>, TArray<FSlotName>)

	USaveGameService()
	{
		// (i) Service will outlive the world it was created in to ensure SaveGame persistence.
		Lifetime = EGameServiceLifetime::ShutdownWithGameInstance;
	}

	///////////////////////////////////////////////////////////////////////////////////////
	/// MULTICAST EVENTS

	/** Event fired after the (async) status of the service has changed. */
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnStatusChanged, EStatus)
	FOnStatusChanged OnStatusChanged;

	/** Event fired after the list of SaveGames available for saving/loading has changed. */
	DECLARE_MULTICAST_DELEGATE(FOnAvailableSaveGamesChanged)
	FOnAvailableSaveGamesChanged OnAvailableSaveGamesChanged;

	/** Event fired right before the current save game has changed. */
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnBeforeCurrentSaveGameChanged, FCurrentSaveGame /* SaveGameBeforeChange */)
	FOnBeforeCurrentSaveGameChanged OnBeforeCurrentSaveGameChanged;

	/** Event fired right after the current save game has changed. */
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnAfterCurrentSaveGameChanged, FCurrentSaveGame /* SaveGameAfterChange */)
	FOnAfterCurrentSaveGameChanged OnAfterCurrentSaveGameChanged;

	/** Event fired right before the current SaveGame is saved. Last chance to push data into the SaveGame. */
	FCurrentSaveGame::FOnBeforeSaved OnBeforeSaved;

	/** Event fired right after the current SaveGame was saved. */
	FCurrentSaveGame::FOnAfterSaved OnAfterSaved;

	/** Event fired right after the current SaveGame was restored. */
	FCurrentSaveGame::FOnAfterRestored OnAfterRestored;

	///////////////////////////////////////////////////////////////////////////////////////
	/// REQUESTS

	/** Asynchronously save the current state of the game into the Autosave file slot. */
	FAsyncSaveGameHandle RequestAutosave(const FDebugContext& Context);
	FAsyncSaveGameHandle RequestAutosave(const FDebugContext& Context, const FOnSaveLoadCompleted& Callback);

	/** Asynchronously save the current state of the game into the given SaveGame file slot. */
	FAsyncSaveGameHandle RequestSaveCurrentSaveGameToSlot(const FDebugContext& Context, const FSlotName& SlotName);
	FAsyncSaveGameHandle RequestSaveCurrentSaveGameToSlot(const FDebugContext& Context, const FSlotName& SlotName, const FOnSaveLoadCompleted& Callback);

	/** Asynchronously load and restore a SaveGame file as current SaveGame from given slot. */
	FAsyncLoadGameHandle RequestLoadCurrentSaveGameFromSlot(const FDebugContext& Context, const FSlotName& SlotName);
	FAsyncLoadGameHandle RequestLoadCurrentSaveGameFromSlot(const FDebugContext& Context, const FSlotName& SlotName, const FOnSaveLoadCompleted& Callback);

	/**
	 * Asynchronously load and restore a SaveGame file as current SaveGame from given slot.
	 * Afterwards, travel into the level stored in the SaveGame. The CurrentSaveGame will persist even after travelling.
	 */
	FAsyncLoadGameHandle RequestLoadAndTravelIntoCurrentSaveGameFromSlot(const FDebugContext& Context, const FSlotName& SlotName);
	FAsyncLoadGameHandle RequestLoadAndTravelIntoCurrentSaveGameFromSlot(const FDebugContext& Context, const FSlotName& SlotName, const FOnSaveLoadCompleted& Callback);

	/** @returns whether a given save request handle is still active (pending or being processed). */
	bool IsSaveRequestAlive(const FAsyncSaveGameHandle& Handle) const;
	/** @returns whether a given load request handle is still active (pending or being processed). */
	bool IsLoadRequestAlive(const FAsyncLoadGameHandle& Handle) const;

	/** Cancel an active save request by handle. Nothing happens if the request does not exist (anymore). */
	void CancelSaveRequest(const FAsyncSaveGameHandle& Handle);
	/** Cancel an active load request by handle. Nothing happens if the request does not exist (anymore). */
	void CancelLoadRequest(const FAsyncLoadGameHandle& Handle);

	/**
	 * Attempt to process any pending async requests, if saving/loading is currently allowed.
	 * This doesn't need to be called from the outside as it is automatically invoked internally.
	 */
	virtual void ProcessPendingRequests();

	/**
	 * Attempts to synchronously load and restore a SaveGame file as current SaveGame from given slot.
	 * @returns whether this operation was allowed and successful.
	 */
	virtual bool TryLoadCurrentSaveGameFromSlotSynchronous(const FSlotName& SlotName);

	/** Synchronously loads (but not restores) SaveGame files into a persistent cache. Preloading is useful for displaying available SaveGames. */
	virtual TSet<USaveGame*> PreloadSaveGamesSynchronous(const TSet<FSlotName>& SlotNames);
	/** Asynchronously loads (but not restores) SaveGame files into a persistent cache. Preloading is useful for displaying available SaveGames. */
	virtual void PreloadSaveGamesAsync(const TSet<FSlotName>& SlotNames, const FOnPreloadCompleted& Callback);

	/** Sets and restores an already loaded SaveGame as current SaveGame. */
	virtual void RestoreAsCurrentSaveGame(USaveGame& SaveGame, TOptional<FSlotName> LoadedFromSlotName = {});
	/** Sets and restores an already loaded SaveGame as current SaveGame. Afterwards, travel into the level stored in the SaveGame. */
	virtual void RestoreAsAndTravelIntoCurrentSaveGame(USaveGame& SaveGame, TOptional<FSlotName> LoadedFromSlotName = {});
	/** Travel into the level that is stored in the current SaveGame. @returns whether this was successful. */
	virtual bool TryTravelIntoCurrentSaveGame();

	/** Resets the current SaveGame to a new one. */
	virtual void CreateNewSaveGameAsCurrent();

	/** Resets the current SaveGame to a new one and tells all restoring listeners. */
	virtual void CreateAndRestoreNewSaveGameAsCurrent(); 

	/** Removes a cached SaveGame, if it exists and deletes the respective save file. */
	virtual void DeleteSaveGameAtSlot(const FSlotName& SlotName, bool bMoveToBackupFolder = true);

	///////////////////////////////////////////////////////////////////////////////////////
	/// LOCKS

	/** Scoped lock for saving or loading that unlocks itself on destruction. Always use static factory methods for creation. */
	struct FScopedSaveLoadLock
	{
		FScopedSaveLoadLock() = default;
		~FScopedSaveLoadLock();

		static TSharedRef<FScopedSaveLoadLock> LockSaving(USaveGameService& Service, const UObject& KeyHolder, const FDebugContext& Context);
		static TSharedRef<FScopedSaveLoadLock> LockLoading(USaveGameService& Service, const UObject& KeyHolder, const FDebugContext& Context);

	private:
		TFunction<void()> UnlockFunc = nullptr;
	};

	virtual FSaveLoadLockHandle LockAutosaving(const UObject& KeyHolder, const FDebugContext& Context = "");
	virtual void UnlockAutosaving(const FSaveLoadLockHandle& Key, const FDebugContext& Context = "");

	virtual FSaveLoadLockHandle LockSaving(const UObject& KeyHolder, const FDebugContext& Context = "");
	virtual void UnlockSaving(const FSaveLoadLockHandle& Key, const FDebugContext& Context = "");

	virtual FSaveLoadLockHandle LockLoading(const UObject& KeyHolder, const FDebugContext& Context = "");
	virtual void UnlockLoading(const FSaveLoadLockHandle& Key, const FDebugContext& Context = "");

	///////////////////////////////////////////////////////////////////////////////////////
	/// COMPOSITE SUBOBJECTS

	template <typename T = USaveLoadBehavior>
	T* GetSaveLoadBehavior() const;

	template <typename T = USaveGameSerializer>
	T* GetSaveGameSerializer() const;

	///////////////////////////////////////////////////////////////////////////////////////
	/// INFORMATION

	friend WEEKENDSAVEGAME_API FString LexToString(const EStatus& Status);
	FORCEINLINE const FCurrentSaveGame& GetCurrentSaveGame() const { return CurrentSaveGame; }
	FORCEINLINE virtual uint32 GetCurrentUserIndex() const { return 0; }
	FORCEINLINE EStatus GetCurrentStatus() const { return CurrentStatus; }
	FORCEINLINE TArray<FString> GetDebugHistory() const { return DebugHistory; }

	virtual bool IsAutosavingAllowed() const;
	virtual bool IsSavingAllowed() const;
	virtual bool IsLoadingAllowed() const;

	virtual bool IsBusyLoading() const;
	virtual bool IsBusySaving() const;
	virtual bool IsBusySavingOrLoading() const;

	virtual bool CanContinueCurrentSaveGame() const;

	virtual FSlotName GetAutosaveSlotName() const;
	virtual TOptional<FSlotName> GetMostRecentlySavedSlotName() const;
	virtual bool DoesSaveFileExist(const FSlotName& SlotName) const;
	virtual TSet<FSlotName> GetSlotNamesAllowedForSaving() const;
	virtual TSet<FSlotName> GetSlotNamesAllowedForLoading() const;

	/** Logs a debug string for given owner object to the debug history (saved to ModularSaveGame). */
	void AddDebugEntry(const UObject& Owner, const FString& Entry);

	///////////////////////////////////////////////////////////////////////////////////////
	/// CACHE

	const USaveGame* GetCachedSaveGameSnapshotAtSlot(const FSlotName& SlotName) const;
	TMap<FSlotName, const USaveGame*> GetAllCachedSaveGameSnapshots() const;
	bool IsCachedSaveGameSnapshot(const USaveGame& SaveGameObject) const;
	bool HasAnyCachedSaveGameSnapshot() const;

protected:
	///////////////////////////////////////////////////////////////////////////////////////
	/// STATE

	UPROPERTY()
	TObjectPtr<USaveLoadBehavior> SaveLoadBehavior = nullptr;

	UPROPERTY()
	TObjectPtr<USaveGameSerializer> SaveGameSerializer = nullptr;

	UPROPERTY()
	FCurrentSaveGame CurrentSaveGame;

	EStatus CurrentStatus = EStatus::Uninitialized;

	///////////////////////////////////////////////////////////////////////////////////////
	/// CACHE

	/** Cached snapshots may never be modified, which is why this container only provides CopyTo/CopyFrom accessors. */
	struct FSaveGamesCache
	{
	public:
		bool Contains(const FSlotName& SlotName) const;
		bool Contains(const USaveGame& SaveGameObject) const;
		bool IsEmpty() const;
		void Remove(const FSlotName& SlotName);
		void Clear();
		void CopyToCache(USaveGameService& InService, const FSlotName& SlotName, const USaveGame& SaveGame);
		USaveGame* CopyFromCache(USaveGameService& InService, const FSlotName& SlotName) const;
		const USaveGame* Find(const FSlotName& SlotName) const;
		TMap<FSlotName, const USaveGame*> GetAllObjectsBySlot() const;

	private:
		TMap<FSlotName, TStrongObjectPtr<const USaveGame>> SnapshotsBySlot = {};
	} CachedSaveGames;

	///////////////////////////////////////////////////////////////////////////////////////
	/// HISTORY

	uint8 DebugHistoryEntriesToKeep = 0;
	TArray<FString> DebugHistory = {};

	///////////////////////////////////////////////////////////////////////////////////////
	/// REQUESTS

	class ISaveLoadRequest : public TSharedFromThis<ISaveLoadRequest>
	{
	public:
		virtual void Process();
		virtual void Finish(USaveGame* RequestedSaveGame, bool bSuccess);
		virtual void Cancel() {}

		USaveGameService& Service;
		FGuid Handle = FGuid::NewGuid();
		TOptional<FOnSaveLoadCompleted> RequestCallback = {};
		TOptional<FSlotName> SlotName = {};
		FDebugContext Context = FDebugContext();
		double StartTime = 0.0;
		double Runtime = 0.0;

	protected:
		virtual ~ISaveLoadRequest() = default;
		explicit ISaveLoadRequest(USaveGameService& InService, const FDebugContext& InContext, TOptional<FSlotName> InSlotName = {}) :
			Service(InService), SlotName(InSlotName), Context(InContext) { }
		explicit ISaveLoadRequest(USaveGameService& InService, const FDebugContext& InContext, const FOnSaveLoadCompleted& Callback, TOptional<FSlotName> InSlotName = {}) :
			Service(InService), RequestCallback(Callback), SlotName(InSlotName), Context(InContext) { }
	};

	class FLoadToCurrentSaveGameRequest : public ISaveLoadRequest
	{
	public:
		explicit FLoadToCurrentSaveGameRequest(USaveGameService& InService, const FDebugContext& InContext, TOptional<FSlotName> InSlotName = {}) :
			ISaveLoadRequest(InService, InContext, InSlotName) {}
		explicit FLoadToCurrentSaveGameRequest(USaveGameService& InService, const FDebugContext& InContext, const FOnSaveLoadCompleted& Callback, TOptional<FSlotName> InSlotName = {}) :
			ISaveLoadRequest(InService, InContext, Callback, InSlotName) {}
		virtual void Finish(USaveGame* RequestedSaveGame, bool bSuccess) override;
	};

	class FLoadAndTravelIntoToCurrentSaveGameRequest : public ISaveLoadRequest
	{
	public:
		explicit FLoadAndTravelIntoToCurrentSaveGameRequest(USaveGameService& InService, const FDebugContext& InContext, TOptional<FSlotName> InSlotName = {}) :
			ISaveLoadRequest(InService, InContext, InSlotName) {}
		explicit FLoadAndTravelIntoToCurrentSaveGameRequest(USaveGameService& InService, const FDebugContext& InContext, const FOnSaveLoadCompleted& Callback, TOptional<FSlotName> InSlotName = {}) :
			ISaveLoadRequest(InService, InContext, Callback, InSlotName) {}
		virtual void Finish(USaveGame* RequestedSaveGame, bool bSuccess) override;
	};

	class FSaveCurrentSaveGameRequest : public ISaveLoadRequest
	{
	public:
		explicit FSaveCurrentSaveGameRequest(USaveGameService& InService, const FDebugContext& InContext) : ISaveLoadRequest(InService, InContext) {}
		explicit FSaveCurrentSaveGameRequest(USaveGameService& InService, const FDebugContext& InContext, const FOnSaveLoadCompleted& Callback) :
			ISaveLoadRequest(InService, InContext, Callback) {}
		virtual void Finish(USaveGame* RequestedSaveGame, bool bSuccess) override;
	};

	TMap<FSlotName, TArray<TSharedRef<ISaveLoadRequest>>> PendingSaveRequestsBySlot = {};
	TArray<TSharedRef<ISaveLoadRequest>> SaveRequestsInProgress = {};

	TMap<FSlotName, TArray<TSharedRef<ISaveLoadRequest>>> PendingLoadRequestsBySlot = {};
	TArray<TSharedRef<ISaveLoadRequest>> LoadRequestsInProgress = {};

	///////////////////////////////////////////////////////////////////////////////////////
	/// LOCKS

	struct FSaveLoadLock
	{
		TWeakObjectPtr<const UObject> KeyHolder = nullptr;
		FDebugContext ContextString = FString();
	};

	TMap<FSaveLoadLockHandle, FSaveLoadLock> ActiveAutosaveLocks = {};
	TMap<FSaveLoadLockHandle, FSaveLoadLock> ActiveSaveLocks = {};
	TMap<FSaveLoadLockHandle, FSaveLoadLock> ActiveLoadLocks = {};
	TOptional<FSaveLoadLockHandle> CurrentLevelSaveLock = {};
	TSharedPtr<FScopedSaveLoadLock> WorldTransitionSaveLock = nullptr;
	TSharedPtr<FScopedSaveLoadLock> WorldTransitionLoadLock = nullptr;

	///////////////////////////////////////////////////////////////////////////////////////
	/// OVERRIDES

	// - UGameServiceBase
	virtual void StartService() override;
	virtual void ShutdownService() override;
	// --

	///////////////////////////////////////////////////////////////////////////////////////
	/// REQUESTS

	FAsyncSaveGameHandle EnqueueSaveRequest(const FSlotName& SlotName, const TSharedRef<ISaveLoadRequest>& Request, bool bCancelIfSavingIsNotAllowed = true);
	void ConsumeSaveRequestsInProgress(USaveGame* SavedSaveGame, bool bSuccess);

	FAsyncLoadGameHandle EnqueueLoadRequest(const FSlotName& SlotName, const TSharedRef<ISaveLoadRequest>& Request, bool bCancelIfLoadingIsNotAllowed = true);
	void ConsumeLoadRequestsInProgress(USaveGame* LoadedSaveGame, bool bSuccess);

	///////////////////////////////////////////////////////////////////////////////////////
	/// SAVE & LOAD

	/** Sets current save game and fires pre and post events. */
	void SetCurrentSaveGame(const FCurrentSaveGame& NewCurrentSaveGame);

	virtual USaveLoadBehavior& CreateSaveLoadBehavior(const USaveGameServiceSettings& Settings);
	virtual USaveGameSerializer& CreateSaveGameSerializer();

	virtual void PerformAsyncSave(const FSlotName& SlotName);
	virtual void PerformAsyncLoad(const FSlotName& SlotName);
	virtual USaveGame* PerformSyncLoad(const FSlotName& SlotName);

	virtual void HandleAsyncSaveCompleted(const FString& SlotName, const int32 UserIndex, bool bSuccess);
	virtual void HandleAsyncLoadCompleted(const FString& SlotName, const int32 UserIndex, USaveGame* LoadedSaveGame);

	///////////////////////////////////////////////////////////////////////////////////////
	/// MISC

	virtual void HandleLevelChanged(UWorld* NewWorld);
	virtual void UpdateSaveLockForLevel(UWorld* NewWorld);
	virtual void CreateWorldTransitionSaveLoadLocks();

	virtual void SetStatus(const EStatus& NewStatus);
	virtual void AddDebugEntry(const FString& Entry);
	virtual void AddDebugEntry(const FString& Operation, const FDebugContext& Context);
	virtual void AddDebugEntry(const FString& Operation, const FDebugContext& Context, bool bSuccess, double ExecTime);
};

///////////////////////////////////////////////////////////////////////////////////////
/// TEMPLATES @USaveGameService

template <typename T>
T* USaveGameService::GetSaveLoadBehavior() const
{
	return Cast<T>(SaveLoadBehavior);
}

template <typename T>
T* USaveGameService::GetSaveGameSerializer() const
{
	return Cast<T>(SaveGameSerializer);
}
