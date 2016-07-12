/*V6*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#pragma warning( disable: 4996 ) //	'fopen': This function or variable may be unsafe. Consider using fopen_s instead.

// Templates

#define TEMPLATE		static const char* const
#define TEXT(...)		#__VA_ARGS__

TEMPLATE solutionGUID						= TEXT(	8BC9CEB8-8B4A-11D0-8D11-00A0C91BC942 );
TEMPLATE solutionHeader						= TEXT(	Microsoft Visual Studio Solution File, Format Version 12.00\n
													# Visual Studio 14\n
													VisualStudioVersion = 14.0.24720.0\n
													MinimumVisualStudioVersion = 10.0.40219.1\n );

TEMPLATE solutionProjectBegin				= TEXT(	Project("{%s}") = "%s", "%s%s.vcxproj", "{%s}"\n );
TEMPLATE solutionProjectDependenciesBegin	= TEXT(		ProjectSection(ProjectDependencies) = postProject\n );
TEMPLATE solutionProjectDependency			= TEXT(			{%s} = {%s}\n );
TEMPLATE solutionProjectDependenciesEnd		= TEXT(		EndProjectSection\n );
TEMPLATE solutionProjectEnd					= TEXT(	EndProject\n );

TEMPLATE solutionGlobalBegin				= TEXT(	Global\n );
TEMPLATE solutionGlobalConfigsBegin			= TEXT(		GlobalSection(SolutionConfigurationPlatforms) = preSolution\n );
TEMPLATE solutionGlobalConfig				= TEXT(			%s = %s\n );
TEMPLATE solutionGlobalConfigsEnd			= TEXT(		EndGlobalSection\n );
TEMPLATE solutionGlobalProjectsBegin		= TEXT(		GlobalSection(ProjectConfigurationPlatforms) = postSolution\n );
TEMPLATE solutionGlobalProject				= TEXT(			{%s}.%s.ActiveCfg = %s\n
															{%s}.%s.Build.0 = %s\n );
TEMPLATE solutionGlobalProjectsEnd			= TEXT(		EndGlobalSection\n );
TEMPLATE solutionGlobalEnd					= TEXT(		GlobalSection(SolutionProperties) = preSolution\n
															HideSolutionNode = FALSE\n
														EndGlobalSection\n
													EndGlobal\n );

TEMPLATE projectBegin						= TEXT( <?xml version="1.0" encoding="utf-8"?>\n
														<Project DefaultTargets="Build" ToolsVersion="14.0" xmlns="http://schemas.microsoft.com/developer/msbuild/2003">\n );
TEMPLATE projectSet							= TEXT(			<PropertyGroup Label="Globals">\n
																<ProjectGuid>{%s}</ProjectGuid>\n
																<Keyword>Win32Proj</Keyword>\n
																<RootNamespace>%s</RootNamespace>\n
															</PropertyGroup>\n
															<Import Project="$(VCTargetsPath)\Microsoft.Cpp.Default.props" />\n
															<PropertyGroup Label="Configuration">\n
																<PlatformToolset>v140</PlatformToolset>\n
																<CharacterSet>Unicode</CharacterSet>\n
																<ConfigurationType>%s</ConfigurationType>\n
															</PropertyGroup>\n
															<PropertyGroup>\n
																<LocalDebuggerWorkingDirectory>$(TargetDir)</LocalDebuggerWorkingDirectory>\n
															</PropertyGroup> \n );

TEMPLATE projectEnd							= TEXT(			<Import Project="$(VCTargetsPath)\Microsoft.Cpp.targets" />\n
														</Project>\n );

TEMPLATE projectConfigsBegin				= TEXT(			<ItemGroup Label="ProjectConfigurations">\n );
TEMPLATE projectConfig						= TEXT(				<ProjectConfiguration Include="%s|%s">\n
																		<Configuration>%s</Configuration>\n
																		<Platform>%s</Platform>\n
																</ProjectConfiguration>\n );
TEMPLATE projectConfigsEnd					= TEXT(			</ItemGroup>\n );

TEMPLATE projectConfigImportGroupsBegin		= TEXT(			<Import Project="$(VCTargetsPath)\Microsoft.Cpp.props" />\n );
TEMPLATE projectConfigImportGroup			= TEXT(			<ImportGroup Condition="'$(Configuration)|$(Platform)'=='%s|%s'" Label="PropertySheets">\n
																<Import Project="%s_common_%s_%s.props" />\n
															</ImportGroup>\n );

TEMPLATE projectItemDefinitionGroup			= TEXT(			<ItemDefinitionGroup>\n
																<Link>\n
																	<SubSystem>%s</SubSystem>\n
																	<AdditionalDependencies>%s;%%(AdditionalDependencies)</AdditionalDependencies>
																</Link>\n
															</ItemDefinitionGroup>\n );

TEMPLATE projectSourcesBegin				= TEXT(			<ItemGroup>\n );
TEMPLATE projectSourceCPP					= TEXT(				<ClCompile Include="%s/%s" />\n );
TEMPLATE projectSourceH						= TEXT(				<ClInclude Include="%s/%s" />\n );
TEMPLATE projectSourceHLSL					= TEXT(				<FxCompile Include="%s/%s" >\n
																	<ShaderType>%s</ShaderType>\n
																	<EntryPointName>main_%s</EntryPointName>\n
																	%s
																	%s
																</FxCompile>\n );
TEMPLATE projectSourceTXT					= TEXT(				<ClInclude Include="%s/%s" />\n );
TEMPLATE projectSourcesEnd					= TEXT(			</ItemGroup>\n );

TEMPLATE commonDebug						= TEXT(	<?xml version="1.0" encoding="utf-8"?>\n
														<Project ToolsVersion="4.0" xmlns="http://schemas.microsoft.com/developer/msbuild/2003">\n
															<PropertyGroup Label="UserMacros">\n
																<OVRSDKROOT>$(SolutionDir)/../../thirdparty/OculusSDK/</OVRSDKROOT>\n
															</PropertyGroup>\n
															<PropertyGroup>\n
																<OutDir>$(SolutionDir)../../bin/$(Configuration)/</OutDir>\n
																<IntDir>$(SolutionDir)../../build/$(ProjectName)/$(Configuration)/</IntDir>\n
															</PropertyGroup>\n
															<PropertyGroup Label="Configuration">\n
															</PropertyGroup>\n
															<ItemDefinitionGroup>\n
																<ClCompile>\n
																	<PreprocessorDefinitions>WIN32;_DEBUG;_LIB;%%(PreprocessorDefinitions)</PreprocessorDefinitions>																
																	<AdditionalIncludeDirectories>$(SolutionDir)../../source;../../thirdparty;$(OVRSDKROOT)/LibOVR/Include;$(OVRSDKROOT)/LibOVRKernel/Src</AdditionalIncludeDirectories>\n
																	<Optimization>Disabled</Optimization>
																	<WarningLevel>Level3</WarningLevel>
																	<TreatWarningAsError>true</TreatWarningAsError>
																</ClCompile>\n
																<FxCompile>\n
																	<EntryPointName/>\n
																	<ShaderModel>5.0</ShaderModel>\n
																	<TreatWarningAsError>true</TreatWarningAsError>\n
																	<DisableOptimizations>false</DisableOptimizations>\n
																	<EnableDebuggingInformation>false</EnableDebuggingInformation>\n
																</FxCompile>
																<Link>\n
																	<AdditionalLibraryDirectories>$(SolutionDir)../../bin/$(Configuration)/</AdditionalLibraryDirectories>\n
																	<SubSystem>Windows</SubSystem>\n
																	<GenerateDebugInformation>true</GenerateDebugInformation>\n
																	//<AdditionalOptions>/VERBOSE %%(AdditionalOptions)</AdditionalOptions>\n
																</Link>\n
																<Lib>\n
																	<AdditionalLibraryDirectories>$(SolutionDir)../../bin/$(Configuration)/</AdditionalLibraryDirectories>\n
																</Lib>\n
															</ItemDefinitionGroup>\n
															<ItemGroup>\n
																<BuildMacro Include="OVRSDKROOT">\n
																	<Value>$(OVRSDKROOT)</Value>\n
																</BuildMacro>\n
															</ItemGroup>\n
														</Project>\n );

TEMPLATE projectUser						= TEXT(	<?xml version="1.0" encoding="utf-8"?>\n
													<Project ToolsVersion="14.0" xmlns="http://schemas.microsoft.com/developer/msbuild/2003">\n
														<PropertyGroup>\n
															<LocalDebuggerWorkingDirectory>$(TargetDir)</LocalDebuggerWorkingDirectory>\n
															<DebuggerFlavor>WindowsLocalDebugger</DebuggerFlavor>\n
														</PropertyGroup>\n
													</Project>\n );

TEMPLATE commonRelease						= TEXT(	<?xml version="1.0" encoding="utf-8"?>\n
														<Project ToolsVersion="4.0" xmlns="http://schemas.microsoft.com/developer/msbuild/2003">\n
															<PropertyGroup Label="UserMacros">\n
																<OVRSDKROOT>$(SolutionDir)/../../thirdparty/OculusSDK/</OVRSDKROOT>\n
															</PropertyGroup>\n
															<PropertyGroup>\n
																<OutDir>$(SolutionDir)../../bin/$(Configuration)/</OutDir>\n
																<IntDir>$(SolutionDir)../../build/$(ProjectName)/$(Configuration)/</IntDir>\n
															</PropertyGroup>\n
															<PropertyGroup Label="Configuration">\n
																<WholeProgramOptimization>true</WholeProgramOptimization>\n
															</PropertyGroup>\n
															<ItemDefinitionGroup>\n
																<ClCompile>\n
																	<AdditionalIncludeDirectories>$(SolutionDir)../../source;../../thirdparty;$(OVRSDKROOT)/LibOVR/Include;$(OVRSDKROOT)/LibOVRKernel/Src</AdditionalIncludeDirectories>\n
																	<PreprocessorDefinitions>WIN32;NDEBUG;_LIB;%%(PreprocessorDefinitions)</PreprocessorDefinitions>
																	<Optimization>MaxSpeed</Optimization>
																	<FunctionLevelLinking>true</FunctionLevelLinking>
																	<IntrinsicFunctions>true</IntrinsicFunctions>
																	<WarningLevel>Level3</WarningLevel>
																	<TreatWarningAsError>true</TreatWarningAsError>
																</ClCompile>\n
																<FxCompile>\n
																	<EntryPointName/>\n
																	<ShaderModel>5.0</ShaderModel>\n
																	<TreatWarningAsError>true</TreatWarningAsError>\n
																	<DisableOptimizations>false</DisableOptimizations>\n
																	<EnableDebuggingInformation>false</EnableDebuggingInformation>\n
																</FxCompile>
																<Link>\n
																	<AdditionalLibraryDirectories>$(SolutionDir)../../bin/$(Configuration)/</AdditionalLibraryDirectories>\n
																	<SubSystem>Windows</SubSystem>\n
																	<GenerateDebugInformation>true</GenerateDebugInformation>\n
																	<EnableCOMDATFolding>true</EnableCOMDATFolding>\n
																	<OptimizeReferences>true</OptimizeReferences>\n
																	//<AdditionalOptions>/VERBOSE %%(AdditionalOptions)</AdditionalOptions>\n
																</Link>\n
																<Lib>\n
																	<AdditionalLibraryDirectories>$(SolutionDir)../../bin/$(Configuration)/</AdditionalLibraryDirectories>\n
																</Lib>\n
															</ItemDefinitionGroup>\n
															<ItemGroup>\n
																<BuildMacro Include="OVRSDKROOT">\n
																	<Value>$(OVRSDKROOT)</Value>\n
																</BuildMacro>\n
															</ItemGroup>\n
														</Project>\n );

TEMPLATE HLSL_OuputBytecodeInHeaderFile		= TEXT( <HeaderFileOutput>%(RelativeDir)%(Filename)_bytecode.h</HeaderFileOutput>\n
													<ObjectFileOutput/>\n );

TEMPLATE HLSL_DisableTreatWarningAsError	= TEXT( <TreatWarningAsError>false</TreatWarningAsError>\n );

// Implemmentation

#define LIB_OVR_KERNEL	"$(OVRSDKROOT)LibOVRKernel/Lib/Windows/$(Platform)/$(Configuration)/VS2015/LibOVRKernel.lib"
#define LIB_OVR			"$(OVRSDKROOT)LibOVR/Lib/Windows/$(Platform)/$(Configuration)/VS2015/LibOVR.lib"

typedef unsigned int u32;

struct Project_s
{
	u32			id;
	const char* guid;
	const char* name;
	u32			dependencies;
	bool		isLib;
	const char* path;
	const char* libPath;
};

struct ProjectFile_s
{
	const char*			name;
	u32					projects;
	const char*			specialCase1;
	const char*			specialCase2;
};

struct Config_s
{
	const char*	name;
	const char*	platform;
	const char*	props;
};

enum
{
	CONFIG_DEBUG_X64,
	CONFIG_RELEASE_X64,

	CONFIG_COUNT
};

enum
{
	PROJECT_BIN2H			= 1 << 0,
	PROJECT_BMFONTPARSER	= 1 << 1,
	PROJECT_CLASSWRAPPER	= 1 << 2,
	PROJECT_COMPRESSOR		= 1 << 3,
	PROJECT_DOC				= 1 << 4,
	PROJECT_ENCODER			= 1 << 5,
	PROJECT_LIBOVR			= 1 << 6,
	PROJECT_LIBOVRKERNEL	= 1 << 7,
	PROJECT_PLAYER			= 1 << 8,
	PROJECT_VIEWER			= 1 << 9,

	PROJECT_COUNT			= 10
};

static const Config_s s_configs[CONFIG_COUNT] = 
{
	{ "Debug" ,		"x64",	commonDebug },
	{ "Release",	"x64",	commonRelease },
};

// https://www.guidgenerator.com/

static const Project_s s_projects[PROJECT_COUNT] = 
{ 
	{ PROJECT_BIN2H, 		"C1954BE7-8364-4327-BFFA-8F36057A2083", "bin2h" },
	{ PROJECT_BMFONTPARSER,	"F8B1604E-BC8E-4B59-AF7F-D31B01DD9650", "bmfont_parser" },
	{ PROJECT_CLASSWRAPPER, "30B4EE66-3B03-4859-9372-974340DC4BD3", "class_wrappper" },
	{ PROJECT_COMPRESSOR,	"EE652B0F-8AEA-42F8-8529-41C075FA4DA8", "compressor" },
	{ PROJECT_DOC,			"3CF8E22B-F0B6-43A7-B472-9E3ACB91591A", "doc" },
	{ PROJECT_ENCODER,		"8B3F2A6F-97DD-4089-82FC-70E0CC3BCC27", "encoder" },
	{ PROJECT_LIBOVR,		"EA50E705-5113-49E5-B105-2512EDC8DDC6", "LibOVR"		, 0, true, "../../thirdparty/OculusSDK/LibOVR/Projects/Windows/VS2015/", LIB_OVR },
	{ PROJECT_LIBOVRKERNEL,	"29FA0962-DDC6-4F72-9D12-E150DF29E279", "LibOVRKernel"	, 0, true, "../../thirdparty/OculusSDK/LibOVRKernel/Projects/Windows/VS2015/", LIB_OVR_KERNEL },
	{ PROJECT_PLAYER,		"4185B5D4-480C-4E72-946F-90185611CE35", "player"		, PROJECT_LIBOVR | PROJECT_LIBOVRKERNEL },
	{ PROJECT_VIEWER,		"CEC43B15-39D4-463B-825C-D630A53DAFB0", "viewer"		, PROJECT_LIBOVR | PROJECT_LIBOVRKERNEL },
};

static ProjectFile_s s_projectFiles[] =
{
	// bin2h	
	{ "source/v6/bin2h/main_bin2h.cpp",					PROJECT_BIN2H },
	{ "source/v6/core/filesystem.cpp",					PROJECT_BIN2H },
	{ "source/v6/core/memory.cpp",						PROJECT_BIN2H },
	
	// bmfont parser
	{ "source/v6/bmfont_parser/main_bmfont_parser.cpp",	PROJECT_BMFONTPARSER },
	{ "source/v6/core/filesystem.cpp",					PROJECT_BMFONTPARSER },
	{ "source/v6/core/memory.cpp",						PROJECT_BMFONTPARSER },
	
	// class wrapper
	{ "source/v6/core/memory.cpp",						PROJECT_CLASSWRAPPER },
	{ "source/v6/core/filesystem.cpp",					PROJECT_CLASSWRAPPER },
	{ "source/v6/class_wrapper/main_class_wrapper.cpp",	PROJECT_CLASSWRAPPER },

	// compressor
	{ "source/v6/codec/compression.cpp",				PROJECT_COMPRESSOR },
	{ "source/v6/core/filesystem.cpp",					PROJECT_COMPRESSOR },
	{ "source/v6/core/image.cpp",						PROJECT_COMPRESSOR },
	{ "source/v6/core/memory.cpp",						PROJECT_COMPRESSOR },
	{ "source/v6/core/optimization.cpp",				PROJECT_COMPRESSOR },
	{ "source/v6/core/stream.cpp",						PROJECT_COMPRESSOR },
	{ "source/v6/core/string.cpp",						PROJECT_COMPRESSOR },
	{ "source/v6/core/time.cpp",						PROJECT_COMPRESSOR },
	{ "source/v6/compressor/main_compressor.cpp",		PROJECT_COMPRESSOR },

	// doc
	{ "source/v6/doc/bench.txt",						PROJECT_DOC },
	{ "source/v6/doc/make_project.cpp",					PROJECT_DOC },
	{ "source/v6/doc/survey.txt",						PROJECT_DOC },
	{ "source/v6/doc/todo.txt",							PROJECT_DOC },
	
	// encoder
	{ "source/v6/codec/codec.cpp",						PROJECT_ENCODER },
	{ "source/v6/codec/compression.cpp",				PROJECT_ENCODER },
	{ "source/v6/codec/decoder.cpp",					PROJECT_ENCODER },
	{ "source/v6/codec/encoder.cpp",					PROJECT_ENCODER },
	{ "source/v6/core/memory.cpp",						PROJECT_ENCODER },
	{ "source/v6/core/optimization.cpp",				PROJECT_ENCODER },
	{ "source/v6/core/stream.cpp",						PROJECT_ENCODER },
	{ "source/v6/encoder/main_encoder.cpp",				PROJECT_ENCODER },
	{ "thirdparty/lz4/lib/lz4.c",						PROJECT_ENCODER },
	{ "thirdparty/lz4/lib/lz4hc.c",						PROJECT_ENCODER },
	
	// player
	{ "source/v6/codec/codec.cpp",						PROJECT_PLAYER },
	{ "source/v6/codec/compression.cpp",				PROJECT_PLAYER },
	{ "source/v6/codec/decoder.cpp",					PROJECT_PLAYER },
	{ "source/v6/core/bit.h",							PROJECT_PLAYER },
	{ "source/v6/core/filesystem.cpp",					PROJECT_PLAYER },
	{ "source/v6/core/memory.cpp",						PROJECT_PLAYER },
	{ "source/v6/core/optimization.cpp",				PROJECT_PLAYER },
	{ "source/v6/core/string.cpp",						PROJECT_PLAYER },
	{ "source/v6/core/stream.cpp",						PROJECT_PLAYER },
	{ "source/v6/core/thread.cpp",						PROJECT_PLAYER },
	{ "source/v6/core/time.cpp",						PROJECT_PLAYER },
	{ "source/v6/core/vec2i.h",							PROJECT_PLAYER },
	{ "source/v6/core/win.cpp",							PROJECT_PLAYER },
	{ "source/v6/graphic/font.cpp",						PROJECT_PLAYER },
	{ "source/v6/graphic/gpu.cpp",						PROJECT_PLAYER },
	{ "source/v6/graphic/hmd.cpp",						PROJECT_PLAYER },
	{ "source/v6/graphic/scene.cpp",					PROJECT_PLAYER },
	{ "source/v6/graphic/trace.cpp",					PROJECT_PLAYER },
	{ "source/v6/graphic/view.cpp",						PROJECT_PLAYER },
	{ "source/v6/player/main_player.cpp",				PROJECT_PLAYER },
	{ "thirdparty/lz4/lib/lz4.c",						PROJECT_PLAYER },
	{ "thirdparty/lz4/lib/lz4hc.c",						PROJECT_PLAYER },

	// player - HLSL
	{ "source/v6/graphic/common_shared.h",				PROJECT_PLAYER },
	{ "source/v6/player/frame_metrics_cs.hlsl",			PROJECT_PLAYER },
	{ "source/v6/player/player_basic.hlsli",			PROJECT_PLAYER },
	{ "source/v6/player/player_basic_ps.hlsl",			PROJECT_PLAYER },
	{ "source/v6/player/player_basic_vs.hlsl",			PROJECT_PLAYER },
	{ "source/v6/player/player_shared.h",				PROJECT_PLAYER },
	{ "source/v6/player/surface_compose_cs.hlsl",		PROJECT_PLAYER },

	// viewer
	{ "source/v6/codec/codec.cpp",						PROJECT_VIEWER },
	{ "source/v6/codec/compression.cpp",				PROJECT_VIEWER },
	{ "source/v6/codec/decoder.cpp",					PROJECT_VIEWER },
	{ "source/v6/codec/encoder.cpp",					PROJECT_VIEWER },
	{ "source/v6/core/bit.h",							PROJECT_VIEWER },
	{ "source/v6/core/filesystem.cpp",					PROJECT_VIEWER },
	{ "source/v6/core/image.cpp",						PROJECT_VIEWER },
	{ "source/v6/core/mat3x3.h",						PROJECT_VIEWER },
	{ "source/v6/core/mat4x4.h",						PROJECT_VIEWER },
	{ "source/v6/core/memory.cpp",						PROJECT_VIEWER },
	{ "source/v6/core/obj_reader.cpp",					PROJECT_VIEWER },
	{ "source/v6/core/optimization.cpp",				PROJECT_VIEWER },
	{ "source/v6/core/stream.cpp",						PROJECT_VIEWER },
	{ "source/v6/core/string.cpp",						PROJECT_VIEWER },
	{ "source/v6/core/thread.cpp",						PROJECT_VIEWER },
	{ "source/v6/core/time.cpp",						PROJECT_VIEWER },
	{ "source/v6/core/vec2.h",							PROJECT_VIEWER },
	{ "source/v6/core/vec2i.h",							PROJECT_VIEWER },
	{ "source/v6/core/vec3.h",							PROJECT_VIEWER },
	{ "source/v6/core/vec3i.h",							PROJECT_VIEWER },
	{ "source/v6/core/vec4.h",							PROJECT_VIEWER },
	{ "source/v6/core/vec4i.h",							PROJECT_VIEWER },
	{ "source/v6/core/win.cpp",							PROJECT_VIEWER },
	{ "source/v6/graphic/capture.cpp",					PROJECT_VIEWER },
	{ "source/v6/graphic/font.cpp",						PROJECT_VIEWER },
	{ "source/v6/graphic/gpu.cpp",						PROJECT_VIEWER },
	{ "source/v6/graphic/hmd.cpp",						PROJECT_VIEWER },
	{ "source/v6/graphic/scene.cpp",					PROJECT_VIEWER },
	{ "source/v6/graphic/trace.cpp",					PROJECT_VIEWER },
	{ "source/v6/graphic/view.cpp",						PROJECT_VIEWER },
	{ "thirdparty/lz4/lib/lz4.c",						PROJECT_VIEWER },
	{ "thirdparty/lz4/lib/lz4hc.c",						PROJECT_VIEWER },
	{ "source/v6/viewer/main_viewer.cpp",				PROJECT_VIEWER },
	{ "source/v6/viewer/scene_info.cpp",				PROJECT_VIEWER },

	// viewer - HLSL
	{ "source/v6/graphic/block_cull_cs_impl.hlsli",		PROJECT_VIEWER },
	{ "source/v6/graphic/block_cull_stats_x4_cs.hlsl",	PROJECT_VIEWER, HLSL_OuputBytecodeInHeaderFile },
	{ "source/v6/graphic/block_cull_stats_x8_cs.hlsl",	PROJECT_VIEWER, HLSL_OuputBytecodeInHeaderFile },
	{ "source/v6/graphic/block_cull_stats_x16_cs.hlsl",	PROJECT_VIEWER, HLSL_OuputBytecodeInHeaderFile },
	{ "source/v6/graphic/block_cull_stats_x32_cs.hlsl",	PROJECT_VIEWER, HLSL_OuputBytecodeInHeaderFile },
	{ "source/v6/graphic/block_cull_stats_x64_cs.hlsl",	PROJECT_VIEWER, HLSL_OuputBytecodeInHeaderFile },
	{ "source/v6/graphic/block_cull_x4_cs.hlsl",		PROJECT_VIEWER, HLSL_OuputBytecodeInHeaderFile },
	{ "source/v6/graphic/block_cull_x8_cs.hlsl",		PROJECT_VIEWER, HLSL_OuputBytecodeInHeaderFile },
	{ "source/v6/graphic/block_cull_x16_cs.hlsl",		PROJECT_VIEWER, HLSL_OuputBytecodeInHeaderFile },
	{ "source/v6/graphic/block_cull_x32_cs.hlsl",		PROJECT_VIEWER, HLSL_OuputBytecodeInHeaderFile },
	{ "source/v6/graphic/block_cull_x64_cs.hlsl",		PROJECT_VIEWER, HLSL_OuputBytecodeInHeaderFile },
	{ "source/v6/graphic/block_trace_cs_impl.hlsli",	PROJECT_VIEWER, HLSL_OuputBytecodeInHeaderFile },
	{ "source/v6/graphic/block_trace_debug_x4_cs.hlsl",	PROJECT_VIEWER, HLSL_OuputBytecodeInHeaderFile },
	{ "source/v6/graphic/block_trace_debug_x8_cs.hlsl",	PROJECT_VIEWER, HLSL_OuputBytecodeInHeaderFile },
	{ "source/v6/graphic/block_trace_debug_x16_cs.hlsl",PROJECT_VIEWER, HLSL_OuputBytecodeInHeaderFile },
	{ "source/v6/graphic/block_trace_debug_x32_cs.hlsl",PROJECT_VIEWER, HLSL_OuputBytecodeInHeaderFile },
	{ "source/v6/graphic/block_trace_debug_x64_cs.hlsl",PROJECT_VIEWER, HLSL_OuputBytecodeInHeaderFile },
	{ "source/v6/graphic/block_trace_init_cs.hlsl",		PROJECT_VIEWER, HLSL_OuputBytecodeInHeaderFile },
	{ "source/v6/graphic/block_trace_x4_cs.hlsl",		PROJECT_VIEWER, HLSL_OuputBytecodeInHeaderFile },
	{ "source/v6/graphic/block_trace_x8_cs.hlsl",		PROJECT_VIEWER, HLSL_OuputBytecodeInHeaderFile },
	{ "source/v6/graphic/block_trace_x32_cs.hlsl",		PROJECT_VIEWER, HLSL_OuputBytecodeInHeaderFile },
	{ "source/v6/graphic/block_trace_x16_cs.hlsl",		PROJECT_VIEWER, HLSL_OuputBytecodeInHeaderFile },
	{ "source/v6/graphic/block_trace_x64_cs.hlsl",		PROJECT_VIEWER, HLSL_OuputBytecodeInHeaderFile },
	{ "source/v6/graphic/capture_shaders.h",			PROJECT_VIEWER },
	{ "source/v6/graphic/capture_shared.h",				PROJECT_VIEWER },
	{ "source/v6/graphic/common_shared.h",				PROJECT_VIEWER },
	{ "source/v6/graphic/font.hlsli",					PROJECT_VIEWER },
	{ "source/v6/graphic/font_ps.hlsl",					PROJECT_VIEWER, HLSL_OuputBytecodeInHeaderFile },
	{ "source/v6/graphic/font_shared.h",				PROJECT_VIEWER },
	{ "source/v6/graphic/font_vs.hlsl",					PROJECT_VIEWER, HLSL_OuputBytecodeInHeaderFile },
	{ "source/v6/graphic/octree_build_inner_cs.hlsl",	PROJECT_VIEWER, HLSL_OuputBytecodeInHeaderFile },
	{ "source/v6/graphic/octree_build_leaf_cs.hlsl",	PROJECT_VIEWER, HLSL_OuputBytecodeInHeaderFile },
	{ "source/v6/graphic/octree_build_node_impl.hlsli",	PROJECT_VIEWER },
	{ "source/v6/graphic/octree_fill_leaf_cs.hlsl",		PROJECT_VIEWER, HLSL_OuputBytecodeInHeaderFile },
	{ "source/v6/graphic/octree_pack_cs.hlsl",			PROJECT_VIEWER, HLSL_OuputBytecodeInHeaderFile },
	{ "source/v6/graphic/pixel_blend_cs.hlsl",			PROJECT_VIEWER, HLSL_OuputBytecodeInHeaderFile },
	{ "source/v6/graphic/pixel_blend_cs_impl.hlsli",	PROJECT_VIEWER, HLSL_OuputBytecodeInHeaderFile },
	{ "source/v6/graphic/pixel_blend_debug_cs.hlsl",	PROJECT_VIEWER, HLSL_OuputBytecodeInHeaderFile },
	{ "source/v6/graphic/pixel_sharpen_cs.hlsl",		PROJECT_VIEWER, HLSL_OuputBytecodeInHeaderFile },
	{ "source/v6/graphic/pixel_tsaa_cs.hlsl",			PROJECT_VIEWER, HLSL_OuputBytecodeInHeaderFile },
	{ "source/v6/graphic/sample_collect_cs.hlsl",		PROJECT_VIEWER, HLSL_OuputBytecodeInHeaderFile, HLSL_DisableTreatWarningAsError },
	{ "source/v6/graphic/sample_pack.hlsli",			PROJECT_VIEWER },
	{ "source/v6/graphic/trace_shaders.h",				PROJECT_VIEWER },
	{ "source/v6/graphic/trace_shared.h",				PROJECT_VIEWER },
	{ "source/v6/viewer/fake_cube.hlsli",				PROJECT_VIEWER },
	{ "source/v6/viewer/fake_cube_ps.hlsl",				PROJECT_VIEWER },
	{ "source/v6/viewer/fake_cube_vs.hlsl",				PROJECT_VIEWER },
	{ "source/v6/viewer/generic.hlsli",					PROJECT_VIEWER },
	{ "source/v6/viewer/generic_alpha_test_ps.hlsl",	PROJECT_VIEWER },
	{ "source/v6/viewer/generic_ps.hlsl",				PROJECT_VIEWER },
	{ "source/v6/viewer/generic_ps.hlsli",				PROJECT_VIEWER },
	{ "source/v6/viewer/generic_vs.hlsl",				PROJECT_VIEWER },
	{ "source/v6/viewer/viewer_basic.hlsli",			PROJECT_VIEWER },
	{ "source/v6/viewer/viewer_basic_ps.hlsl",			PROJECT_VIEWER },
	{ "source/v6/viewer/viewer_basic_vs.hlsl",			PROJECT_VIEWER },
	{ "source/v6/viewer/surface_compose_cs.hlsl",		PROJECT_VIEWER },
	{ "source/v6/viewer/viewer_shared.h",				PROJECT_VIEWER },
};

char*	s_solutionName		= nullptr;
char*	s_solutionVersion	= nullptr;
char	s_solutionPath[256] = "";
char	s_sourcePath[256]	= "../..";

static bool FilePath_Exist( const char* filename )
{
	FILE* f = fopen( filename, "rt" );
	if ( !f )
		return false;
	fclose( f );
	return true;
}

static void FilePath_SplitFilenameAndExtension( char* filePathWithoutExtension, u32 maxFileSize, char* extension, u32 maxExtensionsSize, const char* filePath )
{
	const char *c = filePath;
	const char* lastDot = nullptr; 
	while ( *c )
	{
		if ( *c == '.' )
			lastDot = c;
		++c;
	}

	if ( !lastDot )
	{
		strcpy_s( filePathWithoutExtension, maxFileSize, filePath );
		extension[0] = 0;
		return;
	}

	const u32 count = (u32)(lastDot - filePath);
	strncpy_s( filePathWithoutExtension, maxFileSize, filePath, count );
	strcpy_s( extension, maxExtensionsSize, lastDot+1 );
}

static void FilePath_ExtractFilename( char* filename, u32 maxSize, const char* filePath )
{
	const char *c = filePath;
	const char* lastSeparator = filename; 
	while ( *c )
	{
		if ( *c == '/' || *c == '\\' )
			lastSeparator = c;
		++c;
	}

	if ( !lastSeparator )
	{
		strcpy_s( filename, maxSize, filePath );
		return;
	}
	
	strcpy_s( filename, maxSize, lastSeparator+1 );
}

const char* Project_GetName( const Project_s* project )
{
	if ( project->path )
		return project->name;

	static char s_projectNameBuffer[256]; // not thread safe
	sprintf( s_projectNameBuffer, "%s_%s_%s", s_solutionName, project->name, s_solutionVersion );

	return s_projectNameBuffer;
}

static void Solution_Write( FILE* f )
{
	fprintf( f, solutionHeader );

	for ( u32 projectID = 0; projectID < PROJECT_COUNT; ++projectID )
	{
		const Project_s* project = &s_projects[projectID];
		const char* projectName = Project_GetName( project );
		if ( project->path )
			fprintf( f, solutionProjectBegin, solutionGUID, projectName, project->path, projectName, project->guid );
		else
			fprintf( f, solutionProjectBegin, solutionGUID, projectName, "", projectName, project->guid );
		if ( project->dependencies )
		{
			fprintf( f, solutionProjectDependenciesBegin );
			for ( u32 dependentProjectID = 0; dependentProjectID < PROJECT_COUNT; ++dependentProjectID )
			{			
				const Project_s* dependentProject = &s_projects[dependentProjectID];
				if ( project->dependencies & dependentProject->id )
					fprintf( f, solutionProjectDependency, dependentProject->guid, dependentProject->guid );
			}
			fprintf( f, solutionProjectDependenciesEnd );
		}
		fprintf( f, solutionProjectEnd );
	}

	fprintf( f, solutionGlobalBegin );

	fprintf( f, solutionGlobalConfigsBegin );
	for ( u32 configID = 0; configID < CONFIG_COUNT; ++configID )
	{
		char config[256];
		sprintf_s( config, sizeof( config ), "%s|%s", s_configs[configID].name, s_configs[configID].platform );
		fprintf( f, solutionGlobalConfig, config, config );
	}
	fprintf( f, solutionGlobalConfigsEnd );

	fprintf( f, solutionGlobalProjectsBegin );
	for ( u32 projectID = 0; projectID < PROJECT_COUNT; ++projectID )
	{
		const Project_s* project = &s_projects[projectID];
		for ( u32 configID = 0; configID < CONFIG_COUNT; ++configID )
		{
			char config[256];
			sprintf_s( config, sizeof( config ), "%s|%s", s_configs[configID].name, s_configs[configID].platform );
			fprintf( f, solutionGlobalProject, project->guid, config, config, project->guid, config, config );
		}
	}
	fprintf( f, solutionGlobalProjectsEnd );

	fprintf( f, solutionGlobalEnd );
}

static void Project_AddHeaderFile( FILE* f, const Project_s* project, const char* headerFile )
{
	const u32 projectFileCount = sizeof( s_projectFiles) / sizeof( ProjectFile_s );
	
	char filename[256];
	sprintf( filename, "%s/%s/%s", s_solutionPath, s_sourcePath, headerFile );
	if ( FilePath_Exist( filename ) )
	{
		bool isListed = false;
		for ( u32 headerFileID = 0; headerFileID < projectFileCount; ++headerFileID )
		{
			if ( (s_projectFiles[headerFileID].projects & project->id) && stricmp( s_projectFiles[headerFileID].name, headerFile ) == 0 )
			{
				isListed = true;
				break;
			}
		}

		if ( !isListed )
			fprintf( f, projectSourceH, s_sourcePath, headerFile );
	}
}

static void Project_Write( FILE* f, const Project_s* project )
{
	fprintf( f, projectBegin );

	fprintf( f, projectConfigsBegin );
	for ( u32 configID = 0; configID < CONFIG_COUNT; ++configID )
		fprintf( f, projectConfig, s_configs[configID].name, s_configs[configID].platform, s_configs[configID].name, s_configs[configID].platform );
	fprintf( f, projectConfigsEnd );

	const u32 projectFileCount = sizeof( s_projectFiles) / sizeof( ProjectFile_s );
	fprintf( f, projectSourcesBegin );
	for ( u32 projectFileID = 0; projectFileID < projectFileCount; ++projectFileID )
	{
		if ( s_projectFiles[projectFileID].projects & project->id )
		{
			char fullFilename[256];
			sprintf( fullFilename, "%s/%s/%s", s_solutionPath, s_sourcePath, s_projectFiles[projectFileID].name );
			if ( !FilePath_Exist( fullFilename ) )
				printf( "Warning: source file %s not found.\n", s_projectFiles[projectFileID].name );

			char filenameWithoutExtension[256];
			char extension[16];
			FilePath_SplitFilenameAndExtension( filenameWithoutExtension, sizeof( filenameWithoutExtension ), extension, sizeof( extension ), s_projectFiles[projectFileID].name );
			if ( stricmp( extension, "c" ) == 0 || stricmp( extension, "cpp" ) == 0 )
			{
				fprintf( f, projectSourceCPP, s_sourcePath, s_projectFiles[projectFileID].name );
				
				char headerFile[256];
				sprintf( headerFile, "%s.h", filenameWithoutExtension );
				Project_AddHeaderFile( f, project, headerFile );
			}
			else if ( stricmp( extension, "h" ) == 0 || stricmp( extension, "hlsli" ) == 0 )
			{
				fprintf( f, projectSourceH, s_sourcePath, s_projectFiles[projectFileID].name );
			}
			else if ( stricmp( extension, "hlsl" ) == 0 )
			{
				const u32 len = (u32)strlen( filenameWithoutExtension );
				if ( len < 3 )
				{
					printf( "Error: unsupported HLSL file %s\n", s_projectFiles[projectFileID].name );
					exit( 1 );
				}
				else
				{
					const char* fileType = filenameWithoutExtension + len - 3;
					const char* shaderType = nullptr;
					if ( stricmp( fileType, "_vs" ) == 0 )
						shaderType = "Vertex";
					else if ( stricmp( fileType, "_ps" ) == 0 )
						shaderType = "Pixel";
					else if ( stricmp( fileType, "_cs" ) == 0 )
						shaderType = "Compute";
					if ( shaderType == nullptr )
					{
						printf( "Error: unsupported HLSL type %s\n", s_projectFiles[projectFileID].name );
						exit( 1 );
					}
					else
					{
						char filename[256];
						FilePath_ExtractFilename( filename, sizeof( filename ), filenameWithoutExtension );
						fprintf( f, projectSourceHLSL, s_sourcePath, s_projectFiles[projectFileID].name, shaderType, filename,
							s_projectFiles[projectFileID].specialCase1 ? s_projectFiles[projectFileID].specialCase1 : "",
							s_projectFiles[projectFileID].specialCase2 ? s_projectFiles[projectFileID].specialCase2 : "" );
					}
				}
#if 0
				char headerFile[256];
				sprintf( headerFile, "%s_bytecode.h", filenameWithoutExtension );
				Project_AddHeaderFile( f, project, headerFile );
#endif
			}
			else if ( stricmp( extension, "txt" ) == 0 )
			{
				fprintf( f, projectSourceTXT, s_sourcePath, s_projectFiles[projectFileID].name );
			}
			else
			{
				printf( "Error: unsupported source file %s\n", s_projectFiles[projectFileID].name );
				exit( 1 );
			}
		}
	}
	fprintf( f, projectSourcesEnd );

	fprintf( f, projectSet, project->guid, Project_GetName( project ), project->isLib ? "StaticLibrary" : "Application" );

	fprintf( f, projectConfigImportGroupsBegin );
	for ( u32 configID = 0; configID < CONFIG_COUNT; ++configID )
		fprintf( f, projectConfigImportGroup, s_configs[configID].name, s_configs[configID].platform, s_solutionName, s_configs[configID].name, s_solutionVersion );

	char additionalDependencies[4096] = {};
	if ( project->dependencies )
	{
		for ( u32 dependentProjectFileID = 0; dependentProjectFileID < PROJECT_COUNT; ++dependentProjectFileID )
		{
			const Project_s* dependentProject = &s_projects[dependentProjectFileID];
			if ( (project->dependencies & dependentProject->id) != 0 && dependentProject->isLib )
			{
				if ( dependentProject->libPath )
					sprintf( additionalDependencies, "%s;%s", additionalDependencies, dependentProject->libPath );
				else
					sprintf( additionalDependencies, "%s;%s.lib", additionalDependencies, Project_GetName( dependentProject ) );
			}
		}
	}
	fprintf( f, projectItemDefinitionGroup, project->isLib ? "Windows" : "Console", additionalDependencies );

	fprintf( f, projectEnd );
}

static void ProjectUser_Write( FILE* f, const Project_s* project )
{
	fprintf( f, projectUser );
}

int main( int argc, char* argv[] )
{
	if ( argc < 3 )
	{
		printf( "Usage: make SOLUTION VERSION\n" );
		return 1;
	}

	s_solutionName = argv[1];
	s_solutionVersion = argv[2];
	sprintf( s_solutionPath, "project/vc%s", s_solutionVersion );

	{
		char solutionFilename[256];
		sprintf_s( solutionFilename, sizeof( solutionFilename ), "%s/%s_%s.sln", s_solutionPath, s_solutionName, s_solutionVersion );
		FILE* fSolution = fopen( solutionFilename, "wt" );
		if ( !fSolution )
		{
			printf( "Error: Unable to open %s\n", solutionFilename );
			return 1;
		}
		Solution_Write( fSolution );
		fclose( fSolution );
	}

	for ( u32 configID = 0; configID < CONFIG_COUNT; ++configID )
	{
		char commonFilename[256];
		sprintf_s( commonFilename, sizeof( commonFilename ), "%s/%s_common_%s_%s.props", s_solutionPath, s_solutionName, s_configs[configID].name, s_solutionVersion );
		FILE* fCommon = fopen( commonFilename, "wt" );
		if ( !fCommon )
		{
			printf( "Error: Unable to open %s\n", commonFilename );
			return 1;
		}
		fprintf( fCommon, s_configs[configID].props );
		fclose( fCommon );
	}

	for ( u32 projectID = 0; projectID < PROJECT_COUNT; ++projectID )
	{
		const Project_s* project = &s_projects[projectID];
		if ( project->path )
			continue;

		{
			char projectFilename[256];
			sprintf_s( projectFilename, sizeof( projectFilename ), "%s/%s.vcxproj", s_solutionPath, Project_GetName( project ) );
			FILE* fProject = fopen( projectFilename, "wt" );
			if ( !fProject )
			{
				printf( "Error: Unable to open %s\n", projectFilename );
				return 1;
			}
			Project_Write( fProject, project );
			fclose( fProject );
		}

		{
			char projectUserFilename[256];
			sprintf_s( projectUserFilename, sizeof( projectUserFilename ), "%s/%s.vcxproj.user", s_solutionPath, Project_GetName( project ) );
			if ( !FilePath_Exist(  projectUserFilename ) )
			{
				FILE* fProjectUser = fopen( projectUserFilename, "wt" );
				if ( !fProjectUser )
				{
					printf( "Error: Unable to open %s\n", projectUserFilename );
					return 1;
				}
				ProjectUser_Write( fProjectUser, project );
			}
		}
	}

	return 0; 
}