///////////////////////////////////////////////////////////////////////////////////////
/// Copyright (C) by Benjamin Barz and contributors. See file: CREDITS.md
///
/// This file is part of the WeekendUtils UE5 Plugin.
///
/// Distributed under the MIT License. See file: LICENSE.md
///
///////////////////////////////////////////////////////////////////////////////////////

#include "SaveGame/Modules/LevelObjectRestorer.h"

#include "Engine/Level.h"
#include "Engine/World.h"
#include "Logging/MessageLog.h"
#include "Misc/UObjectToken.h"
#include "SaveGame/SaveGameUtils.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/ObjectAndNameAsStringProxyArchive.h"
#include "Templates/Greater.h"

DEFINE_LOG_CATEGORY_STATIC(LogLevelObjectRestorer, Log, All);

namespace
{
	void CheckLevelObject(const UObject& Object)
	{
		checkf(IsValid(&Object),
			TEXT("ULevelObjectRestorer: Invalid object: %s"), *GetNameSafe(&Object));
		checkf(Object.GetWorld() && Object.GetWorld()->IsGameWorld(),
			TEXT("ULevelObjectRestorer: Non-level-objects are not supported: %s"), *GetNameSafe(&Object));
		checkf(!Object.HasAnyFlags(RF_ArchetypeObject | RF_ClassDefaultObject),
			TEXT("ULevelObjectRestorer: CDO objects are not supported: %s"), *GetNameSafe(&Object));
		checkf(!Object.HasAnyFlags(RF_Standalone | RF_Transient),
			TEXT("ULevelObjectRestorer: Persistent objects are not supported: %s"), *GetNameSafe(&Object));
	}
}

void ULevelObjectRestorer::RegisterLevelObject(UObject& Object, TOptional<FString> CustomUniqueObjectId, bool bImmediatelyRestoreIfPossible)
{
	CheckLevelObject(Object);
	const TWeakObjectPtr<> ObjectPtr = MakeWeakObjectPtr(&Object);
	ensureMsgf(!SimpleRegisteredObjects.Contains(ObjectPtr), TEXT("%s is already registered"), *Object.GetName());
	SimpleRegisteredObjects.Add(ObjectPtr);

	const FString ObjectId = CustomUniqueObjectId.Get(MakeSafeUniqueObjectId(Object));
	UniqueIdsOfRegisteredObjects.Add(ObjectPtr, ObjectId);
	ClaimedUniqueObjectIds.Add(ObjectId);

	if (bImmediatelyRestoreIfPossible && ObjectStates.Contains(ObjectId))
	{
		FLevelObjectSaveGameState& State = ObjectStates[ObjectId];
		State.bUsesCustomUniqueObjectId = CustomUniqueObjectId.IsSet();
		RestoreObjectFromState(State, false, IN OUT Object);
		UE_LOG(LogLevelObjectRestorer, VeryVerbose, TEXT("Registered LevelObject \"%s\" restored an existing state for ObjectId: \"%s\""), *Object.GetName(), *ObjectId)
	}
	else
	{
		FLevelObjectSaveGameState& State = ObjectStates.FindOrAdd(ObjectId);
		State.bUsesCustomUniqueObjectId = CustomUniqueObjectId.IsSet();
		SaveObjectToState(Object, false, IN OUT State);
		UE_LOG(LogLevelObjectRestorer, VeryVerbose, TEXT("Registered LevelObject \"%s\" did NOT restore an existing state for ObjectId: \"%s\""), *Object.GetName(), *ObjectId)
	}
}

