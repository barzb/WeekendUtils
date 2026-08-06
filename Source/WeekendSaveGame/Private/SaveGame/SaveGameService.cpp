///////////////////////////////////////////////////////////////////////////////////////
/// Copyright (C) by Benjamin Barz and contributors. See file: CREDITS.md
///
/// This file is part of the WeekendUtils UE5 Plugin.
///
/// Distributed under the MIT License. See file: LICENSE.md
///
///////////////////////////////////////////////////////////////////////////////////////

#include "SaveGame/SaveGameService.h"

#include "Engine/World.h"
#include "GameFramework/SaveGame.h"
#include "SaveGame/SaveGameSerializer.h"
#include "SaveGame/SaveGameUtils.h"
#include "SaveGame/SaveLoadBehavior.h"
#include "SaveGame/Settings/SaveGameServiceSettings.h"

DEFINE_LOG_CATEGORY(LogSaveGameService);

FAsyncSaveGameHandle USaveGameService::RequestAutosave(const FDebugContext& Context)
{
	if (!IsAutosavingAllowed())
	{
		UE_LOG(LogSaveGameService, Log, TEXT("Autosave request ignored, because autosaving is not allowed. (Context: %s)"), *Context);
		return FAsyncSaveGameHandle();
	}

	return RequestSaveCurrentSaveGameToSlot(Context, GetAutosaveSlotName());
}

FAsyncSaveGameHandle USaveGameService::RequestAutosave(const FDebugContext& Context, const FOnSaveLoadCompleted& Callback)
{
	if (!IsAutosavingAllowed())
	{
		UE_LOG(LogSaveGameService, Log, TEXT("Autosave request ignored, because autosaving is not allowed. (Context: %s)"), *Context);
		return FAsyncSaveGameHandle();
	}

	return RequestSaveCurrentSaveGameToSlot(Context, GetAutosaveSlotName(), Callback);
}

FAsyncSaveGameHandle USaveGameService::RequestSaveCurrentSaveGameToSlot(const FDebugContext& Context, const FSlotName& SlotName)
{
	AddDebugEntry("RequestSaveCurrentSaveGameToSlot", Context + "(for " + SlotName + ")");
	return EnqueueSaveRequest(SlotName, MakeShared<FSaveCurrentSaveGameRequest>(*this, Context));
}

FAsyncSaveGameHandle USaveGameService::RequestSaveCurrentSaveGameToSlot(const FDebugContext& Context, const FSlotName& SlotName, const FOnSaveLoadCompleted& Callback)
{
	AddDebugEntry("RequestSaveCurrentSaveGameToSlot", Context + "(for " + SlotName + ")");
	return EnqueueSaveRequest(SlotName, MakeShared<FSaveCurrentSaveGameRequest>(*this, Context, Callback));
}

FAsyncLoadGameHandle USaveGameService::RequestLoadCurrentSaveGameFromSlot(const FDebugContext& Context, const FSlotName& SlotName)
{
	AddDebugEntry("RequestLoadCurrentSaveGameFromSlot", Context + "(for " + SlotName + ")");
	return EnqueueLoadRequest(SlotName, MakeShared<FLoadToCurrentSaveGameRequest>(*this, Context, SlotName));
}

FAsyncLoadGameHandle USaveGameService::RequestLoadCurrentSaveGameFromSlot(const FDebugContext& Context, const FSlotName& SlotName, const FOnSaveLoadCompleted& Callback)
{
	AddDebugEntry("RequestLoadCurrentSaveGameFromSlot", Context + "(for " + SlotName + ")");
	return EnqueueLoadRequest(SlotName, MakeShared<FLoadToCurrentSaveGameRequest>(*this, Context, Callback, SlotName));
}

FAsyncLoadGameHandle USaveGameService::RequestLoadAndTravelIntoCurrentSaveGameFromSlot(const FDebugContext& Context, const FSlotName& SlotName)
{
	AddDebugEntry("RequestLoadAndTravelIntoCurrentSaveGameFromSlot", Context + "(for " + SlotName + ")");
	return EnqueueLoadRequest(SlotName, MakeShared<FLoadAndTravelIntoToCurrentSaveGameRequest>(*this, Context, SlotName));
}

FAsyncLoadGameHandle USaveGameService::RequestLoadAndTravelIntoCurrentSaveGameFromSlot(const FDebugContext& Context, const FSlotName& SlotName, const FOnSaveLoadCompleted& Callback)
{
	return EnqueueLoadRequest(SlotName, MakeShared<FLoadAndTravelIntoToCurrentSaveGameRequest>(*this, Context, Callback, SlotName));
}

bool USaveGameService::IsSaveRequestAlive(const FAsyncSaveGameHandle& Handle) const
{
	if (!Handle.IsValid())
		return false;

	for (auto Itr = PendingSaveRequestsBySlot.CreateConstIterator(); Itr; ++Itr)
	{
		for (const TSharedRef<ISaveLoadRequest>& SaveRequest : Itr.Value())
		{
			if (SaveRequest->Handle == Handle)
				return true;
		}
	}

	for (const TSharedRef<ISaveLoadRequest>& SaveRequest : SaveRequestsInProgress)
	{
		if (SaveRequest->Handle == Handle)
			return true;
	}

	return false;
}

bool USaveGameService::IsLoadRequestAlive(const FAsyncLoadGameHandle& Handle) const
{
	if (!Handle.IsValid())
		return false;

	for (auto Itr = PendingLoadRequestsBySlot.CreateConstIterator(); Itr; ++Itr)
	{
		for (const TSharedRef<ISaveLoadRequest>& LoadRequest : Itr.Value())
		{
			if (LoadRequest->Handle == Handle)
				return true;
		}
	}

	for (const TSharedRef<ISaveLoadRequest>& LoadRequest : LoadRequestsInProgress)
	{
		if (LoadRequest->Handle == Handle)
			return true;
	}

	return false;
}

