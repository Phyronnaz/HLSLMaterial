// Copyright 2021 Phyronnaz

using System.IO;
using UnrealBuildTool;

public class HLSLMaterialRuntime : ModuleRules
{
    public HLSLMaterialRuntime(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.NoPCHs;

        bEnforceIWYU = true;
        bLegacyPublicIncludePaths = false;
        bUseUnity = false;

        PublicIncludePaths.Add(Path.Combine(ModuleDirectory, "Public"));
        PrivateIncludePaths.Add(Path.Combine(ModuleDirectory, "Private"));

        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "Core",
                "CoreUObject",
                "Engine",
            }
        );

        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
                "RenderCore"
            }
        );

        if (Target.bBuildEditor)
        {
            PrivateDependencyModuleNames.AddRange(
                new string[]
                {
                    "UnrealEd",
                }
            );
        }
        
        BuildVersion Version;
        if (BuildVersion.TryRead(BuildVersion.GetDefaultFileName(), out Version) &&
            Version.BranchName == "++UE5+Release-5.0-EarlyAccess")
        {
            PublicDefinitions.Add("HLSL_MATERIAL_UE5_EA=1");
        }
    }
}