void ULevelObjectRestorer::RegisterLevelObjectWithTransform(AActor& Actor, TOptional<FString> CustomUniqueObjectId, bool bImmediatelyRestoreIfPossible)
{
	CheckLevelObject(Actor);
	const TWeakObjectPtr<> ObjectPtr = MakeWeakObjectPtr(&Actor);
	ensureMsgf(!RegisteredObjectsWithTransform.Contains(ObjectPtr), TEXT("%s is already registered"), *Actor.GetName());
	RegisteredObjectsWithTransform.Add(ObjectPtr);

	const FString ObjectId = CustomUniqueObjectId.Get(MakeSafeUniqueObjectId(Actor));
	UniqueIdsOfRegisteredObjects.Add(ObjectPtr, ObjectId);
	ClaimedUniqueObjectIds.Add(ObjectId);

	if (bImmediatelyRestoreIfPossible && ObjectStates.Contains(ObjectId))
	{
		ObjectStates[ObjectId].bUsesCustomUniqueObjectId = CustomUniqueObjectId.IsSet();
		RestoreObjectFromState(ObjectStates[ObjectId], true, IN OUT Actor);
		UE_LOG(LogLevelObjectRestorer, VeryVerbose, TEXT("Registered LevelObject \"%s\" restored an existing state for ObjectId: \"%s\""), *Actor.GetName(), *ObjectId)
	}
	else
	{
		FLevelObjectSaveGameState& State = ObjectStates.FindOrAdd(ObjectId);
		State.bUsesCustomUniqueObjectId = CustomUniqueObjectId.IsSet();
		SaveObjectToState(Actor, true, IN OUT State);
		UE_LOG(LogLevelObjectRestorer, VeryVerbose, TEXT("Registered LevelObject \"%s\" did NOT restore an existing state for ObjectId: \"%s\""), *Actor.GetName(), *ObjectId)
	}
}

void ULevelObjectRestorer::RegisterLevelObjectWithTransform(USceneComponent& SceneComponent, TOptional<FString> CustomUniqueObjectId, bool bImmediatelyRestoreIfPossible)
{
	CheckLevelObject(SceneComponent);
	const TWeakObjectPtr<> ObjectPtr = MakeWeakObjectPtr(&SceneComponent);
	ensureMsgf(!RegisteredObjectsWithTransform.Contains(ObjectPtr), TEXT("%s is already registered"), *SceneComponent.GetName());
	RegisteredObjectsWithTransform.Add(ObjectPtr);

	const FString ObjectId = CustomUniqueObjectId.Get(MakeSafeUniqueObjectId(SceneComponent));
	UniqueIdsOfRegisteredObjects.Add(ObjectPtr, ObjectId);
	ClaimedUniqueObjectIds.Add(ObjectId);

	if (bImmediatelyRestoreIfPossible && ObjectStates.Contains(ObjectId))
	{
		ObjectStates[ObjectId].bUsesCustomUniqueObjectId = CustomUniqueObjectId.IsSet();
		RestoreObjectFromState(ObjectStates[ObjectId], true, IN OUT SceneComponent);
		UE_LOG(LogLevelObjectRestorer, VeryVerbose, TEXT("Registered LevelObject \"%s\" restored an existing state for ObjectId: \"%s\""), *SceneComponent.GetName(), *ObjectId)
	}
	else
	{
		FLevelObjectSaveGameState& State = ObjectStates.FindOrAdd(ObjectId);
		State.bUsesCustomUniqueObjectId = CustomUniqueObjectId.IsSet();
		SaveObjectToState(SceneComponent, true, IN OUT State);
		UE_LOG(LogLevelObjectRestorer, VeryVerbose, TEXT("Registered LevelObject \"%s\" did NOT restore an existing state for ObjectId: \"%s\""), *SceneComponent.GetName(), *ObjectId)
	}
}

