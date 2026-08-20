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
#include "Utils/ArrayUtils.h"

#include "SaveGameRecordTypes.generated.h"

USTRUCT(DisplayName = "Boolean")
struct WEEKENDSAVEGAME_API FSaveGameRecord_BoolValue
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "SaveGameRecord")
	bool bValue = false;
};

USTRUCT(DisplayName = "Integer")
struct WEEKENDSAVEGAME_API FSaveGameRecord_IntValue
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "SaveGameRecord")
	int32 Value = 0;
};

USTRUCT(DisplayName = "Float Number")
struct WEEKENDSAVEGAME_API FSaveGameRecord_FloatValue
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "SaveGameRecord")
	float Value = 0.f;
};

USTRUCT(DisplayName = "String")
struct WEEKENDSAVEGAME_API FSaveGameRecord_StringValue
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "SaveGameRecord")
	FString Value = "";
};

USTRUCT(DisplayName = "String List")
struct WEEKENDSAVEGAME_API FSaveGameRecord_StringListValue
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "SaveGameRecord")
	TArray<FString> Values = {};

	FSaveGameRecord_StringListValue() = default;
	FSaveGameRecord_StringListValue(const TArray<FString>& InValues) : Values(InValues)
	{
		// Sort lexigraphically so comparison to other records is consistent:
		Values.Sort([](const FString& A, const FString& B) { return FName(A).LexicalLess(FName(B)); });
	}
};