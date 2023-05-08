// Copyright Phyronnaz

#include "HLSLMaterialFunction.h"
#include "HLSLMaterialUtilities.h"

FString FHLSLMaterialFunction::GenerateHashedString(const FString &BaseHash) const
{
	FString StringToHash =
		BaseHash +
		// Changes too often
		// FString::FromInt(StartLine) + " " +
		Comment + " " +
		Metadata + " " +
		ReturnType + " " +
		Name + "(" +
		FString::Join(Arguments, TEXT(",")) + ")" +
		Body;

	StringToHash.ReplaceInline(TEXT("\t"), TEXT(" "));
	StringToHash.ReplaceInline(TEXT("\n"), TEXT(" "));

	while (StringToHash.ReplaceInline(TEXT("  "), TEXT(" ")))
	{
	}

	return "HLSL Hash: " + FHLSLMaterialUtilities::HashString(StringToHash);
}