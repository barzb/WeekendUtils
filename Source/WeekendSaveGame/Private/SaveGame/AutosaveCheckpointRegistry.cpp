///////////////////////////////////////////////////////////////////////////////////////
/// Copyright (C) by Benjamin Barz and contributors. See file: CREDITS.md
///
/// This file is part of the WeekendUtils UE5 Plugin.
///
/// Distributed under the MIT License. See file: LICENSE.md
///
///////////////////////////////////////////////////////////////////////////////////////

#include "SaveGame/AutosaveCheckpointRegistry.h"

#include "EngineUtils.h"
#include "MeshElementCollector.h"
#include "PrimitiveDrawingUtils.h"
#include "Engine/World.h"
#include "SaveGame/AutosaveCheckpoint.h"
#include "SaveGame/AutosaveCheckpointComponent.h"

#if WITH_EDITOR
#include "Editor.h"
#include "FileHelpers.h"
#include "Logging/MessageLog.h"
#include "Misc/DataValidation.h"
#include "Misc/MessageDialog.h"
#include "Misc/UObjectToken.h"
#endif

AAutosaveCheckpointRegistry::AAutosaveCheckpointRegistry()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	bGenerateOverlapEventsDuringLevelStreaming = false;
	bRelevantForLevelBounds = false;
	bEnableAutoLODGeneration = false;
	bReplicates = false;
#if WITH_EDITORONLY_DATA
	bIsSpatiallyLoaded = false;
#endif

	SetHidden(false);
	SetReplicatingMovement(false);
	SetCanBeDamaged(false);

	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("RootComponent")));
	GetRootComponent()->SetMobility(EComponentMobility::Static);

#if WITH_EDITORONLY_DATA
	if (VisualizationComponent = CreateOptionalDefaultSubobject<UAutosaveCheckpointDebugDrawComponent>(TEXT("DebugDrawComponent")); VisualizationComponent)
	{
		VisualizationComponent->SetupAttachment(GetRootComponent());
	}
#endif
}

AAutosaveCheckpointRegistry* AAutosaveCheckpointRegistry::TryFind(UWorld* World)
{
	if (!IsValid(World))
		return nullptr;

	for (AAutosaveCheckpointRegistry* Actor : TActorRange<AAutosaveCheckpointRegistry>(World))
		return Actor;

	return nullptr;
}

AAutosaveCheckpointRegistry* AAutosaveCheckpointRegistry::TrySummon(UWorld* World)
{
	if (!IsValid(World))
		return nullptr;

	if (AAutosaveCheckpointRegistry* FoundRegistry = TryFind(World))
		return FoundRegistry;

#if WITH_EDITOR
	if (World->bIsTearingDown || World->IsGameWorld())
		return nullptr;

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AAutosaveCheckpointRegistry* RegistryActor = World->SpawnActor<AAutosaveCheckpointRegistry>(Params);

	RegistryActor->SetFolderPath("AutosaveCheckpoints");
	RegistryActor->ListenToSpawnedActors();
	RegistryActor->Repopulate();

	return RegistryActor;
#else
	return nullptr;
#endif
}

#if WITH_EDITOR

void AAutosaveCheckpointRegistry::RegisterCheckpoint(AAutosaveCheckpoint& Checkpoint)
{
	if (IsCheckpointRegistered(Checkpoint))
		return;

	const FName CheckpointTag = Checkpoint.PlayerStartTag;
	if (CheckpointTag.IsNone())
	{
		HandleUnnamedCheckpoint(Checkpoint);
		return;
	}

	if (RegisteredCheckpoints.Contains(CheckpointTag) && RegisteredCheckpoints[CheckpointTag].IsValid())
	{
		HandleDuplicateCheckpoint(*RegisteredCheckpoints[CheckpointTag].Get(), Checkpoint);
		return;
	}

	RegisteredCheckpoints.FindOrAdd(CheckpointTag) = &Checkpoint;
	ValidateAllCheckpointsDelayed();
}

