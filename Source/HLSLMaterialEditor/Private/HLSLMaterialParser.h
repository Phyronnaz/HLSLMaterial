// Copyright 2021 Phyronnaz

#pragma once

#include "CoreMinimal.h"

struct FHLSLMaterialFunction;
class UHLSLMaterialFunctionLibrary;

class FHLSLMaterialParser
{
public:
	static FString Parse(
		const UHLSLMaterialFunctionLibrary& Library, 
		FString Text, 
		TArray<FHLSLMaterialFunction>& OutFunctions);
};