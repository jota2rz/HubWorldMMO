using System;
using System.Collections.Generic;
using UnrealBuildTool;

public class OWSHubWorldMMOServerTarget : TargetRules
{
    public OWSHubWorldMMOServerTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Server;
        DefaultBuildSettings = BuildSettingsVersion.Latest;
        IncludeOrderVersion = EngineIncludeOrderVersion.Latest;

        ExtraModuleNames.AddRange(new string[] { "OWSHubWorldMMO" });
    }
}