void USaveGameService::CancelSaveRequest(const FAsyncSaveGameHandle& Handle)
{
	for (TTuple<FSlotName, TArray<TSharedRef<ISaveLoadRequest>>>& RequestsBySlotName : PendingSaveRequestsBySlot)
	{
		TSharedPtr<ISaveLoadRequest> FoundRequest = nullptr;
		for (TSharedRef<ISaveLoadRequest> Request : RequestsBySlotName.Value)
		{
			if (Request->Handle == Handle)
			{
				FoundRequest = Request.ToSharedPtr();
			}
		}

		if (!FoundRequest.IsValid())
			continue;

		FoundRequest->Cancel();
		RequestsBySlotName.Value.Remove(FoundRequest.ToSharedRef());
		if (RequestsBySlotName.Value.IsEmpty())
		{
			PendingSaveRequestsBySlot.Remove(RequestsBySlotName.Key);
		}
		return;
	}
}

void USaveGameService::CancelLoadRequest(const FAsyncLoadGameHandle& Handle)
{
	for (TTuple<FSlotName, TArray<TSharedRef<ISaveLoadRequest>>>& RequestsBySlotName : PendingLoadRequestsBySlot)
	{
		TSharedPtr<ISaveLoadRequest> FoundRequest = nullptr;
		for (TSharedRef<ISaveLoadRequest> Request : RequestsBySlotName.Value)
		{
			if (Request->Handle == Handle)
			{
				FoundRequest = Request.ToSharedPtr();
			}
		}

		if (!FoundRequest.IsValid())
			continue;

		FoundRequest->Cancel();
		RequestsBySlotName.Value.Remove(FoundRequest.ToSharedRef());
		if (RequestsBySlotName.Value.IsEmpty())
		{
			PendingLoadRequestsBySlot.Remove(RequestsBySlotName.Key);
		}
		return;
	}
}

bool USaveGameService::TryLoadCurrentSaveGameFromSlotSynchronous(const FSlotName& SlotName)
{
	if (CachedSaveGames.Contains(SlotName))
		return true;

	if (IsBusySavingOrLoading())
		return false;

	if (!DoesSaveFileExist(SlotName))
		return false;

	SetStatus(EStatus::Loading);

	USaveGame* LoadedSaveGame = PerformSyncLoad(SlotName);
	if (!IsValid(LoadedSaveGame))
	{
		// The file exists, but could not be deserialized (corrupt file, unknown SaveGame class, ..):
		SetStatus(EStatus::Idle);
		AddDebugEntry("[TryLoadCurrentSaveGameFromSlotSynchronous] failed to deserialize " + SlotName);
		return false;
	}

	SetCurrentSaveGame(FCurrentSaveGame::CreateFromLoadedGame(*LoadedSaveGame, SlotName));

	SetStatus(EStatus::Idle);
	AddDebugEntry("[TryLoadCurrentSaveGameFromSlotSynchronous] " + SlotName);

	OnAfterRestored.Broadcast(CurrentSaveGame);
	return true;
}

TSet<USaveGame*> USaveGameService::PreloadSaveGamesSynchronous(const TSet<FSlotName>& SlotNames)
{
	TSet<USaveGame*> Result = {};
	if (SlotNames.IsEmpty())
		return Result;

	SetStatus(EStatus::Loading);
	AddDebugEntry("[PreloadSaveGamesSynchronous] " + FString::Join(SlotNames, TEXT(", ")));

	CachedSaveGames.Clear();
	for (const FSlotName& SlotName : SlotNames)
	{
		if (!DoesSaveFileExist(SlotName))
			continue;
		if (USaveGame* LoadedSaveGame = PerformSyncLoad(SlotName))
		{
			CachedSaveGames.CopyToCache(*this, SlotName, *LoadedSaveGame);
			Result.Add(LoadedSaveGame);
		}
	}

	SetStatus(EStatus::Idle);
	OnAvailableSaveGamesChanged.Broadcast();

	return Result;
}

void USaveGameService::PreloadSaveGamesAsync(const TSet<FSlotName>& SlotNames, const FOnPreloadCompleted& Callback)
{
	CachedSaveGames.Clear();
	if (SlotNames.IsEmpty())
	{
		Callback.ExecuteIfBound({}, {});
		return;
	}

	// (i) Ordered pairs, so the two arrays passed to the callback stay index-correspondent:
	using FPreloadResult = TPair<FSlotName, USaveGame*>;

	class FPreloadRequest : public ISaveLoadRequest
	{
	public:
		explicit FPreloadRequest(USaveGameService& InService,
			const FSlotName& SlotName,
			const TSharedRef<int32>& InRemainingSlots,
			const TSharedRef<TArray<FPreloadResult>>& InResults,
			const TSharedRef<TSet<TStrongObjectPtr<UObject>>>& InObjectsToKeepInMemory,
			const FOnPreloadCompleted& InCallback) :
				ISaveLoadRequest(InService, "", SlotName),
				RemainingSlots(InRemainingSlots),
				Results(InResults),
				ObjectsToKeepInMemory{InObjectsToKeepInMemory},
				Callback(InCallback) {}

		virtual void Finish(USaveGame* RequestedSaveGame, bool bSuccess) override
		{
			Runtime = FPlatformTime::Seconds() - StartTime;
			if (bSuccess && IsValid(RequestedSaveGame))
			{
				ObjectsToKeepInMemory->Add(TStrongObjectPtr{RequestedSaveGame});
				Results->Emplace(*SlotName, RequestedSaveGame);
			}
			if (--(*RemainingSlots) <= 0)
			{
				Service.AddDebugEntry("PreloadSaveGamesAsync", "FPreloadRequest::Finish", bSuccess, Runtime);

				TArray<USaveGame*> ResultSaveGames;
				TArray<FSlotName> ResultSlotNames;
				ResultSaveGames.Reserve(Results->Num());
				ResultSlotNames.Reserve(Results->Num());
				for (const FPreloadResult& Result : *Results)
				{
					ResultSlotNames.Add(Result.Key);
					ResultSaveGames.Add(Result.Value);
				}

				Callback.ExecuteIfBound(ResultSaveGames, ResultSlotNames);
				ObjectsToKeepInMemory->Empty();
			}
		}

		TSharedRef<int32> RemainingSlots;
		TSharedRef<TArray<FPreloadResult>> Results;
		TSharedRef<TSet<TStrongObjectPtr<UObject>>> ObjectsToKeepInMemory;
		FOnPreloadCompleted Callback;
	};

	TSharedRef<int32> RemainingSlots = MakeShared<int32>(0);
	TSharedRef<TArray<FPreloadResult>> Results = MakeShared<TArray<FPreloadResult>>();

	//  As long as the preload queue is working, prevent already loaded objects from being garbage collected:
	TSharedRef<TSet<TStrongObjectPtr<UObject>>> ObjectsToKeepInMemory = MakeShared<TSet<TStrongObjectPtr<UObject>>>();

	TMap<FSlotName, TSharedRef<FPreloadRequest>> Requests = {};
	for (const FSlotName& SlotName : SlotNames)
	{
		if (!DoesSaveFileExist(SlotName))
			continue;

		(*RemainingSlots)++;
		Requests.Add(SlotName, MakeShared<FPreloadRequest>(*this, SlotName, RemainingSlots, Results, ObjectsToKeepInMemory, Callback));
	}

	// No slot had a save file -> nothing will ever call Finish(), so notify here:
	if (Requests.IsEmpty())
	{
		Callback.ExecuteIfBound({}, {});
		return;
	}

	for (auto SlotAndRequest = Requests.CreateIterator(); SlotAndRequest; ++SlotAndRequest)
	{
		static constexpr bool bCancelIfLoadingIsNotAllowed = false; // Always preload, even if something disabled loading.
		EnqueueLoadRequest(SlotAndRequest.Key(), SlotAndRequest.Value(), bCancelIfLoadingIsNotAllowed);
	}
}

