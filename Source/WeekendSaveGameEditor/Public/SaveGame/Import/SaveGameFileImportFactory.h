///////////////////////////////////////////////////////////////////////////////////////
/// Copyright (C) by Benjamin Barz and contributors. See file: CREDITS.md
///
/// This file is part of the WeekendUtils UE5 Plugin.
///
/// Distributed under the MIT License. See file: LICENSE.md
///////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "CoreMinimal.h"
#include "Factories/Factory.h"

#include "SaveGameFileImportFactory.generated.h"

class USaveGame;
class USaveGameFilePreset;
struct FSaveGameAnalysisReport;

/**
 * UFactory that handles drag-and-drop import of .sav files into the content browser.
 * Deserializes the file using @UModularSaveGameSerializer, creates a @USaveGameFilePreset
 * (or configured subclass), runs registered analyzers, and populates the analysis report.
 * The created asset is placed in the configured @USaveGameServiceSettings::DefaultSaveGamePresetFolder.
 */
UCLASS()
class WEEKENDSAVEGAMEEDITOR_API USaveGameFileImportFactory : public UFactory
{
	GENERATED_BODY()

public:
	USaveGameFileImportFactory();

	// - UFactory
	virtual bool FactoryCanImport(const FString& Filename) override;
	// --

protected:
	// - UFactory
	virtual UObject* FactoryCreateBinary(
		UClass* InClass, UObject* InParent, FName InName,
		EObjectFlags Flags, UObject* Context,
		const TCHAR* Type, const uint8*& Buffer,
		const uint8* BufferEnd, FFeedbackContext* Warn) override;
	// --

private:
	static USaveGame* DeserializeSaveFile(const TArray<uint8>& FileData, UObject* Outer);
};
