///////////////////////////////////////////////////////////////////////////////////////
/// Copyright (C) by Benjamin Barz and contributors. See file: CREDITS.md
///
/// This file is part of the WeekendUtils UE5 Plugin.
///
/// Distributed under the MIT License. See file: LICENSE.md
///
///////////////////////////////////////////////////////////////////////////////////////

#include "SaveGame/ModularSaveGame.h"

#include "GameService/GameServiceLocator.h"
#include "Logging/MessageLog.h"
#include "Misc/EngineVersion.h"
#include "Misc/UObjectToken.h"
#include "SaveGame/SaveGameHeader.h"
#include "SaveGame/SaveGameService.h"
#include "SaveGame/SaveGameUtils.h"
#include "Serialization/CustomVersion.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "Templates/SubclassOf.h"

///////////////////////////////////////////////////////////////////////////////////////
/// @UModularSaveGame

const UModularSaveGame* UModularSaveGame::FindCurrent(const UObject* WorldContext)
{
	const USaveGameService* SaveGameService = UGameServiceLocator::FindService<USaveGameService>(WorldContext);
	if (!SaveGameService)
		return nullptr;

	return SaveGameService->GetCurrentSaveGame().GetPtr<UModularSaveGame>();
}

UModularSaveGame* UModularSaveGame::FindMutableCurrent(const UObject* WorldContext)
{
	const USaveGameService* SaveGameService = UGameServiceLocator::FindService<USaveGameService>(WorldContext);
	if (!SaveGameService)
		return nullptr;

	return SaveGameService->GetCurrentSaveGame().GetMutablePtr<UModularSaveGame>();
}

///////////////////////////////////////////////////////////////////////////////////////
/// SAVE GAME HEADER - Mostly copied from UE/GameplayStatic.cpp

FModularSaveGameHeader::FModularSaveGameHeader() :
	FileTypeTag(0),
	SaveGameFileVersion(0),
	CustomVersionFormat(static_cast<int32>(ECustomVersionSerializationFormat::Unknown))
{
}

FModularSaveGameHeader::FModularSaveGameHeader(TSubclassOf<UModularSaveGame> ObjectType, const FInstancedStruct& HeaderData) :
	FileTypeTag(MODULAR_SAVEGAME_FILE_TYPE_TAG),
	SaveGameFileVersion(MODULAR_SAVEGAME_FILE_VERSION),
	PackageFileUEVersion(GPackageFileUEVersion),
	SavedEngineVersion(FEngineVersion::Current()),
	CustomVersionFormat(static_cast<int32>(ECustomVersionSerializationFormat::Latest)),
	CustomVersions(FCurrentCustomVersions::GetAll()),
	SaveGameClassName(ObjectType->GetPathName()),
	CustomHeaderData(HeaderData)
{
}

void FModularSaveGameHeader::Clear()
{
	FileTypeTag = 0;
	SaveGameFileVersion = 0;
	PackageFileUEVersion.Reset();
	SavedEngineVersion.Empty();
	CustomVersionFormat = static_cast<int32>(ECustomVersionSerializationFormat::Unknown);
	CustomVersions.Empty();
	SaveGameClassName.Empty();
	CustomHeaderData.Reset();
}

bool FModularSaveGameHeader::TryRead(FMemoryReader& MemoryReader)
{
	Clear();

	// Check incompatible save file type:
	MemoryReader << FileTypeTag;
	if (FileTypeTag != MODULAR_SAVEGAME_FILE_TYPE_TAG)
	{
		MemoryReader.Seek(0);
		return false;
	}

	// Check incompatible save file version, in either direction:
	MemoryReader << SaveGameFileVersion;
	if (SaveGameFileVersion != MODULAR_SAVEGAME_FILE_VERSION)
	{
		MemoryReader.Seek(0);
		return false;
	}

	// Read engine and UE version information:
	MemoryReader << PackageFileUEVersion;
	MemoryReader << SavedEngineVersion;
	MemoryReader.SetUEVer(PackageFileUEVersion);
	MemoryReader.SetEngineVer(SavedEngineVersion);

	// Read custom version data:
	MemoryReader << CustomVersionFormat;
	CustomVersions.Serialize(MemoryReader, static_cast<ECustomVersionSerializationFormat>(CustomVersionFormat));
	MemoryReader.SetCustomVersions(CustomVersions);

	// Read out custom header data:
	MemoryReader << SaveGameClassName;
	FObjectAndNameAsStringProxyArchive ProxyArchive(MemoryReader, true);
	CustomHeaderData.Serialize(ProxyArchive);

	// A truncated or malformed file trips the archive's error flag instead of throwing:
	if (MemoryReader.IsError())
	{
		Clear();
		MemoryReader.Seek(0);
		return false;
	}

	return true;
}

