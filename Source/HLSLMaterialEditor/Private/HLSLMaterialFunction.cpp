// Copyright 2021 Phyronnaz

#include "HLSLMaterialFunction.h"
#include "HLSLMaterialUtilities.h"

FString FHLSLMaterialFunction::GenerateHashedString(const FString& BaseHash) const
{
	const FString PluginHashVersion = "1";
	const FString StringToHash =
		BaseHash +
		PluginHashVersion + " " +
		FString::FromInt(StartLine) + " " +
		Comment + " " +
		ReturnType + " " +
		Name + "(" +
		FString::Join(Arguments, TEXT(",")) + ")" +
		Body;

	return "HLSL Hash: " + FHLSLMaterialUtilities::HashString(StringToHash);
}