// Copyright 2021 Phyronnaz

using System;
using System.IO;
using UnrealBuildTool;

public class HLSLMaterialEditor : ModuleRules
{
    public HLSLMaterialEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.NoPCHs;

        bEnforceIWYU = true;
        bLegacyPublicIncludePaths = false;
        bUseUnity = false;

        PrivateIncludePaths.Add(Path.Combine(ModuleDirectory, "Private"));
        PublicIncludePaths.Add(Path.Combine(ModuleDirectory, "Public"));

        DynamicallyLoadedModuleNames.AddRange(
            new string[] {
                "AssetRegistry",
            });

        PrivateDependencyModuleNames.AddRange(
            new string[] {
                "Core",
                "CoreUObject",
                "Engine",
                "UnrealEd",
                "RHI",
                "RenderCore",
                "Slate",
                "SlateCore",
                "EditorStyle",
                "MessageLog",
                "DesktopPlatform",
                "MaterialEditor",
                "HLSLMaterialRuntime",
                "DeveloperSettings",
            });

        PrivateIncludePaths.Add(Path.Combine(EngineDirectory, "Source/Developer/MessageLog/Private/"));
    }
}
