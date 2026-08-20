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
#include "GameFramework/PlayerStart.h"
#include "GameService/GameServiceUser.h"

#include "AutosaveCheckpoint.generated.h"

class UTextRenderComponent;
class USaveGameModule_PlayerStart;

WEEKENDSAVEGAME_API DECLARE_LOG_CATEGORY_EXTERN(LogAutosaveCheckpoint, Log, All);

/**
 * Extension of APlayerStart that offers API to save the PlayerStartTag to a dedicated
 * SaveGameModule, so the player can be restored at the location of this actor.
 */
UCLASS(PrioritizeCategories = ("Object", "Autosave Checkpoint"))
class WEEKENDSAVEGAME_API AAutosaveCheckpoint : public APlayerStart, public FGameServiceUser
{
	GENERATED_BODY()

public:
	AAutosaveCheckpoint(const FObjectInitializer& ObjectInitializer);

	// - FGameServiceUser
	virtual FGameServiceUserConfig ConfigureGameServiceUser() const override;
	// - APlayerStart
	virtual void PostInitProperties() override;
	virtual void BeginPlay() override;
	virtual void BeginDestroy() override;
#if WITH_EDITOR
	virtual void CheckForErrors() override;
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	FORCEINLINE virtual bool CanChangeIsSpatiallyLoadedFlag() const override { return false; }
	FORCEINLINE virtual bool ActorTypeSupportsExternalDataLayer() const override { return false; }
#endif
	// --

	/** Requests an autosave and saves this checkpoint's PlayerStartTag into the ModularSaveGame, so the player can be restored here. */
	UFUNCTION(BlueprintCallable, Category = "Autosave Checkpoint")
	virtual void RequestAutosaveHere();

protected:
	/** Shared by all checkpoints, stores which checkpoint has most recently requested an autosave. */
	TWeakObjectPtr<USaveGameModule_PlayerStart> SaveGameModule = nullptr;

#if WITH_EDITOR
	UFUNCTION(CallInEditor, Category = "Autosave Checkpoint")
	void SummonAutosaveCheckpointRegistry();
#endif

	#if WITH_EDITORONLY_DATA
	UPROPERTY(VisibleAnywhere, Category = "Autosave Checkpoint")
	TObjectPtr<UTextRenderComponent> CheckpointNameRenderer;
#endif

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Autosave Checkpoint")
	void HandleGameWasSavedHere();

	void UpdateCheckpointNameTextRenderer();
};
