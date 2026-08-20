///////////////////////////////////////////////////////////////////////////////////////
/// Copyright (C) by Benjamin Barz and contributors. See file: CREDITS.md
///
/// This file is part of the WeekendUtils UE5 Plugin.
///
/// Distributed under the MIT License. See file: LICENSE.md
///
///////////////////////////////////////////////////////////////////////////////////////

#include "SaveGame/SaveGameActorComponent.h"

#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "SaveGame/ModularSaveGame.h"
#include "SaveGame/Modules/LevelObjectRestorer.h"

USaveGameActorComponent::USaveGameActorComponent()
{
	bWantsInitializeComponent = true;
	PrimaryComponentTick.bCanEverTick = false;
}

void USaveGameActorComponent::InitializeComponent()
{
	Super::InitializeComponent();

	if (GetWorld() && GetWorld()->IsGameWorld())
	{
		UModularSaveGame* ModularSaveGame = UModularSaveGame::FindMutableCurrent(this);
		if (!IsValid(ModularSaveGame))
			return;

		LevelObjectRestorer = &ModularSaveGame->FindOrAddModule<ULevelObjectRestorer>();
		RegisterWithSaveGame();
	}
}

void USaveGameActorComponent::UninitializeComponent()
{
	if (GetWorld() && GetWorld()->IsGameWorld() && IsValid(LevelObjectRestorer))
	{
		UnregisterFromSaveGame();
		LevelObjectRestorer = nullptr;
	}

	Super::UninitializeComponent();
}

void USaveGameActorComponent::RegisterWithSaveGame()
{
	if (ObjectIdRedirects.Num() > 0)
	{
		LevelObjectRestorer->RegisterUniqueObjectIdRedirects(ObjectIdRedirects);
	}

	// Owning Actor:
	if (bRestoreActorTransform)
	{
		LevelObjectRestorer->RegisterLevelObjectWithTransform(*GetOwner());
	}
	else
	{
		LevelObjectRestorer->RegisterLevelObject(*GetOwner());
	}

	if (!bRestoreActorComponents)
		return;

	// Components of Owning Actor:
	GetOwner()->ForEachComponent<UActorComponent>(false, [this](UActorComponent* Component)
	{
		if (bOnlyRestoreActorComponentsWithTag && !Component->ComponentTags.Contains(RestorableComponentTag))
			return;

		USceneComponent* SceneComponent = Cast<USceneComponent>(Component);
		bool bRestoreTransform = (bRestoreComponentTransforms && IsValid(SceneComponent));
		if (bRestoreTransform && bOnlyRestoreTransformsOfComponentsWithTag &&
			!Component->ComponentTags.Contains(RestorableComponentTransformTag))
		{
			bRestoreTransform = false;
		}

		if (bRestoreTransform)
		{
			LevelObjectRestorer->RegisterLevelObjectWithTransform(*SceneComponent);
		}
		else
		{
			LevelObjectRestorer->RegisterLevelObject(*Component);
		}
	});
}

void USaveGameActorComponent::UnregisterFromSaveGame()
{
	// Owning Actor:
	if (bRestoreActorTransform)
	{
		LevelObjectRestorer->UnregisterLevelObjectWithTransform(*GetOwner());
	}
	else
	{
		LevelObjectRestorer->UnregisterLevelObject(*GetOwner());
	}

	if (!bRestoreActorComponents)
		return;

	// Components of Owning Actor:
	GetOwner()->ForEachComponent<UActorComponent>(false, [this](UActorComponent* Component)
	{
		if (bOnlyRestoreActorComponentsWithTag && !Component->ComponentTags.Contains(RestorableComponentTag))
			return;

		USceneComponent* SceneComponent = Cast<USceneComponent>(Component);
		bool bRestoreTransform = (bRestoreComponentTransforms && IsValid(SceneComponent));
		if (bRestoreTransform && bOnlyRestoreTransformsOfComponentsWithTag &&
			!Component->ComponentTags.Contains(RestorableComponentTransformTag))
		{
			bRestoreTransform = false;
		}

		if (bRestoreTransform)
		{
			LevelObjectRestorer->UnregisterLevelObjectWithTransform(*SceneComponent);
		}
		else
		{
			LevelObjectRestorer->UnregisterLevelObject(*Component);
		}
	});
}
