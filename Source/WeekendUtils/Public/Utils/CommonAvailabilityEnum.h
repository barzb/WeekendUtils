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
#include "EnumUtils.h"

///////////////////////////////////////////////////////////////////////////////////////
/// UFUNCTION(BlueprintCallable, BlueprintPure = False, ExpandEnumAsExecs = "ReturnValue")
/// ECommonAvailability BranchAvailability() const;

UENUM(BlueprintType)
enum class ECommonAvailability : uint8
{
	Available,
	Unavailable
};

DEFINE_ENUM_STRING_CONVERTERS(CommonAvailability, ECommonAvailability);
DEFINE_ENUM_BOOL_CONVERTERS(CommonAvailability, ECommonAvailability, Available, Unavailable);

///////////////////////////////////////////////////////////////////////////////////////
