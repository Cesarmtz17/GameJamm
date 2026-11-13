using UnrealBuildTool;
using System.Collections.Generic;

public class GameJammEditorTarget : TargetRules
{
	public GameJammEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("GameJamm");
	}
}
