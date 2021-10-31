// Copyright 2021 Phyronnaz

#pragma once

#include "CoreMinimal.h"

class UHLSLMaterialFunctionLibrary;

class FHLSLMaterialFunctionLibraryEditor
{
public:
	static void Register();

	static TSharedRef<FVirtualDestructor> CreateWatcher(UHLSLMaterialFunctionLibrary& Library);
	static void Generate(UHLSLMaterialFunctionLibrary& Library);

private:
	static bool TryLoadFileToString(FString& Text, const FString& FullPath);

	struct FInclude
	{
		FString VirtualPath;
		FString DiskPath;
	};
	static void GetIncludes(const FString& Text, TArray<FInclude>& OutIncludes);
	
private:
	static void ShowErrorImpl(FString Message);

	template <typename FmtType, typename... Types>
	static void ShowError(const FmtType& Fmt, Types... Args)
	{
		ShowErrorImpl(FString::Printf(Fmt, Args...));
	}

	struct FLibraryScope
	{
		explicit FLibraryScope(UHLSLMaterialFunctionLibrary& InLibrary)
			: Guard(Library, &InLibrary)
		{
		}

		static UHLSLMaterialFunctionLibrary* Library;

	private:
		TGuardValue<UHLSLMaterialFunctionLibrary*> Guard;
	};
};