void ULevelObjectRestorer::UnregisterLevelObject(UObject& Object, TOptional<FString> CustomUniqueObjectId, bool bKeepObjectState)
{
	CheckLevelObject(Object);
	const TWeakObjectPtr<> ObjectPtr = MakeWeakObjectPtr(&Object);
	ensureMsgf(SimpleRegisteredObjects.Contains(ObjectPtr), TEXT("%s is not registered"), *Object.GetName());
	SimpleRegisteredObjects.Remove(ObjectPtr);
	UniqueIdsOfRegisteredObjects.Remove(ObjectPtr);

	const FString ObjectId = CustomUniqueObjectId.Get(MakeSafeUniqueObjectId(Object));
	if (bKeepObjectState)
	{
		FLevelObjectSaveGameState& State = ObjectStates.FindOrAdd(ObjectId);
		State.bUsesCustomUniqueObjectId = CustomUniqueObjectId.IsSet();
		SaveObjectToState(Object, false, IN OUT State);
	}
	else
	{
		ObjectStates.Remove(ObjectId);
	}
}

void ULevelObjectRestorer::UnregisterLevelObjectWithTransform(AActor& Actor, TOptional<FString> CustomUniqueObjectId, bool bKeepObjectState)
{
	CheckLevelObject(Actor);
	const TWeakObjectPtr<> ObjectPtr = MakeWeakObjectPtr(&Actor);
	ensureMsgf(RegisteredObjectsWithTransform.Contains(ObjectPtr), TEXT("%s is not registered"), *Actor.GetName());
	RegisteredObjectsWithTransform.Remove(ObjectPtr);
	UniqueIdsOfRegisteredObjects.Remove(ObjectPtr);

	const FString ObjectId = CustomUniqueObjectId.Get(MakeSafeUniqueObjectId(Actor));
	if (bKeepObjectState)
	{
		FLevelObjectSaveGameState& State = ObjectStates.FindOrAdd(ObjectId);
		State.bUsesCustomUniqueObjectId = CustomUniqueObjectId.IsSet();
		SaveObjectToState(Actor, true, IN OUT State);
	}
	else
	{
		ObjectStates.Remove(ObjectId);
	}
}

void ULevelObjectRestorer::UnregisterLevelObjectWithTransform(USceneComponent& SceneComponent, TOptional<FString> CustomUniqueObjectId, bool bKeepObjectState)
{
	CheckLevelObject(SceneComponent);
	const TWeakObjectPtr<> ObjectPtr = MakeWeakObjectPtr(&SceneComponent);
	ensureMsgf(RegisteredObjectsWithTransform.Contains(ObjectPtr), TEXT("%s is not registered"), *SceneComponent.GetName());
	RegisteredObjectsWithTransform.Remove(ObjectPtr);
	UniqueIdsOfRegisteredObjects.Remove(ObjectPtr);

	const FString ObjectId = CustomUniqueObjectId.Get(MakeSafeUniqueObjectId(SceneComponent));
	if (bKeepObjectState)
	{
		FLevelObjectSaveGameState& State = ObjectStates.FindOrAdd(ObjectId);
		State.bUsesCustomUniqueObjectId = CustomUniqueObjectId.IsSet();
		SaveObjectToState(SceneComponent, true, IN OUT State);
	}
	else
	{
		ObjectStates.Remove(ObjectId);
	}
}

const FLevelObjectSaveGameState* ULevelObjectRestorer::FindObjectState(const FString& UniqueObjectId) const
{
	return ObjectStates.Find(UniqueObjectId);
}

FString ULevelObjectRestorer::MakeSafeUniqueObjectId(const UObject& Object) const
{
	// (i) Stop GetPathName() at the world-outer, then prefix the path with just the world name, not the whole path to the world.
	// This is done to avoid ID mismatches between PIE worlds and standalone worlds, so standalone saves can be properly loaded in PIE.
	// See UpgradeSaveGameModule() for more information.
	const FString ObjectPath = Object.GetPathName(Object.GetTypedOuter<ULevel>());
	const FString LevelName = Object.GetTypedOuter<ULevel>()->GetName();
	const FString WorldName = Object.GetWorld()->GetName();
	FString ObjectId = WorldName + ":" + LevelName + "." + ObjectPath;

	// Replace any part of the ObjectId that matches a registered redirect:
	for (const TTuple<FString, FString>& Redirect : UniqueObjectIdRedirects)
	{
		if (!ObjectId.Contains(Redirect.Key))
			continue;

		ObjectId = ObjectId.Replace(*Redirect.Key, *Redirect.Value);
		UE_LOG(LogLevelObjectRestorer, Verbose, TEXT("(!) Redirecting UniqueObjectId: %s -> %s"), *ObjectId, *Redirect.Value);
	}

	return ObjectId;
}

