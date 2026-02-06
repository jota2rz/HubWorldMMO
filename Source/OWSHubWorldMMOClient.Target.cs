using System;
using System.Collections.Generic;
using UnrealBuildTool;

public class OWSHubWorldMMOClientTarget : TargetRules
{
    public OWSHubWorldMMOClientTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Client;
        DefaultBuildSettings = BuildSettingsVersion.Latest;
        IncludeOrderVersion = EngineIncludeOrderVersion.Latest;

        ExtraModuleNames.AddRange(new string[] { "OWSHubWorldMMO" });
    }
}