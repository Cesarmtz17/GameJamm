using UnrealBuildTool;

public class GameJamm : ModuleRules
{
	public GameJamm(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		// Sin unity build: varios .cpp usan nombres cortos en namespaces anonimos (p. ej. K en JamAssembler y JamBuildPart)
		// y chocan al compilarse juntos en un build limpio. El modulo es chico, compila rapido igual.
		bUseUnity = false;

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