void ULevelObjectRestorer::RegisterUniqueObjectIdRedirects(const TMap<FString, FString>& AdditionalUniqueObjectIdRedirects)
{
	for (const TTuple<FString, FString>& Pair : AdditionalUniqueObjectIdRedirects)
	{
		UE_LOG(LogLevelObjectRestorer, Log, TEXT("Registering UniqueObjectId redirect: %s -> %s"), *Pair.Key, *Pair.Value);
		UniqueObjectIdRedirects.Add(Pair.Key, Pair.Value);
	}
}

void ULevelObjectRestorer::Serialize(FArchive& Ar)
{
	// Prepare serialization:
	if (Ar.ArIsSaveGame && Ar.IsSaving())
	{
		PreSaveModule();

		for (TWeakObjectPtr<> RegisteredObject : SimpleRegisteredObjects.Union(RegisteredObjectsWithTransform))
		{
			if (!RegisteredObject.IsValid())
				continue;

			UObject* Object = RegisteredObject.Get();
			const FString& ObjectId = UniqueIdsOfRegisteredObjects[RegisteredObject];
			const bool bHasTransform = RegisteredObjectsWithTransform.Contains(RegisteredObject);
			FLevelObjectSaveGameState& State = ObjectStates.FindOrAdd(ObjectId);
			SaveObjectToState(*Object, bHasTransform, IN OUT State);
		}
	}

	// Prepare deserialization:
	if (Ar.ArIsSaveGame && Ar.IsLoading())
	{
		// Make sure that the default version is set to initial before deserialization in case the Archive.ArNoIntraPropertyDelta was false
		// when the savegame was created. Because that would cause the ModuleVersion to become "ModuleVersion_Current" although it might not be the case!
		// The UObject::Serialize() will now either NOT modify the ModuleVersion (delta = false), or overwrite it with the serialized ModuleVersion (delta = true).
		ModuleVersion = ModuleVersion_Initial;
	}

	UObject::Serialize(Ar);

	// Finalize deserialization:
	if (Ar.ArIsSaveGame && Ar.IsLoading())
	{
		UE_LOG(LogLevelObjectRestorer, Log, TEXT("Restoring %s with ModuleVersion %d"), *GetPathName(), ModuleVersion);
		UpgradeSaveGameModule();

		for (TWeakObjectPtr<> RegisteredObject : SimpleRegisteredObjects.Union(RegisteredObjectsWithTransform))
		{
			if (!RegisteredObject.IsValid())
				continue;

			UObject* Object = RegisteredObject.Get();
			const FString& ObjectId = UniqueIdsOfRegisteredObjects[RegisteredObject];
			const bool bHasTransform = RegisteredObjectsWithTransform.Contains(RegisteredObject);
			FLevelObjectSaveGameState& State = ObjectStates.FindOrAdd(ObjectId);
			RestoreObjectFromState(State, bHasTransform, IN OUT *Object);
		}

		PostRestoreModule();
	}
}

void ULevelObjectRestorer::BeginDestroy()
{
	// Report only if there are any registered objects at all to avoid reports of temporary module instances:
	if (bReportUnclaimedObjectStatesOnDestruction && ClaimedUniqueObjectIds.Num() > 0)
	{
		ReportUnclaimedObjectStates();
	}

	Super::BeginDestroy();
}

