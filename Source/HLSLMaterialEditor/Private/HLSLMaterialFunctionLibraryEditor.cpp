// Copyright 2021 Phyronnaz

#include "HLSLMaterialFunctionLibraryEditor.h"

#include "AssetToolsModule.h"
#include "HLSLMaterialUtilities.h"
#include "HLSLMaterialFunctionLibrary.h"

#include "Misc/ScopeExit.h"
#include "Misc/FileHelper.h"
#include "IMaterialEditor.h"
#include "MaterialEditingLibrary.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Framework/Notifications/NotificationManager.h"
#include "MaterialGraph/MaterialGraph.h"
#include "Materials/MaterialExpressionCustom.h"
#include "Materials/MaterialExpressionFunctionInput.h"
#include "Materials/MaterialExpressionFunctionOutput.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Widgets/Notifications/SNotificationList.h"

UHLSLMaterialFunctionLibraryFactory::UHLSLMaterialFunctionLibraryFactory()
{
	bCreateNew = true;
	bEditAfterNew = true;
	SupportedClass = UHLSLMaterialFunctionLibrary::StaticClass();
}

UObject* UHLSLMaterialFunctionLibraryFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	return NewObject<UObject>(InParent, Class, Name, Flags);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

HLSL_STARTUP_FUNCTION(EDelayedRegisterRunPhase::EndOfEngineInit, FVoxelMaterialExpressionLibraryEditor::Register);

void FVoxelMaterialExpressionLibraryEditor::Register()
{
	UHLSLMaterialFunctionLibrary::OnUpdate.AddStatic(&FVoxelMaterialExpressionLibraryEditor::Generate);

	// Delay to ensure the editor is fully loaded before searching for assets
	FHLSLMaterialUtilities::DelayedCall([]
	{
		const FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));

		// Force load all libraries that have bGenerateOnFileChange, to start their watchers
		TArray<FAssetData> AssetDatas;
		FARFilter Filer;
		Filer.ClassNames.Add(UHLSLMaterialFunctionLibrary::StaticClass()->GetFName());
		Filer.TagsAndValues.Add(GET_MEMBER_NAME_CHECKED(UHLSLMaterialFunctionLibrary, bUpdateOnFileChange), FString("true"));
		AssetRegistryModule.Get().GetAssets(Filer, AssetDatas);

		for (const FAssetData& AssetData : AssetDatas)
		{
			ensure(AssetData.GetAsset());
		}
	});
}