void USaveGameService::RestoreAsCurrentSaveGame(USaveGame& SaveGame, TOptional<FSlotName> LoadedFromSlotName)
{
	checkf(!IsCachedSaveGameSnapshot(SaveGame), TEXT("Restoring cached SaveGame snapshots is now allowed. Use runtime versions or restore by slot"));

	AddDebugEntry("[RestoreAsCurrentSaveGame] " + SaveGame.GetName() + " from slot: " + LoadedFromSlotName.Get("<Unknown>"));
	SetCurrentSaveGame(FCurrentSaveGame::CreateFromLoadedGame(SaveGame, LoadedFromSlotName));
	OnAfterRestored.Broadcast(CurrentSaveGame);
}

void USaveGameService::RestoreAsAndTravelIntoCurrentSaveGame(USaveGame& SaveGame, TOptional<FSlotName> LoadedFromSlotName)
{
	checkf(!IsCachedSaveGameSnapshot(SaveGame), TEXT("Restoring cached SaveGame snapshots is now allowed. Use runtime versions or restore by slot"));

	RestoreAsCurrentSaveGame(SaveGame, LoadedFromSlotName);
	TryTravelIntoCurrentSaveGame();
}

bool USaveGameService::TryTravelIntoCurrentSaveGame()
{
	check(SaveLoadBehavior);
	if (!SaveLoadBehavior->TryTravelToSavedLevel(CurrentSaveGame))
	{
		AddDebugEntry("[TryTravelIntoCurrentSaveGame] failed");
		return false;
	}

	AddDebugEntry("[TryTravelIntoCurrentSaveGame] succeeded");
	return true;
}

void USaveGameService::CreateNewSaveGameAsCurrent()
{
	AddDebugEntry("[CreateNewSaveGameAsCurrent]");
	USaveGame& SaveGameObject = SaveLoadBehavior->CreateNewSavegameObject(*this);
	SetCurrentSaveGame(FCurrentSaveGame::CreateFromNewGame(SaveGameObject));
}

void USaveGameService::CreateAndRestoreNewSaveGameAsCurrent()
{
	AddDebugEntry("[CreateAndRestoreNewSaveGameAsCurrent]");
	USaveGame& SaveGameObject = SaveLoadBehavior->CreateNewSavegameObject(*this);
	SetCurrentSaveGame(FCurrentSaveGame::CreateFromNewGame(SaveGameObject));
	OnAfterRestored.Broadcast(CurrentSaveGame);
}

void USaveGameService::DeleteSaveGameAtSlot(const FSlotName& SlotName, bool bMoveToBackupFolder)
{
	// (i) Not gated on the cache: a slot that was never preloaded still has a file to delete.
	if (DoesSaveFileExist(SlotName))
	{
		SaveGameSerializer->TryDeleteGameInSlot(SlotName, GetCurrentUserIndex(),
			bMoveToBackupFolder ? FString("Backup_" + FDateTime::Now().ToString()) : TOptional<FString>{});
	}

	CachedSaveGames.Remove(SlotName);
	OnAvailableSaveGamesChanged.Broadcast();
}

void USaveGameService::ProcessPendingRequests()
{
	if (IsBusySavingOrLoading())
		return;

	///////////////////////////////////////////////////////////////////////////////////////////////////
	/// Remark: Architecturally, it would be nicer if there was just one queue of pending requests,
	/// since they were build polymorphic, they could perform the save/load in their Process() method.
	/// Additionally, there is currently the potential issue of multiple similar requests for the same
	/// SlotName being grouped together, which can cause multiple "Finish" executions at the same time.
	/// It might be better to accumulate similar requests in one request object to avoid this.
	///////////////////////////////////////////////////////////////////////////////////////////////////

	if (IsSavingAllowed() && PendingSaveRequestsBySlot.Num() > 0)
	{
		auto RequestToProcess = PendingSaveRequestsBySlot.CreateIterator();
		const FSlotName SlotName = RequestToProcess.Key();
		SaveRequestsInProgress += RequestToProcess.Value();
		for (TSharedRef<ISaveLoadRequest> Request : RequestToProcess.Value())
		{
			Request->Process();
		}
		RequestToProcess.RemoveCurrent();

		PerformAsyncSave(SlotName);
		return;
	}

	if (IsLoadingAllowed() && PendingLoadRequestsBySlot.Num() > 0)
	{
		auto RequestToProcess = PendingLoadRequestsBySlot.CreateIterator();
		const FSlotName SlotName = RequestToProcess.Key();
		LoadRequestsInProgress += RequestToProcess.Value();
		for (TSharedRef<ISaveLoadRequest> Request : RequestToProcess.Value())
		{
			Request->Process();
		}
		RequestToProcess.RemoveCurrent();

		PerformAsyncLoad(SlotName);
		return;
	}
}

