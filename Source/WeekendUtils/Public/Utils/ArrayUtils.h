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
#include "UObject/WeakObjectPtrTemplates.h"

namespace WeekendUtils
{
	/**
	 * Returns a TArray<ClassName*> built from a TArray<TWeakObjectPtr<ClassName>>, skipping stale/null entries.
	 * Return-value counterpart of the engine's CopyFromWeakArray(Dest, Src), e.g. for use in range-based for loops:
	 * @code for (UObject* Object : WeekendUtils::CopyFromWeakArray(WeakObjects)) { ... } @endcode
	 * @note The returned raw pointers are only guaranteed valid until the next garbage collection, so do not store the result.
	 */
	template <typename SourceArrayType>
	auto CopyFromWeakArray(const SourceArrayType& Src)
	{
		TArray<decltype(Src[0].Get())> Dest;
		::CopyFromWeakArray(OUT Dest, Src);
		return Dest;
	}

	/**
	 * Returns a TArray<TWeakObjectPtr<ClassName>> built from a TArray<TObjectPtr<ClassName>> or TArray<ClassName*>, skipping null entries.
	 * Return-value counterpart of the engine's CopyToWeakArray(Dest, Src).
	 */
	template <typename SourceArrayType>
	auto CopyToWeakArray(const SourceArrayType& Src)
	{
		using ObjectType = std::remove_reference_t<decltype(*Src[0])>;
		TArray<TWeakObjectPtr<ObjectType>> Dest;
		::CopyToWeakArray(OUT Dest, Src);
		return Dest;
	}

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