void FVoxelMaterialExpressionLibraryEditor::Generate(UHLSLMaterialFunctionLibrary& Library)
{
	ON_SCOPE_EXIT
	{
		Library.MarkPackageDirty();
	};

	const FString FullPath = Library.GetFilePath();
	if (!FPaths::FileExists(FullPath))
	{
		ShowMessage(ESeverity::Error, FString::Printf(TEXT("%s: invalid path %s"), *Library.GetName(), *Library.File.FilePath));
		return;
	}

	FString Text;
	if (!FFileHelper::LoadFileToString(Text, *FullPath))
	{
		FPlatformProcess::Sleep(0.1f);

		if (!FFileHelper::LoadFileToString(Text, *FullPath))
		{
			ShowMessage(ESeverity::Error, FString::Printf(TEXT("%s: failed to read %s"), *Library.GetName(), *Library.File.FilePath));
			return;
		}
	}

	struct FFunction
	{
		FString Comment;
		FString ReturnType;
		FString Name;
		TArray<FString> Arguments;
		FString Body;
	};

	TArray<FFunction> Functions;
	{
		enum class EScope
		{
			Global,
			FunctionComment,
			FunctionReturn,
			FunctionName,
			FunctionArgs,
			FunctionBodyStart,
			FunctionBody
		};

		EScope Scope = EScope::Global;
		int32 Index = 0;
		int32 ScopeDepth = 0;

		while (Index < Text.Len())
		{
			const TCHAR Char = Text[Index++];

			switch (Scope)
			{
			case EScope::Global:
			{
				if (FChar::IsWhitespace(Char))
				{
					continue;
				}

				if (Functions.Num() == 0 || !Functions.Last().ReturnType.IsEmpty())
				{
					Functions.Emplace();
				}
				Index--;

				if (Char == TEXT('/'))
				{
					Scope = EScope::FunctionComment;
				}
				else
				{
					Scope = EScope::FunctionReturn;
				}
			}
			break;
			case EScope::FunctionComment:
			{
				if (!FChar::IsLinebreak(Char))
				{
					Functions.Last().Comment += Char;
					continue;
				}
				Functions.Last().Comment += "\n";

				Scope = EScope::Global;
			}
			break;
			case EScope::FunctionReturn:
			{
				if (!FChar::IsWhitespace(Char))
				{
					Functions.Last().ReturnType += Char;
					continue;
				}

				Scope = EScope::FunctionName;
			}
			break;
			case EScope::FunctionName:
			{
				if (Char != TEXT('('))
				{
					if (!FChar::IsWhitespace(Char))
					{
						Functions.Last().Name += Char;
					}
					continue;
				}

				Scope = EScope::FunctionArgs;
			}
			break;
			case EScope::FunctionArgs:
			{
				if (Char != TEXT(')'))
				{
					if (Char == TEXT(','))
					{
						Functions.Last().Arguments.Emplace();
					}
					else
					{
						if (Functions.Last().Arguments.Num() == 0)
						{
							Functions.Last().Arguments.Emplace();
						}

						Functions.Last().Arguments.Last() += Char;
					}
					continue;
				}

				Scope = EScope::FunctionBodyStart;
			}
			break;
			case EScope::FunctionBodyStart:
			{
				ensure(ScopeDepth == 0);

				if (FChar::IsWhitespace(Char))
				{
					continue;
				}

				if (Char != TEXT('{'))
				{
					ShowMessage(ESeverity::Error, FString::Printf(TEXT("Invalid function body for %s in %s: missing {"), *Functions.Last().Name, *FullPath));
					return;
				}

				Scope = EScope::FunctionBody;
				ScopeDepth++;
			}
			break;
			case EScope::FunctionBody:
			{
				ensure(ScopeDepth > 0);

				if (Char == TEXT('{'))
				{
					ScopeDepth++;
				}
				if (Char == TEXT('}'))
				{
					ScopeDepth--;
				}

				if (ScopeDepth > 0)
				{
					Functions.Last().Body += Char;
					continue;
				}
				if (ScopeDepth < 0)
				{
					ShowMessage(ESeverity::Error, FString::Printf(TEXT("Invalid function body for %s in %s: too many }"), *Functions.Last().Name, *FullPath));
					return;
				}

				ensure(ScopeDepth == 0);
				Scope = EScope::Global;
			}
			break;
			default: ensure(false);
			}
		}

		if (Scope != EScope::Global)
		{
			ShowMessage(ESeverity::Error, FString::Printf(TEXT("Parsing error in %s"), *FullPath));
			return;
		}
	}

	Library.MaterialFunctions.RemoveAll([&](TSoftObjectPtr<UMaterialFunction> InFunction)
	{
		return !InFunction.IsValid();
	});
	
	FMaterialUpdateContext UpdateContext;
	for (FFunction Function : Functions)
	{
		TSoftObjectPtr<UMaterialFunction>* MaterialFunctionPtr = Library.MaterialFunctions.FindByPredicate([&](TSoftObjectPtr<UMaterialFunction> InFunction)
		{
			return ensure(InFunction) && InFunction->GetFName() == *Function.Name;
		});
		if (!MaterialFunctionPtr)
		{
			MaterialFunctionPtr = &Library.MaterialFunctions.Add_GetRef(nullptr);
		}

		UMaterialFunction* MaterialFunction = MaterialFunctionPtr->Get();
		if (!MaterialFunction)
		{
			const FString BasePath = FPackageName::ObjectPathToPackageName(Library.GetPathName()) + "_GeneratedFunctions";

			MaterialFunction = CreateAsset<UMaterialFunction>(Function.Name, BasePath);
		}
		if (!ensure(MaterialFunction))
		{
			continue;
		}
		*MaterialFunctionPtr = MaterialFunction;

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
			for (int32 Index = 0; Index < Description.Len(); Index++)
			{
				const TCHAR Char = Description[Index];

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
			ShowMessage(ESeverity::Error, FString::Printf(TEXT("Parsing error in %s: return type of %s needs to be void"), *FullPath, *Function.Name));
			return;
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
				ShowMessage(ESeverity::Error, FString::Printf(TEXT("Parsing error in %s: invalid arguments for %s"), *FullPath, *Function.Name));
				return;
			}

			const FString Type = TypeAndName[0];
			const FString Name = TypeAndName[1];
			EFunctionInputType FunctionInputType;
			ECustomMaterialOutputType CustomOutputType;

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
			else
			{
				ShowMessage(ESeverity::Error, FString::Printf(TEXT("Parsing error in %s: invalid argument type for %s: %s"), *FullPath, *Function.Name, *Type));
				return;
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
		MaterialExpressionCustom->Code = Function.Body.Replace(TEXT("return"), TEXT("return 0.f"));
		MaterialExpressionCustom->Code = FString::Printf(TEXT("// START %s\n\n%s\n\n// END %s\n\nreturn 0.f;"), *Function.Name, *MaterialExpressionCustom->Code, *Function.Name);
		MaterialExpressionCustom->MaterialExpressionEditorX = 500;
		MaterialExpressionCustom->MaterialExpressionEditorY = 0;
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

		ShowMessage(ESeverity::Info, FString::Printf(TEXT("%s updated"), *MaterialFunction->GetName()));
	}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

IMaterialEditor* FVoxelMaterialExpressionLibraryEditor::FindMaterialEditorForAsset(UObject* InAsset)
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

UObject* FVoxelMaterialExpressionLibraryEditor::CreateAsset(FString AssetName, FString FolderPath, UClass* Class, FString Suffix)
{
	FString PackageName = FolderPath / AssetName;

	FAssetToolsModule& AssetToolsModule = FModuleManager::GetModuleChecked<FAssetToolsModule>("AssetTools");
	AssetToolsModule.Get().CreateUniqueAssetName(PackageName, Suffix, PackageName, AssetName);

#if ENGINE_VERSION < 426
	UPackage* Package = CreatePackage(nullptr, *PackageName);
#else
	UPackage* Package = CreatePackage(*PackageName);
#endif

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

void FVoxelMaterialExpressionLibraryEditor::ShowMessage(ESeverity Severity, FString Message)
{
	FNotificationInfo Info(FText::FromString(Message));
	Info.CheckBoxState = Severity == ESeverity::Info ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
	Info.ExpireDuration = 1.f;
	FSlateNotificationManager::Get().AddNotification(Info);

	if (Severity == ESeverity::Info)
	{
		UE_LOG(LogHLSLMaterial, Log, TEXT("%s"), *Message);
	}
	else
	{
		check(Severity == ESeverity::Error);
		UE_LOG(LogHLSLMaterial, Error, TEXT("%s"), *Message);
	}
}