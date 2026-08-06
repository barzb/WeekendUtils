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

/**
 * Utility container for synchronizing the elements of an object pointer list with an arbitrary array of data.
 *
 * @example:
 *
 *	TArray<TObjectPtr<USomeViewModel>> ViewModels = { ... };
 *	TArray<FSomeModel> Models = { ... };
 *
 *	TObjectListSynchronizer(ViewModels, Models)
 *		.ForEachMissingElement([this](FSomeModel& Model) -> USomeViewModel*
 *		{
 *			USomeViewModel* NewViewModel = NewObject<USomeViewModel>(this);
 *			NewViewModel->BindToModel(Model);
 *			return NewViewModel;
 *		})
 *		.ForEachUpdatedElement([](USomeViewModel& ViewModel, FSomeModel& Model)
 *		{
 *			ViewModel.UnbindFromModel();
 *			ViewModel.BindToModel(Model);
 *		})
 *		.ForEachRemovedElement([](USomeViewModel& ViewModel)
 *		{
 *			ViewModel.UnbindFromModel();
 *		});
 */
template <typename ObjectType, typename InitListType>
class TObjectListSynchronizer
{
public:
	TObjectListSynchronizer(TArray<ObjectType*>& InObjectList, InitListType& InInitDataList) :
		ObjectList(ObjectPtrWrap(InObjectList)), InitDataList(InInitDataList) { Init(); }
	TObjectListSynchronizer(TArray<TObjectPtr<ObjectType>>& InObjectList, InitListType& InInitDataList) :
		ObjectList(InObjectList), InitDataList(InInitDataList) { Init(); }

	/** FunctionType must equivalent to: TFunctionRef<ObjectType*(InitDataType&)> */
	template <typename FunctionType>
	TObjectListSynchronizer& ForEachMissingElement(FunctionType Function)
	{
		const int32 NumMissing = (InitDataList.Num() - ObjectList.Num());
		if (NumMissing <= 0)
			return *this;

		ObjectList.SetNum(InitDataList.Num());
		for (int32 i = (InitDataList.Num() - NumMissing); i < InitDataList.Num(); ++i)
		{
			ObjectList[i] = Function(InitDataList[i]);
		}
		return *this;
	}

	/** FunctionType must equivalent to: TFunctionRef<ObjectType*(InitDataType&, int32)> */
	template <typename FunctionType>
	TObjectListSynchronizer& ForEachMissingElementAndIndex(FunctionType Function)
	{
		const int32 NumMissing = (InitDataList.Num() - ObjectList.Num());
		if (NumMissing <= 0)
			return *this;

		ObjectList.SetNum(InitDataList.Num());
		for (int32 i = (InitDataList.Num() - NumMissing); i < InitDataList.Num(); ++i)
		{
			ObjectList[i] = Function(InitDataList[i], i);
		}
		return *this;
	}

	/** FunctionType must equivalent to: TFunctionRef<void(ObjectType&)> */
	template <typename FunctionType>
	TObjectListSynchronizer& ForEachRemovedElement(FunctionType Function)
	{
		for (int32 i = 0; i < RemovedObjects.Num(); ++i)
		{
			Function(*RemovedObjects[i]);
		}
		return *this;
	}

	/** FunctionType must equivalent to: TFunctionRef<void(ObjectType&, InitDataType&)> */
	template <typename FunctionType>
	TObjectListSynchronizer& ForEachUpdatedElement(FunctionType Function)
	{
		for (int32 i = 0; i < UpdatedObjects.Num(); ++i)
		{
			Function(*UpdatedObjects[i], InitDataList[i]);
		}
		return *this;
	}

	/** FunctionType must equivalent to: TFunctionRef<void(ObjectType&, InitDataType&, int32)> */
	template <typename FunctionType>
	TObjectListSynchronizer& ForEachUpdatedElementAndIndex(FunctionType Function)
	{
		for (int32 i = 0; i < UpdatedObjects.Num(); ++i)
		{
			Function(*UpdatedObjects[i], InitDataList[i], i);
		}
		return *this;
	}

private:
	TArray<TObjectPtr<ObjectType>>& ObjectList;
	InitListType& InitDataList;
	TArray<ObjectType*> RemovedObjects = {};
	TArray<ObjectType*> UpdatedObjects = {};

	void Init()
	{
		while (ObjectList.Num() > InitDataList.Num())
		{
			RemovedObjects.Add(ObjectList.Pop());
		}
		UpdatedObjects = ObjectList;
	}
};
