// Module build rules for the ProceduralGenGade3B primary game module.
// This lists the engine modules our C++ code is allowed to #include and link against.
using UnrealBuildTool;

public class ProceduralGenGade3B : ModuleRules
{
	public ProceduralGenGade3B(ReadOnlyTargetRules Target) : base(Target)
	{
		// Use shared/explicit precompiled headers (the modern, faster default).
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// Modules whose public headers we can include from anywhere in this module.
		//   Core / CoreUObject / Engine : the fundamental gameplay framework.
		//   InputCore                   : key/axis input types.
		// (ProceduralMeshComponent, EnhancedInput and UMG will be added when the
		//  gameplay classes that need them are introduced.)
		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"ProceduralMeshComponent", // Runtime-generated terrain mesh (UProceduralMeshComponent).
			"EnhancedInput",           // Modern input system for the player controller (defender placement).
			"UMG",                     // Runtime UI (health bars, resources, game-over) driven from C++.
			"Slate",
			"SlateCore",
			"NavigationSystem"         // Rebuilding the NavMesh after each procedural terrain generation.
		});

		// Modules used only by this module's private .cpp implementation files.
		PrivateDependencyModuleNames.AddRange(new string[] { });
	}
}
