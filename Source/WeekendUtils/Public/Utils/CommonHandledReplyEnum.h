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

#include "CommonHandledReplyEnum.generated.h"

///////////////////////////////////////////////////////////////////////////////////////
/// UFUNCTION(BlueprintCallable, BlueprintPure = False, ExpandEnumAsExecs = "ReturnValue")
/// ECommonHandledReply BranchHandledReply() const;

UENUM(BlueprintType)
enum class ECommonHandledReply : uint8
{
	Handled,
	NotHandled
};

DEFINE_ENUM_STRING_CONVERTERS(CommonHandledReply, ECommonHandledReply);
DEFINE_ENUM_BOOL_CONVERTERS(CommonHandledReply, ECommonHandledReply, Handled, NotHandled);

///////////////////////////////////////////////////////////////////////////////////////
