// Build target rules for the EDITOR build of ProceduralGenGade3B.
// This is the target compiled when you open the project in the Unreal Editor.
using UnrealBuildTool;
using System.Collections.Generic;

public class ProceduralGenGade3BEditorTarget : TargetRules
{
	public ProceduralGenGade3BEditorTarget(TargetInfo Target) : base(Target)
	{
		// TargetType.Editor = builds the module so it can be loaded inside UnrealEditor.exe.
		Type = TargetType.Editor;

		DefaultBuildSettings = BuildSettingsVersion.V7;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;

		// The same game module is included in the editor target so our C++ classes
		// are available while working in the editor.
		ExtraModuleNames.Add("ProceduralGenGade3B");
	}
}
