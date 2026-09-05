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
#include "UObject/Class.h"
#include "UObject/Object.h"

#include "SaveGameModule.generated.h"

/**
 * Base class for polymorphic SaveGame modules of the @UModularSaveGame.
 * Subclasses should override the @ModuleName in their constructor.
 */
UCLASS(BlueprintType, Abstract, EditInlineNew, CollapseCategories)
class WEEKENDSAVEGAME_API USaveGameModule : public UObject
{
	GENERATED_BODY()

public:
	/** Event fired before the module is being saved, before all SaveGame specified properties have been serialized. */
	DECLARE_MULTICAST_DELEGATE(FOnBeforeModuleSaved)
	FOnBeforeModuleSaved OnBeforeModuleSaved;

	/** Event fired after the module was restored, after all SaveGame specified properties have been deserialized. */
	DECLARE_MULTICAST_DELEGATE(FOnAfterModuleRestored)
	FOnAfterModuleRestored OnAfterModuleRestored;

	/** Default identifier that must be unique across all modules and is used when a module is not registered by custom name. */
	UPROPERTY(SaveGame, VisibleAnywhere, BlueprintReadWrite, Category = "SaveGame")
	FName DefaultModuleName = NAME_None;

	/** Module version for potential compatibility checks. */
	UPROPERTY(SaveGame, EditDefaultsOnly, Category = "SaveGame")
	int32 ModuleVersion = 0;

	// - UObject
	virtual void Serialize(FArchive& Ar) override;
	virtual void PostInitProperties() override;
	// --

#if WITH_EDITOR
	/** Analyzes the composition of this module and reports it to the given MessageLog. */
	virtual void AnalyzeAndReportModuleComposition(class FMessageLog& MessageLog) const {}
#endif

protected:
	/** Called before the module is being saved, before all SaveGame specified properties have been serialized. */
	virtual void PreSaveModule() { OnBeforeModuleSaved.Broadcast(); }

	/** Called after the module was restored, after all SaveGame specified properties have been deserialized. */
	virtual void PostRestoreModule() { OnAfterModuleRestored.Broadcast(); }
};

///////////////////////////////////////////////////////////////////////////////////////

inline void USaveGameModule::Serialize(FArchive& Ar)
{
	if (Ar.ArIsSaveGame && Ar.IsSaving())
	{
		PreSaveModule();
	}

	Super::Serialize(Ar);

	if (Ar.ArIsSaveGame && Ar.IsLoading())
	{
		PostRestoreModule();
	}
}

inline void USaveGameModule::PostInitProperties()
{
	Super::PostInitProperties();

	if (DefaultModuleName.IsNone())
	{
		DefaultModuleName = FName(GetClass()->GetName());
	}
}