FSaveLoadLockHandle USaveGameService::LockAutosaving(const UObject& KeyHolder, const FDebugContext& Context)
{
	const FSaveLoadLockHandle NewKey = FSaveLoadLockHandle::NewGuid();
	FSaveLoadLock& NewLock = ActiveAutosaveLocks.Add(NewKey);
	NewLock.KeyHolder = MakeWeakObjectPtr(&KeyHolder);
	NewLock.ContextString = Context;
	AddDebugEntry("LockAutosaving", FString::Printf(TEXT("KeyHolder: %s, Context: %s, Key: %s"), *KeyHolder.GetName(), *Context, *NewKey.ToString()));
	return NewKey;
}

void USaveGameService::UnlockAutosaving(const FSaveLoadLockHandle& Key, const FDebugContext& Context)
{
	if (!ActiveAutosaveLocks.Contains(Key))
		return;

	ActiveAutosaveLocks.Remove(Key);
	AddDebugEntry("UnlockAutosaving", FString::Printf(TEXT("Context: %s, Key: %s"), *Context, *Key.ToString()));
	ProcessPendingRequests();
}

FSaveLoadLockHandle USaveGameService::LockSaving(const UObject& KeyHolder, const FDebugContext& Context)
{
	const FSaveLoadLockHandle NewKey = FSaveLoadLockHandle::NewGuid();
	FSaveLoadLock& NewLock = ActiveSaveLocks.Add(NewKey);
	NewLock.KeyHolder = MakeWeakObjectPtr(&KeyHolder);
	NewLock.ContextString = Context;
	AddDebugEntry("LockSaving", FString::Printf(TEXT("KeyHolder: %s, Context: %s, Key: %s"), *KeyHolder.GetName(), *Context, *NewKey.ToString()));
	return NewKey;
}

void USaveGameService::UnlockSaving(const FSaveLoadLockHandle& Key, const FDebugContext& Context)
{
	if (!ActiveSaveLocks.Contains(Key))
		return;

	ActiveSaveLocks.Remove(Key);
	AddDebugEntry("UnlockSaving", FString::Printf(TEXT("Context: %s, Key: %s"), *Context, *Key.ToString()));
	ProcessPendingRequests();
}

FSaveLoadLockHandle USaveGameService::LockLoading(const UObject& KeyHolder, const FDebugContext& Context)
{
	const FSaveLoadLockHandle NewKey = FSaveLoadLockHandle::NewGuid();
	FSaveLoadLock& NewLock = ActiveLoadLocks.Add(NewKey);
	NewLock.KeyHolder = MakeWeakObjectPtr(&KeyHolder);
	NewLock.ContextString = Context;
	AddDebugEntry("LockLoading", FString::Printf(TEXT("KeyHolder: %s, Context: %s, Key: %s"), *KeyHolder.GetName(), *Context, *NewKey.ToString()));
	return NewKey;
}

void USaveGameService::UnlockLoading(const FSaveLoadLockHandle& Key, const FDebugContext& Context)
{
	if (!ActiveLoadLocks.Contains(Key))
		return;

	ActiveLoadLocks.Remove(Key);
	AddDebugEntry("UnlockLoading", FString::Printf(TEXT("Context: %s, Key: %s"), *Context, *Key.ToString()));
	ProcessPendingRequests();
}

bool USaveGameService::IsAutosavingAllowed() const
{
	return (IsSavingAllowed() && ActiveAutosaveLocks.IsEmpty());
}

bool USaveGameService::IsSavingAllowed() const
{
	return (SaveLoadBehavior && !IsBusySavingOrLoading() && ActiveSaveLocks.IsEmpty());
}

bool USaveGameService::IsLoadingAllowed() const
{
	return (SaveLoadBehavior && !IsBusySavingOrLoading() && ActiveLoadLocks.IsEmpty());
}

bool USaveGameService::IsBusyLoading() const
{
	return (LoadRequestsInProgress.Num() > 0);
}

bool USaveGameService::IsBusySaving() const
{
	return (SaveRequestsInProgress.Num() > 0);
}

bool USaveGameService::IsBusySavingOrLoading() const
{
	return (IsBusyLoading() || IsBusySaving());
}

bool USaveGameService::CanContinueCurrentSaveGame() const
{
	check(SaveLoadBehavior);
	return SaveLoadBehavior->CanContinueCurrentSaveGame(CurrentSaveGame);
}

void USaveGameService::SetCurrentSaveGame(const FCurrentSaveGame& NewCurrentSaveGame)
{
	OnBeforeCurrentSaveGameChanged.Broadcast(CurrentSaveGame);
	CurrentSaveGame = NewCurrentSaveGame;
	OnAfterCurrentSaveGameChanged.Broadcast(CurrentSaveGame);
}

USaveLoadBehavior& USaveGameService::CreateSaveLoadBehavior(const USaveGameServiceSettings& Settings)
{
	checkf(!SaveLoadBehavior, TEXT("SaveLoadBehavior was already created"));

	TSoftClassPtr<USaveLoadBehavior> BehaviorClass = Settings.SaveLoadBehavior;

#if WITH_EDITOR
	BehaviorClass = (!GetWorld() || GetWorld()->WorldType == EWorldType::PIE)
		? Settings.PlayInEditorSaveLoadBehavior
		: Settings.PlayInStandaloneSaveLoadBehavior;
	{
		bool bHasOverrideBehaviorClass = false;
		FSoftClassPath OverrideBehaviorClass = nullptr;
		USaveGameUtils::GetOverrideSaveLoadBehavior(OUT bHasOverrideBehaviorClass, OUT OverrideBehaviorClass);
		if (bHasOverrideBehaviorClass && !OverrideBehaviorClass.IsNull())
		{
			UE_LOG(LogSaveGameService, Log, TEXT("Using override SaveLoadBehavior: %s"), *OverrideBehaviorClass.ToString());
			BehaviorClass = OverrideBehaviorClass;
		}
	}
#endif

#if WITH_AUTOMATION_TESTS
	if (GIsAutomationTesting)
	{
		BehaviorClass = Settings.AutomationTestSaveLoadBehavior;
	}
#endif

	USaveLoadBehavior* Behavior = NewObject<USaveLoadBehavior>(this, BehaviorClass.LoadSynchronous());
	check(Behavior);

	UE_LOG(LogSaveGameService, Log, TEXT("%s created %s"), *GetName(), *Behavior->GetName());
	OnBeforeSaved.AddUObject(Behavior, &USaveLoadBehavior::HandleBeforeSaveGameSaved, this);
	OnAfterSaved.AddUObject(Behavior, &USaveLoadBehavior::HandleAfterSaveGameSaved, this);
	OnAfterRestored.AddUObject(Behavior, &USaveLoadBehavior::HandleAfterSaveGameRestored, this);
	Behavior->Initialize(*this);

	return *Behavior;
}

