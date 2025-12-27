using UnrealBuildTool;
using System.Collections.Generic;

public class BrumaTarget : TargetRules
{
    public BrumaTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Game;
        DefaultBuildSettings = BuildSettingsVersion.V5;
        IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_4; // OK para 5.6
        ExtraModuleNames.Add("Bruma");
    }
}
