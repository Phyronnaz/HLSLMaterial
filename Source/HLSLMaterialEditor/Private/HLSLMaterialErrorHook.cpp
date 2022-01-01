// Copyright 2021 Phyronnaz

#include "HLSLMaterialErrorHook.h"
#include "HLSLMaterialSettings.h"
#include "HLSLMaterialUtilities.h"
#include "HLSLMaterialFunctionLibrary.h"
#include "MaterialEditorModule.h"
#include "Misc/MessageDialog.h"
#include "Internationalization/Regex.h"

#define private public
#include "MaterialEditor/Private/MaterialEditor.h"
#include "MaterialEditor/Private/MaterialStats.h"
#include "Presentation/MessageLogListingViewModel.h"
#undef private

void FHLSLMaterialErrorHook::Register()
{
	IMaterialEditorModule& MaterialEditorModule = FModuleManager::LoadModuleChecked<IMaterialEditorModule>("MaterialEditor");
	MaterialEditorModule.OnMaterialEditorOpened().AddLambda([](TWeakPtr<IMaterialEditor> WeakMaterialEditor)
	{
		// The material editor is not yet initialized - can't hook into it just yet
		FHLSLMaterialUtilities::DelayedCall([=]
		{
			const TSharedPtr<IMaterialEditor> PinnedMaterialEditor = WeakMaterialEditor.Pin();
			if (PinnedMaterialEditor)
			{
				HookMessageLogHack(*PinnedMaterialEditor);
			}
		});
	});
}
HLSL_STARTUP_FUNCTION(EDelayedRegisterRunPhase::EndOfEngineInit, FHLSLMaterialErrorHook::Register);

void FHLSLMaterialErrorHook::HookMessageLogHack(IMaterialEditor& MaterialEditor)
{
	const TSharedPtr<FMaterialStats> StatsManager = static_cast<FMaterialEditor&>(MaterialEditor).MaterialStatsManager;
	if (!ensure(StatsManager))
	{
		return;
	}

	const TSharedPtr<IMessageLogListing> Listing = StatsManager->GetOldStatsListing();
	if (!Listing)
	{
		return;
	}

	FMessageLogListingViewModel& ViewModel = static_cast<FMessageLogListingViewModel&>(*Listing);
	ViewModel.OnDataChanged().AddLambda([&ViewModel]
	{
		ReplaceMessages(ViewModel);
	});
}