void AAutosaveCheckpointRegistry::RegisterCheckpointWithDialog(AAutosaveCheckpoint* Checkpoint)
{
	if (!IsValid(Checkpoint))
	{
		FMessageDialog::Open(EAppMsgCategory::Error, EAppMsgType::Ok, FText::FromString("Could not register checkpoint: invalid checkpoint!"));
		return;
	}

	if (IsCheckpointRegistered(*Checkpoint))
	{
		FMessageDialog::Open(EAppMsgCategory::Info, EAppMsgType::Ok, FText::FromString("Could not register checkpoint: already registered!"));
		return;
	}

	const FName CheckpointTag = Checkpoint->PlayerStartTag;
	if (CheckpointTag.IsNone())
	{
		HandleUnnamedCheckpoint(*Checkpoint);
		FMessageDialog::Open(EAppMsgCategory::Error, EAppMsgType::Ok, FText::FromString("Could not register checkpoint: invalid PlayerStartTag!"));
		return;
	}

	if (RegisteredCheckpoints.Contains(CheckpointTag) && RegisteredCheckpoints[CheckpointTag].IsValid())
	{
		HandleDuplicateCheckpoint(*RegisteredCheckpoints[CheckpointTag].Get(), *Checkpoint);
		FMessageDialog::Open(EAppMsgCategory::Error, EAppMsgType::Ok, FText::FromString("Could not register checkpoint: PlayerStartTag is not unique!"));
		return;
	}

	RegisteredCheckpoints.FindOrAdd(CheckpointTag) = Checkpoint;
	FMessageDialog::Open(EAppMsgCategory::Success, EAppMsgType::Ok, FText::FromString("Checkpoint was successfully registered."));

	ValidateAllCheckpointsDelayed();
}

void AAutosaveCheckpointRegistry::UnregisterCheckpoint(const AAutosaveCheckpoint& Checkpoint)
{
	if (!IsCheckpointRegistered(Checkpoint))
		return;

	const FName CheckpointTag = Checkpoint.PlayerStartTag;
	RegisteredCheckpoints.Remove(CheckpointTag);

	ValidateAllCheckpointsDelayed();
}

void AAutosaveCheckpointRegistry::UpdateRegisteredCheckpointName(AAutosaveCheckpoint& Checkpoint)
{
	if (IsCheckpointRegistered(Checkpoint))
	{
		UnregisterCheckpoint(Checkpoint);
	}

	if (InvalidCheckpoints.Contains(&Checkpoint))
	{
		InvalidCheckpoints.Remove(&Checkpoint);
	}

	RegisterCheckpoint(Checkpoint);
}

#endif

bool AAutosaveCheckpointRegistry::IsCheckpointRegistered(const AAutosaveCheckpoint& Checkpoint) const
{
	const FName CheckpointTag = Checkpoint.PlayerStartTag;
	TSoftObjectPtr<AAutosaveCheckpoint> const* RegisteredCheckpoint = RegisteredCheckpoints.Find(CheckpointTag);
	if (RegisteredCheckpoint && RegisteredCheckpoint->Get() == &Checkpoint)
		return true;

	return false;
}

bool AAutosaveCheckpointRegistry::IsCheckpointRegistered(FName CheckpointTag) const
{
	return RegisteredCheckpoints.Contains(CheckpointTag);
}

AAutosaveCheckpoint* AAutosaveCheckpointRegistry::GetRegisteredCheckpoint(FName CheckpointTag) const
{
	if (TSoftObjectPtr<AAutosaveCheckpoint> const* FoundCheckpoint = RegisteredCheckpoints.Find(CheckpointTag))
		return FoundCheckpoint->Get();

	return nullptr;
}

void AAutosaveCheckpointRegistry::PostLoad()
{
	Super::PostLoad();

#if WITH_EDITOR
	if (!IsTemplate() && !GetWorld()->IsGameWorld())
	{
		ActorSpawnedDelegateHandle = GetWorld()->AddOnActorSpawnedHandler(FOnActorSpawned::FDelegate::CreateUObject(this, &ThisClass::HandleActorSpawned));
	}
#endif
}