USaveGameSerializer& USaveGameService::CreateSaveGameSerializer()
{
	checkf(!SaveGameSerializer, TEXT("SaveGameSerializer was already created"));
	checkf(SaveLoadBehavior, TEXT("SaveLoadBehavior must be created before SaveGameSerializer"));

	const TSubclassOf<USaveGameSerializer> SerializerClass = SaveLoadBehavior->GetSaveGameSerializerClass();
	check(SerializerClass);

	USaveGameSerializer* Serializer = NewObject<USaveGameSerializer>(this, SerializerClass);
	check(Serializer);
	UE_LOG(LogSaveGameService, Log, TEXT("%s created %s"), *GetName(), *Serializer->GetName());

	return *Serializer;
}

void USaveGameService::StartService()
{
	const USaveGameServiceSettings& Settings = *GetDefault<USaveGameServiceSettings>();
	DebugHistoryEntriesToKeep = Settings.DebugHistoryEntriesToKeep;
	DebugHistory.Empty(DebugHistoryEntriesToKeep);

	SaveLoadBehavior = &CreateSaveLoadBehavior(Settings);
	SaveGameSerializer = &CreateSaveGameSerializer();

	SetStatus(EStatus::Idle);

	// Lock save/load while transitioning between levels, to avoid unexpected behavior:
	CreateWorldTransitionSaveLoadLocks();

	SaveLoadBehavior->HandleGameStart(*this);

	// Need something to save to -> create a new SG object:
	if (!CurrentSaveGame.IsValid())
	{
		CreateAndRestoreNewSaveGameAsCurrent();
	}

	FWorldDelegates::OnPostWorldInitialization.AddWeakLambda(this, [this](UWorld* World, const UWorld::InitializationValues)
	{
		HandleLevelChanged(World);
	});
}

void USaveGameService::ShutdownService()
{
	WorldTransitionSaveLock.Reset();
	WorldTransitionLoadLock.Reset();
	CurrentLevelSaveLock.Reset();
	ActiveAutosaveLocks.Empty();
	ActiveSaveLocks.Empty();
	ActiveLoadLocks.Empty();

	SetStatus(EStatus::Uninitialized);
	SaveGameSerializer = nullptr;

	if (SaveLoadBehavior)
	{
		OnBeforeSaved.RemoveAll(SaveLoadBehavior);
		OnAfterSaved.RemoveAll(SaveLoadBehavior);
		OnAfterRestored.RemoveAll(SaveLoadBehavior);
		SaveLoadBehavior->HandleGameExit(*this);
		SaveLoadBehavior = nullptr;
	}

	CurrentSaveGame.Reset();
	CachedSaveGames.Clear();

	PendingSaveRequestsBySlot.Empty();
	PendingLoadRequestsBySlot.Empty();

	while (SaveRequestsInProgress.Num() > 0)
	{
		SaveRequestsInProgress.Pop()->Cancel();
	}
	while (LoadRequestsInProgress.Num() > 0)
	{
		LoadRequestsInProgress.Pop()->Cancel();
	}

	FWorldDelegates::OnPostWorldInitialization.RemoveAll(this);
}

FAsyncSaveGameHandle USaveGameService::EnqueueSaveRequest(const FSlotName& SlotName, const TSharedRef<ISaveLoadRequest>& Request, bool bCancelIfSavingIsNotAllowed)
{
	if (!IsSavingAllowed() && bCancelIfSavingIsNotAllowed)
	{
		UE_LOG(LogSaveGameService, Log, TEXT("Save request ignored, because saving is not allowed."));
		Request->Finish(nullptr, false);
		return FAsyncSaveGameHandle();
	}

	PendingSaveRequestsBySlot.FindOrAdd(SlotName).Emplace(Request);
	ProcessPendingRequests();
	return Request->Handle;
}

void USaveGameService::ConsumeSaveRequestsInProgress(USaveGame* SavedSaveGame, bool bSuccess)
{
	for (const TSharedRef<ISaveLoadRequest>& SaveRequest : SaveRequestsInProgress)
	{
		SaveRequest->Finish(SavedSaveGame, bSuccess);
	}

	SaveRequestsInProgress.Empty();
}

FAsyncLoadGameHandle USaveGameService::EnqueueLoadRequest(const FSlotName& SlotName, const TSharedRef<ISaveLoadRequest>& Request, bool bCancelIfLoadingIsNotAllowed)
{
	if (!IsLoadingAllowed() && bCancelIfLoadingIsNotAllowed)
	{
		Request->Finish(nullptr, false);
		return FAsyncLoadGameHandle();
	}

	if (CachedSaveGames.Contains(SlotName))
	{
		Request->Finish(CachedSaveGames.CopyFromCache(*this, SlotName), true);
	}
	else
	{
		PendingLoadRequestsBySlot.FindOrAdd(SlotName).Emplace(Request);
		ProcessPendingRequests();
	}

	return Request->Handle;
}

void USaveGameService::ConsumeLoadRequestsInProgress(USaveGame* LoadedSaveGame, bool bSuccess)
{
	for (const TSharedRef<ISaveLoadRequest>& LoadRequest : LoadRequestsInProgress)
	{
		LoadRequest->Finish(LoadedSaveGame, bSuccess);
	}

	LoadRequestsInProgress.Empty();
}

