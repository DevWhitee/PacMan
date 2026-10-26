using UnrealBuildTool;

public class PacMan : ModuleRules
{
    public PacMan(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "Core",
                "CoreUObject",
                "Engine",
                "InputCore",
                "AIModule",
                "NavigationSystem",
                "UMG",
                "Slate",
                "SlateCore"
            }
        );
    }
}