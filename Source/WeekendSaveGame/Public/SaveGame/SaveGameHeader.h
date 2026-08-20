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
#include "Misc/DateTime.h"
#include "Misc/EngineVersion.h"
#include "Serialization/CustomVersion.h"
#include "StructUtils/InstancedStruct.h"

#include "SaveGameHeader.generated.h"

class FMemoryReader;
class FMemoryWriter;
class UModularSaveGame;
struct FInstancedStruct;

///////////////////////////////////////////////////////////////////////////////////////

/** Base struct for SaveGameHeaderData struct implementations. */
USTRUCT()
struct WEEKENDSAVEGAME_API FSaveGameHeaderDataBase
{
	GENERATED_BODY()
};

///////////////////////////////////////////////////////////////////////////////////////

/**
 * Default implementation of simple SaveGame header data, containing meta information
 * about a SaveGame, like how often and when it was saved/loaded and in which level. 
 */
USTRUCT()
struct WEEKENDSAVEGAME_API FSimpleSaveGameHeaderData : public FSaveGameHeaderDataBase
{
	GENERATED_BODY()

public:
	/** How often the SaveGame was saved. */
	UPROPERTY(SaveGame, EditAnywhere, Category = "SaveGame")
	int32 SaveCounter = 0;

	/** How often the SaveGame was restored. */
	UPROPERTY(SaveGame, EditAnywhere, Category = "SaveGame")
	int32 RestoreCounter = 0;

	/** The UTC timestamp of when the SaveGame was last saved. */
	UPROPERTY(SaveGame, EditAnywhere, Category = "SaveGame")
	FDateTime UtcTimeOfLastSave = FDateTime();

	/** The UTC timestamp of when the SaveGame was last restored. */
	UPROPERTY(SaveGame, EditAnywhere, Category = "SaveGame")
	FDateTime UtcTimeOfLastRestore = FDateTime();

	/** The level which was loaded when the SaveGame was last saved. */
	UPROPERTY(SaveGame, EditAnywhere, Category = "SaveGame", meta = (DisplayThumbnail = "false"))
	FSoftObjectPath LoadedLevel = FSoftObjectPath();

	FORCEINLINE bool WasEverSaved() const { return (SaveCounter > 0); }
	FORCEINLINE bool WasEverRestored() const { return (RestoreCounter > 0); }
};

///////////////////////////////////////////////////////////////////////////////////////

#define MODULAR_SAVEGAME_FILE_TYPE_TAG	0x53415648 // = UE_SAVEGAME_FILE_TYPE_TAG + 1
#define MODULAR_SAVEGAME_FILE_VERSION	1 // Increase when file format/compression becomes incompatible to previous version

/**
 * Implementation detail for header de-/serialization.
 * @see ModularSaveGameHeader.cpp
 */
struct FModularSaveGameHeader
{
	FModularSaveGameHeader();
	FModularSaveGameHeader(TSubclassOf<UModularSaveGame> ObjectType, const FInstancedStruct& HeaderData);

	bool TryRead(FMemoryReader& MemoryReader);
	bool TryWrite(FMemoryWriter& MemoryWriter);
	void Clear();

	int32 FileTypeTag;
	int32 SaveGameFileVersion;
	FPackageFileVersion PackageFileUEVersion;
	FEngineVersion SavedEngineVersion;
	int32 CustomVersionFormat;
	FCustomVersionContainer CustomVersions;
	FString SaveGameClassName;
	FInstancedStruct CustomHeaderData;
};
