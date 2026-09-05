///////////////////////////////////////////////////////////////////////////////////////
/// Copyright (C) by Benjamin Barz and contributors. See file: CREDITS.md
///
/// This file is part of the WeekendUtils UE5 Plugin.
///
/// Distributed under the MIT License. See file: LICENSE.md
///
///////////////////////////////////////////////////////////////////////////////////////

#include "SaveGame/AutosaveCheckpointComponent.h"

#include "SaveGame/AutosaveCheckpointRegistry.h"

#if WITH_EDITOR
#include "Logging/MessageLog.h"
#include "Misc/DataValidation.h"
#include "Misc/UObjectToken.h"
#endif

UAutosaveCheckpointComponent::UAutosaveCheckpointComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	Mobility = EComponentMobility::Static;
}

void UAutosaveCheckpointComponent::RequestAutosaveAtCheckpoint()
{
	if (!LinkedCheckpoint.IsValid())
	{
		const AAutosaveCheckpointRegistry* CheckpointRegistry = AAutosaveCheckpointRegistry::TrySummon(GetWorld());
		if (AAutosaveCheckpoint* Checkpoint = CheckpointRegistry ? CheckpointRegistry->GetRegisteredCheckpoint(*GetReadableName()) : nullptr)
		{
			LinkedCheckpoint = Checkpoint;
		}
	}

	if (LinkedCheckpoint.IsValid())
	{
		LinkedCheckpoint->RequestAutosaveHere();
	}
	else
	{
		UE_LOG(LogAutosaveCheckpoint, Error, TEXT("Cannot request autosave on missing AutosaveCheckpoint at %s"), *GetPathName());
	}
}

AAutosaveCheckpoint* UAutosaveCheckpointComponent::GetAutosaveCheckpoint() const
{
	return LinkedCheckpoint.Get();
}

void UAutosaveCheckpointComponent::OnRegister()
{
	Super::OnRegister();

	// OnRegister is also called on CDO's and all kinds of temporary objects, but we don't care about them:
	if (IsTemplate() || !GetWorld() || GetWorld()->IsPreviewWorld() || !GetOwner())
		return;

#if WITH_EDITOR
void UAutosaveCheckpointComponent::CheckForErrors()
{
	Super::CheckForErrors();

	if (!LinkedCheckpoint.IsValid())
	{
		FMessageLog("MapCheck").Error()
			->AddToken(FUObjectToken::Create(this))
			->AddText(INVTEXT("LinkedAutosaveCheckpoint is invalid, but must be linked to a valid AutosaveCheckpoint actor."))
			->AddToken(FActionToken::Create(FText::FromString("Attempt fix"), FText(),
				FOnActionTokenExecuted::CreateWeakLambda(this, [this]()
				{
					AAutosaveCheckpointRegistry* CheckpointRegistry = AAutosaveCheckpointRegistry::TrySummon(GetWorld());
					if (!IsValid(CheckpointRegistry))
						return;

					if (AAutosaveCheckpoint* FoundCheckpoint = CheckpointRegistry->GetRegisteredCheckpoint(*GetReadableName()))
					{
						LinkedCheckpoint = FoundCheckpoint;
						Modify();

						FMessageLog("AssetCheck").Info()
							->AddToken(FUObjectToken::Create(this))
							->AddText(FText::FromString("was linked to"))
							->AddToken(FUObjectToken::Create(FoundCheckpoint));
					}
					else
					{
						FMessageLog("AssetCheck").Error()
							->AddToken(FUObjectToken::Create(this))
							->AddText(FText::FromString("attempted to find AutosaveCheckpoint for \"" + GetReadableName() + "\" was NOT successful."))
							->AddText(FText::FromString("Please manually link a checkpoint to the component."));
					}
				}), true));
	}
}

EDataValidationResult UAutosaveCheckpointComponent::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result{Super::IsDataValid(Context)};
	if (!IsTemplate())
	{
		if (!LinkedCheckpoint)
		{
			Result = CombineDataValidationResults(Result, EDataValidationResult::Invalid);
			Context.AddMessage(EMessageSeverity::Error)
				->AddToken(FUObjectToken::Create(GetOwner()))
				->AddToken(FUObjectToken::Create(this))
				->AddText(INVTEXT("LinkedAutosaveCheckpoint is invalid, but must be linked to a valid AutosaveCheckpoint actor"));
		}
	}
	return Result;
}
#endif

