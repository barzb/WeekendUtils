///////////////////////////////////////////////////////////////////////////////////////
/// Copyright (C) by Benjamin Barz and contributors. See file: CREDITS.md
///
/// This file is part of the WeekendUtils UE5 Plugin.
///
/// Distributed under the MIT License. See file: LICENSE.md
///////////////////////////////////////////////////////////////////////////////////////

#include "SaveGame/Import/SaveGameFileImportFactory.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "EditorUtilityLibrary.h"
#include "Misc/MessageDialog.h"
#include "Misc/Paths.h"
#include "SaveGame/ModularSaveGame.h"
#include "SaveGame/SaveGameFilePreset.h"
#include "SaveGame/SaveGamePreset.h"
#include "SaveGame/Settings/SaveGameServiceSettings.h"

DEFINE_LOG_CATEGORY_STATIC(LogSaveGameImport, Log, All);

///////////////////////////////////////////////////////////////////////////////////////

USaveGameFileImportFactory::USaveGameFileImportFactory()
{
	bCreateNew = false;
	bEditorImport = true;
	bText = false;
	SupportedClass = USaveGameFilePreset::StaticClass();
	Formats.Add(TEXT("sav;SaveGame File"));
}

bool USaveGameFileImportFactory::FactoryCanImport(const FString& Filename)
{
	return FPaths::GetExtension(Filename).Equals(TEXT("sav"), ESearchCase::IgnoreCase);
}

UObject* USaveGameFileImportFactory::FactoryCreateBinary(
	UClass* InClass, UObject* InParent, FName InName,
	EObjectFlags Flags, UObject* Context,
	const TCHAR* Type, const uint8*& Buffer,
	const uint8* BufferEnd, FFeedbackContext* Warn)
{
	const int64 DataSize = (BufferEnd - Buffer);
	if (DataSize <= 0)
	{
		UE_LOG(LogSaveGameImport, Error, TEXT("Empty save file: %s"), *InName.ToString());
		return nullptr;
	}

	TArray<uint8> FileData;
	FileData.Append(Buffer, DataSize);

	USaveGame* TempSaveGame = DeserializeSaveFile(FileData, GetTransientPackage());
	if (!IsValid(TempSaveGame))
	{
		UE_LOG(LogSaveGameImport, Error, TEXT("Failed to deserialize save file: %s"), *InName.ToString());
		FMessageDialog::Open(EAppMsgCategory::Error, EAppMsgType::Ok, INVTEXT("Failed to read save file!"));
		return nullptr;
	}

	const USaveGameServiceSettings* Settings = GetDefault<USaveGameServiceSettings>();
	const FString AssetPrefix = Settings->DefaultSaveGamePresetAssetPrefix;
	const FString SourceFileName = InName.ToString();
	const FString AssetName = AssetPrefix + SourceFileName;

	const FString TargetFolder = Settings->DefaultSaveGamePresetFolder.Path;
	FString PackagePath = TargetFolder / AssetName;

	// Ensure unique package path by incrementing suffix:
	if (FindPackage(nullptr, *PackagePath))
	{
		int32 Suffix = 2;
		FString CandidatePath;
		do
		{
			CandidatePath = FString::Printf(TEXT("%s_%d"), *PackagePath, Suffix++);
		}
		while (FindPackage(nullptr, *CandidatePath));
		PackagePath = CandidatePath;
	}

	const FString FinalAssetName = FPaths::GetCleanFilename(PackagePath);
	UPackage* Package = CreatePackage(*PackagePath);
	Package->FullyLoad();

	const UClass* PresetClass = USaveGameFilePreset::StaticClass();
	if (!Settings->DefaultFilePresetClass.IsNull())
	{
		if (const UClass* ConfiguredClass = Settings->DefaultFilePresetClass.LoadSynchronous())
		{
			PresetClass = ConfiguredClass;
		}
	}

	USaveGameFilePreset* Preset = NewObject<USaveGameFilePreset>(Package, PresetClass, *FinalAssetName, RF_Public | RF_Standalone);
	Preset->PresetName = SourceFileName;
	Preset->SetSaveFileData(MoveTemp(FileData));

	if (const UModularSaveGame* ModularSaveGame = Cast<UModularSaveGame>(TempSaveGame))
	{
		if (const TSharedPtr<FInstancedStruct> Header = ModularSaveGame->GetInstancedHeaderData())
		{
			Preset->HeaderData = *Header;
		}
	}

	Preset->AnalysisReport = USaveGamePreset::CreateAnalysisReport(Preset->GetPresetSaveGame(), &Preset->HeaderData, SourceFileName + TEXT(".sav"));

	FAssetRegistryModule::AssetCreated(Preset);
	Package->MarkPackageDirty();

	UEditorUtilityLibrary::SyncBrowserToFolders({TargetFolder});

	UE_LOG(LogSaveGameImport, Log, TEXT("Imported save file '%s' as preset '%s' with %d analysis records."),
		*SourceFileName, *FinalAssetName, Preset->AnalysisReport.GetRecords().Num());

	return Preset;
}

USaveGame* USaveGameFileImportFactory::DeserializeSaveFile(const TArray<uint8>& FileData, UObject* Outer)
{
	const UModularSaveGameSerializer* Serializer = NewObject<UModularSaveGameSerializer>(Outer);
	USaveGame* SaveGame = nullptr;

	if (!Serializer->TryDeserializeSaveGame(FileData, OUT SaveGame))
		return nullptr;

	return SaveGame;
}