void AAutosaveCheckpointRegistry::BeginDestroy()
{
#if WITH_EDITOR
	if (GetWorld() && ActorSpawnedDelegateHandle.IsValid())
	{
		GetWorld()->RemoveOnActorSpawnedHandler(ActorSpawnedDelegateHandle);
		ActorSpawnedDelegateHandle.Reset();
	}

	if (GEditor && GEditor->IsTimerManagerValid())
	{
		TSharedRef<FTimerManager> TimerManager = GEditor->GetTimerManager();
		TimerManager->ClearAllTimersForObject(this);
	}
#endif

	Super::BeginDestroy();
}

#if WITH_EDITOR

void AAutosaveCheckpointRegistry::CheckForErrors()
{
	Super::CheckForErrors();

	MessageLogCategory = "MapCheck";

	ValidateAllCheckpoints();
}

EDataValidationResult AAutosaveCheckpointRegistry::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result{Super::IsDataValid(Context)};
	if (!IsTemplate())
	{
		if (InvalidCheckpoints.Num() > 0)
		{
			Result = CombineDataValidationResults(Result, EDataValidationResult::Invalid);
			Context.AddMessage(EMessageSeverity::Error)
				->AddToken(FUObjectToken::Create(this))
				->AddText(INVTEXT("contains invalid AutosaveCheckpoints. Please resolve all issues and then [RunValidation] again!"));
		}
	}
	return Result;
}

void AAutosaveCheckpointRegistry::RunValidation()
{
	// Show all warnings again if validation is run manually:
	bHasBeenWarnedAboutInvalidCheckpoints = false;
	bHasBeenWarnedAboutUnconfirmedCheckpoints = false;

	static int32 PageCounter = 1;
	FMessageLog(MessageLogCategory).NewPage(FText::FromString("AutosaveCheckpoints.Validate (" + FString::FromInt(PageCounter++) + ")"));

	ValidateAllCheckpoints();
}

void AAutosaveCheckpointRegistry::DestroyInvalidCheckpoints()
{
	// Remove dead pointers, we can't destroy those:
	InvalidCheckpoints.RemoveAll([](const TSoftObjectPtr<AAutosaveCheckpoint>& Checkpoint){ return !Checkpoint.IsValid(); });
	if (InvalidCheckpoints.IsEmpty())
	{
		FMessageDialog::Open(EAppMsgCategory::Info, EAppMsgType::Ok, FText::FromString("No invalid checkpoints. Nothing has been destroyed."));
		return;
	}

	// Ask warningly:
	const int32 NumInvalidCheckpoints = InvalidCheckpoints.Num();
	const EAppReturnType::Type Choice = FMessageDialog::Open(EAppMsgCategory::Warning, EAppMsgType::OkCancel, 
		FText::FromString("You are about to delete " + FString::FromInt(NumInvalidCheckpoints) + " checkpoints.\n- "
			+ FString::JoinBy(InvalidCheckpoints, TEXT("\n- "), UE_PROJECTION_MEMBER(TSoftObjectPtr<AAutosaveCheckpoint>, ToString))));

	if (Choice != EAppReturnType::Ok)
		return;

	// DESTROY:
	while (InvalidCheckpoints.Num() > 0)
	{
		DestroyCheckpoint(InvalidCheckpoints.Pop().Get());
	}

	FMessageDialog::Open(EAppMsgCategory::Info, EAppMsgType::Ok, 
		FText::FromString(FString::FromInt(NumInvalidCheckpoints) + " invalid checkpoints have been destroyed."));

	UEditorLoadingAndSavingUtils::SaveDirtyPackagesWithDialog(true, false);
}

void AAutosaveCheckpointRegistry::ResetRegistry()
{
	const EAppReturnType::Type Choice = FMessageDialog::Open(EAppMsgCategory::Warning, EAppMsgType::YesNo, 
		FText::FromString("This will clear & re-register all autosave checkpoints without running validation for removed/new ones. Are you sure?"));
	if (Choice != EAppReturnType::Yes)
		return;

	SetFolderPath("AutosaveCheckpoints");
	Repopulate();
}

