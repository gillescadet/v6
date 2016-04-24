// Copyright 2016 Video6DOF.  All rights reserved.

namespace UnrealBuildTool.Rules
{
	public class Video6DOF : ModuleRules
	{
		public Video6DOF( TargetInfo Target )
		{
			PrivateIncludePaths.AddRange(
				new string[] {
					"Runtime/Video6DOF/Private",
				}
			);

			PrivateDependencyModuleNames.AddRange(
				new string[] {
					"Core",
					"CoreUObject",
					"Engine",
					"ImageWrapper",
					"InputCore",
					"RenderCore",
					"ShaderCore",
					"RHI",
					"Slate",
				}
			);
		}
	}
}