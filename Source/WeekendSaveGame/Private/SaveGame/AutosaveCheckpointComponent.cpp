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
