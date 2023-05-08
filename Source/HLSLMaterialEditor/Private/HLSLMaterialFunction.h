// Copyright Phyronnaz

#pragma once

#include "CoreMinimal.h"

struct FHLSLMaterialFunction
{
	int32 StartLine = 0;
	FString Comment;
	FString Metadata;
	FString ReturnType;
	FString Name;
	TArray<FString> Arguments;
	FString Body;

	FString HashedString;

	FString GenerateHashedString(const FString &BaseHash) const;
};