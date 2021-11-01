// Copyright 2021 Phyronnaz

#include "HLSLMaterialFunctionGenerator.h"
#include "HLSLMaterialFunction.h"
#include "HLSLMaterialUtilities.h"
#include "HLSLMaterialErrorHook.h"
#include "HLSLMaterialFunctionLibrary.h"

#include "Misc/ScopeExit.h"
#include "IMaterialEditor.h"
#include "AssetToolsModule.h"
#include "Internationalization/Regex.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Framework/Notifications/NotificationManager.h"

#include "MaterialGraph/MaterialGraph.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialExpressionComment.h"
#include "Materials/MaterialExpressionCustom.h"
#include "Materials/MaterialExpressionStaticBool.h"
#include "Materials/MaterialExpressionStaticSwitch.h"
#include "Materials/MaterialExpressionFunctionInput.h"
#include "Materials/MaterialExpressionFunctionOutput.h"
#include "Materials/MaterialExpressionTextureCoordinate.h"

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

	struct FPin
	{
		FString Name;
		FString Type;
		FString ToolTip;
		bool bIsConst = false;
		EFunctionInputType FunctionInputType = {};
		TOptional<ECustomMaterialOutputType> CustomOutputType;

		FString DefaultValueText;
		bool bDefaultValueBool = false;
		FVector4 DefaultValueVector{ ForceInit };
	};
	TArray<FPin> Inputs;
	TArray<FPin> Outputs;

	if (Function.ReturnType != "void")
	{
		return "Return type needs to be void";
	}

	for (const FString& Argument : Function.Arguments)
	{
		FRegexPattern RegexPattern(R"_(^\s*(?:(const\s+)?|(out\s+)?)(\w*)\s+(\w*)(?:\s*=\s*(.+))?\s*$)_");
		FRegexMatcher RegexMatcher(RegexPattern, Argument);
		if (!RegexMatcher.FindNext())
		{
			return "Invalid arguments syntax";
		}

		const bool bIsConst = !RegexMatcher.GetCaptureGroup(1).IsEmpty();
		const bool bIsOutput = !RegexMatcher.GetCaptureGroup(2).IsEmpty();
		const FString Type = RegexMatcher.GetCaptureGroup(3);
		const FString Name = RegexMatcher.GetCaptureGroup(4);
		const FString DefaultValue = RegexMatcher.GetCaptureGroup(5);

		if ((Type == "FMaterialPixelParameters" || Type == "FMaterialVertexParameters") &&
			Name == "Parameters")
		{
			// Allow passing Parameters explicitly
			// The Custom node will handle passing them
			continue;
		}
		if (Type == "SamplerState")
		{
			FString TextureName = Name;
			if (!TextureName.RemoveFromEnd(TEXT("Sampler")))
			{
				return "Invalid sampler parameter: " + Name + ". Sampler parameters should be named [TextureParameterName]Sampler";
			}

			// The Custom node will add samplers
			continue;
		}

		FPin Pin;
		Pin.Name = Name;
		Pin.Type = Type;
		Pin.bIsConst = bIsConst;
		Pin.DefaultValueText = DefaultValue;

		const FString DefaultValueError = Name + ": invalid default value for type " + Type + ": " + DefaultValue;
		
		if (Type == "bool")
		{
			Pin.FunctionInputType = FunctionInput_StaticBool;

			if (!DefaultValue.IsEmpty())
			{
				if (DefaultValue == "true")
				{
					Pin.bDefaultValueBool = true;
				}
				else if (DefaultValue == "false")
				{
					Pin.bDefaultValueBool = false;
				}
				else
				{
					return DefaultValueError;
				}
			}
		}
		else if (Type == "int")
		{
			Pin.FunctionInputType = FunctionInput_Scalar;

			if (!DefaultValue.IsEmpty() && !ParseDefaultValue(DefaultValue, 1, Pin.DefaultValueVector))
			{
				return DefaultValueError;
			}
		}
		else if (Type == "float")
		{
			Pin.FunctionInputType = FunctionInput_Scalar;
			Pin.CustomOutputType = CMOT_Float1;

			if (!DefaultValue.IsEmpty() && !ParseDefaultValue(DefaultValue, 1, Pin.DefaultValueVector))
			{
				return DefaultValueError;
			}
		}
		else if (Type == "float2")
		{
			Pin.FunctionInputType = FunctionInput_Vector2;
			Pin.CustomOutputType = CMOT_Float2;
			
			if (!DefaultValue.IsEmpty() && !ParseDefaultValue(DefaultValue, 2, Pin.DefaultValueVector))
			{
				return DefaultValueError;
			}
		}
		else if (Type == "float3")
		{
			Pin.FunctionInputType = FunctionInput_Vector3;
			Pin.CustomOutputType = CMOT_Float3;
			
			if (!DefaultValue.IsEmpty() && !ParseDefaultValue(DefaultValue, 3, Pin.DefaultValueVector))
			{
				return DefaultValueError;
			}
		}
		else if (Type == "float4")
		{
			Pin.FunctionInputType = FunctionInput_Vector4;
			Pin.CustomOutputType = CMOT_Float4;
			
			if (!DefaultValue.IsEmpty() && !ParseDefaultValue(DefaultValue, 4, Pin.DefaultValueVector))
			{
				return DefaultValueError;
			}
		}
		else if (Type == "Texture2D")
		{
			Pin.FunctionInputType = FunctionInput_Texture2D;
			
			if (!DefaultValue.IsEmpty())
			{
				return DefaultValueError;
			}
		}
		else if (Type == "TextureCube")
		{
			Pin.FunctionInputType = FunctionInput_TextureCube;
			
			if (!DefaultValue.IsEmpty())
			{
				return DefaultValueError;
			}
		}
		else if (Type == "Texture2DArray")
		{
			Pin.FunctionInputType = FunctionInput_Texture2DArray;
			
			if (!DefaultValue.IsEmpty())
			{
				return DefaultValueError;
			}
		}
		else if (Type == "TextureExternal")
		{
			Pin.FunctionInputType = FunctionInput_TextureExternal;
			
			if (!DefaultValue.IsEmpty())
			{
				return DefaultValueError;
			}
		}
		else if (Type == "Texture3D")
		{
			Pin.FunctionInputType = FunctionInput_VolumeTexture;
			
			if (!DefaultValue.IsEmpty())
			{
				return DefaultValueError;
			}
		}
		else
		{
			return "Invalid argument type: " + Type;
		}

		if (bIsOutput && !Pin.CustomOutputType.IsSet())
		{
			return "Invalid argument type for an output: " + Type;
		}

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
				Pin.ToolTip += Function.Comment[Index++];
			}
			Pin.ToolTip.TrimStartAndEndInline();

			break;
		}

		(bIsOutput ? Outputs : Inputs).Add(Pin);
	}

	// Detect used texture coordinates
	int32 MaxTexCoordinateUsed = -1;
	{
		FRegexPattern RegexPattern(R"_(Parameters.TexCoords\[([0-9]+)\])_");
		FRegexMatcher RegexMatcher(RegexPattern, Function.Body);
		while (RegexMatcher.FindNext())
		{
			MaxTexCoordinateUsed = FMath::Max(MaxTexCoordinateUsed, FCString::Atoi(*RegexMatcher.GetCaptureGroup(1)));
		}
	}

	///////////////////////////////////////////////////////////////////////////////////
	//// Past this point, try to never error out as it'll break existing functions ////
	///////////////////////////////////////////////////////////////////////////////////

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
	MaterialFunction->LibraryCategoriesText = Library.Categories;

	ON_SCOPE_EXIT
	{
		MaterialFunction->StateId = FGuid::NewGuid();
		MaterialFunction->MarkPackageDirty();
	};

	TArray<int32> StaticBoolParameters;
	for (int32 Index = 0; Index < Inputs.Num(); Index++)
	{
		if (Inputs[Index].FunctionInputType == FunctionInput_StaticBool)
		{
			StaticBoolParameters.Add(Index);
		}
	}

	TArray<UMaterialExpressionFunctionInput*> FunctionInputs;
	for (int32 Index = 0; Index < Inputs.Num(); Index++)
	{
		const FPin& Input = Inputs[Index];

		UMaterialExpressionFunctionInput* Expression = NewObject<UMaterialExpressionFunctionInput>(MaterialFunction);
		Expression->MaterialExpressionGuid = FGuid::NewGuid();
		Expression->Id = FunctionInputGuids.FindRef(*Input.Name);
		if (!Expression->Id.IsValid())
		{
			Expression->Id = FGuid::NewGuid();
		}
		Expression->bCollapsed = true;
		Expression->SortPriority = Index;
		Expression->InputName = *Input.Name;
		Expression->InputType = Input.FunctionInputType;
		Expression->Description = Input.ToolTip;
		Expression->MaterialExpressionEditorX = 0;
		Expression->MaterialExpressionEditorY = 200 * Index;

		FunctionInputs.Add(Expression);
		MaterialFunction->FunctionExpressions.Add(Expression);

		if (!Input.DefaultValueText.IsEmpty())
		{
			Expression->bUsePreviewValueAsDefault = true;

			if (!Expression->Description.IsEmpty())
			{
				Expression->Description += "\n";
			}
			Expression->Description += "Default Value = " + Input.DefaultValueText;
			Expression->InputName = *(Input.Name + " ( = " + Input.DefaultValueText + ")");

			if (Input.FunctionInputType == FunctionInput_StaticBool)
			{
				UMaterialExpressionStaticBool* StaticBool = NewObject<UMaterialExpressionStaticBool>(MaterialFunction);
				StaticBool->MaterialExpressionGuid = FGuid::NewGuid();
				StaticBool->MaterialExpressionEditorX = Expression->MaterialExpressionEditorX - 200;
				StaticBool->MaterialExpressionEditorY = Expression->MaterialExpressionEditorY;
				StaticBool->Value = Input.bDefaultValueBool;
				MaterialFunction->FunctionExpressions.Add(StaticBool);
				Expression->Preview.Connect(0, StaticBool);
			}
			else
			{
				Expression->PreviewValue = Input.DefaultValueVector;
			}
		}
	}

	TArray<UMaterialExpressionFunctionOutput*> FunctionOutputs;
	for (int32 Index = 0; Index < Outputs.Num(); Index++)
	{
		const FPin& Output = Outputs[Index];

		UMaterialExpressionFunctionOutput* Expression = NewObject<UMaterialExpressionFunctionOutput>(MaterialFunction);
		Expression->MaterialExpressionGuid = FGuid::NewGuid();
		Expression->Id = FunctionOutputGuids.FindRef(*Output.Name);
		if (!Expression->Id.IsValid())
		{
			Expression->Id = FGuid::NewGuid();
		}
		Expression->bCollapsed = true;
		Expression->SortPriority = Index;
		Expression->OutputName = *Output.Name;
		Expression->Description = Output.ToolTip;
		Expression->MaterialExpressionEditorX = (StaticBoolParameters.Num() + 2) * 500;
		Expression->MaterialExpressionEditorY = 200 * Index;

		FunctionOutputs.Add(Expression);
		MaterialFunction->FunctionExpressions.Add(Expression);
	}

	struct FOutputPin
	{
		UMaterialExpression* Expression = nullptr;
		int32 Index = 0;
	};

	TArray<TArray<FOutputPin>> AllOutputPins;
	for (int32 Width = 0; Width < 1 << StaticBoolParameters.Num(); Width++)
	{
		FString Declarations;
		for (int32 Index = 0; Index < StaticBoolParameters.Num(); Index++)
		{
			bool bValue = Width & (1 << Index);
			// Invert the value, as switches take True as first pin
			bValue = !bValue;
			Declarations += "const bool " + Inputs[StaticBoolParameters[Index]].Name + " = " + (bValue ? "true" : "false") + ";\n";
		}
		for (const FPin& Input : Inputs)
		{
			FString Cast;

			switch (Input.FunctionInputType)
			{
			case FunctionInput_Scalar:
			case FunctionInput_Vector2:
			case FunctionInput_Vector3:
			case FunctionInput_Vector4:
			{
				// Cast float to int if needed
				Cast = Input.Type;
			}
			break;
			case FunctionInput_Texture2D:
			case FunctionInput_TextureCube:
			case FunctionInput_Texture2DArray:
			case FunctionInput_VolumeTexture:
			case FunctionInput_TextureExternal:
			{
				Declarations += (Input.bIsConst ? "const SamplerState " : "SamplerState ") + Input.Name + "Sampler" + " = INTERNAL_IN_" + Input.Name + "Sampler;\n";
			}
			break;
			case FunctionInput_StaticBool:
			case FunctionInput_MaterialAttributes:
			{
				// Nothing to fixup
			}
			break;
			case FunctionInput_MAX:
			default:
				ensure(false);
			}
			Declarations += (Input.bIsConst ? "const " : "") + Input.Type + " " + Input.Name + " = " + Cast + "(INTERNAL_IN_" + Input.Name + ");\n";
		}

		UMaterialExpressionCustom* MaterialExpressionCustom = NewObject<UMaterialExpressionCustom>(MaterialFunction);
		MaterialExpressionCustom->MaterialExpressionGuid = FGuid::NewGuid();
		MaterialExpressionCustom->bCollapsed = true;
		MaterialExpressionCustom->OutputType = CMOT_Float1;
		MaterialExpressionCustom->Code = GenerateFunctionCode(Library, Function, Declarations);
		MaterialExpressionCustom->MaterialExpressionEditorX = 500;
		MaterialExpressionCustom->MaterialExpressionEditorY = 200 * Width;
		MaterialExpressionCustom->IncludeFilePaths = IncludeFilePaths;
		MaterialExpressionCustom->AdditionalDefines = AdditionalDefines;
		MaterialFunction->FunctionExpressions.Add(MaterialExpressionCustom);

		MaterialExpressionCustom->Inputs.Reset();
		for (int32 Index = 0; Index < Inputs.Num(); Index++)
		{
			FPin& Input = Inputs[Index];
			if (Input.FunctionInputType == FunctionInput_StaticBool)
			{
				continue;
			}

			FCustomInput& CustomInput = MaterialExpressionCustom->Inputs.Emplace_GetRef();
			CustomInput.InputName = *("INTERNAL_IN_" + Input.Name);
			CustomInput.Input.Connect(0, FunctionInputs[Index]);
		}
		for (int32 Index = 0; Index < Outputs.Num(); Index++)
		{
			const FPin& Output = Outputs[Index];
			MaterialExpressionCustom->AdditionalOutputs.Add({ *Output.Name, Output.CustomOutputType.GetValue() });
		}

		if (MaxTexCoordinateUsed != -1)
		{
			// Create a dummy texture coordinate index to ensure NUM_TEX_COORD_INTERPOLATORS is correct

			UMaterialExpressionTextureCoordinate* TextureCoordinate = NewObject<UMaterialExpressionTextureCoordinate>(MaterialFunction);
			TextureCoordinate->MaterialExpressionGuid = FGuid::NewGuid();
			TextureCoordinate->bCollapsed = true;
			TextureCoordinate->CoordinateIndex = MaxTexCoordinateUsed;
			TextureCoordinate->MaterialExpressionEditorX = MaterialExpressionCustom->MaterialExpressionEditorX - 200;
			TextureCoordinate->MaterialExpressionEditorY = MaterialExpressionCustom->MaterialExpressionEditorY;
			MaterialFunction->FunctionExpressions.Add(TextureCoordinate);

			FCustomInput& CustomInput = MaterialExpressionCustom->Inputs.Emplace_GetRef();
			CustomInput.InputName = "DUMMY_COORDINATE_INPUT";
			CustomInput.Input.Connect(0, TextureCoordinate);
		}

		MaterialExpressionCustom->PostEditChange();

		TArray<FOutputPin>& OutputPins = AllOutputPins.Emplace_GetRef();
		for (int32 Index = 0; Index < Outputs.Num(); Index++)
		{
			// + 1 as default output pin is result
			OutputPins.Add({ MaterialExpressionCustom, Index + 1 });
		}
	}

	for (int32 Layer = 0; Layer < StaticBoolParameters.Num(); Layer++)
	{
		const TArray<TArray<FOutputPin>> PreviousAllOutputPins = MoveTemp(AllOutputPins);
		
		for (int32 Width = 0; Width < 1 << (StaticBoolParameters.Num() - Layer - 1); Width++)
		{
			TArray<FOutputPin>& OutputPins = AllOutputPins.Emplace_GetRef();
			for (int32 Index = 0; Index < Outputs.Num(); Index++)
			{
				UMaterialExpressionStaticSwitch* StaticSwitch = NewObject<UMaterialExpressionStaticSwitch>(MaterialFunction);
				StaticSwitch->MaterialExpressionGuid = FGuid::NewGuid();
				StaticSwitch->MaterialExpressionEditorX = (Layer + 2) * 500;
				StaticSwitch->MaterialExpressionEditorY = 200 * Width;
				MaterialFunction->FunctionExpressions.Add(StaticSwitch);

				const FOutputPin& OutputPinA = PreviousAllOutputPins[2 * Width + 0][Index];
				const FOutputPin& OutputPinB = PreviousAllOutputPins[2 * Width + 1][Index];

				StaticSwitch->A.Connect(OutputPinA.Index, OutputPinA.Expression);
				StaticSwitch->B.Connect(OutputPinB.Index, OutputPinB.Expression);
				StaticSwitch->Value.Connect(0, FunctionInputs[StaticBoolParameters[Layer]]);

				OutputPins.Add({ StaticSwitch, 0 });
			}
		}
	}

	ensure(AllOutputPins.Num() == 1);
	for (int32 Index = 0; Index < Outputs.Num(); Index++)
	{
		const FOutputPin& Pin = AllOutputPins[0][Index];
		FunctionOutputs[Index]->GetInput(0)->Connect(Pin.Index, Pin.Expression);
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

FString FHLSLMaterialFunctionGenerator::GenerateFunctionCode(const UHLSLMaterialFunctionLibrary& Library, const FHLSLMaterialFunction& Function, const FString& Declarations)
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

	return FString::Printf(TEXT("// START %s\n\n%s\n%s\n\n// END %s\n\nreturn 0.f;\n//%s\n"), *Function.Name, *Declarations, *Code, *Function.Name, *Function.HashedString);
}

