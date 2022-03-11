// Copyright 2021 Phyronnaz

#pragma once

#include "CoreMinimal.h"
#include "Materials/MaterialExpressionCustom.h"
#include "Materials/MaterialExpressionFunctionInput.h"

class IMaterialEditor;
class UHLSLMaterialFunctionLibrary;
struct FHLSLMaterialFunction;

class FHLSLMaterialFunctionGenerator
{
public:
	static FString GenerateFunction(
		UHLSLMaterialFunctionLibrary& Library,
		const TArray<FString>& IncludeFilePaths,
		const TArray<FCustomDefine>& AdditionalDefines,
		FHLSLMaterialFunction Function,
		FMaterialUpdateContext& UpdateContext);

private:
	struct FPin
	{
		const FString Name;
		const FString Type;
		const bool bIsConst;
		const bool bIsOutput;
		const bool bIsInternal;
		const FString DefaultValue;
		const FString ToolTip;
		const TMap<FString, FString> Metadata;

		FPin(
			const FString& Name,
			const FString& Type,
			bool bIsConst,
			bool bIsOutput,
			bool bIsInternal,
			const FString& DefaultValue,
			const FString& ToolTip,
			const TMap<FString, FString>& Metadata)
			: Name(Name)
			, Type(Type)
			, bIsConst(bIsConst)
			, bIsOutput(bIsOutput)
			, bIsInternal(bIsInternal)
			, DefaultValue(DefaultValue)
			, ToolTip(ToolTip)
			, Metadata(Metadata)
		{
		}

		EFunctionInputType FunctionInputType = {};
		TOptional<ECustomMaterialOutputType> CustomOutputType;

		bool bDefaultValueBool = false;
		FVector4 DefaultValueVector{ ForceInit };

		FString ParseTypeAndDefaultValue();
	};

	static constexpr const TCHAR* META_Expose = TEXT("Expose");
	static constexpr const TCHAR* META_Category = TEXT("Category");

	static FString GenerateFunctionCode(const UHLSLMaterialFunctionLibrary& Library, const FHLSLMaterialFunction& Function, const FString& Declarations);
	static bool ParseDefaultValue(const FString& DefaultValue, int32 Dimension, FVector4& OutValue);
	static FString GenerateTooltip(const FString& ParamName, const FString& FunctionComment);
	static TMap<FString, FString> GenerateMetadata(const FString& Metadata);
		
	static IMaterialEditor* FindMaterialEditorForAsset(UObject* InAsset);
	static UObject* CreateAsset(FString AssetName, FString FolderPath, UClass* Class, FString& OutError);

	template<typename T>
	static T* CreateAsset(FString AssetName, FString FolderPath, FString& OutError)
	{
		return CastChecked<T>(CreateAsset(AssetName, FolderPath, T::StaticClass(), OutError), ECastCheckedType::NullAllowed);
	}
};