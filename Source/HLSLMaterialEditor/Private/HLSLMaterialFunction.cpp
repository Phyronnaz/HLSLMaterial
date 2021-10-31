// Copyright 2021 Phyronnaz

#include "HLSLMaterialFunction.h"
#include "HLSLMaterialUtilities.h"

FString FHLSLMaterialFunction::GenerateHashedString(const FString& IncludesHash) const
{
	const FString PluginHashVersion = "1";
	const FString StringToHash =
		PluginHashVersion + " " +
		FString::FromInt(StartLine) + " " +
		Comment + " " +
		ReturnType + " " +
		Name + "(" +
		FString::Join(Arguments, TEXT(",")) + ")" +
		Body +
		IncludesHash;

	return "HLSL Hash: " + FHLSLMaterialUtilities::HashString(StringToHash);
}