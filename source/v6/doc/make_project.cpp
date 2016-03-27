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
TEMPLATE projectSourceCPP					= TEXT(				<ClCompile Include="../../source/v6/%s" />\n );
TEMPLATE projectSourceHLSL					= TEXT(				<FxCompile Include="../../source/v6/%s" >\n
																	<ShaderType>%s</ShaderType>\n
																	%s
																</FxCompile>\n );
TEMPLATE projectSourceTXT					= TEXT(				<ClInclude Include="../../source/v6/%s" />\n );																
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
																	<AdditionalIncludeDirectories>$(SolutionDir)../../source;$(OVRSDKROOT)/LibOVR/Include;$(OVRSDKROOT)/LibOVRKernel/Src</AdditionalIncludeDirectories>\n
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
																	<AdditionalIncludeDirectories>$(SolutionDir)../../source;$(OVRSDKROOT)/LibOVR/Include;$(OVRSDKROOT)/LibOVRKernel/Src</AdditionalIncludeDirectories>\n
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
	const char*			specialCase;
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
	PROJECT_COMPRESSOR		= 1 << 0,
	PROJECT_CORE			= 1 << 1,
	PROJECT_DOC				= 1 << 2,
	PROJECT_ENCODER			= 1 << 3,
	PROJECT_LIBOVR			= 1 << 4,
	PROJECT_LIBOVRKERNEL	= 1 << 5,
	PROJECT_SORT			= 1 << 6,
	PROJECT_VIEWER			= 1 << 7,

	PROJECT_COUNT			= 8 
};

static const Config_s s_configs[CONFIG_COUNT] = 
{
	{ "Debug" ,		"x64",	commonDebug },
	{ "Release",	"x64",	commonRelease },
};

// https://www.guidgenerator.com/

static const Project_s s_projects[PROJECT_COUNT] = 
{ 
	{ PROJECT_COMPRESSOR,	"EE652B0F-8AEA-42F8-8529-41C075FA4DA8", "compressor"	, PROJECT_CORE },
	{ PROJECT_CORE,			"E3F17175-50AF-4521-8877-58B667ED3DE4", "core"			, 0, true },
	{ PROJECT_DOC,			"3CF8E22B-F0B6-43A7-B472-9E3ACB91591A", "doc" },
	{ PROJECT_ENCODER,		"8B3F2A6F-97DD-4089-82FC-70E0CC3BCC27", "encoder"		, PROJECT_CORE },
	{ PROJECT_LIBOVR,		"EA50E705-5113-49E5-B105-2512EDC8DDC6", "LibOVR"		, 0, true, "../../thirdparty/OculusSDK/LibOVR/Projects/Windows/VS2015/", LIB_OVR },
	{ PROJECT_LIBOVRKERNEL,	"29FA0962-DDC6-4F72-9D12-E150DF29E279", "LibOVRKernel"	, 0, true, "../../thirdparty/OculusSDK/LibOVRKernel/Projects/Windows/VS2015/", LIB_OVR_KERNEL },
	{ PROJECT_SORT,			"243F021E-5EA3-44C8-A995-715B12463643", "sort"			, PROJECT_CORE },
	{ PROJECT_VIEWER,		"CEC43B15-39D4-463B-825C-D630A53DAFB0", "viewer"		, PROJECT_CORE | PROJECT_LIBOVR | PROJECT_LIBOVRKERNEL },
};

