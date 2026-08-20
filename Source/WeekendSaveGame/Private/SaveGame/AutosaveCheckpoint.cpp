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
#include "Engine/World.h"
#include "SaveGame/AutosaveCheckpointRegistry.h"
#include "SaveGame/ModularSaveGame.h"
#include "SaveGame/SaveGameService.h"
#include "SaveGame/Modules/SaveGameModule_PlayerStart.h"

#if WITH_EDITOR
#include "Logging/MessageLog.h"
#include "Misc/DataValidation.h"
#include "Misc/UObjectToken.h"
#endif

DEFINE_LOG_CATEGORY(LogAutosaveCheckpoint);

AAutosaveCheckpoint::AAutosaveCheckpoint(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = false;

#if WITH_EDITORONLY_DATA
	GetCapsuleComponent()->bDrawOnlyIfSelected = false;

	bIsSpatiallyLoaded = false;
	bIsMainWorldOnly = true;

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

#if WITH_EDITOR
	UpdateCheckpointNameTextRenderer();
#endif
}

void AAutosaveCheckpoint::BeginPlay()
{
	Super::BeginPlay();

	const AAutosaveCheckpointRegistry* CheckpointRegistry = AAutosaveCheckpointRegistry::TryFind(GetWorld());
	checkf(IsValid(CheckpointRegistry), TEXT("World with AutosaveCheckpoint does not contain a CheckpointRegistry actor"));

	// If this check triggers, run the (editor) validators on the CheckpointRegistry actor again and save the asset!
	checkf(CheckpointRegistry->IsCheckpointRegistered(*this), TEXT("%s (\"%s\") is not registered in the CheckpointRegistry"), *GetName(), *PlayerStartTag.ToString());
}

void AAutosaveCheckpoint::BeginDestroy()
{
#if WITH_EDITOR
	if (UWorld* World = GetWorld(); IsValid(World) && !World->IsGameWorld())
	{
		if (AAutosaveCheckpointRegistry* CheckpointRegistry = AAutosaveCheckpointRegistry::TryFind(World))
		{
			CheckpointRegistry->UnregisterCheckpoint(*this);
		}
	}
#endif

	Super::BeginDestroy();
}

#if WITH_EDITOR
void AAutosaveCheckpoint::CheckForErrors()
{
	Super::CheckForErrors();

	if (PlayerStartTag.IsNone())
	{
		FMessageLog("MapCheck").Error()
			->AddToken(FUObjectToken::Create(this))
			->AddText(INVTEXT("PlayerStartTag must be unique and not empty! It is used to uniquely identify this checkpoint."));
	}

	AAutosaveCheckpointRegistry* CheckpointRegistry = AAutosaveCheckpointRegistry::TryFind(GetWorld());
	if (!IsValid(CheckpointRegistry))
	{
		FMessageLog("MapCheck").Error()
			->AddToken(FUObjectToken::Create(this))
			->AddText(INVTEXT("Could not find AutosaveCheckpointRegistry in a world with AutosaveCheckpoints."))
			->AddToken(FActionToken::Create(FText::FromString("Spawn registry"), FText(),
				FOnActionTokenExecuted::CreateUObject(this, &ThisClass::SummonAutosaveCheckpointRegistry), true));
	}
	else if (!CheckpointRegistry->IsCheckpointRegistered(*this))
	{
		FMessageLog("MapCheck").Error()
			->AddToken(FUObjectToken::Create(this))
			->AddText(INVTEXT("AutosaveCheckpoint is not registered in AutosaveCheckpointRegistry."))
			->AddToken(FActionToken::Create(FText::FromString("Register checkpoint"), FText(),
			FOnActionTokenExecuted::CreateWeakLambda(this, [this]()
			{
				if (AAutosaveCheckpointRegistry* CheckpointRegistry = AAutosaveCheckpointRegistry::TryFind(GetWorld()))
				{
					CheckpointRegistry->RegisterCheckpointWithDialog(this);
				}
			}), true));
	}
}

