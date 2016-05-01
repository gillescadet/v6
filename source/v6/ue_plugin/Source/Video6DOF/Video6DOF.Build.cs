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
                    "../../../../Source/Runtime/Renderer/Private",
                }
			);

			PrivateDependencyModuleNames.AddRange(
				new string[] 
                {
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

            if ( Target.Platform == UnrealTargetPlatform.Win64 )
            {
                PrivateDependencyModuleNames.AddRange(
                    new string[]
                    {
                        "D3D11RHI",
                    }
                );

                PrivateIncludePaths.AddRange(
                    new string[]
                    {
                        "../../../../Source/Runtime/Windows/D3D11RHI/Private",
                        "../../../../Source/Runtime/Windows/D3D11RHI/Private/Windows",
                    }
                );
            }
        }
	}
}