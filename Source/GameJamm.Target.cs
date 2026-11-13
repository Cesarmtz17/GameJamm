using UnrealBuildTool;
using System.Collections.Generic;

public class GameJammTarget : TargetRules
{
	public GameJammTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("GameJamm");
	}
}