bool FModularSaveGameHeader::TryWrite(FMemoryWriter& MemoryWriter)
{
	// Write file type tag that identifies this file type:
	MemoryWriter << FileTypeTag;

	// Write version for this file format, for compatibility checks:
	MemoryWriter << SaveGameFileVersion;

	// Write out engine and UE version information:
	MemoryWriter << PackageFileUEVersion;
	MemoryWriter << SavedEngineVersion;

	// Write out custom version data:
	MemoryWriter << CustomVersionFormat;
	CustomVersions.Serialize(MemoryWriter, static_cast<ECustomVersionSerializationFormat>(CustomVersionFormat));

	// Write custom header data:
	MemoryWriter << SaveGameClassName;
	FObjectAndNameAsStringProxyArchive ProxyArchive(MemoryWriter, true);
	CustomHeaderData.Serialize(ProxyArchive);

	return true;
}

void UModularSaveGame::ForEachModule(const TFunction<void(const FName&, USaveGameModule&)>& Function)
{
	for (const TPair<FName, TObjectPtr<USaveGameModule>>& Itr : Modules)
	{
		if (Itr.Value)
		{
			Function(Itr.Key, *Itr.Value);
		}
	}
}

void UModularSaveGame::ForEachModule(const TFunction<void(const FName&, const USaveGameModule&)>& Function) const
{
	for (const TPair<FName, TObjectPtr<USaveGameModule>>& Itr : Modules)
	{
		if (Itr.Value)
		{
			Function(Itr.Key, *Itr.Value);
		}
	}
}

#if WITH_EDITOR
void UModularSaveGame::AnalyzeAndReportSaveGameComposition() const
{
	const int64 TotalSize = USaveGameUtils::CalculateObjectSizeForSaveGame(*this, EUnit::Bytes);
	const int64 TotalSizeKb = FUnitConversion::Convert(TotalSize, EUnit::Bytes, EUnit::Kilobytes);

	FMessageLog MessageLog("AssetCheck");
	MessageLog.NewPage(FText::FromString(GetName()));
	MessageLog.Info()
		->AddToken(FTextToken::Create(FText::FromString("Analysis for"))) 
		->AddToken(FUObjectToken::Create(this));

	TMap<int64, FString> SortedAnalysisEntries;
	ForEachModule([this, &SortedAnalysisEntries, TotalSize](const FName& ModuleName, const USaveGameModule& Module)
	{
		const int64 ModuleSize = USaveGameUtils::CalculateObjectSizeForSaveGame(Module, EUnit::Bytes);
		const int64 ModuleSizeKb = FUnitConversion::Convert(ModuleSize, EUnit::Bytes, EUnit::Kilobytes);
		const double Pct = 100.0 * StaticCast<double>(ModuleSize) / StaticCast<double>(TotalSize);

		int64 SortIndex = ModuleSize;
		while (SortedAnalysisEntries.Contains(SortIndex))
		{
			++SortIndex; // TMap needs unique keys, this handles same-sized entries in a good-enough way.
		}

		const FString PctString = Pct < 0.01 ? FString("<0.01%") :  FString::Printf(TEXT("%05.2f%%"), Pct);
		const FString SizeString = ModuleSize < 10000 ? FString::Printf(TEXT("%lld bytes"), ModuleSize) : FString::Printf(TEXT("%lld KB"), ModuleSizeKb);
		SortedAnalysisEntries.Add(SortIndex, FString::Printf(TEXT("- [%s] Module: %s (~%s)"), *PctString, *ModuleName.ToString(), *SizeString));
	});
	SortedAnalysisEntries.KeySort(TGreater<int64>());

	for (const TPair<int64, FString>& Itr : SortedAnalysisEntries)
	{
		MessageLog.Info()->AddToken(FTextToken::Create(FText::FromString(Itr.Value))); 
	}
	MessageLog.Info()->AddToken(FTextToken::Create(FText::FromString(FString::Printf(TEXT("=> Total: %lld bytes (%lld KB)"), TotalSize, TotalSizeKb)))); 

	MessageLog.Info()->AddToken(FTextToken::Create(FText::FromString("==================================="))); 
	ForEachModule([this, &MessageLog](const FName&, const USaveGameModule& Module)
	{
		Module.AnalyzeAndReportModuleComposition(MessageLog);
	});

	MessageLog.Notify(FText::FromString("SaveGame analysis is now available"), EMessageSeverity::Info, true);
}

