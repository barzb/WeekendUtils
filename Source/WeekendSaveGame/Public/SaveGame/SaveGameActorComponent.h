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
#include "Components/ActorComponent.h"

#include "SaveGameActorComponent.generated.h"

class ULevelObjectRestorer;

/**
 * Registers the actor this component is attached to with the current SaveGame.
 * This means that the actor and all its components will automatically save and load
 * properties marked with the "SaveGame" specifier.
 * @note that this will only work reliably for level actors that are not spawned at runtime!
 * @note that the restoration of saved data will happen before BeginPlay (InitializeComponent).
 * @requires UModularSaveGame
 */
UCLASS(meta = (BlueprintSpawnableComponent))
class WEEKENDSAVEGAME_API USaveGameActorComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USaveGameActorComponent();

protected:
	///////////////////////////////////////////////////////////////////////////////////////
	/// CLASS CONFIG

	/** When enabled, the actors transform will be saved and restored by the SaveGame. */
	UPROPERTY(EditAnywhere, Category = "SaveGame")
	bool bRestoreActorTransform = true;

	/** When enabled, the actors components will be saved and restored by the SaveGame. */
	UPROPERTY(EditAnywhere, Category = "SaveGame")
	bool bRestoreActorComponents = true;

	/** When enabled, only components on the actor with configured ComponentTag will be saved and restored by the SaveGame. */
	UPROPERTY(EditAnywhere, Category = "SaveGame", meta = (EditCondition = "bRestoreActorComponents"))
	bool bOnlyRestoreActorComponentsWithTag = false;

	/** Only components on the actor with this ComponentTag will be saved and restored by the SaveGame. */
	UPROPERTY(EditAnywhere, Category = "SaveGame", meta = (EditCondition = "bRestoreActorComponents && bOnlyRestoreActorComponentsWithTag"))
	FString RestorableComponentTag = FString("SaveGame.Properties");

	/** When enabled, all of the actors SceneComponent transforms will be saved and restored by the SaveGame. */
	UPROPERTY(EditAnywhere, Category = "SaveGame", meta = (EditCondition = "bRestoreActorComponents"))
	bool bRestoreComponentTransforms = false;

	/** When enabled, only transforms of SceneComponents on the actor with configured ComponentTag will be saved and restored by the SaveGame. */
	UPROPERTY(EditAnywhere, Category = "SaveGame", meta = (EditCondition = "bRestoreActorComponents && bRestoreComponentTransforms"))
	bool bOnlyRestoreTransformsOfComponentsWithTag = false;

	/** Only transforms of SceneComponents on the actor with this ComponentTag will be saved and restored by the SaveGame. */
	UPROPERTY(EditAnywhere, Category = "SaveGame", meta = (EditCondition = "bRestoreActorComponents && bRestoreComponentTransforms && bOnlyRestoreTransformsOfComponentsWithTag"))
	FString RestorableComponentTransformTag = FString("SaveGame.Transform");

	/** (Optional) Redirects for any UniqueObjectId's of this actor (or any of its components). Can also replace substring of UniqueObjectId's. */
	UPROPERTY(EditAnywhere, Category = "SaveGame")
	TMap<FString, FString> ObjectIdRedirects = {};

	///////////////////////////////////////////////////////////////////////////////////////
	/// RUNTIME STATE

	UPROPERTY(Transient)
	TObjectPtr<ULevelObjectRestorer> LevelObjectRestorer = nullptr;

	///////////////////////////////////////////////////////////////////////////////////////

	// - UActorComponent
	virtual void InitializeComponent() override;
	virtual void UninitializeComponent() override;
	// --

	virtual void RegisterWithSaveGame();
	virtual void UnregisterFromSaveGame();
};
