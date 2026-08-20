///////////////////////////////////////////////////////////////////////////////////////
/// Copyright (C) by Benjamin Barz and contributors. See file: CREDITS.md
///
/// This file is part of the WeekendUtils UE5 Plugin.
///
/// Distributed under the MIT License. See file: LICENSE.md
///
///////////////////////////////////////////////////////////////////////////////////////

#include "SaveGame/Analysis/SaveGameAnalysisReport.h"

///////////////////////////////////////////////////////////////////////////////////////
/// FSaveGameRecord

FSaveGameRecord::FSaveGameRecord(const FGameplayTag& InRecordTag)
	: UniqueIdentifier(InRecordTag.ToString())
	, RecordTag(InRecordTag)
{
}

FSaveGameRecord::FSaveGameRecord(const FNativeGameplayTag& InRecordTag)
	: UniqueIdentifier(InRecordTag.GetTag().ToString())
	, RecordTag(InRecordTag.GetTag())
{
}

FSaveGameRecord::FSaveGameRecord(const FNativeGameplayTag& InRecordTag, const FString& UniqueIdSuffix)
	: UniqueIdentifier(*(InRecordTag.GetTag().ToString() + "." + UniqueIdSuffix))
	, RecordTag(InRecordTag.GetTag())
{
}

///////////////////////////////////////////////////////////////////////////////////////
/// FSaveGameAnalysisReport

TArray<FSaveGameRecord> FSaveGameAnalysisReport::GetRecords() const
{
	TArray<FSaveGameRecord> Result = GeneratedRecords;
	for (const FSaveGameRecord& OverwriteRecord : OverrideRecords)
	{
		if (const int32 Index = Result.IndexOfByKey(OverwriteRecord); Index != INDEX_NONE)
		{
			Result[Index] = OverwriteRecord;
		}
		else
		{
			Result.Add(OverwriteRecord);
		}
	}
	return Result;
}

bool FSaveGameAnalysisReport::HasRecords() const
{
	return (GeneratedRecords.Num() + OverrideRecords.Num()) > 0;
}

TArray<FSaveGameRecord> FSaveGameAnalysisReport::FindRecordsByTag(const FGameplayTag& ParentTag) const
{
	TArray<FSaveGameRecord> Result;
	for (const FSaveGameRecord& Record : GetRecords())
	{
		if (Record.RecordTag.MatchesTag(ParentTag))
		{
			Result.Add(Record);
		}
	}
	return Result;
}

TOptional<FSaveGameRecord> FSaveGameAnalysisReport::FindRecordByExactTag(const FGameplayTag& ExactTag) const
{
	TArray<FSaveGameRecord> Result;
	for (const FSaveGameRecord& Record : GetRecords())
	{
		if (Record.RecordTag.MatchesTagExact(ExactTag))
			return Record;
	}
	return {};
}

TOptional<FSaveGameRecord> FSaveGameAnalysisReport::FindRecordByUniqueId(const FName& UniqueId) const
{
	TArray<FSaveGameRecord> Result;
	for (const FSaveGameRecord& Record : GetRecords())
	{
		if (Record.UniqueIdentifier == UniqueId)
			return Record;
	}
	return {};
}

FSaveGameRecord& FSaveGameAnalysisReport::AddGeneratedRecord(const FSaveGameRecord& NewRecord)
{
	return GeneratedRecords[GeneratedRecords.Add(NewRecord)];
}

#if WITH_EDITOR

void FSaveGameAnalysisReport::AddMismatchRecord(const FSaveGameRecordMismatch& MismatchToAdd) const
{
	// Replace older mismatch with same ID with new one:
	if (MismatchedRecords.Contains(MismatchToAdd))
	{
		MismatchedRecords.Remove(MismatchToAdd);
	}

	MismatchedRecords.AddUnique(MismatchToAdd);
}

void FSaveGameAnalysisReport::RemoveMismatchRecord(const FSaveGameRecord& RecordWithPotentialMismatch) const
{
	if (const int32 Index = MismatchedRecords.IndexOfByKey(RecordWithPotentialMismatch.UniqueIdentifier); Index != INDEX_NONE)
	{
		MismatchedRecords.RemoveAt(Index);
	}
}

void FSaveGameAnalysisReport::UpdateGeneratedDescription()
{
	TMap<FString, TArray<FSaveGameRecord>> RecordsByCategory;
	for (const FSaveGameRecord& Record : GetRecords())
	{
		TArray<FString> Segments;
		Record.RecordTag.ToString().ParseIntoArray(OUT Segments, TEXT("."));

		// Expected format: "SaveGame.Analysis.<Category>.<Record>"
		const FString Category = Segments.IsValidIndex(2) ? Segments[2] : TEXT("OTHER");
		RecordsByCategory.FindOrAdd(Category).Add(Record);
	}

	TArray<FString> CategoryDescriptions;
	for (const auto& [Category, CategoryRecords] : RecordsByCategory)
	{
		TArray<FString> RecordDescriptions;
		Algo::Transform(CategoryRecords, OUT RecordDescriptions, [](const FSaveGameRecord& Record){ return FString::Printf(TEXT(" - %s"), *Record.Description.ToString()); });

		CategoryDescriptions.Add(FString::Printf(TEXT("## %s ##\n"), *Category) + FString::Join(RecordDescriptions, TEXT("\n")));
	}

	const FString Result = FString::Join(CategoryDescriptions, TEXT("\n\n"));
	GeneratedDescription = FText::FromString(Result);
}

void FSaveGameAnalysisReport::AddOverrideForActualRecord(const FSaveGameRecord& ActualRecord)
{
	FString Description;
	if (const FSaveGameRecord* GeneratedExpectedRecord = GeneratedRecords.FindByKey(ActualRecord))
	{
		// Rare case where an outdated override exists, and now the actual record matches the original record again:
		if (GeneratedExpectedRecord->IsIdenticalTo(ActualRecord) && OverrideRecords.Contains(ActualRecord))
		{
			OverrideRecords.Remove(ActualRecord);
			return;
		}

		Description = FString::Printf(TEXT("Override: %s | Original: %s"),
			*ActualRecord.Description.ToString(),
			*GeneratedExpectedRecord->Description.ToString());
	}
	else
	{
		Description = FString::Printf(TEXT("Override: %s | Original: (missing)"),
			*ActualRecord.Description.ToString());
	}

	FSaveGameRecord NewOverwriteRecord = ActualRecord;
	NewOverwriteRecord.ExpectedValue = ActualRecord.ExpectedValue;
	NewOverwriteRecord.Description = FText::FromString(Description);

	// Remove outdated overrides for this record:
	if (OverrideRecords.Contains(ActualRecord))
	{
		OverrideRecords.Remove(ActualRecord);
	}

	OverrideRecords.Add(NewOverwriteRecord);
}

#endif