static ProjectFile_s s_projectFiles[] =
{
	{ "compressor/main_compressor.cpp",			PROJECT_COMPRESSOR },

	{ "core/codec.cpp",							PROJECT_CORE },
	{ "core/compute.cpp",						PROJECT_CORE },
	{ "core/encoder.cpp",						PROJECT_CORE },
	{ "core/filesystem.cpp",					PROJECT_CORE },
	{ "core/grid.cpp",							PROJECT_CORE },
	{ "core/image.cpp",							PROJECT_CORE },
	{ "core/kdtree_sphere.cpp",					PROJECT_CORE },
	{ "core/memory.cpp",						PROJECT_CORE },
	{ "core/octree.cpp",						PROJECT_CORE },
	{ "core/stream.cpp",						PROJECT_CORE },
	{ "core/thread.cpp",						PROJECT_CORE },
	{ "core/time.cpp",							PROJECT_CORE },
	{ "core/vec3.cpp",							PROJECT_CORE },

	{ "doc/make_project.cpp",					PROJECT_DOC },
	{ "doc/survey.txt",							PROJECT_DOC },
	{ "doc/todo.txt",							PROJECT_DOC },
	
	{ "encoder/main_encoder.cpp",				PROJECT_ENCODER },

	{ "sort/main_sort.cpp",						PROJECT_SORT },

	{ "viewer/hmd.cpp",							PROJECT_VIEWER },
	{ "viewer/main_viewer.cpp",					PROJECT_VIEWER },
	{ "viewer/obj_reader.cpp",					PROJECT_VIEWER },
	{ "viewer/scene_info.cpp",					PROJECT_VIEWER },

	{ "viewer/basic_ps.hlsl",					PROJECT_VIEWER },
	{ "viewer/basic_vs.hlsl",					PROJECT_VIEWER },
	{ "viewer/block_cull_stats_x16_cs.hlsl",	PROJECT_VIEWER },
	{ "viewer/block_cull_stats_x32_cs.hlsl",	PROJECT_VIEWER },
	{ "viewer/block_cull_stats_x4_cs.hlsl",		PROJECT_VIEWER },
	{ "viewer/block_cull_stats_x64_cs.hlsl",	PROJECT_VIEWER },
	{ "viewer/block_cull_stats_x8_cs.hlsl",		PROJECT_VIEWER },
	{ "viewer/block_cull_x16_cs.hlsl",			PROJECT_VIEWER },
	{ "viewer/block_cull_x32_cs.hlsl",			PROJECT_VIEWER },
	{ "viewer/block_cull_x4_cs.hlsl",			PROJECT_VIEWER },
	{ "viewer/block_cull_x64_cs.hlsl",			PROJECT_VIEWER },
	{ "viewer/block_cull_x8_cs.hlsl",			PROJECT_VIEWER },
	{ "viewer/block_trace_cs.hlsl",				PROJECT_VIEWER },
	{ "viewer/block_trace_debug_cs.hlsl",		PROJECT_VIEWER },
	{ "viewer/block_trace_debug_x16_cs.hlsl",	PROJECT_VIEWER },
	{ "viewer/block_trace_debug_x32_cs.hlsl",	PROJECT_VIEWER },
	{ "viewer/block_trace_debug_x4_cs.hlsl",	PROJECT_VIEWER },
	{ "viewer/block_trace_debug_x64_cs.hlsl",	PROJECT_VIEWER },
	{ "viewer/block_trace_debug_x8_cs.hlsl",	PROJECT_VIEWER },
	{ "viewer/block_trace_init_cs.hlsl",		PROJECT_VIEWER },
	{ "viewer/block_trace_x16_cs.hlsl",			PROJECT_VIEWER },
	{ "viewer/block_trace_x32_cs.hlsl",			PROJECT_VIEWER },
	{ "viewer/block_trace_x4_cs.hlsl",			PROJECT_VIEWER },
	{ "viewer/block_trace_x64_cs.hlsl",			PROJECT_VIEWER },
	{ "viewer/block_trace_x8_cs.hlsl",			PROJECT_VIEWER },
	{ "viewer/fake_cube_ps.hlsl",				PROJECT_VIEWER },
	{ "viewer/fake_cube_vs.hlsl",				PROJECT_VIEWER },
	{ "viewer/generic_alpha_test_ps.hlsl",		PROJECT_VIEWER },
	{ "viewer/generic_ps.hlsl",					PROJECT_VIEWER },
	{ "viewer/generic_vs.hlsl",					PROJECT_VIEWER },
	{ "viewer/octree_build_inner_cs.hlsl",		PROJECT_VIEWER },
	{ "viewer/octree_build_leaf_cs.hlsl",		PROJECT_VIEWER },
	{ "viewer/octree_fill_leaf_cs.hlsl",		PROJECT_VIEWER },
	{ "viewer/octree_pack_cs.hlsl",				PROJECT_VIEWER },
	{ "viewer/pixel_blend_cs.hlsl",				PROJECT_VIEWER },
	{ "viewer/pixel_blend_overdraw_cs.hlsl",	PROJECT_VIEWER },
	{ "viewer/sample_collect_cs.hlsl",			PROJECT_VIEWER, HLSL_DisableTreatWarningAsError },
	{ "viewer/stream_prefix_sum_cs.hlsl",		PROJECT_VIEWER },
	{ "viewer/stream_scatter_x16_cs.hlsl",		PROJECT_VIEWER },
	{ "viewer/stream_scatter_x32_cs.hlsl",		PROJECT_VIEWER },
	{ "viewer/stream_scatter_x4_cs.hlsl",		PROJECT_VIEWER },
	{ "viewer/stream_scatter_x64_cs.hlsl",		PROJECT_VIEWER },
	{ "viewer/stream_scatter_x8_cs.hlsl",		PROJECT_VIEWER },
	{ "viewer/stream_setbit_x16_cs.hlsl",		PROJECT_VIEWER },
	{ "viewer/stream_setbit_x32_cs.hlsl",		PROJECT_VIEWER },
	{ "viewer/stream_setbit_x4_cs.hlsl",		PROJECT_VIEWER },
	{ "viewer/stream_setbit_x64_cs.hlsl",		PROJECT_VIEWER },
	{ "viewer/stream_setbit_x8_cs.hlsl",		PROJECT_VIEWER },
	{ "viewer/stream_summarize_cs.hlsl",		PROJECT_VIEWER },
	{ "viewer/surface_compose_cs.hlsl",			PROJECT_VIEWER },
};

