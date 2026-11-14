using UnrealBuildTool;

public class GameJamm : ModuleRules
{
	public GameJamm(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// Permite includes tipo "Core/HealthComponent.h" desde cualquier carpeta del módulo.
		PublicIncludePaths.Add(ModuleDirectory);

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"AIModule",
			"NavigationSystem",
			"GameplayTasks",
			"UMG",
			"HTTP",
			"Json",
			"JsonUtilities",
			"AudioCaptureCore" // microfono para pedir construcciones por voz (Building/JamVoice)
		});
	}
}