void UAutosaveCheckpointComponent::FindOrCreateCheckpoint()
{
	UAutosaveCheckpointRegistry* CheckpointRegistry = UAutosaveCheckpointRegistry::Get(this);
	if (!CheckpointRegistry)
		return;

	LinkedCheckpoint = CheckpointRegistry->FindCheckpoint(*GetReadableName());

#if WITH_EDITOR
	// Checkpoint link is broken, but the actor might still be attached to our own actor:
	if (!LinkedCheckpoint && !GetOwner()->GetIsSpatiallyLoaded())
	{
		GetOwner()->ForEachAttachedActors([this](AActor* AttachedActor) -> bool
		{
			if (AAutosaveCheckpoint* AttachedCheckpoint = Cast<AAutosaveCheckpoint>(AttachedActor))
			{
				LinkedCheckpoint = AttachedCheckpoint;
			}

			return !LinkedCheckpoint; // true = continue; false = break;
		});
	}

	// Spawn a new checkpoint in case we did not find the one that belongs to us:
	if (!LinkedCheckpoint)
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		LinkedCheckpoint = GetWorld()->SpawnActor<AAutosaveCheckpoint>(CheckpointClass, SpawnParams);
		if (LinkedCheckpoint)
		{
			CheckpointRegistry->RegisterCheckpoint(*LinkedCheckpoint);
		}
	}

	// Make sure the checkpoint is correctly attached to us:
	if (LinkedCheckpoint)
	{
		LinkedCheckpoint->SetActorLocationAndRotation(GetComponentLocation(), GetComponentRotation());
		if (GetOwner()->GetIsSpatiallyLoaded())
		{
			LinkedCheckpoint->SetFolderPath("AutosaveCheckpoints");
		}
		else
		{
			if (USceneComponent* CheckpointRoot = LinkedCheckpoint->GetRootComponent())
			{
				CheckpointRoot->Mobility = Mobility;
			}
			LinkedCheckpoint->AttachToComponent(this, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
		}
		RenameCheckpoint();
	}
#endif
}

#if WITH_EDITOR
void UAutosaveCheckpointComponent::DespawnCheckpoint()
{
	if (!LinkedCheckpoint)
		return;

	if (UAutosaveCheckpointRegistry* CheckpointRegistry = UAutosaveCheckpointRegistry::Get(this))
	{
		CheckpointRegistry->UnregisterCheckpoint(*LinkedCheckpoint);
	}

	LinkedCheckpoint->Destroy();
	LinkedCheckpoint = nullptr;
}

void UAutosaveCheckpointComponent::RespawnCheckpoint()
{
	if (IsTemplate())
		return;

	DespawnCheckpoint();
	FindOrCreateCheckpoint();
}

void UAutosaveCheckpointComponent::RenameCheckpoint()
{
	if (LinkedCheckpoint)
	{
		LinkedCheckpoint->SetPlayerStartTag(*GetReadableName());
		FActorLabelUtilities::RenameExistingActor(
			LinkedCheckpoint.Get(), MakeUniqueObjectName(GetWorld(), CheckpointClass, *(GetOwner()->GetActorLabel() + "_Checkpoint")).ToString()); 
	}
}

void UAutosaveCheckpointComponent::HandleEditorActorMoved(AActor* MovedActor)
{
	if (!LinkedCheckpoint || MovedActor->GetActorTransform().Equals(GetComponentTransform()))
		return;

	// You moved the Checkpoint, but not the component it's attached to, so we'll casually correct that for you:
	if (MovedActor == LinkedCheckpoint.Get())
	{
		SetWorldLocationAndRotation(LinkedCheckpoint->GetActorLocation(), LinkedCheckpoint->GetActorRotation());
		if (LinkedCheckpoint->IsAttachedTo(GetOwner()))
		{
			LinkedCheckpoint->AttachToComponent(this, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
			LinkedCheckpoint->SetActorRelativeTransform(FTransform::Identity);
			GetOwner()->Modify(); // Mark owning actor as dirty, so it shows up as change to be saved.
		}
	}

	// You moved the actor of this component, but not the checkpoint, so we'll casually correct that for you: 
	if (MovedActor == GetOwner())
	{
		LinkedCheckpoint->SetActorLocationAndRotation(GetComponentLocation(), GetComponentRotation());
	}
}

void UAutosaveCheckpointComponent::HandleLevelActorDeleted(AActor* DeletedActor)
{
	// Ignore the deletion of other actors:
	if (DeletedActor != GetOwner())
		return;

	// Ignore this happening to deletions that weren't directly caused by the user (i.e. implicit re-instancing from changes to the BP):
	if (!LinkedCheckpoint || IsTemplate() || GIsReinstancing ||
		GetOwner()->bIsEditorPreviewActor ||
		GetOwner()->GetClass()->GetName().StartsWith(TEXT("SKEL_")) ||
		GetOwner()->GetClass()->GetName().StartsWith(TEXT("REINST_")) ||
		GetOwner()->HasAnyFlags(RF_NewerVersionExists) ||
		GetOwner()->GetClass()->HasAnyClassFlags(CLASS_NewerVersionExists))
		return;

	// Prompt the user to also delete the linked checkpoint:
	if (FMessageDialog::Open(EAppMsgCategory::Warning, EAppMsgType::YesNo,
		INVTEXT("You are deleting an actor that is linked to an Autosave Checkpoint.\nDo you also wish to delete the linked checkpoint?"),
		INVTEXT("Delete Autosave Checkpoint?")) == EAppReturnType::Yes)
	{
		DespawnCheckpoint();
	}
}

void UAutosaveCheckpointComponent::HandleActorLabelChanged(AActor* RenamedActor)
{
	if (RenamedActor != GetOwner())
		return;

	// You might think we should just call RenameCheckpoint() here, but that would be too easy.
	// The actor label would not refresh in the scene outliner for reasons only known by gods,
	// so instead... we respawn the checkpoint with the correct name.
	if (LinkedCheckpoint.IsValid())
	{
		RespawnCheckpoint();
	}
}
#endif
