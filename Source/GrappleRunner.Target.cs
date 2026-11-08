// GrappleRunner.Target.cs - settings for building the finished GAME (the version players run).
// "Latest" = use the newest build rules of this Unreal version.

using UnrealBuildTool;
using System.Collections.Generic;

public class GrappleRunnerTarget : TargetRules
{
	public GrappleRunnerTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("GrappleRunner");
	}
}
