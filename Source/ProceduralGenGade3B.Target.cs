// Build target rules for the packaged/standalone GAME build of ProceduralGenGade3B.
// UnrealBuildTool reads this class to know how to compile the game (non-editor) target.
using UnrealBuildTool;
using System.Collections.Generic;

public class ProceduralGenGade3BTarget : TargetRules
{
	public ProceduralGenGade3BTarget(TargetInfo Target) : base(Target)
	{
		// TargetType.Game = a runnable game executable (as opposed to Editor/Server/Client).
		Type = TargetType.Game;

		// Pin build-setting and include-order behaviour to modern UE5 defaults so the
		// project compiles consistently across engine updates.
		DefaultBuildSettings = BuildSettingsVersion.V7;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;

		// Register our primary game module so it gets compiled into this target.
		ExtraModuleNames.Add("ProceduralGenGade3B");
	}
}
