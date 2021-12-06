// Copyright 2021 Phyronnaz

#include "HLSLMaterialParser.h"
#include "HLSLMaterialFunction.h"
#include "HLSLMaterialFunctionLibrary.h"
#include "HLSLMaterialMessages.h"
#include "Internationalization/Regex.h"

FString FHLSLMaterialParser::Parse(
	const UHLSLMaterialFunctionLibrary& Library, 
	FString Text, 
	TArray<FHLSLMaterialFunction>& OutFunctions)
{
	enum class EScope
	{
		Global,
		Preprocessor,
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
	int32 ArgParenthesisScopeDepth = 0;
	int32 ArgBracketScopeDepth = 0;
	int32 LineNumber = 0;

	// Simplify line breaks handling
	Text.ReplaceInline(TEXT("\r\n"), TEXT("\n"));

	while (Index < Text.Len())
	{
		const TCHAR Char = Text[Index++];
		
		if (FChar::IsLinebreak(Char))
		{
			LineNumber++;
		}

		switch (Scope)
		{
		case EScope::Global:
		{
			ensure(ScopeDepth == 0);
			ensure(ArgParenthesisScopeDepth == 0);

			if (FChar::IsLinebreak(Char))
			{
				// Clear any pending comment when there's an empty line with no //
				if (OutFunctions.Num() > 0 && 
					OutFunctions.Last().ReturnType.IsEmpty())
				{
					OutFunctions.Pop();
				}
				continue;
			}
			if (FChar::IsWhitespace(Char))
			{
				continue;
			}

			if (OutFunctions.Num() == 0 || !OutFunctions.Last().ReturnType.IsEmpty())
			{
				OutFunctions.Emplace();
			}
			Index--;

			if (Char == TEXT('#'))
			{
				Scope = EScope::Preprocessor;
			}
			else if (Char == TEXT('/'))
			{
				Scope = EScope::FunctionComment;
			}
			else
			{
				Scope = EScope::FunctionReturn;
			}
		}
		break;
		case EScope::Preprocessor:
		{
			if (!FChar::IsLinebreak(Char))
			{
				continue;
			}

			Scope = EScope::Global;
		}
		break;
		case EScope::FunctionComment:
		{
			if (!FChar::IsLinebreak(Char))
			{
				OutFunctions.Last().Comment += Char;
				continue;
			}
			OutFunctions.Last().Comment += "\n";

			Scope = EScope::Global;
		}
		break;
		case EScope::FunctionReturn:
		{
			if (!FChar::IsWhitespace(Char))
			{
				OutFunctions.Last().ReturnType += Char;
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
					OutFunctions.Last().Name += Char;
				}
				continue;
			}

			Scope = EScope::FunctionArgs;

			ensure(ArgParenthesisScopeDepth == 0);
			ArgParenthesisScopeDepth++;

			ArgBracketScopeDepth = 0;
		}
		break;
		case EScope::FunctionArgs:
		{
			if (Char == TEXT('('))
			{
				ArgParenthesisScopeDepth++;
			}
			else if (Char == TEXT(')'))
			{
				ArgParenthesisScopeDepth--;
				ensure(ArgParenthesisScopeDepth >= 0);
			}

			if (Char == TEXT('['))
			{
				ArgBracketScopeDepth++;
			}
			else if (Char == TEXT(']'))
			{
				ArgBracketScopeDepth--;
			}

			if (ArgParenthesisScopeDepth > 0)
			{
				if (Char == TEXT(',') &&
					ArgBracketScopeDepth == 0 &&
					ArgParenthesisScopeDepth == 1)
				{
					OutFunctions.Last().Arguments.Emplace();
				}
				else
				{
					if (OutFunctions.Last().Arguments.Num() == 0)
					{
						OutFunctions.Last().Arguments.Emplace();
					}

					OutFunctions.Last().Arguments.Last() += Char;
				}
				continue;
			}

			ensure(ArgParenthesisScopeDepth == 0);
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
				return FString::Printf(TEXT("Invalid function body for %s: missing {"), *OutFunctions.Last().Name);
			}

			if (Library.bAccurateErrors)
			{
				OutFunctions.Last().StartLine = LineNumber;
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
				OutFunctions.Last().Body += Char;
				continue;
			}
			if (ScopeDepth < 0)
			{
				return FString::Printf(TEXT("Invalid function body for %s: too many }"), *OutFunctions.Last().Name);
			}

			ensure(ScopeDepth == 0);
			Scope = EScope::Global;
		}
		break;
		default: ensure(false);
		}
	}

	if (Scope == EScope::FunctionComment)
	{
		// Can have a commented out function at the end
		if (ensure(OutFunctions.Num() > 0) &&
			ensure(OutFunctions.Last().ReturnType.IsEmpty()))
		{
			OutFunctions.Pop();
			Scope = EScope::Global;
		}
	}

	if (Scope != EScope::Global)
	{
		return TEXT("Parsing error");
	}
	ensure(ScopeDepth == 0);
	ensure(ArgParenthesisScopeDepth == 0);

	return {};
}

TArray<FHLSLMaterialParser::FInclude> FHLSLMaterialParser::GetIncludes(const FString& FilePath, const FString& Text)
{
	FString VirtualFolder;
	if (UHLSLMaterialFunctionLibrary::TryConvertFilenameToShaderPath(FilePath, VirtualFolder))
	{
		VirtualFolder = FPaths::GetPath(VirtualFolder);
	}

	TArray<FInclude> OutIncludes;

	FRegexPattern RegexPattern(R"_((\A|\v)\s*#include "([^"]+)")_");
	FRegexMatcher RegexMatcher(RegexPattern, Text);
	while (RegexMatcher.FindNext())
	{
		FString VirtualPath = RegexMatcher.GetCaptureGroup(2);
		if (!VirtualPath.StartsWith(TEXT("/")) && !VirtualFolder.IsEmpty())
		{
			// Relative path
			VirtualPath = VirtualFolder / VirtualPath;
		}

		FString DiskPath = GetShaderSourceFilePath(VirtualPath);
		if (DiskPath.IsEmpty())
		{
			FHLSLMaterialMessages::ShowError(TEXT("Failed to map include %s"), *VirtualPath);
		}
		else
		{
			DiskPath = FPaths::ConvertRelativePathToFull(DiskPath);
		}

		OutIncludes.Add({ VirtualPath, DiskPath });
	}

	return OutIncludes;
}

TArray<FCustomDefine> FHLSLMaterialParser::GetDefines(const FString& Text)
{
	TArray<FCustomDefine> OutDefines;

	FRegexPattern RegexPattern(R"_((\A|\v)\s*#define (\w*) (.*))_");
	FRegexMatcher RegexMatcher(RegexPattern, Text);
	while (RegexMatcher.FindNext())
	{
		OutDefines.Add({ RegexMatcher.GetCaptureGroup(2), RegexMatcher.GetCaptureGroup(3) });
	}

	return OutDefines;
}