EDataValidationResult AAutosaveCheckpoint::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result{Super::IsDataValid(Context)};
	if (!IsTemplate())
	{
		if (PlayerStartTag.IsNone())
		{
			Result = CombineDataValidationResults(Result, EDataValidationResult::Invalid);
			Context.AddMessage(EMessageSeverity::Error)
				->AddToken(FUObjectToken::Create(this))
				->AddText(INVTEXT("PlayerStartTag must be unique and not empty! It is used to uniquely identify this checkpoint."));
		}

		const AAutosaveCheckpointRegistry* CheckpointRegistry = AAutosaveCheckpointRegistry::TryFind(GetWorld());
		if (!IsValid(CheckpointRegistry) || !CheckpointRegistry->IsCheckpointRegistered(*this))
		{
			Result = CombineDataValidationResults(Result, EDataValidationResult::Invalid);
			if (IsValid(CheckpointRegistry) && CheckpointRegistry->IsCheckpointRegistered(PlayerStartTag))
			{
				Context.AddMessage(EMessageSeverity::Error)
					->AddToken(FUObjectToken::Create(this))
					->AddText(INVTEXT("failed to register with AutosaveCheckpointRegistry because PlayerStartTag \""))
					->AddText(FText::FromName(PlayerStartTag))
					->AddText(INVTEXT("\" is already reserved by "))
					->AddToken(FUObjectToken::Create(CheckpointRegistry->GetRegisteredCheckpoint(PlayerStartTag)));
			}
			else
			{
				Context.AddMessage(EMessageSeverity::Error)
					->AddToken(FUObjectToken::Create(this))
					->AddText(INVTEXT("Checkpoint is not registered in AutosaveCheckpointRegistry."));
			}
		}
	}
	return Result;
}

void AAutosaveCheckpoint::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	if (PropertyChangedEvent.GetMemberPropertyName() == GET_MEMBER_NAME_CHECKED(APlayerStart, PlayerStartTag))
	{
		UpdateCheckpointNameTextRenderer();
		if (AAutosaveCheckpointRegistry* CheckpointRegistry = AAutosaveCheckpointRegistry::TrySummon(GetWorld()))
		{
			CheckpointRegistry->UpdateRegisteredCheckpointName(*this);
		}
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif

void AAutosaveCheckpoint::RequestAutosaveHere()
{
	check(GetWorld() && GetWorld()->IsGameWorld());

	if (!SaveGameModule.IsValid())
	{
		SaveGameModule = &UModularSaveGame::SummonModule<USaveGameModule_PlayerStart>(*this);
	}

	SaveGameModule->PlayerStartTag = PlayerStartTag.ToString();
	SaveGameModule->WorldCoordinates = GetActorTransform();

	UE_LOG(LogAutosaveCheckpoint, Log, TEXT("Requesting autosave at %s with PlayerStartTag: %s"), *GetName(), *PlayerStartTag.ToString());
	USaveGameService& SaveGameService = UseGameService<USaveGameService>();
	const FAsyncSaveGameHandle Handle = SaveGameService.RequestAutosave("AAutosaveCheckpoint::RequestAutosaveHere @ " + PlayerStartTag.ToString());
	if (Handle.IsValid())
	{
		HandleGameWasSavedHere();
	}
}

#if WITH_EDITOR
void AAutosaveCheckpoint::SummonAutosaveCheckpointRegistry()
{
	if (UWorld* World = GetWorld(); IsValid(World) && !World->IsGameWorld())
	{
		AAutosaveCheckpointRegistry::TrySummon(World);
	}
}
#endif

void AAutosaveCheckpoint::HandleGameWasSavedHere_Implementation()
{
	// Implement in child class.
}

void AAutosaveCheckpoint::UpdateCheckpointNameTextRenderer()
{
#if WITH_EDITORONLY_DATA
	CheckpointNameRenderer->SetText(FText::FromString("-- Checkpoint --\n" + PlayerStartTag.ToString()));
#endif
}