USaveGameService::FSlotName USaveGameService::GetAutosaveSlotName() const
{
	check(SaveLoadBehavior);
	return (SaveLoadBehavior ? SaveLoadBehavior->GetAutosaveSlotName(GetCurrentSaveGame()) : FSlotName());
}

TOptional<USaveGameService::FSlotName> USaveGameService::GetMostRecentlySavedSlotName() const
{
	return CurrentSaveGame.GetSlotLastSavedTo();
}

const USaveGame* USaveGameService::GetCachedSaveGameSnapshotAtSlot(const FSlotName& SlotName) const
{
	return CachedSaveGames.Find(SlotName);
}

TMap<USaveGameService::FSlotName, const USaveGame*> USaveGameService::GetAllCachedSaveGameSnapshots() const
{
	return CachedSaveGames.GetAllObjectsBySlot();
}

bool USaveGameService::IsCachedSaveGameSnapshot(const USaveGame& SaveGameObject) const
{
	return CachedSaveGames.Contains(SaveGameObject);
}

bool USaveGameService::HasAnyCachedSaveGameSnapshot() const
{
	return !CachedSaveGames.IsEmpty();
}

bool USaveGameService::DoesSaveFileExist(const FSlotName& SlotName) const
{
	return (SaveGameSerializer && SaveGameSerializer->DoesSaveGameExist(SlotName, GetCurrentUserIndex()));
}

TSet<USaveGameService::FSlotName> USaveGameService::GetSlotNamesAllowedForSaving() const
{
	return SaveLoadBehavior ? SaveLoadBehavior->GetSaveSlotNamesAllowedForSaving(GetCurrentSaveGame()) : TSet<FSlotName>{};
}

TSet<USaveGameService::FSlotName> USaveGameService::GetSlotNamesAllowedForLoading() const
{
	if (!IsLoadingAllowed())
		return {};

	TSet<FSlotName> Result = {};
	for (const FSlotName& SlotName : SaveLoadBehavior->GetSaveSlotNamesAllowedForLoading(GetCurrentSaveGame()))
	{
		// Check if file exists and if the preloaded savegame is loadable:
		if (DoesSaveFileExist(SlotName) && CachedSaveGames.Contains(SlotName))
		{
			Result.Add(SlotName);
		}
	}

	return Result;
}

void USaveGameService::AddDebugEntry(const UObject& Owner, const FString& Entry)
{
	AddDebugEntry(FString::Printf(TEXT("[%s] %s"), *Owner.GetPathName(Owner.GetWorld()), *Entry));
}

void USaveGameService::PerformAsyncSave(const FSlotName& SlotName)
{
	const int32& UserIndex = GetCurrentUserIndex();
	if (!CurrentSaveGame.IsValid())
	{
		HandleAsyncSaveCompleted(SlotName, UserIndex, false);
		return;
	}

	SetStatus(EStatus::Saving);

	// Last chance to populate the save game with data:
	OnBeforeSaved.Broadcast(CurrentSaveGame);

	SaveGameSerializer->AsyncSaveGameToSlot(CurrentSaveGame.GetRef(), SlotName, UserIndex,
		USaveGameSerializer::FOnAsyncSaveCompleted::CreateUObject(this, &ThisClass::HandleAsyncSaveCompleted));
}

void USaveGameService::HandleAsyncSaveCompleted(const FSlotName& SlotName, const int32 UserIndex, bool bSuccess)
{
	if (bSuccess && CurrentSaveGame.IsValid())
	{
		CurrentSaveGame.UpdateTimeOfLastSave();
		CurrentSaveGame.SetSlotLastSavedTo(SlotName);

		// Cache current save game as snapshot copy, so it can be restored as the state it was saved in:
		CachedSaveGames.CopyToCache(*this, SlotName, CurrentSaveGame.GetRef());
		OnAvailableSaveGamesChanged.Broadcast();
	}
	else
	{
		UE_LOG(LogSaveGameService, Warning, TEXT("Saving to slot %s failed."), *SlotName);
		AddDebugEntry("HandleAsyncSaveCompleted", "failed for slot: " + SlotName);
	}

	ConsumeSaveRequestsInProgress(CurrentSaveGame.GetMutablePtr(), bSuccess);
	SetStatus(EStatus::Idle);

	if (bSuccess)
	{
		OnAfterSaved.Broadcast(CurrentSaveGame);
	}

	ProcessPendingRequests();
}

void USaveGameService::PerformAsyncLoad(const FSlotName& SlotName)
{
	if (!DoesSaveFileExist(SlotName))
	{
		// (!) Requests are already in progress at this point and must be consumed,
		// otherwise the service stays busy loading forever.
		ConsumeLoadRequestsInProgress(nullptr, false);
		SetStatus(EStatus::Idle);
		AddDebugEntry("PerformAsyncLoad", "No save file exists for slot: " + SlotName);
		ProcessPendingRequests();
		return;
	}

	SetStatus(EStatus::Loading);

	const int32& UserIndex = GetCurrentUserIndex();
	SaveGameSerializer->AsyncLoadGameFromSlot(SlotName, UserIndex,
		USaveGameSerializer::FOnAsyncLoadCompleted::CreateUObject(this, &ThisClass::HandleAsyncLoadCompleted));
}

USaveGame* USaveGameService::PerformSyncLoad(const FSlotName& SlotName)
{
	const int32 UserIndex = GetCurrentUserIndex();

	USaveGame* LoadedSaveGame = nullptr;
	SaveGameSerializer->TryLoadGameFromSlot(SlotName, UserIndex, OUT LoadedSaveGame);
	if (LoadedSaveGame && !CachedSaveGames.Contains(SlotName))
	{
		CachedSaveGames.CopyToCache(*this, SlotName, *LoadedSaveGame);
		OnAvailableSaveGamesChanged.Broadcast();
	}

	return LoadedSaveGame;
}

