// Copyright 2021 Phyronnaz

using System;
using System.IO;
using UnrealBuildTool;

public class HLSLMaterialEditor : ModuleRules
{
    public HLSLMaterialEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        bEnforceIWYU = true;
        bLegacyPublicIncludePaths = false;
        bUseUnity = false;

        PrivateIncludePaths.Add(Path.Combine(ModuleDirectory, "Private"));

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
                "Slate",
                "SlateCore",
                "MaterialEditor",
                "HLSLMaterialRuntime",
            });
    }
}
