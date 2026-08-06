///////////////////////////////////////////////////////////////////////////////////////
/// Copyright (C) by Benjamin Barz and contributors. See file: CREDITS.md
///
/// This file is part of the WeekendUtils UE5 Plugin.
///
/// Distributed under the MIT License. See file: LICENSE.md
///
///////////////////////////////////////////////////////////////////////////////////////

#include "SaveGame/AutosaveCheckpoint.h"

#include "Components/CapsuleComponent.h"
#include "Components/TextRenderComponent.h"
#include "SaveGame/ModularSaveGame.h"
#include "SaveGame/SaveGameService.h"
#include "SaveGame/Modules/SaveGameModule_PlayerStart.h"

DEFINE_LOG_CATEGORY(LogAutosaveCheckpoint);

AAutosaveCheckpoint::AAutosaveCheckpoint(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = false;

	GetCapsuleComponent()->bDrawOnlyIfSelected = false;

#if WITH_EDITORONLY_DATA
	bIsSpatiallyLoaded = false;

	CheckpointNameRenderer = CreateDefaultSubobject<UTextRenderComponent>("Checkpoint Name Preview");
	CheckpointNameRenderer->SetupAttachment(GetRootComponent());
	CheckpointNameRenderer->SetRelativeLocation(FVector::UpVector * 200.f);
	CheckpointNameRenderer->SetRelativeRotation(FRotator(45.f, 0.f, 0.f)); // Tilt up 45deg.
	CheckpointNameRenderer->HorizontalAlignment = EHTA_Center;
	CheckpointNameRenderer->VerticalAlignment = EVRTA_TextCenter;
	CheckpointNameRenderer->WorldSize = 18;
	CheckpointNameRenderer->bHiddenInGame = true;
#endif
}

FGameServiceUserConfig AAutosaveCheckpoint::ConfigureGameServiceUser() const
{
	return FGameServiceUserConfig(this)
		.AddServiceDependency<USaveGameService>();
}

void AAutosaveCheckpoint::PostInitProperties()
{
	Super::PostInitProperties();

	UpdateCheckpointNameTextRenderer();
}

#if WITH_EDITOR
void AAutosaveCheckpoint::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	if (PropertyChangedEvent.GetMemberPropertyName() == GET_MEMBER_NAME_CHECKED(APlayerStart, PlayerStartTag))
	{
		UpdateCheckpointNameTextRenderer();
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif

void AAutosaveCheckpoint::RequestAutosaveHere()
{
	if (!SaveGameModule.IsValid())
	{
		SaveGameModule = &UModularSaveGame::SummonModule<USaveGameModule_PlayerStart>(*this);
	}

	SaveGameModule->PlayerStartTag = PlayerStartTag.ToString();
	SaveGameModule->WorldCoordinates = GetActorTransform();

	UE_LOG(LogAutosaveCheckpoint, Log, TEXT("Requesting autosave at %s with PlayerStartTag: %s"), *GetName(), *PlayerStartTag.ToString());
	UseGameService<USaveGameService>().RequestAutosave("AAutosaveCheckpoint::RequestAutosaveHere @ " + PlayerStartTag.ToString());
}

void AAutosaveCheckpoint::SetPlayerStartTag(const FName& NewPlayerStartTag)
{
	UE_LOG(LogAutosaveCheckpoint, Log, TEXT("%s changes it's PlayerStartTag from \"%s\" to \"%s\""),
		*GetName(), *PlayerStartTag.ToString(), *NewPlayerStartTag.ToString());

	PlayerStartTag = NewPlayerStartTag;
	UpdateCheckpointNameTextRenderer();
}

void AAutosaveCheckpoint::UpdateCheckpointNameTextRenderer()
{
#if WITH_EDITORONLY_DATA
	// (i) Called from PostInitProperties(), where the subobject can still be missing:
	if (!CheckpointNameRenderer)
		return;

	CheckpointNameRenderer->SetText(FText::FromString("-- Checkpoint --\n" + PlayerStartTag.ToString()));
#endif
}
