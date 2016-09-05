// Copyright 2016 Video6DOF.  All rights reserved.

namespace UnrealBuildTool.Rules
{
	public class Video6DOF : ModuleRules
	{
		public Video6DOF( TargetInfo Target )
		{
            PrivateIncludePaths.Add("Video6DOF/Private");
            PrivateIncludePaths.Add("../../../../Source/Runtime/Renderer/Private");
            
            PrivateDependencyModuleNames.Add("Core");
            PrivateDependencyModuleNames.Add("CoreUObject");
            PrivateDependencyModuleNames.Add("Engine");
            PrivateDependencyModuleNames.Add("ImageWrapper");
            PrivateDependencyModuleNames.Add("InputCore");
            PrivateDependencyModuleNames.Add("RenderCore");
            PrivateDependencyModuleNames.Add("ShaderCore");
            PrivateDependencyModuleNames.Add("RHI");
            PrivateDependencyModuleNames.Add("Slate");

            if ( Target.Platform == UnrealTargetPlatform.Win64 )
            {
                AddThirdPartyPrivateStaticDependencies(Target, "DX11");

                PrivateIncludePaths.Add("../../../../Source/Runtime/Windows/D3D11RHI/Private");
                PrivateIncludePaths.Add("../../../../Source/Runtime/Windows/D3D11RHI/Private/Windows");
                                
                PrivateDependencyModuleNames.Add("D3D11RHI");
            }
        }
	}
}