char* s_solutionName	= nullptr;
char* s_solutionVersion = nullptr;

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
			char filenameWithoutExtension[256];
			char extension[16];
			FilePath_SplitFilenameAndExtension( filenameWithoutExtension, sizeof( filenameWithoutExtension ), extension, sizeof( extension ), s_projectFiles[projectFileID].name );
			if ( stricmp( extension, "c" ) == 0 || stricmp( extension, "cpp" ) == 0 )
			{
				fprintf( f, projectSourceCPP, s_projectFiles[projectFileID].name );
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
						fprintf( f, projectSourceHLSL, s_projectFiles[projectFileID].name, shaderType, s_projectFiles[projectFileID].specialCase ? s_projectFiles[projectFileID].specialCase : "" );
					}
				}
			}
			else if ( stricmp( extension, "txt" ) == 0 )
			{
				fprintf( f, projectSourceTXT, s_projectFiles[projectFileID].name );
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

	{
		char solutionFilename[256];
		sprintf_s( solutionFilename, sizeof( solutionFilename ), "project/vc%s/%s_%s.sln", s_solutionVersion, s_solutionName, s_solutionVersion );
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
		sprintf_s( commonFilename, sizeof( commonFilename ), "project/vc%s/%s_common_%s_%s.props", s_solutionVersion, s_solutionName, s_configs[configID].name, s_solutionVersion );
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
			sprintf_s( projectFilename, sizeof( projectFilename ), "project/vc%s/%s.vcxproj", s_solutionVersion, Project_GetName( project ) );
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
			sprintf_s( projectUserFilename, sizeof( projectUserFilename ), "project/vc%s/%s.vcxproj.user", s_solutionVersion, Project_GetName( project ) );
			FILE* fProjectUser = fopen( projectUserFilename, "rt" );
			if ( !fProjectUser )
			{
				fProjectUser = fopen( projectUserFilename, "wt" );
				if ( !fProjectUser )
				{
					printf( "Error: Unable to open %s\n", projectUserFilename );
					return 1;
				}
				ProjectUser_Write( fProjectUser, project );
			}
			fclose( fProjectUser );
		}
	}

	return 0; 
}