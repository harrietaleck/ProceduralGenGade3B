// Build rules for the main ProceduralGenGade3B game module.
// It lists the engine modules our C++ code can include and link to.
using UnrealBuildTool;

public class ProceduralGenGade3B : ModuleRules
{
	public ProceduralGenGade3B(ReadOnlyTargetRules Target) : base(Target)
	{
		// Use shared precompiled headers. This is the faster default.
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// Modules we can include from anywhere in this module.
		// Core, CoreUObject and Engine are the basic gameplay framework.
		// InputCore has the key and axis input types.
		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"ProceduralMeshComponent", // For the terrain mesh we build at runtime.
			"EnhancedInput",           // Input system used by the player controller for placing defenders.
			"UMG",                     // UI made in C++, like health bars, resources and the game over screen.
			"Slate",
			"SlateCore",
			"NavigationSystem"         // So we can rebuild the NavMesh every time the terrain is generated.
		});

		// Modules that only this module's private .cpp files use.
		PrivateDependencyModuleNames.AddRange(new string[] { });
	}
}