void AAutosaveCheckpointRegistry::ConfirmCurrentCheckpoints()
{
	const EAppReturnType::Type Choice = FMessageDialog::Open(EAppMsgCategory::Warning, EAppMsgType::OkCancel, 
		FText::FromString("Confirming the current list of checkpoints will use it as future reference for removed/added checkpoints."));
	if (Choice != EAppReturnType::Ok)
		return;

	TArray<FName> StandaloneCheckpointTags;
	RegisteredCheckpoints.GetKeys(OUT StandaloneCheckpointTags);
	LastConfirmedCheckpointTags = TSet(StandaloneCheckpointTags);

	Modify();
	FMessageDialog::Open(EAppMsgCategory::Info, EAppMsgType::Ok, FText::FromString("Checkpoints have been confirmed."));
	UEditorLoadingAndSavingUtils::SaveDirtyPackagesWithDialog(true, false);
	bHasBeenWarnedAboutUnconfirmedCheckpoints = false;
}

void AAutosaveCheckpointRegistry::ValidateAllCheckpointsDelayed()
{
	if (GEditor && GEditor->IsTimerManagerValid())
	{
		TSharedRef<FTimerManager> TimerManager = GEditor->GetTimerManager();
		TimerManager->ClearAllTimersForObject(this);
		TimerManager->SetTimerForNextTick(this, &ThisClass::ValidateAllCheckpoints);
	}
}

void AAutosaveCheckpointRegistry::ValidateAllCheckpoints()
{
	CheckAgainstLastConfirmedCheckpoints();
	CheckForUnregisteredCheckpoints();
	WarnAboutInvalidCheckpoints();

	FMessageLog(MessageLogCategory).Notify(INVTEXT("AutosaveCheckpointRegistry reported some issues"), EMessageSeverity::Warning);

	if (!IsRunningCommandlet())
	{
		UEditorLoadingAndSavingUtils::SaveDirtyPackagesWithDialog(true, false);
	}
}

void AAutosaveCheckpointRegistry::Repopulate()
{
	RegisteredCheckpoints.Empty();
	InvalidCheckpoints.Empty();
	bHasBeenWarnedAboutInvalidCheckpoints = false;

	// Register all existing checkpoints:
	for (AAutosaveCheckpoint* Checkpoint : TActorRange<AAutosaveCheckpoint>(GetWorld()))
	{
		RegisterCheckpoint(*Checkpoint);
	}

	Modify();
	ValidateAllCheckpointsDelayed();
}

