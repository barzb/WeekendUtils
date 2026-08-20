///////////////////////////////////////////////////////////////////////////////////////
/// Copyright (C) by Benjamin Barz and contributors. See file: CREDITS.md
///
/// This file is part of the WeekendUtils UE5 Plugin.
///
/// Distributed under the MIT License. See file: LICENSE.md
///////////////////////////////////////////////////////////////////////////////////////

#include "SaveGame/SaveGameEditor.h"

#include "DesktopPlatformModule.h"
#include "Editor.h"
#include "IAssetTools.h"
#include "IDesktopPlatform.h"
#include "Factories/DataAssetFactory.h"
#include "Framework/Application/SlateApplication.h"
#include "GameService/GameServiceLocator.h"
#include "Misc/FileHelper.h"
#include "Misc/MessageDialog.h"
#include "SaveGame/ModularSaveGame.h"
#include "SaveGame/SaveGameObjectPreset.h"
#include "SaveGame/SaveGamePreset.h"
#include "SaveGame/SaveGameService.h"
#include "SaveGame/Settings/SaveGameServiceSettings.h"
#include "Subsystems/AssetEditorSubsystem.h"

namespace
{
	TStrongObjectPtr<USaveGameEditor> ShownInstance = nullptr;
}

void USaveGameEditor::OpenSaveGameEditor(const USaveGame* SaveGame)
{
	if (!GEditor)
		return;

	if (!ShownInstance.IsValid())
	{
		ShownInstance = TStrongObjectPtr(NewObject<USaveGameEditor>(GetTransientPackage(), FName("Savegame Editor")));
	}

	ShownInstance->SetSaveGame(SaveGame);

	UAssetEditorSubsystem* AssetEditorSubsystem = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
	AssetEditorSubsystem->OpenEditorForAsset<USaveGameEditor>(ShownInstance.Get());
}

void USaveGameEditor::OpenSaveGameEditorForCurrentSaveGame()
{
	const USaveGame* SaveGame = nullptr;
	if (const USaveGameService* SaveGameService = UGameServiceLocator::FindService<USaveGameService>(GEngine->GetCurrentPlayWorld()))
	{
		const FCurrentSaveGame& CurrentSaveGame = SaveGameService->GetCurrentSaveGame();
		SaveGame = CurrentSaveGame.GetPtr();
	}
	OpenSaveGameEditor(SaveGame);
}

void USaveGameEditor::ConvertToPreset()
{
	if (!SaveGame)
		return;

	const USaveGameServiceSettings* Settings = GetDefault<USaveGameServiceSettings>();
	const FString DefaultFolder = Settings->DefaultSaveGamePresetFolder.Path;
	const FString AssetPrefix = Settings->DefaultSaveGamePresetAssetPrefix;

	UDataAssetFactory* PresetFactory = NewObject<UDataAssetFactory>();
	PresetFactory->DataAssetClass = USaveGamePreset::StaticClass();
	if (UObject* CreatedAsset = IAssetTools::Get().CreateAssetWithDialog(
		AssetPrefix, DefaultFolder, USaveGamePreset::StaticClass(), PresetFactory, NAME_None, false))
	{
		USaveGameObjectPreset* CreatedPreset = Cast<USaveGameObjectPreset>(CreatedAsset);
		CreatedPreset->HeaderData = HeaderData;
		CreatedPreset->SetPresetSaveGame(*DuplicateObject<USaveGame>(SaveGame, CreatedAsset));

		const FString AssetNameWithoutPrefix = CreatedAsset->GetName().Replace(*AssetPrefix, TEXT(""));
		CreatedPreset->PresetName = AssetNameWithoutPrefix;

		UAssetEditorSubsystem* AssetEditorSubsystem = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
		AssetEditorSubsystem->OpenEditorForAsset<USaveGamePreset>(CreatedPreset);
	}
}

