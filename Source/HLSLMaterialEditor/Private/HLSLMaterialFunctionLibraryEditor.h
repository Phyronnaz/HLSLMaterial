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
};