void ULevelObjectRestorer::SaveObjectToState(UObject& Object, bool bSaveTransform, FLevelObjectSaveGameState& InOutState) const
{
	FMemoryWriter MemWriter(InOutState.ByteData);
	if (bSaveTransform)
	{
		FTransform Transform = GetObjectTransform(Object);
		MemWriter << Transform;
	}

	FObjectAndNameAsStringProxyArchive Archive(MemWriter, true);
	Archive.ArIsSaveGame = true;
	Object.Serialize(Archive);
	InOutState.ByteDataSize = FMath::Min(InOutState.ByteData.Num(), INT32_MAX);
}

void ULevelObjectRestorer::RestoreObjectFromState(const FLevelObjectSaveGameState& State, bool bRestoreTransform, UObject& InOutObject) const
{
	FMemoryReader MemReader(State.ByteData);
	if (bRestoreTransform)
	{
		FTransform Transform = FTransform::Identity;
		MemReader << Transform;
		SetObjectTransform(InOutObject, Transform);
	}

	FObjectAndNameAsStringProxyArchive Archive(MemReader, true);
	Archive.ArIsSaveGame = true;
	InOutObject.Serialize(Archive);
}

FTransform ULevelObjectRestorer::GetObjectTransform(UObject& Object) const
{
	if (const AActor* Actor = Cast<AActor>(&Object))
		return Actor->GetTransform();

	if (const USceneComponent* SceneComponent = Cast<USceneComponent>(&Object))
		return SceneComponent->GetComponentTransform();

	return FTransform::Identity;
}

void ULevelObjectRestorer::SetObjectTransform(UObject& Object, const FTransform& Transform) const
{
	if (AActor* Actor = Cast<AActor>(&Object))
	{
		Actor->SetActorTransform(Transform);
	}
	else if (USceneComponent* SceneComponent = Cast<USceneComponent>(&Object))
	{
		SceneComponent->SetWorldTransform(Transform);
	}
}

