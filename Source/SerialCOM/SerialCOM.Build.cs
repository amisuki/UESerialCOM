using UnrealBuildTool;

public class SerialCOM : ModuleRules
{
    public SerialCOM(ReadOnlyTargetRules Target) : base(Target)
    {
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PrivateIncludePaths.AddRange(new string[] { "SerialCOM/Private" });

        PrivateDependencyModuleNames.AddRange(
            new string[]
			{
                "Engine",
                "Core",
                "CoreUObject"
            }
        );
    }
}