void USaveGameService::HandleAsyncLoadCompleted(const FSlotName& SlotName, const int32 UserIndex, USaveGame* LoadedSaveGame)
{
	UE_CLOG(!IsValid(LoadedSaveGame), LogSaveGameService, Log, TEXT("AsyncLoad of SaveGame in slot %s failed."), *SlotName);
	if (IsValid(LoadedSaveGame) && !CachedSaveGames.Contains(SlotName))
	{
		CachedSaveGames.CopyToCache(*this, SlotName, *LoadedSaveGame);
		OnAvailableSaveGamesChanged.Broadcast();
	}

	ConsumeLoadRequestsInProgress(LoadedSaveGame, IsValid(LoadedSaveGame));

	SetStatus(EStatus::Idle);
	ProcessPendingRequests();
}

void USaveGameService::HandleLevelChanged(UWorld* NewWorld)
{
	if (!IsValid(NewWorld))
		return;

	// don't update save locks for e.g. editor preview worlds such as the thumbnail renderer worlds
	if (NewWorld->WorldType != EWorldType::Game && NewWorld->WorldType != EWorldType::PIE)
		return;

	UpdateSaveLockForLevel(NewWorld);

	if (SaveLoadBehavior)
	{
		SaveLoadBehavior->HandleLevelChanged(*this, NewWorld);
	}
}

void USaveGameService::UpdateSaveLockForLevel(UWorld* NewWorld)
{
	const bool bIsSavingAllowed = USaveGameUtils::IsSavingAllowedForWorld(NewWorld);
	const bool bWasSavingAllowed = !CurrentLevelSaveLock.IsSet();
	if (bIsSavingAllowed == bWasSavingAllowed)
		return;

	if (bIsSavingAllowed)
	{
		UnlockSaving(*CurrentLevelSaveLock, "UpdateSaveLockForLevel: " + NewWorld->GetName());
		CurrentLevelSaveLock.Reset();
	}
	else
	{
		CurrentLevelSaveLock = LockSaving(*NewWorld, "UpdateSaveLockForLevel: " + NewWorld->GetName());
	}
}

void USaveGameService::CreateWorldTransitionSaveLoadLocks()
{
	UWorld* World = GetWorld();

	// Callback to unlock save/load again:
	auto UnlockSaveLoadLocks = [this](UWorld* World)
	{
		if (!IsValid(this))
			return;

		// This will unlock save/load again:
		WorldTransitionSaveLock.Reset();
		WorldTransitionLoadLock.Reset();

		World->OnWorldBeginPlay.RemoveAll(this);
	};

	// Lock save/load when leaving a world:
	FWorldDelegates::OnWorldBeginTearDown.AddWeakLambda(this,
		[this](UWorld* World)
		{
			World->OnWorldBeginPlay.RemoveAll(this);

			WorldTransitionSaveLock = FScopedSaveLoadLock::LockSaving(*this, *this, "USaveGameService - World Transition (teardown)");
			WorldTransitionLoadLock = FScopedSaveLoadLock::LockLoading(*this, *this, "USaveGameService - World Transition (teardown)");
		});

	// Unlock save/load when a new world has begun play:
	FWorldDelegates::OnPostWorldInitialization.AddWeakLambda(this,
		[this, UnlockSaveLoadLocks](UWorld* World, const UWorld::InitializationValues)
		{
			if (World->HasBegunPlay())
			{
				UnlockSaveLoadLocks(World);
			}
			else
			{
				World->OnWorldBeginPlay.AddWeakLambda(this, UnlockSaveLoadLocks, World);
			}
		});

	// Lock initially, if existing world has not yet begun play:
	if (!World || !World->HasBegunPlay())
	{
		WorldTransitionSaveLock = FScopedSaveLoadLock::LockSaving(*this, *this, "USaveGameService - World Transition (init)");
		WorldTransitionLoadLock = FScopedSaveLoadLock::LockLoading(*this, *this, "USaveGameService - World Transition (init)");
	}

	// Unlock when existing world has begun play:
	if (World && World->IsInitialized() && !World->HasBegunPlay())
	{
		World->OnWorldBeginPlay.AddWeakLambda(this, UnlockSaveLoadLocks, World);
	}
}

void USaveGameService::SetStatus(const EStatus& NewStatus)
{
	if (CurrentStatus == NewStatus)
		return;

	UE_LOG(LogSaveGameService, VeryVerbose, TEXT("%s changed status: %s -> %s"), *GetName(), *LexToString(CurrentStatus), *LexToString(NewStatus));
	CurrentStatus = NewStatus;
	OnStatusChanged.Broadcast(NewStatus);
}

void USaveGameService::AddDebugEntry(const FString& Entry)
{
	if (DebugHistoryEntriesToKeep < 1)
		return;

	DebugHistory.Add(FString::Printf(TEXT("(%s UTC)\t %s"), *FDateTime::UtcNow().ToString(), *Entry));
	UE_LOG(LogSaveGameService, Verbose, TEXT("%s"), *Entry);

	while (DebugHistory.Num() >= DebugHistoryEntriesToKeep)
	{
		// Shrink head-first to fit desired size:
		DebugHistory.RemoveAt(0);
	}
}

void USaveGameService::AddDebugEntry(const FString& Operation, const FDebugContext& Context)
{
	AddDebugEntry(FString::Printf(TEXT("[%s] %s"), *Operation, *Context));
}

void USaveGameService::AddDebugEntry(const FString& Operation, const FDebugContext& Context, bool bSuccess, double ExecTime)
{
	AddDebugEntry(FString::Printf(TEXT("[%s] %s (took %.3f sec): %s"), *Operation, (bSuccess ? TEXT("succeeded") : TEXT("failed")), ExecTime, *Context));
}

///////////////////////////////////////////////////////////////////////////////////////
/// CACHE

bool USaveGameService::FSaveGamesCache::Contains(const FSlotName& SlotName) const
{
	return SnapshotsBySlot.Num() > 0 && SnapshotsBySlot.Contains(SlotName);
}

bool USaveGameService::FSaveGamesCache::Contains(const USaveGame& SaveGameObject) const
{
	for (const TPair<FSlotName, TStrongObjectPtr<const USaveGame>>& Itr : SnapshotsBySlot)
	{
		if (Itr.Value.Get() == &SaveGameObject)
			return true;
	}
	return false;
}

bool USaveGameService::FSaveGamesCache::IsEmpty() const
{
	return SnapshotsBySlot.IsEmpty();
}