void ULevelObjectRestorer::UpgradeSaveGameModule()
{
	// Convert old PathName-based ObjectIds to new safe ObjectIds that don't care about PIE or standalone level naming:
	if (ModuleVersion < ModuleVersion_WithSafeUniqueObjectId)
	{
		// The old object ids were (by default) based on Object.GetPathName() which includes the whole level PathName.
		// -> In PIE worlds, this string is different from standalone worlds (e.g. "...UEDPIE_0_L_WorldName.L_WorldName..."),
		// which will lead to mismatches when trying to load a savegame file from a non-PIE session and vice versa.
		// The following data migration rips out the full PathName of the level prefix and replaces it with just the
		// object-name of the level (e.g. "L_WorldName"), which will now also the result of MakeSafeUniqueObjectId(..).
		int32 NumUpgradedObjectStates = 0;
		TMap<FString, FLevelObjectSaveGameState> ObjectStatesToAdd{};
		UE_LOG(LogLevelObjectRestorer, Log, TEXT("%s needs to be upgraded to ModuleVersion_WithSafeUniqueObjectId ..."), *GetPathName());
		for (auto Itr = ObjectStates.CreateIterator(); Itr; ++Itr)
		{
			if (Itr.Value().bUsesCustomUniqueObjectId)
				continue;

			// Convert "/Game/<MapPath>/<PackageName>:<ObjectPath>" into "<WorldName>:<ObjectPath>"
			const FString OldObjectId = Itr.Key();
			if (int32 IndexOfObjectSeparator = INDEX_NONE; OldObjectId.FindChar(':', OUT IndexOfObjectSeparator))
			{
				FString OldObjectWorldName = OldObjectId.Left(IndexOfObjectSeparator);
				FString NewObjectId_Part2 = OldObjectId.RightChop(IndexOfObjectSeparator + 1);
				if (int32 IndexOfWorldNameSeparator = INDEX_NONE; OldObjectWorldName.FindLastChar('.', OUT IndexOfWorldNameSeparator))
				{
					const FString NewObjectId_Part1 = OldObjectWorldName.RightChop(IndexOfWorldNameSeparator + 1);
					const FString NewObjectId = NewObjectId_Part1 + ":" + NewObjectId_Part2;
					if (NewObjectId != OldObjectId)
					{
						UE_LOG(LogLevelObjectRestorer, Verbose, TEXT("Upgrading UniqueObjectId \"%s\" -> \"%s\""), *OldObjectId, *NewObjectId);
						// Defer adding the new key-value pair to prevent contamination of this iteration with already upgraded data.
						ObjectStatesToAdd.Emplace(NewObjectId, Itr.Value());
						Itr.RemoveCurrent();
						++NumUpgradedObjectStates;
					}
				}
			}
		}
		ObjectStates.Append(ObjectStatesToAdd);

		UE_LOG(LogLevelObjectRestorer, Log, TEXT("%s upgraded %d of %d UniqueObjectIds"), *GetPathName(), NumUpgradedObjectStates, ObjectStates.Num());
		ModuleVersion = ModuleVersion_WithSafeUniqueObjectId;
	}

	if (ModuleVersion < ModuleVersion_WithSafeUniqueObjectId_Hotfix)
	{
		// The previous module upgrade caused some object ID's to be incorrectly formatted. This upgrade will fix that.
		int32 NumUpgradedObjectStates = 0;
		TMap<FString, FLevelObjectSaveGameState> ObjectStatesToAdd{};
		UE_LOG(LogLevelObjectRestorer, Log, TEXT("%s needs to be upgraded to ModuleVersion_WithSafeUniqueObjectId_Hotfix ..."), *GetPathName());
		for (auto Itr = ObjectStates.CreateIterator(); Itr; ++Itr)
		{
			if (Itr->Value.bUsesCustomUniqueObjectId)
				continue;

			const FString OldObjectId = Itr->Key;
			if (!OldObjectId.Contains(":/"))
				continue;

			int32 IndexOfFirstColon = INDEX_NONE, IndexOfSecondColon = INDEX_NONE;
			if (!OldObjectId.FindChar(':', OUT IndexOfFirstColon) || !OldObjectId.FindLastChar(':', OUT IndexOfSecondColon) || IndexOfFirstColon == IndexOfSecondColon)
			{
				UE_LOG(LogLevelObjectRestorer, Warning, TEXT("\"%s\" was selected for upgrade to ModuleVersion_WithSafeUniqueObjectId_Hotfix, but ObjectId has unexpected format"), *OldObjectId);
				continue;
			}

			const FString NewObjectId_Part1 = OldObjectId.Left(IndexOfFirstColon + 1);
			const FString NewObjectId_Part2 = OldObjectId.RightChop(IndexOfSecondColon + 1);
			const FString NewObjectId = NewObjectId_Part1 + NewObjectId_Part2;
			if (ObjectStates.Contains(NewObjectId))
			{
				// Info on wording:
				// - conflicting state = what we currently have in Itr and want to update with the transformed NewObjectId
				// - existing state    = key-value pair existing somewhere in the map which surprisingly is keyed with NewObjectId
				UE_LOG(LogLevelObjectRestorer, Warning, TEXT("Upgrading UniqueObjectId \"%s\" -> \"%s\" conflicts with already existing object state for that ObjectId"), *OldObjectId, *NewObjectId);
				switch (ConflictResolutionPolicy)
				{
				case ELevelObjectRestorerConflictResolutionPolicy::KeepExistingDiscardConflicting:
				{
					++NumUpgradedObjectStates;
					// Keep the existing state by not doing anything with it (it already has correct key), discard the state we're currently looking at.
					Itr.RemoveCurrent();
					break;
				}
				case ELevelObjectRestorerConflictResolutionPolicy::DiscardExistingKeepConflicting:
				{
					// Keep the value of the conflicting state by:
					// 1. overwriting the value of the existing key-value pair (which already has the correct key) with the conflicting state
					// 2. discard the now useless conflicting key-value pair
					ObjectStates[NewObjectId] = Itr->Value;
					Itr.RemoveCurrent();
					++NumUpgradedObjectStates;
					break;
				}
				case ELevelObjectRestorerConflictResolutionPolicy::DoNotResolveKeepBoth:
					break;
				}
			}
			else
			{
				UE_LOG(LogLevelObjectRestorer, Verbose, TEXT("Upgrading UniqueObjectId \"%s\" -> \"%s\""), *OldObjectId, *NewObjectId);
				// Discard the current faulty key-value pair.
				// Defer adding the new key-value pair to prevent contamination of this iteration with already upgraded data.
				ObjectStatesToAdd.Emplace(NewObjectId, Itr->Value);
				Itr.RemoveCurrent();
				++NumUpgradedObjectStates;
			}
		}
		ObjectStates.Append(ObjectStatesToAdd);

		UE_LOG(LogLevelObjectRestorer, Log, TEXT("%s upgraded %d of %d UniqueObjectIds"), *GetPathName(), NumUpgradedObjectStates, ObjectStates.Num());
		ModuleVersion = ModuleVersion_WithSafeUniqueObjectId_Hotfix;
	}

	{
		// Detect duplicate keys and determine the value that will win in the end (all other values will be discarded).
		//
		// We're choosing the winning value by maximizing byte data size - this could be wrong in some cases, but we have
		// no way to know and chances are, that over time more, not less data is stored per state -> so this is our best guess.
		struct FStateKeyDuplicationInfo
		{
			int32 NumOccurrencesOfKey{0};
			int32 IndexOfStateWithHighestByteDataSize{-1};
			int32 HighestByteDataSize{-1};
		};
		TMap<FString, FStateKeyDuplicationInfo> KeyDuplicationInfos{};

		TArray<FString> ObjectStateKeys;
		ObjectStates.GenerateKeyArray(OUT ObjectStateKeys);
		TArray<FLevelObjectSaveGameState> ObjectStatesValues;
		ObjectStates.GenerateValueArray(OUT ObjectStatesValues);

		int32 NumKeysWithDuplicates = 0;

		for (int32 ObjectStateIndex = 0; ObjectStateIndex < ObjectStateKeys.Num(); ++ObjectStateIndex)
		{
			FStateKeyDuplicationInfo& KeyInfo = KeyDuplicationInfos.FindOrAdd(ObjectStateKeys[ObjectStateIndex]);
			KeyInfo.NumOccurrencesOfKey++;
			if (KeyInfo.HighestByteDataSize == -1 || ObjectStatesValues[ObjectStateIndex].ByteDataSize > KeyInfo.HighestByteDataSize)
			{
				KeyInfo.IndexOfStateWithHighestByteDataSize = ObjectStateIndex;
				KeyInfo.HighestByteDataSize = ObjectStatesValues[ObjectStateIndex].ByteDataSize;
			}
			// the first time we find a duplicate for this key, note it down for our statistics log later
			if (KeyInfo.NumOccurrencesOfKey == 2)
			{
				NumKeysWithDuplicates++;
			}
		}

		// For any duplicate key, remove all entries from ObjectStates (since TMap's API does not allow removing a specific pair by index (understandably))
		// then re-add the key with the winning value.
		if (NumKeysWithDuplicates > 0)
		{
			UE_LOG(LogLevelObjectRestorer, Log, TEXT("Found %ix duplicate ObjectState keys for %s! Commencing cleanup ..."), NumKeysWithDuplicates, *GetPathName());
		}
		for (const TPair<FString, FStateKeyDuplicationInfo>& KeyDuplicationInfo : KeyDuplicationInfos)
		{
			if (KeyDuplicationInfo.Value.NumOccurrencesOfKey > 1)
			{
				UE_LOG(LogLevelObjectRestorer, Log, TEXT("> ObjectState key is multiple times (%ix) in the set! Commencing cleanup ... %s"),
					KeyDuplicationInfo.Value.NumOccurrencesOfKey, *KeyDuplicationInfo.Key);
				while (ObjectStates.Contains(KeyDuplicationInfo.Key))
				{
					ObjectStates.Remove(KeyDuplicationInfo.Key);
				}
				ObjectStates.Add(KeyDuplicationInfo.Key, ObjectStatesValues[KeyDuplicationInfo.Value.IndexOfStateWithHighestByteDataSize]);
			}
		}
	}
}

