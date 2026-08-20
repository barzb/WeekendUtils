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
#include "GameplayTagContainer.h"
#include "NativeGameplayTags.h"
#include "StructUtils/InstancedStruct.h"

#include "SaveGameAnalysisReport.generated.h"

struct FSaveGameRecord;

///////////////////////////////////////////////////////////////////////////////////////

/** Error verbosity level for a savegame analysis record that determines how to treat failed record verification. */
UENUM(BlueprintType)
enum class ESaveGameRecordVerbosity : uint8
{
	Error,
	Warning,
	Info
};

///////////////////////////////////////////////////////////////////////////////////////

/** A single verifiable record extracted from a deserialized savegame. */
USTRUCT(BlueprintType)
struct WEEKENDSAVEGAME_API FSaveGameRecord
{
	GENERATED_BODY()

public:
	/** Unique identifier to address this record amongst other records. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Analysis")
	FName UniqueIdentifier = NAME_None;

	/** Hierarchical tag identifying this record (suggested format: "SaveGame.Analysis.Feature.Record"). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Analysis", meta = (Categories = "SaveGame.Analysis"))
	FGameplayTag RecordTag = FGameplayTag();

	/** Human-readable summary of what this record asserts. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Analysis")
	FText Description = FText::GetEmpty();

	/** Machine-verifiable expected state. Project defines concrete struct types. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Analysis", meta = (ShowOnlyInnerProperties))
	FInstancedStruct ExpectedValue = FInstancedStruct();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Analysis")
	ESaveGameRecordVerbosity MismatchVerbosity = ESaveGameRecordVerbosity::Error;

	FSaveGameRecord() = default;
	FSaveGameRecord(const FGameplayTag& InRecordTag);
	FSaveGameRecord(const FNativeGameplayTag& InRecordTag);
	FSaveGameRecord(const FNativeGameplayTag& InRecordTag, const FString& UniqueIdSuffix);

	/** @returns whether the identifier of both records match. For list contain/find checks. */
	FORCEINLINE bool operator==(const FSaveGameRecord& Other) const { return UniqueIdentifier == Other.UniqueIdentifier; }
	FORCEINLINE friend uint32 GetTypeHash(const FSaveGameRecord& Record) { return GetTypeHash(Record.UniqueIdentifier); }

	FORCEINLINE bool operator<(const FSaveGameRecord& Other) const { return  UniqueIdentifier.LexicalLess(Other.UniqueIdentifier); }

	/** @returns whether the expected value of this record matches the other record's expected value. */
	bool IsIdenticalTo(const FSaveGameRecord& Other) const
	{
		return UniqueIdentifier == Other.UniqueIdentifier
			&& RecordTag == Other.RecordTag
			&& ExpectedValue == Other.ExpectedValue;
	}
};

///////////////////////////////////////////////////////////////////////////////////////

/** Result of a comparison between an Actual vs. Expected record. */
USTRUCT(BlueprintType)
struct WEEKENDSAVEGAME_API FSaveGameRecordMismatch
{
	GENERATED_BODY()

public:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Analysis")
	FName UniqueIdentifier = NAME_None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Analysis")
	FSaveGameRecord ActualRecord;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Analysis")
	FSaveGameRecord ExpectedRecord;

	/** @returns whether the identifier of referenced records match. For list contain/find checks. */
	FORCEINLINE bool operator==(const FSaveGameRecordMismatch& Other) const { return UniqueIdentifier == Other.UniqueIdentifier; }
	FORCEINLINE bool operator==(const FName& OtherUniqueIdentifier) const { return UniqueIdentifier == OtherUniqueIdentifier; }
};

///////////////////////////////////////////////////////////////////////////////////////

/** Complete analysis snapshot burned into a SaveGamePreset at creation time. */
USTRUCT(BlueprintType)
struct WEEKENDSAVEGAME_API FSaveGameAnalysisReport
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Analysis", meta = (MultiLine, ToolTip = ""))
	FText GeneratedDescription = FText::GetEmpty();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Analysis")
	FDateTime AnalyzedAtUtc = FDateTime();

	/** Original filename that was imported (empty if created from running game). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Analysis")
	TOptional<FString> SourceFileName = {};

	/** Automatically generated records from SaveGameAnalyzers.  */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Analysis", meta = (TitleProperty = "UniqueIdentifier"))
	TArray<FSaveGameRecord> GeneratedRecords = {};

	/** Manually entered records that overwrite entries with the same UniqueIdentifier of @Records.  */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Analysis", meta = (TitleProperty = "UniqueIdentifier"))
	TArray<FSaveGameRecord> OverrideRecords = {};

#if WITH_EDITORONLY_DATA
	UPROPERTY(Transient, EditAnywhere, Category = "Testing", meta = (TitleProperty = "UniqueIdentifier"))
	mutable TArray<FSaveGameRecordMismatch> MismatchedRecords = {};
#endif

	/** @returns all records of this report. Entries of GeneratedRecords may be overwritten with OverwriteRecords. */
	TArray<FSaveGameRecord> GetRecords() const;

	/** @returns whether this report has any records, generated or overwritten. */
	bool HasRecords() const;

	/** @returns all records of this report that have a tag that is a child of @ParentTag. */
	TArray<FSaveGameRecord> FindRecordsByTag(const FGameplayTag& ParentTag) const;

	/** @returns all records of this report that have a tag that equals @ExactTag. */
	TOptional<FSaveGameRecord> FindRecordByExactTag(const FGameplayTag& ExactTag) const;

	/** @returns all records of this report that have a unique identifier that equals @UniqueId. */
	TOptional<FSaveGameRecord> FindRecordByUniqueId(const FName& UniqueId) const;

	/** Adds a new generated record to the report and returns a reference to the new record. */
	FSaveGameRecord& AddGeneratedRecord(const FSaveGameRecord& NewRecord = {});

#if WITH_EDITOR
	void AddMismatchRecord(const FSaveGameRecordMismatch& MismatchToAdd) const;
	void RemoveMismatchRecord(const FSaveGameRecord& RecordWithPotentialMismatch) const;

	/** Updates the description of all generated and overwritten records. */
	void UpdateGeneratedDescription();

	void AddOverrideForActualRecord(const FSaveGameRecord& ActualRecord);
#endif
};
