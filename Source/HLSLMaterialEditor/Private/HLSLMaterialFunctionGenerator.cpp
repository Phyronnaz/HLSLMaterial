// Copyright 2021 Phyronnaz

#include "HLSLMaterialFunctionGenerator.h"
#include "HLSLMaterialFunction.h"
#include "HLSLMaterialUtilities.h"
#include "HLSLMaterialErrorHook.h"
#include "HLSLMaterialFunctionLibrary.h"

#include "Misc/ScopeExit.h"
#include "IMaterialEditor.h"
#include "AssetToolsModule.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Framework/Notifications/NotificationManager.h"

#include "MaterialGraph/MaterialGraph.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialExpressionComment.h"
#include "Materials/MaterialExpressionCustom.h"
#include "Materials/MaterialExpressionFunctionInput.h"
#include "Materials/MaterialExpressionFunctionOutput.h"

FString FHLSLMaterialFunctionGenerator::GenerateFunction(
	UHLSLMaterialFunctionLibrary& Library,
	const TArray<FString>& IncludeFilePaths,
	const TArray<FCustomDefine>& AdditionalDefines,
	FHLSLMaterialFunction Function,
	FMaterialUpdateContext& UpdateContext)
{
	TSoftObjectPtr<UMaterialFunction>* MaterialFunctionPtr = Library.MaterialFunctions.FindByPredicate([&](TSoftObjectPtr<UMaterialFunction> InFunction)
	{
		return InFunction && InFunction->GetFName() == *Function.Name;
	});
	if (!MaterialFunctionPtr)
	{
		Library.MarkPackageDirty();
		MaterialFunctionPtr = &Library.MaterialFunctions.Add_GetRef(nullptr);
	}

	UMaterialFunction* MaterialFunction = MaterialFunctionPtr->Get();
	if (!MaterialFunction)
	{
		FString BasePath = FPackageName::ObjectPathToPackageName(Library.GetPathName());
		if (Library.bPutFunctionsInSubdirectory)
		{
			BasePath += "_GeneratedFunctions";
		}
		else
		{
			BasePath = FPaths::GetPath(BasePath);
		}

		FString Error;
		MaterialFunction = CreateAsset<UMaterialFunction>(Function.Name, BasePath, Error);

		if (!Error.IsEmpty())
		{
			ensure(!MaterialFunction);
			return Error;
		}
	}
	if (!MaterialFunction)
	{
		return "Failed to create asset";
	}
	if (*MaterialFunctionPtr != MaterialFunction)
	{
		Library.MarkPackageDirty();
	}
	*MaterialFunctionPtr = MaterialFunction;

	for (UMaterialExpressionComment* Comment : MaterialFunction->FunctionEditorComments)
	{
		if (Comment && Comment->Text.Contains(Function.HashedString))
		{
			UE_LOG(LogHLSLMaterial, Log, TEXT("%s already up to date"), *Function.Name);
			return {};
		}
	}

	TMap<FName, FGuid> FunctionInputGuids;
	TMap<FName, FGuid> FunctionOutputGuids;
	for (UMaterialExpression* Expression : MaterialFunction->FunctionExpressions)
	{
		if (UMaterialExpressionFunctionInput* FunctionInput = Cast<UMaterialExpressionFunctionInput>(Expression))
		{
			FunctionInputGuids.Add(FunctionInput->InputName, FunctionInput->Id);
		}
		if (UMaterialExpressionFunctionOutput* FunctionOutput = Cast<UMaterialExpressionFunctionOutput>(Expression))
		{
			FunctionOutputGuids.Add(FunctionOutput->OutputName, FunctionOutput->Id);
		}
	}
	MaterialFunction->FunctionExpressions.Empty();
	MaterialFunction->FunctionEditorComments.Empty();

	{
		FString Description;
		Description = Function.Comment
			.Replace(TEXT("// "), TEXT(""))
			.Replace(TEXT("\t"), TEXT(" "))
			.Replace(TEXT("@param "), TEXT(""));

		Description.TrimStartAndEndInline();
		while (Description.Contains(TEXT("  ")))
		{
			Description.ReplaceInline(TEXT("  "), TEXT(" "));
		}
		while (Description.Contains(TEXT("\n ")))
		{
			Description.ReplaceInline(TEXT("\n "), TEXT("\n"));
		}

		FString FinalDescription;
		// Force ConvertToMultilineToolTip(40) to do something nice
		for (const TCHAR Char : Description)
		{
			if (Char == TEXT('\n'))
			{
				while (FinalDescription.Len() % 41 != 0)
				{
					FinalDescription += TEXT(' ');
				}
			}

			FinalDescription += Char;
		}

		MaterialFunction->Description = FinalDescription;
	}

	MaterialFunction->bExposeToLibrary = true;

	ON_SCOPE_EXIT
	{
		MaterialFunction->StateId = FGuid::NewGuid();
		MaterialFunction->MarkPackageDirty();
	};

	struct FPin
	{
		FName Name;
		FString ToolTip;
		EFunctionInputType FunctionType;
		ECustomMaterialOutputType CustomOutputType;
	};
	TArray<FPin> Inputs;
	TArray<FPin> Outputs;

	if (Function.ReturnType != "void")
	{
		return "Return type needs to be void";
	}
	for (FString Argument : Function.Arguments)
	{
		Argument.TrimStartAndEndInline();

		bool bIsOutput = false;
		if (Argument.StartsWith("out"))
		{
			bIsOutput = true;
			Argument.RemoveFromStart("out");
			Argument.TrimStartAndEndInline();
		}

		TArray<FString> TypeAndName;
		Argument.ParseIntoArray(TypeAndName, TEXT(" "));
		if (TypeAndName.Num() != 2)
		{
			return "Invalid arguments syntax";
		}

		const FString Type = TypeAndName[0];
		const FString Name = TypeAndName[1];
		EFunctionInputType FunctionInputType = {};
		ECustomMaterialOutputType CustomOutputType = {};

		if (Type == "float")
		{
			FunctionInputType = FunctionInput_Scalar;
			CustomOutputType = CMOT_Float1;
		}
		else if (Type == "float2")
		{
			FunctionInputType = FunctionInput_Vector2;
			CustomOutputType = CMOT_Float2;
		}
		else if (Type == "float3")
		{
			FunctionInputType = FunctionInput_Vector3;
			CustomOutputType = CMOT_Float3;
		}
		else if (Type == "float4")
		{
			FunctionInputType = FunctionInput_Vector4;
			CustomOutputType = CMOT_Float4;
		}
		else if (Type == "Texture2D")
		{
			FunctionInputType = FunctionInput_Texture2D;
		}
		else if (Type == "TextureCube")
		{
			FunctionInputType = FunctionInput_TextureCube;
		}
		else if (Type == "Texture2DArray")
		{
			FunctionInputType = FunctionInput_Texture2DArray;
		}
		else if (Type == "TextureExternal")
		{
			FunctionInputType = FunctionInput_TextureExternal;
		}
		else if (Type == "Texture3D")
		{
			FunctionInputType = FunctionInput_VolumeTexture;
		}
		else
		{
			return "Invalid argument type: " + Type;
		}

		FString ToolTip;
		int32 Index = 0;
		while (Index < Function.Comment.Len())
		{
			Index = Function.Comment.Find(TEXT("@param"), ESearchCase::IgnoreCase, ESearchDir::FromStart, Index);
			if (Index == -1)
			{
				break;
			}

			Index += FString(TEXT("@param")).Len();

			while (Index < Function.Comment.Len() && FChar::IsWhitespace(Function.Comment[Index]))
			{
				Index++;
			}

			FString ParamName;
			while (Index < Function.Comment.Len() && !FChar::IsWhitespace(Function.Comment[Index]))
			{
				ParamName += Function.Comment[Index++];
			}

			if (ParamName != Name)
			{
				continue;
			}

			while (Index < Function.Comment.Len() && FChar::IsWhitespace(Function.Comment[Index]))
			{
				Index++;
			}

			while (Index < Function.Comment.Len() && !FChar::IsLinebreak(Function.Comment[Index]))
			{
				ToolTip += Function.Comment[Index++];
			}
			ToolTip.TrimStartAndEndInline();

			break;
		}

		(bIsOutput ? Outputs : Inputs).Add({ *Name, ToolTip, FunctionInputType, CustomOutputType });
	}

	TArray<UMaterialExpressionFunctionInput*> FunctionInputs;
	for (int32 Index = 0; Index < Inputs.Num(); Index++)
	{
		FPin& Input = Inputs[Index];

		UMaterialExpressionFunctionInput* Expression = NewObject<UMaterialExpressionFunctionInput>(MaterialFunction);
		Expression->MaterialExpressionGuid = FGuid::NewGuid();
		Expression->Id = FunctionInputGuids.FindRef(Input.Name);
		if (!Expression->Id.IsValid())
		{
			Expression->Id = FGuid::NewGuid();
		}
		Expression->SortPriority = Index;
		Expression->InputName = Input.Name;
		Expression->InputType = Input.FunctionType;
		Expression->Description = Input.ToolTip;
		Expression->MaterialExpressionEditorX = 0;
		Expression->MaterialExpressionEditorY = 200 * Index;

		FunctionInputs.Add(Expression);
		MaterialFunction->FunctionExpressions.Add(Expression);
	}

	TArray<UMaterialExpressionFunctionOutput*> FunctionOutputs;
	for (int32 Index = 0; Index < Outputs.Num(); Index++)
	{
		FPin& Output = Outputs[Index];

		UMaterialExpressionFunctionOutput* Expression = NewObject<UMaterialExpressionFunctionOutput>(MaterialFunction);
		Expression->MaterialExpressionGuid = FGuid::NewGuid();
		Expression->Id = FunctionOutputGuids.FindRef(Output.Name);
		if (!Expression->Id.IsValid())
		{
			Expression->Id = FGuid::NewGuid();
		}
		Expression->SortPriority = Index;
		Expression->OutputName = Output.Name;
		Expression->Description = Output.ToolTip;
		Expression->MaterialExpressionEditorX = 1000;
		Expression->MaterialExpressionEditorY = 200 * Index;

		FunctionOutputs.Add(Expression);
		MaterialFunction->FunctionExpressions.Add(Expression);
	}

	UMaterialExpressionCustom* MaterialExpressionCustom = NewObject<UMaterialExpressionCustom>(MaterialFunction);
	MaterialExpressionCustom->MaterialExpressionGuid = FGuid::NewGuid();
	MaterialExpressionCustom->OutputType = CMOT_Float1;
	MaterialExpressionCustom->Code = GenerateFunctionCode(Library, Function);
	MaterialExpressionCustom->MaterialExpressionEditorX = 500;
	MaterialExpressionCustom->MaterialExpressionEditorY = 0;
	MaterialExpressionCustom->IncludeFilePaths = IncludeFilePaths;
	MaterialExpressionCustom->AdditionalDefines = AdditionalDefines;
	MaterialFunction->FunctionExpressions.Add(MaterialExpressionCustom);

	MaterialExpressionCustom->Inputs.Reset();
	for (int32 Index = 0; Index < Inputs.Num(); Index++)
	{
		FCustomInput& Input = MaterialExpressionCustom->Inputs.Add_GetRef({ Inputs[Index].Name });
		Input.Input.Connect(0, FunctionInputs[Index]);
	}
	for (int32 Index = 0; Index < Outputs.Num(); Index++)
	{
		MaterialExpressionCustom->AdditionalOutputs.Add({ Outputs[Index].Name, Outputs[Index].CustomOutputType });
	}

	MaterialExpressionCustom->PostEditChange();
	for (int32 Index = 0; Index < Outputs.Num(); Index++)
	{
		// + 1 as default output pin is result
		FunctionOutputs[Index]->GetInput(0)->Connect(Index + 1, MaterialExpressionCustom);
	}

	{
		UMaterialExpressionComment* Comment = NewObject<UMaterialExpressionComment>(MaterialFunction);
		Comment->MaterialExpressionGuid = FGuid::NewGuid();
		Comment->MaterialExpressionEditorX = 0;
		Comment->MaterialExpressionEditorY = -200;
		Comment->SizeX = 1000;
		Comment->SizeY = 100;
		Comment->Text = "DO NOT MODIFY THIS\nAutogenerated from " + Library.File.FilePath + "\nLibrary " + Library.GetPathName() + "\n" + Function.HashedString;
		MaterialFunction->FunctionEditorComments.Add(Comment);
	}

	// Update open material editors
	for (TObjectIterator<UMaterial> It; It; ++It)
	{
		UMaterial* CurrentMaterial = *It;
		if (!CurrentMaterial->bIsPreviewMaterial)
		{
			continue;
		}

		IMaterialEditor* MaterialEditor = FindMaterialEditorForAsset(CurrentMaterial);
		if (!MaterialEditor)
		{
			continue;
		}

		UpdateContext.AddMaterial(CurrentMaterial);

		// Propagate the function change to this material
		CurrentMaterial->PreEditChange(nullptr);
		CurrentMaterial->PostEditChange();
		CurrentMaterial->MarkPackageDirty();

		if (CurrentMaterial->MaterialGraph)
		{
			CurrentMaterial->MaterialGraph->RebuildGraph();
		}

		MaterialEditor->NotifyExternalMaterialChange();
	}
	
	FNotificationInfo Info(FText::Format(INVTEXT("{0} updated"), FText::FromString(Function.Name)));
	Info.ExpireDuration = 5.f;
	Info.CheckBoxState = ECheckBoxState::Checked;
	FSlateNotificationManager::Get().AddNotification(Info);

	return {};
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FString FHLSLMaterialFunctionGenerator::GenerateFunctionCode(const UHLSLMaterialFunctionLibrary& Library, const FHLSLMaterialFunction& Function)
{
	FString Code = Function.Body.Replace(TEXT("return"), TEXT("return 0.f"));

	if (Library.bAccurateErrors)
	{
		Code = FString::Printf(TEXT(
			"#line %d \"%s%s%s\"\n%s\n#line 10000 \"Error occured outside of Custom HLSL node, line number will be inaccurate. "
			"Untick bAccurateErrors on your HLSL library to fix this (%s)\""),
			Function.StartLine + 1,
			FHLSLMaterialErrorHook::PathPrefix,
			*Library.File.FilePath,
			FHLSLMaterialErrorHook::PathSuffix,
			*Code,
			*Library.GetPathName());
	}

	return FString::Printf(TEXT("// START %s\n\n%s\n\n// END %s\n\nreturn 0.f;\n//%s\n"), *Function.Name, *Code, *Function.Name, *Function.HashedString);
}

IMaterialEditor* FHLSLMaterialFunctionGenerator::FindMaterialEditorForAsset(UObject* InAsset)
{
	// From MaterialEditor\Private\MaterialEditingLibrary.cpp

	if (IAssetEditorInstance* AssetEditorInstance = (InAsset != nullptr) ? GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->FindEditorForAsset(InAsset, false) : nullptr)
	{
		// Ensure this is not a UMaterialInstanceDynamic, as that doesn't use IMaterialEditor as its editor
		if (!InAsset->IsA(UMaterialInstanceDynamic::StaticClass()))
		{
			return static_cast<IMaterialEditor*>(AssetEditorInstance);
		}
	}

	return nullptr;
}

UObject* FHLSLMaterialFunctionGenerator::CreateAsset(FString AssetName, FString FolderPath, UClass* Class, FString& OutError)
{
	const FString PackageName = FolderPath / AssetName;

	{
		FString NewPackageName;
		FString NewAssetName;

		const FAssetToolsModule& AssetToolsModule = FModuleManager::GetModuleChecked<FAssetToolsModule>("AssetTools");
		AssetToolsModule.Get().CreateUniqueAssetName(PackageName, {}, NewPackageName, NewAssetName);

		if (NewAssetName != AssetName)
		{
			OutError = FString::Printf(
				TEXT("Asset %s already exists! Add it back to the HLSL library MaterialFunctions if you want it to be updated"),
				*PackageName);
			return nullptr;
		}
	}

	UPackage* Package = CreatePackage(*PackageName);
	if (!ensure(Package))
	{
		return nullptr;
	}

	auto* Object = NewObject<UObject>(Package, Class, *AssetName, RF_Public | RF_Standalone);
	if (!ensure(Object))
	{
		return nullptr;
	}

	Object->MarkPackageDirty();
	FAssetRegistryModule::AssetCreated(Object);

	return Object;
}