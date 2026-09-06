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

#include "CommonEnabledStateEnum.generated.h"

///////////////////////////////////////////////////////////////////////////////////////
/// UFUNCTION(BlueprintCallable, BlueprintPure = False, ExpandEnumAsExecs = "ReturnValue")
/// ECommonEnabledState BranchIsEnabled() const;

UENUM(BlueprintType)
enum class ECommonEnabledState : uint8
{
	Enabled,
	Disabled
};

DEFINE_ENUM_STRING_CONVERTERS(CommonEnabledState, ECommonEnabledState);
DEFINE_ENUM_BOOL_CONVERTERS(CommonEnabledState, ECommonEnabledState, Enabled, Disabled);

///////////////////////////////////////////////////////////////////////////////////////
