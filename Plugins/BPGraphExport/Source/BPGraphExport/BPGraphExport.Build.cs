using UnrealBuildTool;

public class BPGraphExport : ModuleRules
{
    public BPGraphExport(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[] {
            "Core","CoreUObject","Engine",
            "UnrealEd","AssetRegistry","BlueprintGraph","Kismet",
            "Json","JsonUtilities","ToolMenus"
        });

        PrivateDependencyModuleNames.AddRange(new string[] {
            "Slate","SlateCore","EditorFramework","ApplicationCore","Projects"
        });
    }
}
