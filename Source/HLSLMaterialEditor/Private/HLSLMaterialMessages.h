// Copyright 2021 Phyronnaz

#pragma once

#include "CoreMinimal.h"

class UHLSLMaterialFunctionLibrary;

class FHLSLMaterialMessages
{
public:
	template <typename FmtType, typename... Types>
	static void ShowError(const FmtType& Fmt, Types... Args)
	{
		ShowErrorImpl(FString::Printf(Fmt, Args...));
	}

	class FLibraryScope
	{
	public:
		explicit FLibraryScope(UHLSLMaterialFunctionLibrary& InLibrary)
			: Guard(Library, &InLibrary)
		{
		}

	private:
		static UHLSLMaterialFunctionLibrary* Library;
		TGuardValue<UHLSLMaterialFunctionLibrary*> Guard;

		friend class FHLSLMaterialMessages;
	};

private:
	static void ShowErrorImpl(FString Message);
};