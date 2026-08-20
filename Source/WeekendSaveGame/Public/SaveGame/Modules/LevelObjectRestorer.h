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
#include "HAL/IConsoleManager.h"
#include "SaveGame/SaveGameModule.h"

#include "LevelObjectRestorer.generated.h"

class USceneComponent;
class AActor;

///////////////////////////////////////////////////////////////////////////////////////

/** Implementation detail of @USaveGameModule_LevelObjects. */
USTRUCT()
struct WEEKENDSAVEGAME_API FLevelObjectSaveGameState
{
	GENERATED_BODY()

public:
	UPROPERTY(SaveGame, VisibleAnywhere, Category = "SaveGame")
	int32 ByteDataSize = 0;

	UPROPERTY(SaveGame)
	TArray<uint8> ByteData = {};

	UPROPERTY(SaveGame)
	bool bUsesCustomUniqueObjectId = false;
};

/** Behavior in case of ObjectId conflicts when upgrading a saved state to a new module version. */
UENUM()
enum class ELevelObjectRestorerConflictResolutionPolicy : uint8
{
	KeepExistingDiscardConflicting,
	DiscardExistingKeepConflicting,
	DoNotResolveKeepBoth
};

///////////////////////////////////////////////////////////////////////////////////////

/**
 * Saves and restores objects (Actors, Components or UObjects) that are part of a level.
 * By default, objects are matched by their PathName. Changing the name in the world outliner can break data of older save games.
 * Runtime spawned objects (like GameMode, GameState, ...) MUST provide a CustomUniqueObjectId to make them identifiable.
 * @requires UModularSaveGame
 */
UCLASS(DisplayName = "Level Objects", Config = "Game", DefaultConfig)
class WEEKENDSAVEGAME_API ULevelObjectRestorer : public USaveGameModule
{
	GENERATED_BODY()

public:
	/** See @UpgradeSaveGameModule. */
	enum EModuleVersion : int32
	{
		ModuleVersion_Initial = 0,
		ModuleVersion_WithSafeUniqueObjectId = 1,
		ModuleVersion_WithSafeUniqueObjectId_Hotfix = 2,

		// ----------- Do NOT touch hardcoded versions below! -----------
		ModuleVersion_LastPlusOne, // Automatically set to one higher than the last custom entry.
		ModuleVersion_Current = ModuleVersion_LastPlusOne - 1 // Automatically set to the last custom entry.
	};

	ULevelObjectRestorer()
	{
		DefaultModuleName = "LevelObjectRestorer";
		ModuleVersion = ModuleVersion_Current;
	}

	/**
	 * Registers an object nested somewhere within a world to save and restore it's data from the @UModularSaveGame.
	 * By default, objects immediately restore their data upon registration (@bImmediatelyRestoreIfPossible).
	 * Only saves and restores properties with the "SaveGame" specifier. Does not automatically save and restore sub-objects.
	 * Objects that are not uniquely identifiable via the objects PathName (like runtime spawned objects) MUST provide a CustomUniqueObjectId.
	 */
	void RegisterLevelObject(UObject& Object, TOptional<FString> CustomUniqueObjectId = {}, bool bImmediatelyRestoreIfPossible = true);
	void RegisterLevelObjectWithTransform(AActor& Actor, TOptional<FString> CustomUniqueObjectId = {}, bool bImmediatelyRestoreIfPossible = true);
	void RegisterLevelObjectWithTransform(USceneComponent& SceneComponent, TOptional<FString> CustomUniqueObjectId = {}, bool bImmediatelyRestoreIfPossible = true);

	/**
	 * Deregisters an previously registered object. This should be called when the object dies.
	 * By default, the saved object state will be updated and kept in the @UModularSaveGame and can be restored once the object registers again (@bKeepObjectState).
	 * Objects that are not uniquely identifiable via the objects PathName (like runtime spawned objects) MUST provide a CustomUniqueObjectId.
	 */
	void UnregisterLevelObject(UObject& Object, TOptional<FString> CustomUniqueObjectId = {}, bool bKeepObjectState = true);
	void UnregisterLevelObjectWithTransform(AActor& Actor, TOptional<FString> CustomUniqueObjectId = {}, bool bKeepObjectState = true);
	void UnregisterLevelObjectWithTransform(USceneComponent& SceneComponent, TOptional<FString> CustomUniqueObjectId = {}, bool bKeepObjectState = true);

	/** @returns the saved state of the object with the given UniqueObjectId, or nullptr. */
	const FLevelObjectSaveGameState* FindObjectState(const FString& UniqueObjectId) const;

	/** Constructs a unique object id for given object based on the objects PathName, with some adjustments to the world-path prefix. */
	FString MakeSafeUniqueObjectId(const UObject& Object) const;

	/** Redirects for any UniqueObjectId's of upcoming registering objects. Can also replace substring of UniqueObjectId's. */
	void RegisterUniqueObjectIdRedirects(const TMap<FString, FString>& AdditionalUniqueObjectIdRedirects);

	// - USaveGameModule
	virtual void Serialize(FArchive& Ar) override;
	virtual void BeginDestroy() override;
#if WITH_EDITOR
	virtual void AnalyzeAndReportModuleComposition(FMessageLog& MessageLog) const override;
#endif
	// --

protected:
	/** Conflict resolution behavior when upgrading the SaveGameModule to a newer version. */
	UPROPERTY(Transient, Config, EditDefaultsOnly, Category = "SaveGame")
	ELevelObjectRestorerConflictResolutionPolicy ConflictResolutionPolicy = ELevelObjectRestorerConflictResolutionPolicy::KeepExistingDiscardConflicting;

	UPROPERTY(Transient, VisibleAnywhere, meta = (DisplayThumbnail = "false"), Category = "SaveGame")
	TSet<TWeakObjectPtr<UObject>> SimpleRegisteredObjects = {};
	UPROPERTY(Transient, VisibleAnywhere, meta = (DisplayThumbnail = "false"), Category = "SaveGame")
	TSet<TWeakObjectPtr<UObject>> RegisteredObjectsWithTransform = {};
	UPROPERTY(Transient, VisibleAnywhere, meta = (DisplayThumbnail = "false"), Category = "SaveGame")
	TMap<TWeakObjectPtr<UObject>, FString> UniqueIdsOfRegisteredObjects = {};
	UPROPERTY(Transient, VisibleAnywhere, Category = "SaveGame")
	TMap<FString, FString> UniqueObjectIdRedirects = {};

	UPROPERTY(SaveGame, VisibleAnywhere, Category = "SaveGame")
	TMap<FString, FLevelObjectSaveGameState> ObjectStates;

	virtual void SaveObjectToState(UObject& Object, bool bSaveTransform, FLevelObjectSaveGameState& InOutState) const;
	virtual void RestoreObjectFromState(const FLevelObjectSaveGameState& State, bool bRestoreTransform, UObject& InOutObject) const;

	virtual FTransform GetObjectTransform(UObject& Object) const;
	virtual void SetObjectTransform(UObject& Object, const FTransform& Transform) const;

	virtual void UpgradeSaveGameModule();

private:
	/** Debugging option to log which (restored) object states were not claimed on this module. */
	UPROPERTY(Config)
	bool bReportUnclaimedObjectStatesOnDestruction = false;
	TSet<FString> ClaimedUniqueObjectIds = {};

	void ReportUnclaimedObjectStates() const;
};