void AAutosaveCheckpointRegistry::CheckAgainstLastConfirmedCheckpoints()
{
	if (bHasBeenWarnedAboutUnconfirmedCheckpoints)
		return;

	TSet<FName> MissingCheckpointTags, AddedCheckpointTags;
	for (const FName& LastConfirmedCheckpointTag : LastConfirmedCheckpointTags)
	{
		if (IsCheckpointRegistered(LastConfirmedCheckpointTag))
			continue;

		MissingCheckpointTags.Add(LastConfirmedCheckpointTag);
		FMessageLog(MessageLogCategory).Error()
			->AddToken(FTextToken::Create(FText::FromString(
				"Previously confirmed AutosaveCheckpoint (PlayerStartTag=\"" + LastConfirmedCheckpointTag.ToString() + "\") is now missing from CheckpointRegistry."))) 
			->AddToken(FActionToken::Create(FText::FromString("Unregister checkpoint"), FText(),
		FOnActionTokenExecuted::CreateUObject(this, &ThisClass::RemoveConfirmedCheckpoint, LastConfirmedCheckpointTag), true));
	}

	for (const TPair<FName, TSoftObjectPtr<AAutosaveCheckpoint>>& Itr : RegisteredCheckpoints)
	{
		if (LastConfirmedCheckpointTags.Contains(Itr.Key))
			continue;

		AddedCheckpointTags.Add(Itr.Key);
		FMessageLog(MessageLogCategory).Error()
			->AddToken(FUObjectToken::Create(Itr.Value.Get()))
			->AddToken(FTextToken::Create(FText::FromString(
				"A new AutosaveCheckpoint (PlayerStartTag=\"" + Itr.Key.ToString() + "\") was found that has not been confirmed in the CheckpointRegistry, yet."))) 
			->AddToken(FActionToken::Create(FText::FromString("Confirm new checkpoint"), FText(),
		FOnActionTokenExecuted::CreateUObject(this, &ThisClass::ConfirmNewCheckpoint, Itr.Key), true));
	}

	if (MissingCheckpointTags.IsEmpty() && AddedCheckpointTags.IsEmpty())
		return;

	if (IsRunningCommandlet())
		return;

	FMessageDialog::Open(EAppMsgCategory::Info, EAppMsgType::Ok, FText::FromString(FString::Printf(
		TEXT("When checking against the list of last confirmed checkpoints, the following difference was detected:\n")
		TEXT("[%d Missing checkpoints]:\n- %s")
		TEXT("[%d New checkpoints:]\n- %s")
		TEXT("\nIf this is intentional, click the [ConfirmCurrentCheckpoints] button on the AutosaveCheckpointRegistry.")
		TEXT("\nSee MessageLog for more information and fix suggestions."),
		MissingCheckpointTags.Num(), *FString::JoinBy(MissingCheckpointTags, TEXT("\n- "), UE_PROJECTION_MEMBER(FName, ToString)),
		AddedCheckpointTags.Num(), *FString::JoinBy(AddedCheckpointTags, TEXT("\n- "), UE_PROJECTION_MEMBER(FName, ToString))
	)));

	bHasBeenWarnedAboutUnconfirmedCheckpoints = true;
}

void AAutosaveCheckpointRegistry::CheckForUnregisteredCheckpoints()
{
	for (AAutosaveCheckpoint* Checkpoint : TActorRange<AAutosaveCheckpoint>(GetWorld()))
	{
		if (IsCheckpointRegistered(*Checkpoint))
			continue;

		if (InvalidCheckpoints.Contains(Checkpoint))
			continue;

		FMessageLog(MessageLogCategory).Error()
			->AddToken(FUObjectToken::Create(this))
			->AddToken(FTextToken::Create(FText::FromString("found an unregistered AutosaveCheckpoint"))) 
			->AddToken(FUObjectToken::Create(Checkpoint))
			->AddToken(FActionToken::Create(FText::FromString("Register checkpoint"), FText(),
			FOnActionTokenExecuted::CreateWeakLambda(this, [this, Checkpoint = MakeWeakObjectPtr(Checkpoint)]()
			{
				if (Checkpoint.IsValid())
				{
					RegisterCheckpoint(*Checkpoint.Get());
				}
			}), true));
	}
}

void AAutosaveCheckpointRegistry::ConfirmNewCheckpoint(FName CheckpointTag)
{
	LastConfirmedCheckpointTags.Add(CheckpointTag);
}

void AAutosaveCheckpointRegistry::RemoveConfirmedCheckpoint(FName CheckpointTag)
{
	if (LastConfirmedCheckpointTags.Contains(CheckpointTag))
	{
		LastConfirmedCheckpointTags.Remove(CheckpointTag);
	}
}

void AAutosaveCheckpointRegistry::HandleDuplicateCheckpoint(const AAutosaveCheckpoint& CheckpointToKeep, AAutosaveCheckpoint& DuplicateCheckpoint)
{
	InvalidCheckpoints.Add(&DuplicateCheckpoint);
	bHasBeenWarnedAboutInvalidCheckpoints = false;

	FMessageLog(MessageLogCategory).Error()
		->AddToken(FUObjectToken::Create(&DuplicateCheckpoint))
		->AddToken(FTextToken::Create(FText::FromString("(duplicate) cannot be registered (PlayerStartTag=\"" + CheckpointToKeep.PlayerStartTag.ToString() + "\") because "))) 
		->AddToken(FUObjectToken::Create(&CheckpointToKeep))
		->AddToken(FTextToken::Create(FText::FromString("is already registered for that tag."))) 
		->AddToken(FActionToken::Create(FText::FromString("Delete duplicate"), FText(),
		FOnActionTokenExecuted::CreateUObject(this, &ThisClass::DestroyCheckpoint, &DuplicateCheckpoint), true));
}