void FHLSLMaterialErrorHook::ReplaceMessages(FMessageLogListingViewModel& ViewModel)
{
	ensure(ViewModel.GetCurrentPageIndex() == 0);

	const int32 NumMessages = ViewModel.NumMessages();
	for (int32 MessageIndex = 0; MessageIndex < NumMessages; MessageIndex++)
	{
		const TSharedPtr<FTokenizedMessage> Message = ViewModel.GetMessageAtIndex(MessageIndex);
		if (!ensure(Message))
		{
			continue;
		}

		TArray<TSharedRef<IMessageToken>> NewTokens;
		for (const TSharedRef<IMessageToken>& Token : Message->GetMessageTokens())
		{
			if (Token->GetType() != EMessageToken::Text)
			{
				NewTokens.Add(Token);
				continue;
			}
			
			FString Path;
			FString FullPath;
			FString ErrorPrefix;
			FString ErrorSuffix;
			{
				const FString Error = Token->ToText().ToString();

				// Try to parse error with [HLSLMaterial] markup
				FString ActualError;
				if (Error.Split(PathPrefix, &ErrorPrefix, &ActualError))
				{
					if (!ensure(ActualError.Split(PathSuffix, &Path, &ErrorSuffix)))
					{
						NewTokens.Add(Token);
						continue;
					}

					FullPath = UHLSLMaterialFunctionLibrary::GetFilePath(Path);
				}
				else
				{
					// Try to parse a shader file path

					// [FeatureLevel] /Path(line info): error
					FRegexPattern RegexPattern(R"_((\[.*\] )(\/.*)(\(.*\): .*))_");
					FRegexMatcher RegexMatcher(RegexPattern, Error);
					if (!RegexMatcher.FindNext())
					{
						NewTokens.Add(Token);
						continue;
					}

					ErrorPrefix = RegexMatcher.GetCaptureGroup(1);
					Path = RegexMatcher.GetCaptureGroup(2);
					ErrorSuffix = RegexMatcher.GetCaptureGroup(3);

					FullPath = GetShaderSourceFilePath(Path);
				}
			}

			// Avoid doing silly stuff with generated files
			if (!FPaths::FileExists(FullPath))
			{
				NewTokens.Add(Token);
				continue;
			}
			
			FString LineNumber;
			FString CharStart;
			FString CharEnd;

			{
				// Error prefix (line,char) error suffix
				// Error prefix (line,char-char) error suffix
				FRegexPattern RegexPattern(R"_(\(([0-9]*),([0-9]*)(-([0-9]*))?\)(.*))_");
				FRegexMatcher RegexMatcher(RegexPattern, ErrorSuffix);
				if (RegexMatcher.FindNext())
				{
					LineNumber = RegexMatcher.GetCaptureGroup(1);
					CharStart = RegexMatcher.GetCaptureGroup(2);
					CharEnd = RegexMatcher.GetCaptureGroup(4);
					ErrorSuffix = RegexMatcher.GetCaptureGroup(5);
				}
			}

			if (LineNumber.IsEmpty())
			{
				// Error prefix (line): error suffix
				// Error prefix (line): (char) error suffix
				FRegexPattern RegexPattern(R"_(\(([0-9]*)\): (\(([0-9]*)\))?(.*))_");
				FRegexMatcher RegexMatcher(RegexPattern, ErrorSuffix);
				if (RegexMatcher.FindNext())
				{
					LineNumber = RegexMatcher.GetCaptureGroup(1);
					CharStart = RegexMatcher.GetCaptureGroup(3);
					ErrorSuffix = RegexMatcher.GetCaptureGroup(4);
				}
			}

			if (!ensure(!LineNumber.IsEmpty()))
			{
				NewTokens.Add(Token);
				continue;
			}
			
			FString DisplayText = FString::Printf(TEXT("%s:%s:%s"), *Path, *LineNumber, *CharStart);
			if (!CharEnd.IsEmpty())
			{
				DisplayText += "-" + CharEnd;
			}

			NewTokens.Add(FTextToken::Create(FText::FromString(ErrorPrefix)));
			NewTokens.Add(FActionToken::Create(
				FText::FromString(DisplayText),
				FText::Format(INVTEXT("Open {0}"), FText::FromString(FullPath)),
				FOnActionTokenExecuted::CreateLambda([=]
				{
					FString ExePath = GetDefault<UHLSLMaterialSettings>()->HLSLEditor.FilePath;

					{
						// Replace environment variables on Windows

						FString Variable;
						bool bIsInVariable = false;
						for (int32 Index = 0; Index < ExePath.Len(); Index++)
						{
							const TCHAR Char = ExePath[Index];
							if (Char == TEXT('%'))
							{
								if (bIsInVariable)
								{
									if (ensure(ExePath.ReplaceInline(
										*FString::Printf(TEXT("%%%s%%"), *Variable),
										*FPlatformMisc::GetEnvironmentVariable(*Variable))))
									{
										Variable = {};
										bIsInVariable = false;
										Index = 0;
									}
									else
									{
										break;
									}
								}
								else
								{
									bIsInVariable = true;
								}
							}
							else
							{
								if (bIsInVariable)
								{
									Variable += Char;
								}
							}
						}
					}

					FString Args = GetDefault<UHLSLMaterialSettings>()->HLSLEditorArgs;
					Args.ReplaceInline(TEXT("%FILE%"), *FullPath);
					Args.ReplaceInline(TEXT("%LINE%"), *LineNumber);
					Args.ReplaceInline(TEXT("%CHAR%"), *CharStart);

					FProcHandle Handle = FPlatformProcess::CreateProc(*ExePath, *Args, true, false, false, nullptr, 0, nullptr, nullptr);
					if (Handle.IsValid())
					{
						FPlatformProcess::CloseProc(Handle);
					}
					else
					{
						FMessageDialog::Open(EAppMsgType::Ok, FText::Format(
							INVTEXT("Failed to open {0}\n\nYou can update the application used to open HLSL files in your editor settings, under Plugins -> HLSL Material"), 
							FText::FromString(ExePath + Args)));
					}
				})));
			NewTokens.Add(FTextToken::Create(FText::FromString(ErrorSuffix)));
		}
		HLSL_CONST_CAST(Message->GetMessageTokens()) = NewTokens;
	}
}