void UModularSaveGame::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	if (PropertyChangedEvent.GetPropertyName() == GET_MEMBER_NAME_CHECKED(ThisClass, Modules))
	{
		for (auto Itr = Modules.CreateIterator(); Itr; ++Itr)
		{
			if (!Itr.Key().IsNone() || !Itr.Value())
				continue;

			Itr.Key() = Itr.Value()->DefaultModuleName;
		}
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif

///////////////////////////////////////////////////////////////////////////////////////
/// @UModularSaveGameSerializer

bool UModularSaveGameSerializer::TrySerializeSaveGame(USaveGame& InSaveGameObject, TArray<uint8>& OutSaveData) const
{
	const UModularSaveGame* ModularSaveGame = Cast<UModularSaveGame>(&InSaveGameObject);
	FMemoryWriter MemoryWriter(OutSaveData, true);
	MemoryWriter.ArIsSaveGame = true;
	MemoryWriter.ArNoDelta = true;
	MemoryWriter.ArNoIntraPropertyDelta = true;

	// Serialize header data:
	const FInstancedStruct CustomHeaderData = (ModularSaveGame && ModularSaveGame->GetInstancedHeaderData().IsValid())
		? *ModularSaveGame->GetInstancedHeaderData()
		: FInstancedStruct::Make<FSimpleSaveGameHeaderData>();
	FModularSaveGameHeader SaveHeader(InSaveGameObject.GetClass(), CustomHeaderData);
	if (!SaveHeader.TryWrite(MemoryWriter))
		return false;

	// Serialize the save game object and all supported properties:
	FWeekendUtilsSubobjectProxyArchive Archive(MemoryWriter, InSaveGameObject);
	InSaveGameObject.Serialize(Archive);

	return true;
}

bool UModularSaveGameSerializer::TryDeserializeSaveGame(const TArray<uint8>& InSaveData, USaveGame*& OutSaveGameObject) const
{
	OutSaveGameObject = nullptr;
	if (InSaveData.IsEmpty())
		return false;

	FMemoryReader MemoryReader(InSaveData, true);
	MemoryReader.ArIsSaveGame = true;

	// Restore header data:
	FModularSaveGameHeader SaveHeader;
	if (!SaveHeader.TryRead(MemoryReader))
		return false;

	// Restore the save game class info:
	UClass* SaveGameClass = UClass::TryFindTypeSlow<UClass>(SaveHeader.SaveGameClassName);
	if (!SaveGameClass)
	{
		SaveGameClass = LoadObject<UClass>(nullptr, *SaveHeader.SaveGameClassName);
	}

	if (!SaveGameClass)
		return false;

	// (!) The class name comes from the user-writable save file, so a tampered file must not be able
	// to instance an arbitrary class that is then reinterpreted as a USaveGame:
	if (!SaveGameClass->IsChildOf(USaveGame::StaticClass())
		|| SaveGameClass->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists))
	{
		UE_LOG(LogSaveGameService, Error, TEXT("UModularSaveGameSerializer: save file names \"%s\", which is not an instantiable USaveGame class. Refusing to load."), 	*SaveHeader.SaveGameClassName);
		return false;
	}

	// Create (empty) save game object and then restore all of its saved properties:
	USaveGame* LoadedSaveGame = NewObject<USaveGame>(GetOuter(), SaveGameClass);
	FWeekendUtilsSubobjectProxyArchive Archive(MemoryReader, *LoadedSaveGame);
	LoadedSaveGame->Serialize(Archive);

	// A corrupt payload leaves the archive in an error state -> don't hand out a half-restored object:
	if (MemoryReader.IsError())
	{
		UE_LOG(LogSaveGameService, Error, TEXT("UModularSaveGameSerializer: deserialization of \"%s\" failed (corrupt or incompatible save data)."), *SaveHeader.SaveGameClassName);
		OutSaveGameObject = nullptr;
		return false;
	}

	if (UModularSaveGame* ModularSaveGame = Cast<UModularSaveGame>(LoadedSaveGame))
	{
		ModularSaveGame->SetInstancedHeaderData(SaveHeader.CustomHeaderData);
	}

	OutSaveGameObject = LoadedSaveGame;
	return true;
}