void AAutosaveCheckpointRegistry::HandleUnnamedCheckpoint(AAutosaveCheckpoint& UnnamedCheckpoint)
{
	InvalidCheckpoints.Add(&UnnamedCheckpoint);
	bHasBeenWarnedAboutInvalidCheckpoints = false;

	FMessageLog(MessageLogCategory).Error()
		->AddToken(FUObjectToken::Create(&UnnamedCheckpoint))
		->AddToken(FTextToken::Create(FText::FromString("has no valid name (PlayerStartTag=\"" + UnnamedCheckpoint.PlayerStartTag.ToString() + "\").")));
}

void AAutosaveCheckpointRegistry::WarnAboutInvalidCheckpoints()
{
	// Remove dead pointers:
	InvalidCheckpoints.RemoveAll([](const TSoftObjectPtr<AAutosaveCheckpoint>& Checkpoint){ return !Checkpoint.IsValid(); });
	if (InvalidCheckpoints.IsEmpty() || bHasBeenWarnedAboutInvalidCheckpoints)
		return;

	FMessageLog(MessageLogCategory).Error()
		->AddToken(FUObjectToken::Create(this))
		->AddToken(FTextToken::Create(FText::FromString("found invalid AutosaveCheckpoints.")))
		->AddToken(FActionToken::Create(FText::FromString("Destroy all invalid checkpoints"), FText(),
		FOnActionTokenExecuted::CreateUObject(this, &ThisClass::DestroyInvalidCheckpoints), true));
}

void AAutosaveCheckpointRegistry::DestroyCheckpoint(AAutosaveCheckpoint* Checkpoint)
{
	if (!IsValid(Checkpoint))
		return;

	if (IsCheckpointRegistered(*Checkpoint))
	{
		UnregisterCheckpoint(*Checkpoint);
	}

	if (InvalidCheckpoints.Contains(Checkpoint))
	{
		InvalidCheckpoints.Remove(Checkpoint);
	}

	Checkpoint->Destroy();
}

void AAutosaveCheckpointRegistry::ListenToSpawnedActors()
{
	if (ActorSpawnedDelegateHandle.IsValid())
		return;

	ActorSpawnedDelegateHandle = GetWorld()->AddOnActorSpawnedHandler(FOnActorSpawned::FDelegate::CreateUObject(this, &ThisClass::HandleActorSpawned));
}

void AAutosaveCheckpointRegistry::HandleActorSpawned(AActor* SpawnedActor)
{
	AAutosaveCheckpoint* SpawnedCheckpoint = Cast<AAutosaveCheckpoint>(SpawnedActor);
	if (!IsValid(SpawnedCheckpoint))
		return; // Not a checkpoint.

	RegisterCheckpoint(*SpawnedCheckpoint);
}

#endif

///////////////////////////////////////////////////////////////////////////////////////
/// VISUALIZATION

UAutosaveCheckpointDebugDrawComponent::UAutosaveCheckpointDebugDrawComponent()
{
	Super::SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);

#if WITH_EDITORONLY_DATA
	SetIsVisualizationComponent(true);
#endif
	SetCanEverAffectNavigation(false);
	SetGenerateOverlapEvents(false);
	SetVisibility(true);

	bHiddenInGame = false;
	CastShadow = false;
	bUseEditorCompositing = true;
	Mobility = EComponentMobility::Static;
	bIgnoreBoundsForEditorFocus = true;
}

#if UE_ENABLE_DEBUG_DRAWING

FBoxSphereBounds UAutosaveCheckpointDebugDrawComponent::CalcBounds(const FTransform& LocalToWorld) const
{
	FBox Box{ForceInit};
	if (UWorld* World = GetWorld(); IsValid(World))
	{
		for (const AActor* Checkpoint : TActorRange<AAutosaveCheckpoint>(World))
		{
			FVector Origin, BoxExtent;
			Checkpoint->GetActorBounds(false, OUT Origin, OUT BoxExtent);
			Box += FBox(Origin, BoxExtent);
		}
	}
	return Box;
}