bool FHLSLMaterialFunctionGenerator::ParseDefaultValue(const FString& DefaultValue, int32 Dimension, FVector4& OutValue)
{
	const FString FloatPattern = R"_(\s*((?:[0-9]*[.])?[0-9]*)f?\s*)_";

	const auto TryParseFloat = [&](const FString& String, float& OutFloatValue)
	{
		FRegexPattern RegexPattern("^" + FloatPattern + "$");
		FRegexMatcher RegexMatcher(RegexPattern, String);
		if (!RegexMatcher.FindNext())
		{
			return false;
		}

		OutFloatValue = FCString::Atof(*RegexMatcher.GetCaptureGroup(1));
		return true;
	};

	if (Dimension == 1)
	{
		return TryParseFloat(DefaultValue, OutValue.X);
	}
	else if (Dimension == 2)
	{
		float SingleValue;
		if (TryParseFloat(DefaultValue, SingleValue))
		{
			OutValue.X = SingleValue;
			OutValue.Y = SingleValue;

			return true;
		}

		FRegexPattern RegexPattern("^float2\\("+ FloatPattern + "," + FloatPattern + "\\)$");
		FRegexMatcher RegexMatcher(RegexPattern, DefaultValue);
		if (!RegexMatcher.FindNext())
		{
			return false;
		}

		OutValue.X = FCString::Atof(*RegexMatcher.GetCaptureGroup(1));
		OutValue.Y = FCString::Atof(*RegexMatcher.GetCaptureGroup(2));

		return true;
	}
	else if (Dimension == 3)
	{
		float SingleValue;
		if (TryParseFloat(DefaultValue, SingleValue))
		{
			OutValue.X = SingleValue;
			OutValue.Y = SingleValue;
			OutValue.Z = SingleValue;

			return true;
		}

		FRegexPattern RegexPattern("^float3\\("+ FloatPattern + "," + FloatPattern + "," + FloatPattern + "\\)$");
		FRegexMatcher RegexMatcher(RegexPattern, DefaultValue);
		if (!RegexMatcher.FindNext())
		{
			return false;
		}

		OutValue.X = FCString::Atof(*RegexMatcher.GetCaptureGroup(1));
		OutValue.Y = FCString::Atof(*RegexMatcher.GetCaptureGroup(2));
		OutValue.Z = FCString::Atof(*RegexMatcher.GetCaptureGroup(3));

		return true;
	}
	else
	{
		check(Dimension == 4);

		float SingleValue;
		if (TryParseFloat(DefaultValue, SingleValue))
		{
			OutValue.X = SingleValue;
			OutValue.Y = SingleValue;
			OutValue.Z = SingleValue;

			OutValue.Z = SingleValue;
			return true;
		}

		FRegexPattern RegexPattern("^float4\\("+ FloatPattern + "," + FloatPattern + "," + FloatPattern + "," + FloatPattern + "\\)$");
		FRegexMatcher RegexMatcher(RegexPattern, DefaultValue);
		if (!RegexMatcher.FindNext())
		{
			return false;
		}

		OutValue.X = FCString::Atof(*RegexMatcher.GetCaptureGroup(1));
		OutValue.Y = FCString::Atof(*RegexMatcher.GetCaptureGroup(2));
		OutValue.Z = FCString::Atof(*RegexMatcher.GetCaptureGroup(3));
		OutValue.W = FCString::Atof(*RegexMatcher.GetCaptureGroup(4));

		return true;
	}
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