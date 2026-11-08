// GrappleRunnerEditor.Target.cs - settings for building the game code INTO THE EDITOR,
// so Unreal can load our C++ (e.g. the GrappleRig) while we build levels.

using UnrealBuildTool;
using System.Collections.Generic;

public class GrappleRunnerEditorTarget : TargetRules
{
	public GrappleRunnerEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("GrappleRunner");
	}
}
