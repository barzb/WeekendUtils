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
#include "Debug/DebugDrawComponent.h"
#include "GameFramework/Actor.h"

#include "AutosaveCheckpointRegistry.generated.h"

class AAutosaveCheckpoint;

/**
 * Singleton registry for registering AutosaveCheckpoint actors at runtime, on-demand.
 * Takes care of editor validation & error handling for all placed checkpoints.
 * Offers API to find checkpoints by PlayerStartTag.
 */
UCLASS(NotPlaceable,
	PrioritizeCategories = ("Autosave Checkpoint"),
	HideCategories = ("Transform", "Input", "Movement", "Collision", "Physics", "Lighting", "Rendering", "HLOD", "WorldPartition", "DataLayers", "Transformation"))
class WEEKENDSAVEGAME_API AAutosaveCheckpointRegistry : public AActor
{
	GENERATED_BODY()

	friend class UAutosaveCheckpointDebugDrawComponent;

public:
	AAutosaveCheckpointRegistry();

	/** @returns the first (and hopefully ONLY) found registry in the given world. */
	static AAutosaveCheckpointRegistry* TryFind(UWorld* World);

	/** @returns the first (and hopefully ONLY) found registry in the given world. In non-play worlds, will spawn the registry on-demand. */
	static AAutosaveCheckpointRegistry* TrySummon(UWorld* World);

#if WITH_EDITOR
	void RegisterCheckpoint(AAutosaveCheckpoint& Checkpoint);
	void RegisterCheckpointWithDialog(AAutosaveCheckpoint* Checkpoint);
	void UnregisterCheckpoint(const AAutosaveCheckpoint& Checkpoint);
	void UpdateRegisteredCheckpointName(AAutosaveCheckpoint& Checkpoint);
#endif

	bool IsCheckpointRegistered(const AAutosaveCheckpoint& Checkpoint) const;
	bool IsCheckpointRegistered(FName CheckpointTag) const;

	/** @returns the registered checkpoint with given PlayerStartTag. */
	AAutosaveCheckpoint* GetRegisteredCheckpoint(FName CheckpointTag) const;

	// - AActor
	virtual void PostLoad() override;
	virtual void BeginDestroy() override;
#if WITH_EDITOR
	virtual void CheckForErrors() override;
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
	FORCEINLINE virtual bool ActorTypeSupportsDataLayer() const override { return false; }
	FORCEINLINE virtual bool CanChangeIsSpatiallyLoadedFlag() const override { return false; }
#endif
	// --

private:
#if WITH_EDITOR
	/** Validates all AutosaveCheckpoints and throws warnings & errors if something is wrong. */
	UFUNCTION(CallInEditor, Category = "Autosave Checkpoint")
	void RunValidation();

	/** Destroys all invalid AutosaveCheckpoint actors that have been recorded (with confirmation). */
	UFUNCTION(CallInEditor, Category = "Autosave Checkpoint")
	void DestroyInvalidCheckpoints();

	/** DANGEROUS! Completely resets the registry, all registered AutosaveCheckpoints and the confirmation list. */
	UFUNCTION(CallInEditor, Category = "Autosave Checkpoint")
	void ResetRegistry();

	/** Overwrites the previous confirmation list with the currently registered AutosaveCheckpoints. Future validations will be run against this new list. */
	UFUNCTION(CallInEditor, Category = "Autosave Checkpoint")
	void ConfirmCurrentCheckpoints();

	void ValidateAllCheckpointsDelayed();
	void ValidateAllCheckpoints();

	void Repopulate();

	void CheckAgainstLastConfirmedCheckpoints();
	void CheckForUnregisteredCheckpoints();

	void ConfirmNewCheckpoint(FName CheckpointTag);
	void RemoveConfirmedCheckpoint(FName CheckpointTag);

	void HandleDuplicateCheckpoint(const AAutosaveCheckpoint& CheckpointToKeep, AAutosaveCheckpoint& DuplicateCheckpoint);
	void HandleUnnamedCheckpoint(AAutosaveCheckpoint& UnnamedCheckpoint);
	void WarnAboutInvalidCheckpoints();

	void DestroyCheckpoint(AAutosaveCheckpoint* Checkpoint);

	void ListenToSpawnedActors();
	void HandleActorSpawned(AActor* SpawnedActor);
#endif

#if WITH_EDITORONLY_DATA
	UPROPERTY(VisibleAnywhere, Category = "Visualization")
	TObjectPtr<UPrimitiveComponent> VisualizationComponent;
#endif

	/** Validation will run always against these checkpoints. Any added/removed checkpoints will cause error messages. */
	UPROPERTY(VisibleInstanceOnly, Category = "Autosave Checkpoint")
	TSet<FName> LastConfirmedCheckpointTags = {};

	UPROPERTY(VisibleInstanceOnly, Category = "Autosave Checkpoint", meta = (DisplayThumbnail = false))
	TMap<FName, TSoftObjectPtr<AAutosaveCheckpoint>> RegisteredCheckpoints = {};

	UPROPERTY(VisibleInstanceOnly, Category = "Autosave Checkpoint", meta = (DisplayThumbnail = false))
	TArray<TSoftObjectPtr<AAutosaveCheckpoint>> InvalidCheckpoints = {};

	bool bHasBeenWarnedAboutUnconfirmedCheckpoints = false;
	bool bHasBeenWarnedAboutInvalidCheckpoints = false;

#if WITH_EDITOR
	FDelegateHandle ActorSpawnedDelegateHandle = {};
	FName MessageLogCategory = "AssetCheck";
#endif
};

///////////////////////////////////////////////////////////////////////////////////////
/// VISUALIZATION

/** Internal visualization component of AutosaveCheckpointRegistry. */
UCLASS(MinimalAPI, Within = "AutosaveCheckpointRegistry")
class UAutosaveCheckpointDebugDrawComponent : public UPrimitiveComponent
{
	GENERATED_BODY()

public:
	UAutosaveCheckpointDebugDrawComponent();

#if UE_ENABLE_DEBUG_DRAWING
	// - UDebugDrawComponent
	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;
	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
	// --
#endif 
};