void USaveGameEditor::EditCurrentSaveGame()
{
	if (const USaveGameService* SaveGameService = UGameServiceLocator::FindService<USaveGameService>(GEngine->GetCurrentPlayWorld()))
	{
		const FCurrentSaveGame& CurrentSaveGame = SaveGameService->GetCurrentSaveGame();
		SetSaveGame(CurrentSaveGame.GetPtr(), FString("(Current SaveGame)"));
	}
	else
	{
		SetSaveGame(nullptr);
	}
}

void USaveGameEditor::EditSaveGameFromFile()
{
	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
	if (!DesktopPlatform)
	{
		FMessageDialog::Open(EAppMsgCategory::Error, EAppMsgType::Ok, INVTEXT("Could not load file! (Unknown desktop platform)"));
		return;
	}

	TArray<FString> FileNames;
	const bool bSuccess = DesktopPlatform->OpenFileDialog(
		FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr),
		"Select file to load",
		FPaths::ProjectSavedDir() / "SaveGames", TEXT(""), TEXT("SaveGame files|*.sav"),
		EFileDialogFlags::None, OUT FileNames);
	if (!bSuccess || FileNames.IsEmpty() || !FPaths::FileExists(FileNames[0]))
		return;

	TArray<uint8> FileData;
	if (!FFileHelper::LoadFileToArray(OUT FileData, *FileNames[0]))
	{
		FMessageDialog::Open(EAppMsgCategory::Error, EAppMsgType::Ok, INVTEXT("Could not load file! (Invalid file)"));
		return;
	}

	USaveGameService* SaveGameService = NewObject<USaveGameService>(this);
	Cast<UGameServiceBase>(SaveGameService)->StartService();
	const USaveLoadBehavior* Behavior = SaveGameService->GetSaveLoadBehavior();
	const USaveGameSerializer* Serializer = SaveGameService->GetSaveGameSerializer();

	USaveGame* NewSaveGame = DuplicateObject(&Behavior->CreateNewSavegameObject(*SaveGameService), this);
	if (Serializer->TryDeserializeSaveGame(FileData, OUT NewSaveGame))
	{
		SetSaveGame(NewSaveGame, FString("File: " + FileNames[0]));
	}
	else
	{
		FMessageDialog::Open(EAppMsgCategory::Error, EAppMsgType::Ok, INVTEXT("Could not load file! (Unknown format -> cannot deserialize)"));
	}

	Cast<UGameServiceBase>(SaveGameService)->ShutdownService();
}

void USaveGameEditor::AnalyzeSaveGameComposition()
{
	if (!SaveGame)
	{
		FMessageDialog::Open(EAppMsgCategory::Error, EAppMsgType::Ok, INVTEXT("No SaveGame loaded to this editor that can be analyzed!"));
		return;
	}

	const UModularSaveGame* ModularSaveGame = Cast<UModularSaveGame>(SaveGame.Get());
	if (!ModularSaveGame)
	{
		FMessageDialog::Open(EAppMsgCategory::Error, EAppMsgType::Ok, INVTEXT("Only ModularSaveGame classes are supported for this operation!"));
		return;
	}

	ModularSaveGame->AnalyzeAndReportSaveGameComposition();
}

void USaveGameEditor::SetSaveGame(const USaveGame* InSaveGame, TOptional<FString> OptionalInfo)
{
	SaveGame = InSaveGame;
	if (const UModularSaveGame* ModularSaveGame = Cast<UModularSaveGame>(SaveGame))
	{
		ShownInstance->HeaderData = ModularSaveGame->GetInstancedHeaderData().IsValid()
			? *ModularSaveGame->GetInstancedHeaderData().Get()
			: FInstancedStruct();
	}

	EditorInfo = FString("Editing: ") + GetNameSafe(SaveGame);
	if (OptionalInfo.IsSet())
	{
		EditorInfo += "\n" + *OptionalInfo;
	}
}
