// Copyright 2021 Phyronnaz

#pragma once

#include "CoreMinimal.h"

struct FCustomDefine;
struct FHLSLMaterialFunction;
class UHLSLMaterialFunctionLibrary;

class FHLSLMaterialParser
{
public:
	static FString Parse(
		const UHLSLMaterialFunctionLibrary& Library, 
		FString Text, 
		TArray<FHLSLMaterialFunction>& OutFunctions);

	struct FInclude
	{
		FString VirtualPath;
		FString DiskPath;
	};
	static TArray<FInclude> GetIncludes(const FString& FilePath, const FString& Text);
	static TArray<FCustomDefine> GetDefines(const FString& Text);
};