FPrimitiveSceneProxy* UAutosaveCheckpointDebugDrawComponent::CreateSceneProxy()
{
#if WITH_EDITOR
	class FAutosaveCheckpointRegistrySceneProxy final : public FPrimitiveSceneProxy
	{
	public:
		TWeakObjectPtr<AAutosaveCheckpointRegistry> Registry;
		explicit FAutosaveCheckpointRegistrySceneProxy(const UAutosaveCheckpointDebugDrawComponent& Component) :
		FPrimitiveSceneProxy(&Component), Registry(Component.GetOwner<AAutosaveCheckpointRegistry>()) {}
		virtual SIZE_T GetTypeHash() const override
		{
			static size_t UniquePointer;
			return reinterpret_cast<size_t>(&UniquePointer);
		}
		virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) const override
		{
			FPrimitiveViewRelevance Result;
			Result.bDrawRelevance = (Registry.IsValid() && Registry->IsSelectedInEditor());
			Result.bDynamicRelevance = true;
			Result.bShadowRelevance = IsShadowCast(View);
			Result.bEditorPrimitiveRelevance = true;
			return Result;
		}
		virtual uint32 GetMemoryFootprint() const override { return sizeof(*this) + FPrimitiveSceneProxy::GetAllocatedSize(); }
		virtual void GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector) const override
		{
			QUICK_SCOPE_CYCLE_COUNTER(STAT_AutosaveCheckpointRegistrySceneProxy_GetDynamicMeshElements);

			if (!Registry.IsValid() || !Registry->IsSelectedInEditor())
				return;

			for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
			{
				FPrimitiveDrawInterface* PDI = Collector.GetPDI(ViewIndex);
				if (!(VisibilityMap & (1 << ViewIndex)))
					continue;

				// Standalone Checkpoints:
				for (const TPair<FName, TSoftObjectPtr<AAutosaveCheckpoint>>& Itr : Registry->RegisteredCheckpoints)
				{
					if (const AAutosaveCheckpoint* Checkpoint = Itr.Value.Get(); IsValid(Checkpoint))
					{
						DrawCheckpoint(PDI, Checkpoint, FColor::Cyan);
					}
				}

				// Invalid Checkpoints:
				for (const TSoftObjectPtr<AAutosaveCheckpoint>& StandaloneCheckpoint : Registry->InvalidCheckpoints)
				{
					if (const AAutosaveCheckpoint* Checkpoint = StandaloneCheckpoint.Get(); IsValid(Checkpoint))
					{
						DrawCheckpoint(PDI, Checkpoint, FColor::Red);
					}
				}
			}
		}

	private:
		static void DrawCheckpoint(FPrimitiveDrawInterface* PDI, const AAutosaveCheckpoint* Checkpoint, const FColor& Color)
		{
			static constexpr float DrawThickness = 20.f;
			static constexpr float DrawHeight = 10000.f;
			static constexpr float DrawRadius = 200.f;
			static constexpr int32 NumCylinderSides = 8;

			const FVector Location = Checkpoint->GetActorLocation();
			float CheckpointRadius, CheckpointHalfHeight;
			Checkpoint->GetComponentsBoundingCylinder(OUT CheckpointRadius, OUT CheckpointHalfHeight, true);

			DrawWireCylinder(PDI, Location, FVector::ForwardVector, FVector::RightVector, FVector::UpVector,
				Color, DrawRadius, DrawHeight, NumCylinderSides, SDPG_World, DrawThickness);

			DrawCircle(PDI, Location, FVector::ForwardVector, FVector::RightVector, Color, DrawRadius,
				NumCylinderSides, SDPG_World, DrawThickness);
		}
	};

	return new FAutosaveCheckpointRegistrySceneProxy(*this);
#else
	return nullptr;
#endif
}

#endif
