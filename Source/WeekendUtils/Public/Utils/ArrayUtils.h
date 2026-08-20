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

namespace WeekendUtils
{
	/** Compares two unsorted arrays using a custom sorting function. Stolen from WorldPartitionActorDesc.h */
	template <typename T, class F>
	bool AreUnsortedArraysEqual(const TArray<T>& Array1, const TArray<T>& Array2, F Func)
	{
		if (Array1.Num() == Array2.Num())
		{
			TArray<T> SortedArray1(Array1);
			TArray<T> SortedArray2(Array2);
			SortedArray1.Sort(Func);
			SortedArray2.Sort(Func);
			return SortedArray1 == SortedArray2;
		}
		return false;
	}

	/** Compares two unsorted arrays using the default operator< sorting function. Stolen from WorldPartitionActorDesc.h */
	template <typename T>
	bool AreUnsortedArraysEqual(const TArray<T>& Array1, const TArray<T>& Array2)
	{
		return AreUnsortedArraysEqual(Array1, Array2, [](const T& A, const T& B) { return A < B; });
	}

	/** Compares two unsorted FName arrays using the LexicalLess sorting function. Stolen from WorldPartitionActorDesc.h */
	template <>
	inline bool AreUnsortedArraysEqual(const TArray<FName>& Array1, const TArray<FName>& Array2)
	{
		return AreUnsortedArraysEqual(Array1, Array2, [](const FName& A, const FName& B) { return A.LexicalLess(B); });
	}

	/** Compares two unsorted FString arrays using the LexicalLess sorting function. Stolen from WorldPartitionActorDesc.h */
	template <>
	inline bool AreUnsortedArraysEqual(const TArray<FString>& Array1, const TArray<FString>& Array2)
	{
		return AreUnsortedArraysEqual(Array1, Array2, [](const FString& A, const FString& B) { return FName(A).LexicalLess(FName(B)); });
	}
}
