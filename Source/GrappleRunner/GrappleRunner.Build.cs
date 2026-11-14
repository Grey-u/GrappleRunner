// GrappleRunner.Build.cs - the "shopping list" for compiling the game code.
// It tells Unreal's build tool which engine parts (modules) our C++ uses:
//   Core / CoreUObject / Engine = the basics every Unreal game needs
//   InputCore                   = keyboard, mouse and controller button names
//   HeadMountedDisplay          = VR headset and hand-controller support
// If new C++ code uses another engine feature, its module name gets added here.

using UnrealBuildTool;

public class GrappleRunner : ModuleRules
{
	public GrappleRunner(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "HeadMountedDisplay" });
		// Slate + SlateCore: Unreal's UI toolkit, used to draw the start menu (GRMenuGameMode.cpp).
		PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore", "ProceduralMeshComponent", "UMG" });
	}
}