void USaveGameService::FSaveGamesCache::Remove(const FSlotName& SlotName)
{
	SnapshotsBySlot.Remove(SlotName);
}

void USaveGameService::FSaveGamesCache::Clear()
{
	SnapshotsBySlot.Empty();
}

void USaveGameService::FSaveGamesCache::CopyToCache(USaveGameService& InService, const FSlotName& SlotName, const USaveGame& SaveGame)
{
	const USaveGame& Copy = InService.SaveLoadBehavior->DuplicateSaveGameObject(InService, SaveGame);
	SnapshotsBySlot.FindOrAdd(SlotName) = TStrongObjectPtr(&Copy);
}

USaveGame* USaveGameService::FSaveGamesCache::CopyFromCache(USaveGameService& InService, const FSlotName& SlotName) const
{
	if (!Contains(SlotName))
		return nullptr;
	USaveGame& Copy = InService.SaveLoadBehavior->DuplicateSaveGameObject(InService, *SnapshotsBySlot[SlotName].Get());
	return &Copy;
}

const USaveGame* USaveGameService::FSaveGamesCache::Find(const FSlotName& SlotName) const
{
	return SnapshotsBySlot.Contains(SlotName) ? SnapshotsBySlot[SlotName].Get() : nullptr;
}

///////////////////////////////////////////////////////////////////////////////////////
/// REQUESTS

TMap<USaveGameService::FSlotName, const USaveGame*> USaveGameService::FSaveGamesCache::GetAllObjectsBySlot() const
{
	TMap<FSlotName, const USaveGame*> Result;
	Algo::Transform(SnapshotsBySlot, OUT Result,
		[](const TPair<FSlotName, TStrongObjectPtr<const USaveGame>>& Itr) { return TPair<FSlotName, const USaveGame*>(Itr.Key, Itr.Value.Get()); });
	return Result;
}

void USaveGameService::ISaveLoadRequest::Process()
{
	StartTime = FPlatformTime::Seconds();
}

void USaveGameService::ISaveLoadRequest::Finish(USaveGame* RequestedSaveGame, bool bSuccess)
{
	if (RequestCallback.IsSet())
	{
		RequestCallback->ExecuteIfBound(RequestedSaveGame, bSuccess);
	}
}

void USaveGameService::FLoadToCurrentSaveGameRequest::Finish(USaveGame* RequestedSaveGame, bool bSuccess)
{
	if (bSuccess)
	{
		Service.RestoreAsCurrentSaveGame(*RequestedSaveGame, SlotName);
	}

	Runtime = FPlatformTime::Seconds() - StartTime;
	Service.AddDebugEntry("LoadToCurrentSaveGameRequest", Context, bSuccess, Runtime);
	ISaveLoadRequest::Finish(RequestedSaveGame, bSuccess);
}

void USaveGameService::FLoadAndTravelIntoToCurrentSaveGameRequest::Finish(USaveGame* RequestedSaveGame, bool bSuccess)
{
	if (bSuccess)
	{
		Service.RestoreAsAndTravelIntoCurrentSaveGame(*RequestedSaveGame, SlotName);
	}

	Runtime = FPlatformTime::Seconds() - StartTime;
	Service.AddDebugEntry("LoadAndTravelIntoToCurrentSaveGameRequest", Context, bSuccess, Runtime);
	ISaveLoadRequest::Finish(RequestedSaveGame, bSuccess);
}

void USaveGameService::FSaveCurrentSaveGameRequest::Finish(USaveGame* RequestedSaveGame, bool bSuccess)
{
	Runtime = FPlatformTime::Seconds() - StartTime;
	Service.AddDebugEntry("SaveCurrentSaveGameRequest", Context, bSuccess, Runtime);
	ISaveLoadRequest::Finish(RequestedSaveGame, bSuccess);
}

///////////////////////////////////////////////////////////////////////////////////////
/// SCOPED SAVE/LOAD LOCKS

TSharedRef<USaveGameService::FScopedSaveLoadLock> USaveGameService::FScopedSaveLoadLock::LockSaving(USaveGameService& Service, const UObject& KeyHolder, const FDebugContext& Context)
{
	TSharedRef<FScopedSaveLoadLock> ScopedLock = MakeShared<FScopedSaveLoadLock>();
	FSaveLoadLockHandle ActualHandle = Service.LockSaving(KeyHolder, "[Lock] " + Context);
	ScopedLock->UnlockFunc = [Service = MakeWeakObjectPtr(&Service), ActualHandle, Context = CopyTemp(Context)]()
	{
		if (Service.IsValid() && ActualHandle.IsValid())
		{
			Service->UnlockSaving(ActualHandle, "[Unlock] " + Context);
		}
	};
	return ScopedLock;
}

TSharedRef<USaveGameService::FScopedSaveLoadLock> USaveGameService::FScopedSaveLoadLock::LockLoading(USaveGameService& Service, const UObject& KeyHolder, const FDebugContext& Context)
{
	TSharedRef<FScopedSaveLoadLock> ScopedLock = MakeShared<FScopedSaveLoadLock>();
	FSaveLoadLockHandle ActualHandle = Service.LockLoading(KeyHolder, "[Lock] " + Context);
	ScopedLock->UnlockFunc = [Service = MakeWeakObjectPtr(&Service), ActualHandle, Context = CopyTemp(Context)]()
	{
		if (Service.IsValid() && ActualHandle.IsValid())
		{
			Service->UnlockLoading(ActualHandle, "[Unlock] " + Context);
		}
	};
	return ScopedLock;
}

USaveGameService::FScopedSaveLoadLock::~FScopedSaveLoadLock()
{
	if (UnlockFunc.IsSet())
	{
		UnlockFunc();
		UnlockFunc.Reset();
	}
}

///////////////////////////////////////////////////////////////////////////////////////

FString LexToString(const USaveGameService::EStatus& Status)
{
	switch (Status)
	{
	case USaveGameService::EStatus::Uninitialized: return "Uninitialized";
	case USaveGameService::EStatus::Saving: return "Saving";
	case USaveGameService::EStatus::Loading: return "Loading";
	case USaveGameService::EStatus::Idle: return "Idle";
	default: return "???";
	}
}
