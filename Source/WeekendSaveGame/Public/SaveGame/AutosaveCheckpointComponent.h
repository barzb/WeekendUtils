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
#include "AutosaveCheckpoint.h"
#include "Components/ActorComponent.h"

#include "AutosaveCheckpointComponent.generated.h"

/**
 * Component that links to a persistent @AAutosaveCheckpoint actor in the level.
 * Offers functionalities to autosave at the linked checkpoint.
 */
UCLASS(meta = (BlueprintSpawnableComponent))
class WEEKENDSAVEGAME_API UAutosaveCheckpointComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	UAutosaveCheckpointComponent();

	/** Requests an autosave and saves the linked checkpoints PlayerStartTag into the ModularSaveGame, so the player can be restored here. */
	UFUNCTION(BlueprintCallable, Category = "Autosave Checkpoint")
	void RequestAutosaveAtCheckpoint();

	/** @returns the linked AutosaveCheckpoint actor. */
	UFUNCTION(BlueprintCallable, Category = "Autosave Checkpoint")
	AAutosaveCheckpoint* GetAutosaveCheckpoint() const;

	// - UActorComponent
#if WITH_EDITOR
	virtual void CheckForErrors() override;
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
	// --

protected:
	/** Soft link to our checkpoint actor. The checkpoint is never spatially loaded, but this actor could be. */
	UPROPERTY(EditInstanceOnly, Category = "Autosave Checkpoint")
	TSoftObjectPtr<AAutosaveCheckpoint> LinkedCheckpoint = nullptr;
};