void ULevelObjectRestorer::ReportUnclaimedObjectStates() const
{
	for (const TPair<FString, FLevelObjectSaveGameState>& ObjectState : ObjectStates)
	{
		if (ClaimedUniqueObjectIds.Contains(ObjectState.Key))
			continue;

		UE_LOG(LogLevelObjectRestorer, Log, TEXT("%s reports unclaimed ObjectState for UniqueObjectId: %s"), *GetPathName(), *ObjectState.Key);
	}
}

#if WITH_EDITOR
void ULevelObjectRestorer::AnalyzeAndReportModuleComposition(FMessageLog& MessageLog) const
{
	const int64 TotalSize = USaveGameUtils::CalculateObjectSizeForSaveGame(*this, EUnit::Bytes);
	const int64 TotalSizeKb = FUnitConversion::Convert(TotalSize, EUnit::Bytes, EUnit::Kilobytes);

	MessageLog.Info()
		->AddToken(FTextToken::Create(FText::FromString("Analysis for")))
		->AddToken(FUObjectToken::Create(this));

	int32 NumEntriesBelowThreshold = 0;
	constexpr double PctThreshold = 0.1;
	TMap<int64, FString> SortedAnalysisEntries;
	for (const TPair<FString, FLevelObjectSaveGameState>& Itr : ObjectStates)
	{
		const int64 ObjectSize = Itr.Value.ByteDataSize;
		const int64 ObjectSizeKb = FUnitConversion::Convert(ObjectSize, EUnit::Bytes, EUnit::Kilobytes);
		const double Pct = 100.0 * static_cast<double>(ObjectSize) / static_cast<double>(TotalSize);
		if (Pct <= PctThreshold)
		{
			++NumEntriesBelowThreshold;
			continue;
		}

		int64 SortIndex = ObjectSize;
		while (SortedAnalysisEntries.Contains(SortIndex))
		{
			++SortIndex; // TMap needs unique keys, this handles same-sized entries in a good-enough way.
		}

		const FString PctString = FString::Printf(TEXT("%05.2f%%"), Pct);
		const FString SizeString = ObjectSize < 10000 ? FString::Printf(TEXT("%lld bytes"), ObjectSize) : FString::Printf(TEXT("%lld KB"), ObjectSizeKb);
		SortedAnalysisEntries.Add(SortIndex, FString::Printf(TEXT("- [%s] State: %s (~%s)"), *PctString, *Itr.Key, *SizeString));
	}
	SortedAnalysisEntries.KeySort(TGreater<int64>());

	for (const TPair<int64, FString>& Itr : SortedAnalysisEntries)
	{
		MessageLog.Info()->AddToken(FTextToken::Create(FText::FromString(Itr.Value)));
	}
	if (NumEntriesBelowThreshold > 0)
	{
		MessageLog.Info()->AddToken(FTextToken::Create(FText::FromString(FString::Printf(TEXT("- [<%04.2f%%] %d other states"), PctThreshold, NumEntriesBelowThreshold))));
	}
	MessageLog.Info()->AddToken(FTextToken::Create(FText::FromString(FString::Printf(TEXT("=> Total: %lld bytes (%lld KB)"), TotalSize, TotalSizeKb))));
	MessageLog.Info()->AddToken(FTextToken::Create(FText::FromString("===================================")));
}
#endif
