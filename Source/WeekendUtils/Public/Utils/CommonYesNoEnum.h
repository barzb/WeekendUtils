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

#include "CommonYesNoEnum.generated.h"

///////////////////////////////////////////////////////////////////////////////////////
/// UFUNCTION(BlueprintCallable, BlueprintPure = False, ExpandEnumAsExecs = "ReturnValue")
/// ECommonYesNo BranchYesOrNo() const;

UENUM(BlueprintType)
enum class ECommonYesNo : uint8
{
	Yes,
	No
};

DEFINE_ENUM_STRING_CONVERTERS(CommonYesNo, ECommonYesNo);
DEFINE_ENUM_BOOL_CONVERTERS(CommonYesNo, ECommonYesNo, Yes, No);

///////////////////////////////////////////////////////////////////////////////////////
