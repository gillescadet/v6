/*V6*/

#pragma warning( push, 3 )
#include <windows.h>
#include <Windowsx.h>
#include <d3d11_1.h>
#pragma warning( pop )

#include <v6/viewer/common.h>
#include <v6/viewer/common_shared.h>

#include <v6/core/codec.h>
#include <v6/core/color.h>
#include <v6/core/filesystem.h>
#include <v6/core/image.h>
#include <v6/core/math.h>
#include <v6/core/mat4x4.h>
#include <v6/core/memory.h>
#include <v6/core/random.h>
#include <v6/core/stream.h>
#include <v6/core/time.h>
#include <v6/core/thread.h>
#include <v6/core/vec2.h>
#include <v6/core/vec2i.h>
#include <v6/core/vec3.h>
#include <v6/core/vec3i.h>

#include <v6/viewer/obj_reader.h>
#include <v6/viewer/scene_info.h>

#pragma comment(lib, "d3d11.lib")

#define V6_GPU_PROFILING		1
#define V6_D3D_DEBUG			0
#define V6_LOAD_EXTERNAL		1
#define V6_SIMPLE_SCENE			0
#define V6_USE_ALPHA_COVERAGE	1
#define V6_ENABLE_HMD			1
#define V6_USE_HMD				(V6_ENABLE_HMD == 1 && HLSL_STEREO == 1)
#define V6_SORTED_STREAM		0

#if V6_USE_HMD == 1
#include <v6/viewer/hmd.h>
#endif // #if HLSL_STEREO == 1

#define V6_ASSERT_D3D11( EXP )  { HRESULT hRes = EXP; V6_ASSERT( hRes == S_OK ); }
#define V6_RELEASE_D3D11( EXP )  { V6_ASSERT( EXP ); EXP->Release(); EXP = nullptr; }

#define KB( X )					((X) >> 10)
#define MB( X )					((X) >> 20)
#define GB( X )					((X) >> 30)

BEGIN_V6_VIEWER_NAMESPACE

static const float AVERAGE_LAYER_COUNT			= 4.0f; // temp for low res
//static const float AVERAGE_LAYER_COUNT			= 2.0f;
#if HLSL_CELL_SUPER_SAMPLING_WIDTH > 1
static const float AVERAGE_SAMPLE_PER_PIXEL		= 0.25f * HLSL_CELL_SUPER_SAMPLING_WIDTH * HLSL_CELL_SUPER_SAMPLING_WIDTH;
#else
static const float AVERAGE_SAMPLE_PER_PIXEL		= 1.0f;
#endif
static const core::u32 CUBE_SIZE				= HLSL_GRID_WIDTH * HLSL_CELL_SUPER_SAMPLING_WIDTH;
static const float GRID_MAX_SCALE				= 2000.0f;
static const float GRID_MIN_SCALE				= 50.0f;

static const core::u32 ANY_EYE					= 0;
#if HLSL_STEREO == 1
static const core::u32 LEFT_EYE					= 0;
static const core::u32 RIGHT_EYE				= 1;
static const float IPD							= 6.5f;
#else
static const core::u32 LEFT_EYE					= 0;
static const core::u32 RIGHT_EYE				= 0;
static const float IPD							= 0.0f;
#endif
static const float ZNEAR						= GRID_MIN_SCALE * 0.5f;
static const float ZFAR							= 10000.0f;
#if V6_SIMPLE_SCENE == 1
static const float FOV							= core::DegToRad( 90.0f );
#else
static const float FOV							= core::DegToRad( 90.0f );
#endif
static const core::u32 GRID_COUNT				= 1 + core::u32( ceil( log2f( (float)GRID_MAX_SCALE / GRID_MIN_SCALE ) ) );
static const int SAMPLE_MAX_COUNT				= 9;
static const float FREE_SCALE					= 50.0f;
static const core::u32 RANDOM_CUBE_COUNT		= 100;

static const core::u32 HMD_FPS					= 75;
static const core::u32 VIDEO_FRAME_MAX_COUNT	= 75;

static const core::u32 VERTEX_INPUT_MAX_COUNT	= 6;
static const core::u32 MESH_MAX_COUNT			= 16384;
static const core::u32 TEXTURE_MAX_COUNT		= 1024;
static const core::u32 MATERIAL_MAX_COUNT		= 256;
static const core::u32 ENTITY_MAX_COUNT			= 16384;

static const core::u32 ENTITY_TEXTURE_MAX_COUNT	= 4;
static const core::u32 ENTITY_TEXTURE_INVALID	= (core::u32)-1;

static const core::u32 DEBUG_BLOCK_MAX_COUNT	= HLSL_BLOCK_THREAD_GROUP_SIZE * 80;
static const core::u32 DEBUG_TRACE_MAX_COUNT	= HLSL_BLOCK_THREAD_GROUP_SIZE * 80;

#if V6_USE_HMD
v6::core::u32		s_hmdState				= v6::viewer::HMD_TRACKING_STATE_OFF;
#else
v6::core::u32		s_hmdState				= 0;
#endif // #if V6_USE_HMD

enum DrawMode_e
{
	DRAW_MODE_DEFAULT,	
	DRAW_MODE_BLOCK,
	
	DRAW_MODE_COUNT
};

enum
{
	VERTEX_FORMAT_POSITION		= 1 << 0,
	
	VERTEX_FORMAT_COLOR			= 1 << 1,
		
	VERTEX_FORMAT_USER0_SHIFT	= 2,
	VERTEX_FORMAT_USER0_MASK	= 7 << VERTEX_FORMAT_USER0_SHIFT,
	VERTEX_FORMAT_USER0_F1		= 1 << VERTEX_FORMAT_USER0_SHIFT,
	VERTEX_FORMAT_USER0_F2		= 2 << VERTEX_FORMAT_USER0_SHIFT,
	VERTEX_FORMAT_USER0_F3		= 3 << VERTEX_FORMAT_USER0_SHIFT,
	VERTEX_FORMAT_USER0_F4		= 4 << VERTEX_FORMAT_USER0_SHIFT,
	
	
	VERTEX_FORMAT_USER1_SHIFT	= 5,
	VERTEX_FORMAT_USER1_MASK	= 7 << VERTEX_FORMAT_USER1_SHIFT,
	VERTEX_FORMAT_USER1_F1		= 1 << VERTEX_FORMAT_USER1_SHIFT,
	VERTEX_FORMAT_USER1_F2		= 2 << VERTEX_FORMAT_USER1_SHIFT,
	VERTEX_FORMAT_USER1_F3		= 3 << VERTEX_FORMAT_USER1_SHIFT,
	VERTEX_FORMAT_USER1_F4		= 4 << VERTEX_FORMAT_USER1_SHIFT,

	VERTEX_FORMAT_USER2_SHIFT	= 8,
	VERTEX_FORMAT_USER2_MASK	= 7 << VERTEX_FORMAT_USER2_SHIFT,
	VERTEX_FORMAT_USER2_F1		= 1 << VERTEX_FORMAT_USER2_SHIFT,
	VERTEX_FORMAT_USER2_F2		= 2 << VERTEX_FORMAT_USER2_SHIFT,
	VERTEX_FORMAT_USER2_F3		= 3 << VERTEX_FORMAT_USER2_SHIFT,
	VERTEX_FORMAT_USER2_F4		= 4 << VERTEX_FORMAT_USER2_SHIFT,

	VERTEX_FORMAT_USER3_SHIFT	= 11,
	VERTEX_FORMAT_USER3_MASK	= 7 << VERTEX_FORMAT_USER3_SHIFT,
	VERTEX_FORMAT_USER3_F1		= 1 << VERTEX_FORMAT_USER3_SHIFT,
	VERTEX_FORMAT_USER3_F2		= 2 << VERTEX_FORMAT_USER3_SHIFT,
	VERTEX_FORMAT_USER3_F3		= 3 << VERTEX_FORMAT_USER3_SHIFT,
	VERTEX_FORMAT_USER3_F4		= 4 << VERTEX_FORMAT_USER3_SHIFT,
};

enum
{
	CONSTANT_BUFFER_BASIC		=	hlsl::CBBasicSlot,
	CONSTANT_BUFFER_GENERIC		=	hlsl::CBGenericSlot,
	CONSTANT_BUFFER_SAMPLE		=	hlsl::CBSampleSlot,
	CONSTANT_BUFFER_OCTREE		=	hlsl::CBOctreeSlot,
	CONSTANT_BUFFER_CULL		=	hlsl::CBCullSlot,
	CONSTANT_BUFFER_BLOCK		=	hlsl::CBBlockSlot,
	CONSTANT_BUFFER_PIXEL		=	hlsl::CBPixelSlot,
	CONSTANT_BUFFER_COMPOSE		=	hlsl::CBComposeSlot,
	CONSTANT_BUFFER_STREAM		=	hlsl::CBStreamSlot,

	CONSTANT_BUFFER_COUNT
};

enum
{
	COMPUTE_SAMPLECOLLECT,
	COMPUTE_BUILDINNER,
	COMPUTE_BUILDLEAF,
	COMPUTE_FILLLEAF,
	COMPUTE_PACKCOLOR,
	COMPUTE_STREAM_SETBIT4,
	COMPUTE_STREAM_SETBIT8,
	COMPUTE_STREAM_SETBIT16,
	COMPUTE_STREAM_SETBIT32,
	COMPUTE_STREAM_SETBIT64,
	COMPUTE_STREAM_PREFIX_SUM,
	COMPUTE_STREAM_SUMMARIZE,
	COMPUTE_STREAM_SCATTER4,
	COMPUTE_STREAM_SCATTER8,
	COMPUTE_STREAM_SCATTER16,
	COMPUTE_STREAM_SCATTER32,
	COMPUTE_STREAM_SCATTER64,
	COMPUTE_BLOCK_CULL4,
	COMPUTE_BLOCK_CULL8,
	COMPUTE_BLOCK_CULL16,
	COMPUTE_BLOCK_CULL32,
	COMPUTE_BLOCK_CULL64,
	COMPUTE_BLOCK_CULL_STATS4,
	COMPUTE_BLOCK_CULL_STATS8,
	COMPUTE_BLOCK_CULL_STATS16,
	COMPUTE_BLOCK_CULL_STATS32,
	COMPUTE_BLOCK_CULL_STATS64,
	COMPUTE_BLOCK_TRACE_INIT,
#if HLSL_ENCODE_DATA == 1
	COMPUTE_BLOCK_TRACE,
	COMPUTE_BLOCK_TRACE_STATS,
#else
	COMPUTE_BLOCK_TRACE4,
	COMPUTE_BLOCK_TRACE8,
	COMPUTE_BLOCK_TRACE16,
	COMPUTE_BLOCK_TRACE32,
	COMPUTE_BLOCK_TRACE64,
	COMPUTE_BLOCK_TRACE_STATS4,
	COMPUTE_BLOCK_TRACE_STATS8,
	COMPUTE_BLOCK_TRACE_STATS16,
	COMPUTE_BLOCK_TRACE_STATS32,
	COMPUTE_BLOCK_TRACE_STATS64,
#endif
	COMPUTE_BLENDPIXEL,
	COMPUTE_BLENDPIXEL_OVERDRAW,
	COMPUTE_COMPOSESURFACE,

	COMPUTE_COUNT
};

enum
{
	SHADER_BASIC,
	SHADER_FAKE_CUBE,
	SHADER_GENERIC,
	SHADER_GENERIC_ALPHA_TEST,

	SHADER_COUNT
};

enum
{
	QUERY_FREQUENCY,
	QUERY_FRAME_BEGIN,
	QUERY_T0,
	QUERY_T1,
	QUERY_T2,
	QUERY_T3,
	QUERY_FRAME_END,

	QUERY_COUNT
};

enum
{
	MESH_TRIANGLE,
	MESH_BOX_WIREFRAME,
	MESH_BOX_RED,
	MESH_BOX_BLUE,
	MESH_BOX_GREEN,
	MESH_QUAD_RED0,
	MESH_QUAD_RED1,
	MESH_QUAD_RED2,
	MESH_QUAD_BLUE,
	MESH_QUAD_GREEN,
	MESH_VIRTUAL_QUAD,
	MESH_FAKE_CUBE,
	MESH_POINT,
	MESH_LINE,
	MESH_VIRTUAL_BOX,

	MESH_COUNT
};

enum
{
	MATERIAL_DEFAULT_BASIC,
	MATERIAL_DEFAULT_FAKE_CUBE,

	MATERIAL_DEFAULT_COUNT
};

enum
{
	TEXTURE_GENERIC_DIFFUSE,
	TEXTURE_GENERIC_ALPHA,
	TEXTURE_GENERIC_NORMAL,

	TEXTURE_GENERIC_COUNT
};

enum CubeAxis_e
{
	CUBE_AXIS_POSITIVE_X,
	CUBE_AXIS_NEGATIVE_X,
	CUBE_AXIS_POSITIVE_Y,
	CUBE_AXIS_NEGATIVE_Y,
	CUBE_AXIS_POSITIVE_Z,
	CUBE_AXIS_NEGATIVE_Z,

	CUBE_AXIS_COUNT
};

struct CameraPath_s
{
	static const core::u32	MAX_POINT_COUNT = 128;
	core::Vec3				positions[MAX_POINT_COUNT];
	float					times[MAX_POINT_COUNT];
	int						keyCount;
	float					duration;
	int						activeKey;
	bool					dirty;
};

struct BasicVertex_s
{
	core::Vec3 position;
	core::Color_s color;
};

struct GenericVertex_s
{
	core::Vec3		position;
	core::Vec3		normal;
	core::Vec2		uv;
};

struct CubeVertex_s
{
	core::Vec3 pos;
	core::Vec2 uv;
};

enum GPUBufferCreationFlag_e
{
	GPUBUFFER_CREATION_FLAG_READ_BACK = 1 << 0
};

struct GPUBuffer_s
{
	ID3D11Buffer*					buf;
	ID3D11Buffer*					staging;
	ID3D11ShaderResourceView*		srv;
	ID3D11UnorderedAccessView*		uav;
};

enum GPUTextureMipMapState_e
{
	GPUTEXTURE_MIPMAP_STATE_NONE,
	GPUTEXTURE_MIPMAP_STATE_REQUIRED,
	GPUTEXTURE_MIPMAP_STATE_GENERATED,
};

struct GPUTexture2D_s
{
	ID3D11Texture2D*				tex;
	ID3D11ShaderResourceView*		srv;
	ID3D11UnorderedAccessView*		uav;
	GPUTextureMipMapState_e			mipmapState;
};

struct GPUConstantBuffer_s
{
	ID3D11Buffer*					buf;
};

struct GPUCompute_s
{
	ID3D11ComputeShader* m_computeShader;
};

struct GPUShader_s
{
	ID3D11VertexShader* m_vertexShader;
	ID3D11PixelShader*	m_pixelShader;

	ID3D11InputLayout*	m_inputLayout;

	core::u32			m_vertexFormat;
};

struct GPUMesh_s
{
	ID3D11Buffer*		m_vertexBuffer;
	ID3D11Buffer*		m_indexBuffer;	
	core::u32			m_vertexCount;
	core::u32			m_vertexSize;
	core::u32			m_vertexFormat;
	core::u32			m_indexCount;
	core::u32			m_indexSize;
	D3D11_PRIMITIVE_TOPOLOGY m_topology;	
};

struct GPUQuery_s 
{
#if V6_GPU_PROFILING == 1
	ID3D11Query*	query;
#endif
	core::u64		data;
};

struct GPUContext_s
{
	IDXGISwapChain*				swapChain;
	ID3D11Device*				device;
	D3D_FEATURE_LEVEL			featureLevel;
	ID3D11DeviceContext*		deviceContext;
	ID3DUserDefinedAnnotation*	userDefinedAnnotation;
	ID3D11Texture2D*			surfaceBuffer;
	ID3D11RenderTargetView*		surfaceView;
	ID3D11UnorderedAccessView*	surfaceUAV;
	ID3D11Texture2D*			colorBuffers[HLSL_EYE_COUNT];
	ID3D11RenderTargetView*		colorViews[HLSL_EYE_COUNT];
	ID3D11ShaderResourceView*	colorSRVs[HLSL_EYE_COUNT];
	ID3D11UnorderedAccessView*	colorUAVs[HLSL_EYE_COUNT];
	ID3D11Texture2D*			depthStencilBuffer;
	ID3D11DepthStencilView*		depthStencilView;
	ID3D11Texture2D*			colorBufferMSAA;
	ID3D11RenderTargetView*		colorViewMSAA;
	ID3D11Texture2D*			depthStencilBufferMSAA;
	ID3D11DepthStencilView*		depthStencilViewMSAA;
	ID3D11DepthStencilState*	depthStencilStateNoZ;
	ID3D11DepthStencilState*	depthStencilStateZRO;
	ID3D11DepthStencilState*	depthStencilStateZRW;
	ID3D11BlendState*			blendStateNoColor;
	ID3D11BlendState*			blendStateOpaque;
	ID3D11BlendState*			blendStateAlphaCoverage;
	ID3D11BlendState*			blendStateAdditif;
	ID3D11RasterizerState*		rasterState;	
	ID3D11SamplerState*			samplerState;

	GPUConstantBuffer_s			constantBuffers[CONSTANT_BUFFER_COUNT];
	GPUCompute_s				computes[COMPUTE_COUNT];
	GPUShader_s					shaders[SHADER_COUNT];
	GPUQuery_s					queries[2][QUERY_COUNT];
	GPUQuery_s*					pendingQueries;
};

struct RenderingView_s
{
	ID3D11Texture2D*			texture2D;
	ID3D11RenderTargetView*		rtv;
	ID3D11UnorderedAccessView*	uav;
	core::Vec3					org;
	core::Vec3					forward;
	core::Vec3					right;
	core::Vec3					up;
	core::Mat4x4				viewMatrix;
	core::Mat4x4				projMatrix;
	core::u32					eye; // neeeded?
	float						tanHalfFOVLeft;
	float						tanHalfFOVRight;
	float						tanHalfFOVUp;
	float						tanHalfFOVDown;
};

struct RenderingSettings_s
{
	bool			isCapturing;
	bool			useAlphaCoverage;
};

struct Material_s;
struct Entity_s;
struct Scene_s;
typedef void (*MaterialDraw_f)( Material_s* material, Entity_s* entity, Scene_s* scene, GPUContext_s* ctx, const RenderingView_s* view, const RenderingSettings_s* settings );

struct Material_s
{
	MaterialDraw_f	drawFunction;
	core::u32		textureIDs[ENTITY_TEXTURE_MAX_COUNT];
	core::Vec3		diffuse;
};

struct Entity_s
{
	core::u32		materialID;
	core::u32		meshID;
	core::Vec3		pos;
	float			scale;
	bool			visible;
};

struct Scene_s
{
	char			filename[256];
	SceneInfo_s		info;
	GPUMesh_s		meshes[MESH_MAX_COUNT];	
	GPUTexture2D_s	textures[TEXTURE_MAX_COUNT];
	Material_s		materials[MATERIAL_MAX_COUNT];
	Entity_s		entities[ENTITY_MAX_COUNT];
	core::u32		meshCount;
	core::u32		textureCount;
	core::u32		materialCount;
	core::u32		entityCount;
};

struct SceneDebug_s : Scene_s
{
	core::u32 meshLineID;
	core::u32 meshGridID;
	core::u32 meshCellIDs[HLSL_CELL_SUPER_SAMPLING_WIDTH_CUBE][2];
	core::u32 meshBlockIDs[DEBUG_BLOCK_MAX_COUNT][2];
	core::u32 entityLineID;
	core::u32 entityGridID;
	core::u32 entityCellIDs[HLSL_CELL_SUPER_SAMPLING_WIDTH_CUBE][2];	
	core::u32 entityBlockIDs[DEBUG_BLOCK_MAX_COUNT][2];
	core::u32 entityTraceIDs[DEBUG_BLOCK_MAX_COUNT];
};

struct SceneCameraPath_s : Scene_s
{
	core::u32 meshLineIDs[CameraPath_s::MAX_POINT_COUNT-1];
	core::u32 meshBoxID;
	core::u32 meshSelectedBoxID;
	core::u32 entityLineIDs[CameraPath_s::MAX_POINT_COUNT];
	core::u32 entityBoxIDs[CameraPath_s::MAX_POINT_COUNT];
	core::u32 entitySelectedBoxID;
};

struct Cube_s
{	
	ID3D11Texture2D* colorBuffer;	
	ID3D11ShaderResourceView* colorSRV;	
	ID3D11RenderTargetView* colorRTV;
	
	ID3D11Texture2D* depthBuffer;
	ID3D11ShaderResourceView* depthSRV;
	ID3D11DepthStencilView* depthRTV;

	core::u32 size;
};

struct Config_s
{
	core::u32 screenWidth;
	core::u32 screenHeight;
	core::u32 sampleCount;
	core::u32 leafCount;
	core::u32 nodeCount;	
	core::u32 cellCount;
	core::u32 blockCount;
	core::u32 culledBlockCount;
	core::u32 culledCellCount;
	core::u32 cellItemCount;
};

struct Sample_s
{
	GPUBuffer_s					samples;
	GPUBuffer_s					indirectArgs;
};

struct Octree_s
{
	GPUBuffer_s					sampleNodeOffsets;
	GPUBuffer_s					firstChildOffsets;
	ID3D11UnorderedAccessView*	firstChildOffsetsLimitedUAV;
	GPUBuffer_s					leaves;
	GPUBuffer_s					indirectArgs;
};

struct Stream_s
{
#if V6_SORTED_STREAM == 1
	GPUBuffer_s					groupBits;
	GPUBuffer_s					bits;
	GPUBuffer_s					counts;
	GPUBuffer_s					addresses;
#endif // #if V6_SORTED_STREAM == 1
	GPUBuffer_s					blockPos;
	GPUBuffer_s					blockData;

	core::u32					blockCount;
	core::u32					cellCount;
};

struct Block_s
{
	GPUBuffer_s					blockPos;
	GPUBuffer_s					blockData;
	GPUBuffer_s					blockIndirectArgs;
};

struct Trace_s
{
	GPUBuffer_s					traceCell;
	GPUBuffer_s					traceIndirectArgs;
	
	GPUBuffer_s					cellItems;
	GPUBuffer_s					cellItemCounters;

	GPUTexture2D_s				colors;

	GPUBuffer_s					cullStats;
	GPUBuffer_s					traceStats;
};

struct TraceData_s
{
	float		tIn;
	float		tOut;
	core::Vec3i	hitFoundCoords;
	core::u32	hitFailBits;
};

static float g_translation_speed			= 200.0f;
static bool g_mousePressed					= false;
static int g_mouseDeltaX					= 0;
static int g_mouseDeltaY					= 0;
static int g_mousePickPosX					= 0;
static int g_mousePickPosY					= 0;
static bool g_mousePicked					= false;
static core::u32 g_pickedPackedID			= (core::u32)-1;
static int g_keyLeftPressed					= false;	
static int g_keyRightPressed				= false;
static int g_keyUpPressed					= false;
static int g_keyDownPressed					= false;

static DrawMode_e g_drawMode				= DRAW_MODE_DEFAULT;

static int g_sample							= 0;

static bool g_cameraKey						= false;
static bool g_showCameraPath				= false;
static bool g_playCameraPath				= false;
static float g_playCameraPathTime			= 0.0f;
static core::u32 g_playCameraPathFrame		= 0;
static float g_cameraSpeed					= 100.0f;
static int g_limit							= false; 
static bool g_showMip						= false;
static bool g_showOverdraw					= false;
static int g_pixelMode						= 0;
static bool g_randomBackground				= false;
static bool g_traceGrid						= false;
#if V6_SIMPLE_SCENE == 1
static bool g_useMSAA						= false;
#else
static bool g_useMSAA						= true;
#endif
static bool g_debugBlocks					= false;
static bool g_transparentDebug				= false;
static bool g_reloadShaders					= false;

static float s_yaw							= 0.0f;
static float s_pitch						= 0.0f;
static core::Vec3 s_headOffset				= core::Vec3_Zero();
static core::Vec3 s_buildOrigin				= core::Vec3_Zero();

static core::u32 gpuMemory					= 0;

static bool s_logReadBack					= false;

static Scene_s* s_activeScene				= nullptr;

static core::Vec3 s_gridCenter = {};
static float s_gridScale = 0;
static core::u32 s_gridOccupancy = 0;
static core::Vec3 s_rayOrg = {};
static core::Vec3 s_rayEnd = {};

static CameraPath_s s_cameraPath;

void CameraPath_Init( CameraPath_s* cameraPath )
{
	memset( cameraPath, 0, sizeof( *cameraPath ) );
	cameraPath->dirty = true;
}

void CameraPath_Release( CameraPath_s* cameraPath )
{
}

void CameraPath_SelectKey( CameraPath_s* cameraPath, int key )
{
	cameraPath->activeKey = core::Clamp( key, 0, cameraPath->keyCount-1 );
	cameraPath->dirty = true;
}

void CameraPath_DeleteKey( CameraPath_s* cameraPath, core::u32 key )
{
	V6_ASSERT( cameraPath->keyCount > 0 );
	V6_ASSERT( key < (core::u32)cameraPath->keyCount );
	for ( core::u32 keyNext = key+1; keyNext < (core::u32)cameraPath->keyCount; ++keyNext )
		cameraPath->positions[keyNext-1] = cameraPath->positions[keyNext];
	--cameraPath->keyCount;

	cameraPath->activeKey = core::Min( (int)key, cameraPath->keyCount );
	cameraPath->dirty = true;
}

void CameraPath_InsertKey( CameraPath_s* cameraPath, const core::Vec3* position, core::u32 key )
{
	V6_ASSERT( cameraPath->keyCount < CameraPath_s::MAX_POINT_COUNT );
	if ( key != 0 || cameraPath->keyCount != 0 )
	{
		++key;
		for ( core::u32 keyNext = cameraPath->keyCount; keyNext > key; --keyNext )
			cameraPath->positions[keyNext] = cameraPath->positions[keyNext-1];
	}
	V6_ASSERT( key <= (core::u32)cameraPath->keyCount );
	cameraPath->positions[key] = *position;
	cameraPath->times[key] = -1.0f;
	++cameraPath->keyCount;

	cameraPath->activeKey = key;
	cameraPath->dirty = true;
}

void CameraPath_Compute( CameraPath_s* cameraPath, float speed )
{
	V6_ASSERT( speed > 0.0f );
	const float invSpeed = 1.0f / speed;
	cameraPath->times[0] = 0.0f;
	cameraPath->duration = 0.0f;
	for ( core::u32 key = 1; key < (core::u32)cameraPath->keyCount; ++key )
	{
		const core::Vec3 delta = cameraPath->positions[key] - cameraPath->positions[key-1];
		const float interval = delta.Length() * invSpeed;
		V6_ASSERT( interval > 0.00001f );
		cameraPath->times[key] = cameraPath->times[key-1] + interval;
		cameraPath->duration += interval;
	}

	cameraPath->dirty = false;
}

core::Vec3 CameraPath_GetPosition( CameraPath_s* cameraPath, float time )
{
	V6_ASSERT( !cameraPath->dirty );
	V6_ASSERT( cameraPath->keyCount > 0 );
	V6_ASSERT( time >= 0.0f );
	for ( core::u32 key = 1; key < (core::u32)cameraPath->keyCount; ++key )
	{
		if ( cameraPath->times[key] >= time )
		{
			const float alpha = (time - cameraPath->times[key-1]) / (cameraPath->times[key] - cameraPath->times[key-1]);
			return (1.0f - alpha) * cameraPath->positions[key-1] + alpha * cameraPath->positions[key];
		}
	}
	return cameraPath->positions[cameraPath->keyCount-1];
}

void CameraPath_Load( CameraPath_s* cameraPath, const SceneInfo_s* sceneInfo )
{
	for ( core::u32 cameraPositionID = 0; cameraPositionID < sceneInfo->cameraPositionCount; ++cameraPositionID )
		cameraPath->positions[cameraPositionID] = sceneInfo->cameraPositions[cameraPositionID];
	cameraPath->keyCount = sceneInfo->cameraPositionCount;
	cameraPath->dirty = true;
}

void CameraPath_Save( const CameraPath_s* cameraPath, SceneInfo_s* sceneInfo )
{
	for ( core::u32 key = 0; key < (core::u32)cameraPath->keyCount; ++key )
		sceneInfo->cameraPositions[key] = cameraPath->positions[key];
	sceneInfo->cameraPositionCount = cameraPath->keyCount;
	sceneInfo->dirty = true;
}

LRESULT CALLBACK WndProc( HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam )
{
	switch (message)
	{
	case WM_DESTROY:
		PostQuitMessage(0);
		break;
	case WM_KEYDOWN:
	case WM_KEYUP:
		{
			
		}
		break;
#if 0
	case WM_LBUTTONDOWN:
	case WM_LBUTTONUP:
		{
			g_mousePressed = message == WM_LBUTTONDOWN;
			g_mousePosX = GET_X_LPARAM( lParam ); 
			g_mousePosY = GET_Y_LPARAM( lParam );

			if ( g_mousePressed )
			{				
				SetCapture( hWnd ) ;
				ShowCursor( false );
			}
			else
			{
				ShowCursor( true );
				ReleaseCapture();				
			}
		}
		break;
	case WM_RBUTTONDOWN:
		{
			g_mousePickPosX = GET_X_LPARAM( lParam ); 
			g_mousePickPosY = GET_Y_LPARAM( lParam );
			g_mousePicked = true;
			V6_MSG( "Pick %d, %d\n", g_mousePickPosX, g_mousePickPosY );
		}
		break;
	case WM_MOUSEMOVE:
		{
			if ( g_mousePressed )
			{
				g_mousePosX = GET_X_LPARAM( lParam ); 
				g_mousePosY = GET_Y_LPARAM( lParam );
			}
		}
		break;
#endif
	case WM_INPUT: 
	{		
		UINT dwSize;
		GetRawInputData( (HRAWINPUT)lParam, RID_INPUT, nullptr, &dwSize, sizeof(RAWINPUTHEADER));
		
		LPBYTE lpb[4096];
		V6_ASSERT( dwSize <= sizeof( lpb ) );
		GetRawInputData((HRAWINPUT)lParam, RID_INPUT, lpb, &dwSize, sizeof(RAWINPUTHEADER) );

		RAWINPUT* raw = (RAWINPUT*)lpb;

		if ( raw->header.dwType == RIM_TYPEKEYBOARD ) 
		{
			const bool pressed = raw->data.keyboard.Message == 0x100;

			if ( g_cameraKey )
			{
				if ( pressed )
				{
					switch( raw->data.keyboard.VKey )
					{
					case 'C':
						break;
					case 'P':
						g_playCameraPath = g_playCameraPath ? false : s_cameraPath.keyCount > 0;
						g_playCameraPathTime = 0.0f;
						g_playCameraPathFrame = 0;
						V6_MSG( "Camera path: %s\n", g_playCameraPath ? "play" : "stop" );
						break;
					case 'R':
						if ( s_cameraPath.keyCount > 0 )
						{
							g_playCameraPath = true;
							g_playCameraPathTime = 0.0f;
							g_playCameraPathFrame = 0;
							g_drawMode = DRAW_MODE_BLOCK;
							g_sample = 0;
							V6_MSG( "Camera path: record\n" );
						}
						break;
					case 'S':
						g_showCameraPath = !g_showCameraPath;
						V6_MSG( "Camera path: %s\n", g_showCameraPath ? "show" : "hide" );
						break;
					case 0x21:
						CameraPath_SelectKey( &s_cameraPath, s_cameraPath.activeKey + 1 );
						V6_MSG( "Camera path: key #%03u/#%03u\n", s_cameraPath.activeKey, s_cameraPath.keyCount );
						break;
					case 0x22:
						CameraPath_SelectKey( &s_cameraPath, s_cameraPath.activeKey - 1 );
						V6_MSG( "Camera path: key #%03u/#%03u\n", s_cameraPath.activeKey, s_cameraPath.keyCount );
						break;
					case 0x23:
						CameraPath_SelectKey( &s_cameraPath, s_cameraPath.keyCount-1 );
						V6_MSG( "Camera path: key #%03u/#%03u\n", s_cameraPath.activeKey, s_cameraPath.keyCount );
						break;
					case 0x24:
						CameraPath_SelectKey( &s_cameraPath, 0 );
						V6_MSG( "Camera path: key #%03u/#%03u\n", s_cameraPath.activeKey, s_cameraPath.keyCount );
						break;
					case 0x6A:
						CameraPath_Save( &s_cameraPath, &s_activeScene->info );
						V6_MSG( "Camera path: saved.\n" );
						break;
					case 0x6B:
						CameraPath_InsertKey( &s_cameraPath, &s_headOffset, s_cameraPath.activeKey );
						V6_MSG( "Camera path: key #%03u/#%03u\n", s_cameraPath.activeKey, s_cameraPath.keyCount );
						break;
					case 0x6D:
						if ( s_cameraPath.keyCount )
						{
							CameraPath_DeleteKey( &s_cameraPath, s_cameraPath.activeKey );
							V6_MSG( "Camera path: key #%03u/#%03u\n", s_cameraPath.activeKey, s_cameraPath.keyCount );
						}
						break;
#if 0
					case 0x6F:
						CameraPath_Release( &s_cameraPath );
						CameraPath_Init( &s_cameraPath );
						V6_MSG( "Camera path: key #%03u/#%03u\n", s_cameraPath.activeKey, s_cameraPath.keyCount );
						break;
#endif
					default:
						V6_MSG( "Unknow camera path key: %04x\n", raw->data.keyboard.VKey );
					}
				}
				else if ( raw->data.keyboard.VKey == 'C' )
				{
					g_cameraKey = false;
				}
			}
			else
			{
				switch( raw->data.keyboard.VKey )
				{			
				case 0x1B:
					DestroyWindow( hWnd );
					break;
				case 'A': g_keyLeftPressed = pressed; break;
				case 'B': g_drawMode = pressed ? (g_drawMode == DRAW_MODE_BLOCK ? DRAW_MODE_DEFAULT : DRAW_MODE_BLOCK) : g_drawMode; break;
				case 'C': g_cameraKey = pressed; break;
				case 'D': g_keyRightPressed = pressed; break;
				case 'E': g_useMSAA = pressed ? !g_useMSAA : g_useMSAA; break;
				case 'G': if ( pressed ) { g_debugBlocks = true; } break;
				case 'H': g_transparentDebug = pressed ? !g_transparentDebug: g_transparentDebug; break;
				case 'I': if ( pressed ) { s_logReadBack = true; } break;
				case 'L': g_limit = pressed ? !g_limit : g_limit; break;
				case 'M': g_showMip = pressed ? !g_showMip : g_showMip; break;
				case 'O': g_showOverdraw = pressed ? !g_showOverdraw : g_showOverdraw; break;
				case 'P': g_pixelMode = pressed ? ((g_pixelMode+1)%6) : g_pixelMode; break;
				case 'Q': if ( pressed ) { g_traceGrid = true; } break;
				case 'R': if ( pressed ) { g_sample = 0; } break;
				case 'S': g_keyDownPressed = pressed; break;
				case 'U': if ( pressed ) { s_activeScene->info.dirty = false; }; break;
				case 'W': g_keyUpPressed = pressed; break;
				case 'X': g_randomBackground = pressed ? !g_randomBackground : g_randomBackground; break;
				case 'Z': if ( pressed ) { g_reloadShaders = true; }; break;
				case ' ':
					if ( pressed ) 
					{
						s_yaw = core::DegToRad( s_activeScene->info.cameraYaw );
						s_pitch = 0.0f;
					}
					break;
				case '0':
				case '1':
				case '2':
				case '3':
				case '4':
				case '5':
				case '6':
				case '7':
				case '8':
				case '9':
				{
					if ( pressed ) 
					{
#if 0
						s_yaw = core::DegToRad( -90.0f );
						s_pitch = 0.0f;
						s_headOffset = core::Vec3_Make( 0.0f, 500.0f, 0.0f );
#else
						const int key = core::Min( raw->data.keyboard.VKey - '0', s_cameraPath.keyCount-1 );
						if ( s_cameraPath.keyCount == 0 )
							s_headOffset = core::Vec3_Zero();
						else
							s_headOffset = s_cameraPath.positions[key];
#endif
					}
					break;
				}
				case 109:
					if ( pressed ) 
					{
						g_translation_speed *= 0.5f;
						V6_MSG( "Translation speed: %g\n", g_translation_speed );
					}
					break;
				case 107:
					if ( pressed ) 
					{
						g_translation_speed *= 2.0f;
						V6_MSG( "Translation speed: %g\n", g_translation_speed );
					}
					break;
				}
			}
#if 0
			V6_MSG( "Kbd: make=%04x Flags:%04x Reserved:%04x ExtraInformation:%08x, msg=%04x VK=%04x\n",
				raw->data.keyboard.MakeCode, 
				raw->data.keyboard.Flags, 
				raw->data.keyboard.Reserved, 
				raw->data.keyboard.ExtraInformation, 
				raw->data.keyboard.Message, 
				raw->data.keyboard.VKey );
#endif
		}
		else if ( raw->header.dwType == RIM_TYPEMOUSE ) 
		{
			static POINT cursorPos = {};
			if ( raw->data.mouse.ulButtons & 1 )
			{				
				GetCursorPos( &cursorPos );
				SetCapture( hWnd ) ;
				ShowCursor( false );
				g_mousePressed = true;
			}
			
			if ( raw->data.mouse.ulButtons & 2 )
			{
				SetCursorPos( cursorPos.x, cursorPos.y );
				ShowCursor( true );
				ReleaseCapture();
				g_mousePressed = false;
			}

			if ( g_mousePressed )
			{
				g_mouseDeltaX += raw->data.mouse.lLastX; 
				g_mouseDeltaY += raw->data.mouse.lLastY;
			}

#if 0
			V6_MSG( "Mouse: usFlags=%04x ulButtons=%04x usButtonFlags=%04x usButtonData=%04x ulRawButtons=%04x lLastX=%04x lLastY=%04x ulExtraInformation=%04x\n",
				raw->data.mouse.usFlags, 
				raw->data.mouse.ulButtons, 
				raw->data.mouse.usButtonFlags, 
				raw->data.mouse.usButtonData, 
				raw->data.mouse.ulRawButtons, 
				raw->data.mouse.lLastX, 
				raw->data.mouse.lLastY, 
				raw->data.mouse.ulExtraInformation );
#endif
		} 
		break;
	} 
	default:
		return DefWindowProcA(hWnd, message, wParam, lParam);
		break;
	}

	return 0;
}

static bool CaptureInputs( HWND hWnd )
{
	RAWINPUTDEVICE Rid[2];

	Rid[0].usUsagePage = 0x01; 
	Rid[0].usUsage = 0x02; 
	Rid[0].dwFlags = RIDEV_NOLEGACY;   // adds HID mouse and also ignores legacy mouse messages
	Rid[0].hwndTarget = hWnd;

	Rid[1].usUsagePage = 0x01; 
	Rid[1].usUsage = 0x06; 
	Rid[1].dwFlags = RIDEV_NOLEGACY;   // adds HID keyboard and also ignores legacy keyboard messages
	Rid[1].hwndTarget = hWnd;

	if ( RegisterRawInputDevices( Rid, 2, sizeof( Rid[0] ) ) == FALSE )
	{
		V6_ERROR( "Call to RegisterRawInputDevices failed!\n" );
		return false;
	}

	SetCursor( LoadCursor(NULL, IDC_ARROW) );

	return true;
}

static HWND CreateMainWindow( const char * sTitle, int nWidth, int nHeight )
{
	WNDCLASSEXA wcex;

	wcex.cbSize = sizeof(WNDCLASSEX);
	wcex.style = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc = WndProc;
	wcex.cbClsExtra = 0;
	wcex.cbWndExtra = 0;
	wcex.hInstance = NULL;
	wcex.hIcon = NULL;
	wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
	wcex.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
	wcex.lpszMenuName = NULL;
	wcex.lpszClassName = "v6";
	wcex.hIconSm = NULL;

	if (!RegisterClassExA(&wcex))
	{
		V6_ERROR( "Call to RegisterClassEx failed!\n" );
		return 0;
	}

	const int style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU| WS_MINIMIZEBOX;
		
	RECT rect = { 0, 0, nWidth, nHeight };
	AdjustWindowRect( &rect, style, false );

	HWND hWnd = CreateWindowA(
		"v6",
		sTitle,
		style,
		CW_USEDEFAULT, CW_USEDEFAULT,
		rect.right - rect.left, rect.bottom - rect.top,
		NULL,
		NULL,
		NULL,
		NULL
		);

	return hWnd;
}

static const bool IsBakingMode( DrawMode_e drawMode )
{
	return drawMode == DRAW_MODE_BLOCK && g_sample < SAMPLE_MAX_COUNT;
}

static const char* ModeToString( DrawMode_e drawMode )
{
	switch ( drawMode )
	{
		case DRAW_MODE_DEFAULT: return "default";
		case DRAW_MODE_BLOCK: 
		{
			if ( g_sample < SAMPLE_MAX_COUNT )
			{
				static char buffer[256];
				sprintf_s( buffer, sizeof( buffer ), "baking #%d", g_sample );
				return buffer;
			}
			return "block";
		}
	}
	return "unknown";
}

static const char* FormatInteger_Unsafe( core::u32 n )
{
	static char buffer[10+3+1];
	char* s = buffer;
	if ( n > 1000000000 )
	{
		const core::u32 billion = n / 1000000000;
		s += sprintf_s( s, sizeof( buffer ) + buffer - s, "%d,", billion );
		n -= billion * 1000000000;
	}
	if ( n > 1000000 )
	{
		const core::u32 million = n / 1000000;
		if ( s == buffer )
			s += sprintf_s( s, sizeof( buffer ) + buffer - s, "%d,", million );
		else
			s += sprintf_s( s, sizeof( buffer ) + buffer - s, "%03d,", million );
		n -= million * 1000000;
	}
	if ( n > 1000 )
	{
		const core::u32 thousand = n / 1000;
		if ( s == buffer )
			s += sprintf_s( s, sizeof( buffer ) + buffer - s, "%d,", thousand );
		else
			s += sprintf_s( s, sizeof( buffer ) + buffer - s, "%03d,", thousand );
		n -= thousand * 1000;
	}

	if ( s == buffer )
		s += sprintf_s( s, sizeof( buffer ) + buffer - s, "%d", n );
	else
		s += sprintf_s( s, sizeof( buffer ) + buffer - s, "%03d", n );

	*s = 0;

	return buffer;
}

static void RenderingView_MakeForStereo( RenderingView_s* renderingView, const core::Vec3* org, const core::Vec3* forward, const core::Vec3* up, const core::Vec3* right, const core::u32 eye, float aspectRatio )
{
	const core::Vec3 eyeOffset = *right * 0.5f * IPD;
	renderingView->texture2D = nullptr;
	renderingView->rtv = nullptr;
	renderingView->uav = nullptr;
	renderingView->org = *org + (eye == 0 ? -eyeOffset : eyeOffset);
	renderingView->forward = *forward;
	renderingView->right = *right;
	renderingView->up = *up;
	renderingView->viewMatrix = core::Mat4x4_View( &renderingView->org, forward, up, right );
	renderingView->projMatrix = core::Mat4x4_Projection( ZNEAR, ZFAR, FOV, aspectRatio );
	renderingView->eye = eye;
	renderingView->tanHalfFOVLeft = core::Tan( FOV * 0.5f );
	renderingView->tanHalfFOVRight = renderingView->tanHalfFOVLeft;
	renderingView->tanHalfFOVUp = renderingView->tanHalfFOVLeft;
	renderingView->tanHalfFOVDown = renderingView->tanHalfFOVLeft;
}

#if V6_USE_HMD

static void RenderingView_MakeForHMD( RenderingView_s* renderingView, const HmdEyePose_s* eyePose, const core::Vec3* orgOffset, float yawOffset, core::u32 eye )
{
	renderingView->texture2D = nullptr;
	renderingView->rtv = nullptr;
	renderingView->uav = nullptr;
	const core::Mat4x4 yawMatrix = core::Mat4x4_RotationY( s_yaw );
	core::Mat4x4_Mul( &renderingView->viewMatrix, yawMatrix, eyePose->lookAt );
	renderingView->org = *orgOffset + renderingView->viewMatrix.GetTranslation();
	core::Mat4x4_SetTranslation( &renderingView->viewMatrix, renderingView->org );
	core::Mat4x4_AffineInverse( &renderingView->viewMatrix );
	renderingView->forward = -renderingView->viewMatrix.GetZAxis()->Normalized();
	renderingView->right = renderingView->viewMatrix.GetXAxis()->Normalized();
	renderingView->up = renderingView->viewMatrix.GetYAxis()->Normalized();
	renderingView->projMatrix = eyePose->projection;
	renderingView->eye = eye;
	renderingView->tanHalfFOVLeft = eyePose->tanHalfFOVLeft;
	renderingView->tanHalfFOVRight = eyePose->tanHalfFOVRight;
	renderingView->tanHalfFOVUp = eyePose->tanHalfFOVUp;
	renderingView->tanHalfFOVDown = eyePose->tanHalfFOVDown;
}

#endif // #if V6_USE_HMD

static void Cube_GetLookAt( core::Vec3& lookAt, core::Vec3& up, CubeAxis_e axis )
{
	switch ( axis )
    {
        case CUBE_AXIS_POSITIVE_X:            
			lookAt  = core::Vec3_Make( 1.0f,  0.0f,  0.0f );
            up		= core::Vec3_Make( 0.0f,  1.0f,  0.0f );
            break;								 	 	    
        case CUBE_AXIS_NEGATIVE_X:				 	 	    
            lookAt	= core::Vec3_Make( -1.0f , 0.0f, 0.0f );
            up		= core::Vec3_Make(  0.0f , 1.0f, 0.0f );
            break;								 	 	    
        case CUBE_AXIS_POSITIVE_Y:				 	 	    
            lookAt	= core::Vec3_Make( 0.0f,  1.0f,  0.0f );
            up		= core::Vec3_Make( 0.0f,  0.0f, -1.0f );
            break;						 		 	 	    
        case CUBE_AXIS_NEGATIVE_Y:		 		 	 	    
            lookAt	= core::Vec3_Make( 0.0f, -1.0f,  0.0f );
            up		= core::Vec3_Make( 0.0f,  0.0f,  1.0f );
            break;						 		 	 	    
        case CUBE_AXIS_POSITIVE_Z:		 		 	 	    
            lookAt	= core::Vec3_Make( 0.0f,  0.0f,  1.0f );
            up		= core::Vec3_Make( 0.0f,  1.0f,  0.0f );
            break;						 		 	 	    
        case CUBE_AXIS_NEGATIVE_Z:		 		 	 	    
            lookAt	= core::Vec3_Make( 0.0f,  0.0f, -1.0f );
            up		= core::Vec3_Make( 0.0f,  1.0f,  0.0f );
            break;
		default:
			V6_ASSERT_NOT_SUPPORTED();
    }
}

static void Cube_MakeViewMatrix( core::Mat4x4* matrix, const core::Vec3& center, CubeAxis_e axis )
{
	core::Vec3 lookAt;
	core::Vec3 up;
	Cube_GetLookAt( lookAt, up, axis );
	
	*matrix = Mat4x4_Rotation( lookAt, up );
	Mat4x4_SetTranslation( matrix, center );
	Mat4x4_AffineInverse( matrix );
}

static void GPUResource_LogMemory( const char* res, core::u32 size, const char* name )
{
	if ( MB( size ) >= 1 )
		V6_MSG( "%-16s %-30s: %8s MB\n", res, name, FormatInteger_Unsafe( MB( size ) ) );
	core::Atomic_Add( &gpuMemory, size );
}

static void GPUResource_LogMemoryUsage()
{
	V6_MSG( "%-16s %-30s: %8s MB\n", "GPU", "total", FormatInteger_Unsafe( MB( gpuMemory ) ) );
}

static core::u32 DXGIFormat_Size( DXGI_FORMAT format )
{
	switch( format )
	{	
	case DXGI_FORMAT_R8G8_UNORM:
	case DXGI_FORMAT_R8G8_SNORM:
		return 2;
	case DXGI_FORMAT_D32_FLOAT:		
	case DXGI_FORMAT_R8G8B8A8_UNORM:
	case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:	
	case DXGI_FORMAT_R16G16_FLOAT:
	case DXGI_FORMAT_R32_FLOAT:
	case DXGI_FORMAT_R32_UINT:
	case DXGI_FORMAT_R32_TYPELESS:
		return 4;
	case DXGI_FORMAT_R32G32_FLOAT:
		return 8;
	case DXGI_FORMAT_R32G32B32A32_FLOAT:
		return 16;
	default:
		V6_ASSERT_NOT_SUPPORTED();
		return 0;
	}
}

static bool Compute_Create( ID3D11Device* device, GPUCompute_s* compute, const char* cs, core::CFileSystem* fileSystem, core::IStack* stack )
{
	core::ScopedStack scopedStack( stack );

	void* csBytecode = nullptr;
	const int csBytecodeSize = fileSystem->ReadFile( cs, &csBytecode, stack );
	if ( csBytecodeSize <= 0 )
	{
		return false;
	}	

	{
		HRESULT hRes = device->CreateComputeShader( csBytecode, csBytecodeSize, nullptr, &compute->m_computeShader );

		if ( FAILED( hRes) )
		{
			V6_ERROR( "ID3D11Device::CreateComputeShader failed!\n" );
		}
	}	

	return true;
}

static void Compute_Release( GPUCompute_s* compute )
{	
	V6_RELEASE_D3D11( compute->m_computeShader );
}

static bool Shader_Create( ID3D11Device* device, GPUShader_s* shader, const char* vs, const char* ps, core::u32 vertexFormat, core::CFileSystem* fileSystem, core::IStack* stack )
{
	core::ScopedStack scopedStack( stack );

	void* vsBytecode = nullptr;
	const int vsBytecodeSize = fileSystem->ReadFile( vs, &vsBytecode, stack );
	if ( vsBytecodeSize <= 0 )
	{
		return false;
	}	

	{
		HRESULT hRes = device->CreateVertexShader( vsBytecode, vsBytecodeSize, nullptr, &shader->m_vertexShader );

		if ( FAILED( hRes) )
		{
			V6_ERROR( "ID3D11Device::CreateVertexShader failed!\n" );
		}
	}

	void* psBytecode = nullptr;
	const int psBytecodeSize = fileSystem->ReadFile( ps, &psBytecode, stack );
	if ( psBytecodeSize <= 0 )
	{
		return false;
	}

	{
		HRESULT hRes = device->CreatePixelShader( psBytecode, psBytecodeSize, nullptr, &shader->m_pixelShader );

		if ( FAILED( hRes) )
		{
			V6_ERROR( "ID3D11Device::CreatePixelShader failed!\n" );
		}
	}
	
	{
		D3D11_INPUT_ELEMENT_DESC idesc[VERTEX_INPUT_MAX_COUNT] = {};

		int stride = 0;
		int inputCount = 0;		
		
		if ( vertexFormat & VERTEX_FORMAT_POSITION )
		{
			V6_ASSERT( inputCount < VERTEX_INPUT_MAX_COUNT );

			idesc[inputCount].SemanticName = "POSITION";
			idesc[inputCount].SemanticIndex = 0;
			idesc[inputCount].Format = DXGI_FORMAT_R32G32B32_FLOAT;
			idesc[inputCount].InputSlot = 0;
			idesc[inputCount].AlignedByteOffset = stride;
			idesc[inputCount].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
			idesc[inputCount].InstanceDataStepRate = 0;

			stride += 12;
			++inputCount;
		}

		if ( vertexFormat & VERTEX_FORMAT_COLOR )
		{
			V6_ASSERT( inputCount < VERTEX_INPUT_MAX_COUNT );

			idesc[inputCount].SemanticName = "COLOR";
			idesc[inputCount].SemanticIndex = 0;
			idesc[inputCount].Format = DXGI_FORMAT_R8G8B8A8_UNORM;
			idesc[inputCount].InputSlot = 0;
			idesc[inputCount].AlignedByteOffset = stride;
			idesc[inputCount].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
			idesc[inputCount].InstanceDataStepRate = 0;

			stride += 4;
			++inputCount;
		}

		const static DXGI_FORMAT widthToFloatFormats[] = { DXGI_FORMAT_UNKNOWN, DXGI_FORMAT_R32_FLOAT, DXGI_FORMAT_R32G32_FLOAT, DXGI_FORMAT_R32G32B32_FLOAT, DXGI_FORMAT_R32G32B32A32_FLOAT };

		if ( vertexFormat & VERTEX_FORMAT_USER0_MASK )
		{
			V6_ASSERT( inputCount < VERTEX_INPUT_MAX_COUNT );

			idesc[inputCount].SemanticName = "USER";
			idesc[inputCount].SemanticIndex = 0;
			const core::u32 width = ( vertexFormat & VERTEX_FORMAT_USER0_MASK ) >> VERTEX_FORMAT_USER0_SHIFT;
			V6_ASSERT( width >= 1 && width <= 4 );
			idesc[inputCount].Format = widthToFloatFormats[width];
			idesc[inputCount].InputSlot = 0;
			idesc[inputCount].AlignedByteOffset = stride;
			idesc[inputCount].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
			idesc[inputCount].InstanceDataStepRate = 0;

			stride += 4 * width;
			++inputCount;
		}

		if ( vertexFormat & VERTEX_FORMAT_USER1_MASK )
		{
			V6_ASSERT( inputCount < VERTEX_INPUT_MAX_COUNT );

			idesc[inputCount].SemanticName = "USER";
			idesc[inputCount].SemanticIndex = 1;
			const core::u32 width = ( vertexFormat & VERTEX_FORMAT_USER1_MASK ) >> VERTEX_FORMAT_USER1_SHIFT;
			V6_ASSERT( width >= 1 && width <= 4 );
			idesc[inputCount].Format = widthToFloatFormats[width];
			idesc[inputCount].InputSlot = 0;
			idesc[inputCount].AlignedByteOffset = stride;
			idesc[inputCount].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
			idesc[inputCount].InstanceDataStepRate = 0;

			stride += 4 * width;
			++inputCount;
		}

		if ( vertexFormat & VERTEX_FORMAT_USER2_MASK )
		{
			V6_ASSERT( inputCount < VERTEX_INPUT_MAX_COUNT );

			idesc[inputCount].SemanticName = "USER";
			idesc[inputCount].SemanticIndex = 2;
			const core::u32 width = ( vertexFormat & VERTEX_FORMAT_USER2_MASK ) >> VERTEX_FORMAT_USER2_SHIFT;
			V6_ASSERT( width >= 1 && width <= 4 );
			idesc[inputCount].Format = widthToFloatFormats[width];
			idesc[inputCount].InputSlot = 0;
			idesc[inputCount].AlignedByteOffset = stride;
			idesc[inputCount].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
			idesc[inputCount].InstanceDataStepRate = 0;

			stride += 4 * width;
			++inputCount;
		}

		if ( vertexFormat & VERTEX_FORMAT_USER3_MASK )
		{
			V6_ASSERT( inputCount < VERTEX_INPUT_MAX_COUNT );

			idesc[inputCount].SemanticName = "USER";
			idesc[inputCount].SemanticIndex = 3;
			const core::u32 width = ( vertexFormat & VERTEX_FORMAT_USER3_MASK ) >> VERTEX_FORMAT_USER3_SHIFT;
			V6_ASSERT( width >= 1 && width <= 4 );
			idesc[inputCount].Format = widthToFloatFormats[width];
			idesc[inputCount].InputSlot = 0;
			idesc[inputCount].AlignedByteOffset = stride;
			idesc[inputCount].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
			idesc[inputCount].InstanceDataStepRate = 0;

			stride += 4 * width;
			++inputCount;
		}

		HRESULT hRes = device->CreateInputLayout( idesc, inputCount, vsBytecode, vsBytecodeSize, &shader->m_inputLayout );

		if ( FAILED( hRes) )
		{
			V6_ERROR( "ID3D11Device::CreateInputLayout failed!\n" );
			return false;
		}

		shader->m_vertexFormat = vertexFormat;
	}

	return true;
}

static void Shader_Release( GPUShader_s* shader )
{
	V6_RELEASE_D3D11( shader->m_inputLayout );
	V6_RELEASE_D3D11( shader->m_vertexShader );
	V6_RELEASE_D3D11( shader->m_pixelShader );
}

static void ReadBack_Log( const char* res, core::u32 value, const char* name )
{
	V6_MSG( "%-16s %-30s: %10d\n", res, name, value );
}

static void ReadBack_Log( const char* res, core::hex32 value, const char* name )
{
	V6_MSG( "%-16s %-30s: 0x%08X\n", res, name, value.n );
}

static void ReadBack_Log( const char* res, float value, const char* name )
{
	V6_MSG( "%-16s %-30s: %g\n", res, name, value );
}

static void ReadBack_Log( const char* res, core::Vec2 value, const char* name )
{
	V6_MSG( "%-16s %-30s: (%g, %g)\n", res, name, value.x, value.y );
}

static void ReadBack_Log( const char* res, core::Vec3 value, const char* name )
{
	V6_MSG( "%-16s %-30s: (%g, %g, %g)\n", res, name, value.x, value.y, value.z );
}

static void ReadBack_Log( const char* res, core::Vec4 value, const char* name )
{
	V6_MSG( "%-16s %-30s: (%g, %g, %g, %g)\n", res, name, value.x, value.y, value.z, value.w );
}

static void ReadBack_Log( const char* res, core::Vec2u value, const char* name )
{
	V6_MSG( "%-16s %-30s: (%4u, %4u)\n", res, name, value.x, value.y );
}

static void ReadBack_Log( const char* res, core::Vec3u value, const char* name )
{
	V6_MSG( "%-16s %-30s: (%4u, %4u, %4u)\n", res, name, value.x, value.y, value.z );
}

static void ReadBack_Log( const char* res, core::Vec2i value, const char* name )
{
	V6_MSG( "%-16s %-30s: (%4d, %4d)\n", res, name, value.x, value.y );
}

static void ReadBack_Log( const char* res, core::Vec3i value, const char* name )
{
	V6_MSG( "%-16s %-30s: (%4d, %4d, %4d)\n", res, name, value.x, value.y, value.z );
}

static void ReadBack_Log( const char* res, core::Mat4x4 value, const char* name )
{
	V6_MSG( "%-16s %-30s:\n", res, name );
	V6_MSG(	"[%g, %g, %g, %g]\n", value.m_row0.x, value.m_row0.y, value.m_row0.z, value.m_row0.w );
	V6_MSG(	"[%g, %g, %g, %g]\n", value.m_row1.x, value.m_row1.y, value.m_row1.z, value.m_row1.w );	
	V6_MSG(	"[%g, %g, %g, %g]\n", value.m_row2.x, value.m_row2.y, value.m_row2.z, value.m_row2.w );
	V6_MSG(	"[%g, %g, %g, %g]\n", value.m_row3.x, value.m_row3.y, value.m_row3.z, value.m_row3.w );
}

static void GPUBuffer_CreateIndirectArgs( ID3D11Device* device, GPUBuffer_s* buffer, core::u32 count, core::u32 flags, const char* name )
{
	memset( buffer, 0, sizeof( *buffer ) );

	{
		D3D11_BUFFER_DESC bufferDesc = {};
		bufferDesc.ByteWidth = count * sizeof( core::u32 );
		bufferDesc.Usage = D3D11_USAGE_DEFAULT;
		bufferDesc.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;
		bufferDesc.CPUAccessFlags = 0;
		bufferDesc.MiscFlags = D3D11_RESOURCE_MISC_DRAWINDIRECT_ARGS;
		bufferDesc.StructureByteStride = 0;
		
		V6_ASSERT_D3D11( device->CreateBuffer( &bufferDesc, nullptr, &buffer->buf ) );
		GPUResource_LogMemory( "GPUBuffer", bufferDesc.ByteWidth, name );
	}

	if ( (flags & GPUBUFFER_CREATION_FLAG_READ_BACK) != 0 )
	{
		D3D11_BUFFER_DESC bufferDesc = {};
		bufferDesc.ByteWidth = count * sizeof( core::u32 );
		bufferDesc.Usage = D3D11_USAGE_STAGING;
		bufferDesc.BindFlags = 0;
		bufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
		bufferDesc.MiscFlags = 0;
		bufferDesc.StructureByteStride = 0;
		
		V6_ASSERT_D3D11( device->CreateBuffer( &bufferDesc, nullptr, &buffer->staging ) );
		GPUResource_LogMemory( "GPUBuffer", bufferDesc.ByteWidth, name );
	}

	{
		D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Format = DXGI_FORMAT_R32_UINT;
		srvDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
		srvDesc.Buffer.FirstElement = 0;
        srvDesc.Buffer.NumElements = count;

		V6_ASSERT_D3D11( device->CreateShaderResourceView( buffer->buf, &srvDesc, &buffer->srv ) );
	}

	{
		D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
		uavDesc.Format = DXGI_FORMAT_R32_UINT;
		uavDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
		uavDesc.Buffer.FirstElement = 0;
		uavDesc.Buffer.NumElements = count;
		uavDesc.Buffer.Flags = 0;

		V6_ASSERT_D3D11( device->CreateUnorderedAccessView( buffer->buf, &uavDesc, &buffer->uav ) );
	}
}

static void GPUBuffer_CreateIndirectArgsWithStaticData( ID3D11Device* device, GPUBuffer_s* buffer, const void* data, core::u32 count, core::u32 flags, const char* name )
{
	memset( buffer, 0, sizeof( *buffer ) );

	{
		D3D11_BUFFER_DESC bufferDesc = {};
		bufferDesc.ByteWidth = count * sizeof( core::u32 );
		bufferDesc.Usage = D3D11_USAGE_IMMUTABLE;
		bufferDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		bufferDesc.CPUAccessFlags = 0;
		bufferDesc.MiscFlags = D3D11_RESOURCE_MISC_DRAWINDIRECT_ARGS;
		bufferDesc.StructureByteStride = 0;

		D3D11_SUBRESOURCE_DATA dataDesc = {};
		dataDesc.pSysMem = data;
		dataDesc.SysMemPitch = 0;
		dataDesc.SysMemSlicePitch = 0;

		V6_ASSERT_D3D11( device->CreateBuffer( &bufferDesc, &dataDesc, &buffer->buf ) );
		GPUResource_LogMemory( "GPUBuffer", bufferDesc.ByteWidth, name );
	}

	V6_ASSERT( (flags & GPUBUFFER_CREATION_FLAG_READ_BACK) == 0 );

	{
		D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Format = DXGI_FORMAT_R32_UINT;
		srvDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
		srvDesc.Buffer.FirstElement = 0;
		srvDesc.Buffer.NumElements = count;

		V6_ASSERT_D3D11( device->CreateShaderResourceView( buffer->buf, &srvDesc, &buffer->srv ) );
	}
}

static void GPUBuffer_CreateTyped( ID3D11Device* device, GPUBuffer_s* buffer, DXGI_FORMAT format, core::u32 count, core::u32 flags, const char* name )
{
	memset( buffer, 0, sizeof( *buffer ) );

	{
		D3D11_BUFFER_DESC bufferDesc = {};
		bufferDesc.ByteWidth = count * DXGIFormat_Size( format );
		bufferDesc.Usage = D3D11_USAGE_DEFAULT;
		bufferDesc.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;
		bufferDesc.CPUAccessFlags = 0;
		bufferDesc.MiscFlags = 0;
		bufferDesc.StructureByteStride = 0;
		
		V6_ASSERT_D3D11( device->CreateBuffer( &bufferDesc, nullptr, &buffer->buf ) );
		GPUResource_LogMemory( "GPUBuffer", bufferDesc.ByteWidth, name );
	}

	if ( (flags & GPUBUFFER_CREATION_FLAG_READ_BACK) != 0 )
	{
		D3D11_BUFFER_DESC bufferDesc = {};
		bufferDesc.ByteWidth = count * DXGIFormat_Size( format );
		bufferDesc.Usage = D3D11_USAGE_STAGING;
		bufferDesc.BindFlags = 0;
		bufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
		bufferDesc.MiscFlags = 0;
		bufferDesc.StructureByteStride = 0;
		
		V6_ASSERT_D3D11( device->CreateBuffer( &bufferDesc, nullptr, &buffer->staging ) );
		GPUResource_LogMemory( "GPUBuffer", bufferDesc.ByteWidth, name );
	}

	{
		D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Format = format;
		srvDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
		srvDesc.Buffer.FirstElement = 0;
        srvDesc.Buffer.NumElements = count;

		V6_ASSERT_D3D11( device->CreateShaderResourceView( buffer->buf, &srvDesc, &buffer->srv ) );
	}

	{
		D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
		uavDesc.Format = format;
		uavDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
		uavDesc.Buffer.FirstElement = 0;
		uavDesc.Buffer.NumElements = count;
		uavDesc.Buffer.Flags = 0;

		V6_ASSERT_D3D11( device->CreateUnorderedAccessView( buffer->buf, &uavDesc, &buffer->uav ) );
	}
}

static void GPUBuffer_CreateTypedWithStaticData( ID3D11Device* device, GPUBuffer_s* buffer, const void* data, DXGI_FORMAT format, core::u32 count, core::u32 flags, const char* name )
{
	memset( buffer, 0, sizeof( *buffer ) );

	{
		D3D11_BUFFER_DESC bufferDesc = {};
		bufferDesc.ByteWidth = count * DXGIFormat_Size( format );
		bufferDesc.Usage = D3D11_USAGE_IMMUTABLE;
		bufferDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		bufferDesc.CPUAccessFlags = 0;
		bufferDesc.MiscFlags = 0;
		bufferDesc.StructureByteStride = 0;

		D3D11_SUBRESOURCE_DATA dataDesc = {};
		dataDesc.pSysMem = data;
		dataDesc.SysMemPitch = 0;
		dataDesc.SysMemSlicePitch = 0;

		V6_ASSERT_D3D11( device->CreateBuffer( &bufferDesc, &dataDesc, &buffer->buf ) );
		GPUResource_LogMemory( "GPUBuffer", bufferDesc.ByteWidth, name );
	}

	V6_ASSERT( (flags & GPUBUFFER_CREATION_FLAG_READ_BACK) == 0 );

	{
		D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Format = format;
		srvDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
		srvDesc.Buffer.FirstElement = 0;
		srvDesc.Buffer.NumElements = count;

		V6_ASSERT_D3D11( device->CreateShaderResourceView( buffer->buf, &srvDesc, &buffer->srv ) );
	}
}

static void GPUBuffer_CreateStructured( ID3D11Device* device, GPUBuffer_s* buffer, core::u32 elementSize, core::u32 count, core::u32 flags, const char* name )
{
	memset( buffer, 0, sizeof( *buffer ) );

	{
		D3D11_BUFFER_DESC bufferDesc = {};
		bufferDesc.ByteWidth = count * elementSize;
		bufferDesc.Usage = D3D11_USAGE_DEFAULT;
		bufferDesc.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;
		bufferDesc.CPUAccessFlags = 0;
		bufferDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
		bufferDesc.StructureByteStride = elementSize;
		
		V6_ASSERT_D3D11( device->CreateBuffer( &bufferDesc, nullptr, &buffer->buf ) );
		GPUResource_LogMemory( "GPUBuffer", bufferDesc.ByteWidth, name );
	}
	
	if ( (flags & GPUBUFFER_CREATION_FLAG_READ_BACK) != 0 )
	{
		D3D11_BUFFER_DESC bufferDesc = {};
		bufferDesc.ByteWidth = count * elementSize;
		bufferDesc.Usage = D3D11_USAGE_STAGING;
		bufferDesc.BindFlags = 0;
		bufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
		bufferDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
		bufferDesc.StructureByteStride = elementSize;
		
		V6_ASSERT_D3D11( device->CreateBuffer( &bufferDesc, nullptr, &buffer->staging ) );
		GPUResource_LogMemory( "GPUBuffer", bufferDesc.ByteWidth, name );
	}

	{
		D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Format = DXGI_FORMAT_UNKNOWN;
		srvDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
		srvDesc.Buffer.FirstElement = 0;
        srvDesc.Buffer.NumElements = count;

		V6_ASSERT_D3D11( device->CreateShaderResourceView( buffer->buf, &srvDesc, &buffer->srv ) );
	}

	{
		D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
		uavDesc.Format = DXGI_FORMAT_UNKNOWN;
		uavDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
		uavDesc.Buffer.FirstElement = 0;
		uavDesc.Buffer.NumElements = count;
		uavDesc.Buffer.Flags = 0;

		V6_ASSERT_D3D11( device->CreateUnorderedAccessView( buffer->buf, &uavDesc, &buffer->uav ) );
	}
}

static void GPUBuffer_Release( ID3D11Device* device, GPUBuffer_s* buffer )
{
	V6_RELEASE_D3D11( buffer->buf );
	if ( buffer->staging )
		V6_RELEASE_D3D11( buffer->staging );
	V6_RELEASE_D3D11( buffer->srv );
	if ( buffer->uav )
		V6_RELEASE_D3D11( buffer->uav );
}

template < typename T >
static const T* GPUBUffer_MapReadBack( ID3D11DeviceContext* context, GPUBuffer_s* buffer )
{
	context->CopyResource( buffer->staging, buffer->buf );

	D3D11_MAPPED_SUBRESOURCE res;
	V6_ASSERT_D3D11( context->Map( buffer->staging, 0, D3D11_MAP_READ, 0, &res ) );
	return (T*)res.pData;
}

static void GPUBUffer_UnmapReadBack( ID3D11DeviceContext* context, GPUBuffer_s* buffer )
{
	context->Unmap( buffer->staging, 0 );
}

static void Texture2D_Create( ID3D11Device* device, GPUTexture2D_s* tex, core::u32 width, core::u32 height, core::Color_s* pixels, bool mipmap, const char* name )
{
	mipmap = mipmap && core::IsPowOfTwo( width ) && core::IsPowOfTwo( height );

	{
		D3D11_TEXTURE2D_DESC texDesc = {};
		texDesc.Width = width;
		texDesc.Height = height;
		texDesc.MipLevels = mipmap ? 0 : 1;
		texDesc.ArraySize = 1;
		texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
		texDesc.SampleDesc.Count = 1;
		texDesc.SampleDesc.Quality = 0;
		texDesc.Usage = D3D11_USAGE_DEFAULT;
		texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		if ( mipmap )
			texDesc.BindFlags |= D3D11_BIND_RENDER_TARGET;
		texDesc.CPUAccessFlags = 0;
		texDesc.MiscFlags = 0;
		if ( mipmap )
			texDesc.MiscFlags |= D3D11_RESOURCE_MISC_GENERATE_MIPS;

		D3D11_SUBRESOURCE_DATA data[16] = {};
		void* mipPixels = nullptr;
		
		core::u32 pixelCount = 0;
		for ( core::u32 mip = 0; mip < 16; ++mip )
		{
			data[mip].pSysMem = pixels;
			data[mip].SysMemPitch = width * DXGIFormat_Size( texDesc.Format );
			data[mip].SysMemSlicePitch = width * height * DXGIFormat_Size( texDesc.Format );

			pixelCount += width * height;

			if ( !mipmap || (width == 1 && height == 1))
				break;

			if ( width > 1 )
				width >>= 1;

			if ( height > 1 )
				height >>= 1;
		}

		if ( pixelCount > width * height )
		
		V6_ASSERT( !mipmap || width == 1 );
		V6_ASSERT( !mipmap || height == 1 );
		V6_ASSERT_D3D11( device->CreateTexture2D( &texDesc, data, &tex->tex ) );

		GPUResource_LogMemory( "Texture2D", pixelCount * texDesc.ArraySize * DXGIFormat_Size( texDesc.Format ) * texDesc.SampleDesc.Count, name );
	}

	{
		D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
		srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MipLevels = -1;
		srvDesc.Texture2D.MostDetailedMip = 0;

		V6_ASSERT_D3D11( device->CreateShaderResourceView( tex->tex, &srvDesc, &tex->srv ) );
	}
	
	tex->uav = nullptr;
	tex->mipmapState = mipmap ? GPUTEXTURE_MIPMAP_STATE_REQUIRED : GPUTEXTURE_MIPMAP_STATE_NONE;
}

static void Texture2D_CreateRW( ID3D11Device* device, GPUTexture2D_s* tex, core::u32 width, core::u32 height, const char* name )
{
	{
		D3D11_TEXTURE2D_DESC texDesc = {};
		texDesc.Width = width;
		texDesc.Height = height;
		texDesc.MipLevels = 1;
		texDesc.ArraySize = 1;
		texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		texDesc.SampleDesc.Count = 1;
		texDesc.SampleDesc.Quality = 0;
		texDesc.Usage = D3D11_USAGE_DEFAULT;
		texDesc.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;
		texDesc.CPUAccessFlags = 0;
		texDesc.MiscFlags = 0;
		
		V6_ASSERT_D3D11( device->CreateTexture2D( &texDesc, nullptr, &tex->tex ) );
		GPUResource_LogMemory( "Texture2D", texDesc.Width * texDesc.Height * texDesc.ArraySize * DXGIFormat_Size( texDesc.Format ) * texDesc.SampleDesc.Count, name );
	}

	{
		D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;		
		srvDesc.Texture2D.MipLevels = 1;
		srvDesc.Texture2D.MostDetailedMip = 0;

		V6_ASSERT_D3D11( device->CreateShaderResourceView( tex->tex, &srvDesc, &tex->srv ) );
	}

	{
		D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
		uavDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		uavDesc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;		
		uavDesc.Texture2D.MipSlice = 0;

		V6_ASSERT_D3D11( device->CreateUnorderedAccessView( tex->tex, &uavDesc, &tex->uav ) );
	}

	tex->mipmapState = GPUTEXTURE_MIPMAP_STATE_NONE;
}

static void GPUTexture_Release( GPUTexture2D_s* tex )
{
	V6_RELEASE_D3D11( tex->tex );
	V6_RELEASE_D3D11( tex->srv );
	if ( tex->uav )
		V6_RELEASE_D3D11( tex->uav );
}

static void ConstantBuffer_Create( ID3D11Device* device, GPUConstantBuffer_s* buffer, core::u32 sizeOfStruct, const char* name )
{
	D3D11_BUFFER_DESC bufDesc = {};	
	bufDesc.Usage = D3D11_USAGE_DYNAMIC;
	bufDesc.ByteWidth = sizeOfStruct;
	bufDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	bufDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	bufDesc.MiscFlags = 0;
	bufDesc.StructureByteStride = 0;

	V6_ASSERT_D3D11( device->CreateBuffer( &bufDesc, nullptr, &buffer->buf ) );
	GPUResource_LogMemory( "ConstantBuffer", bufDesc.ByteWidth, name );
}

static void ConstantBuffer_Release( GPUConstantBuffer_s* buffer )
{
	V6_RELEASE_D3D11( buffer->buf );
}

template < typename T >
static T* ConstantBuffer_MapWrite( ID3D11DeviceContext* context, GPUConstantBuffer_s* buffer )
{
	D3D11_MAPPED_SUBRESOURCE res;
	V6_ASSERT_D3D11( context->Map( buffer->buf, 0, D3D11_MAP_WRITE_DISCARD, 0, &res ) );
	return (T*)res.pData;
}

static void ConstantBuffer_UnmapWrite( ID3D11DeviceContext* context, GPUConstantBuffer_s* buffer )
{
	context->Unmap( buffer->buf, 0 );
}

static void GPUQuery_CreateTimeStamp( ID3D11Device* device, GPUQuery_s* query )
{
	query->data = 0;

#if V6_GPU_PROFILING == 1
	D3D11_QUERY_DESC queryDesc = {};
	queryDesc.Query = D3D11_QUERY_TIMESTAMP;
    queryDesc.MiscFlags = 0;
	V6_ASSERT_D3D11( device->CreateQuery( &queryDesc, &query->query ) );
#endif
}

static void GPUQuery_CreateTimeStampDisjoint( ID3D11Device* device, GPUQuery_s* query )
{
	query->data = 0;

#if V6_GPU_PROFILING == 1
	D3D11_QUERY_DESC queryDesc = {};
	queryDesc.Query = D3D11_QUERY_TIMESTAMP_DISJOINT;
    queryDesc.MiscFlags = 0;
	V6_ASSERT_D3D11( device->CreateQuery( &queryDesc, &query->query ) );
#endif
}

static void GPUQuery_BeginTimeStampDisjoint( ID3D11DeviceContext* context, GPUQuery_s* query )
{
	V6_ASSERT( query->data != (core::u64)-1 );
	query->data = (core::u64)-1;
#if V6_GPU_PROFILING == 1
	context->Begin( query->query );
#endif
}

static void GPUQuery_EndTimeStampDisjoint( ID3D11DeviceContext* context, GPUQuery_s* query )
{
	V6_ASSERT( query->data == (core::u64)-1 );
	query->data = 0;
#if V6_GPU_PROFILING == 1
	context->End( query->query );
#endif
}

static bool GPUQuery_ReadTimeStampDisjoint( ID3D11DeviceContext* context, GPUQuery_s* query )
{
	V6_ASSERT( query->data != (core::u64)-1 );
#if V6_GPU_PROFILING == 1
	D3D11_QUERY_DATA_TIMESTAMP_DISJOINT timestampDisjoint = {};
	if ( context->GetData( query->query, &timestampDisjoint, sizeof( timestampDisjoint ), D3D11_ASYNC_GETDATA_DONOTFLUSH ) != S_OK )
		return false;
	if ( timestampDisjoint.Disjoint || timestampDisjoint.Frequency == 0 )
		return false;
	query->data = timestampDisjoint.Frequency;
	return true;
#else
	query->data = 0;
	return false;
#endif
	
}

static void GPUQuery_WriteTimeStamp( ID3D11DeviceContext* context, GPUQuery_s* query )
{
	query->data = 0;
#if V6_GPU_PROFILING == 1
	context->End( query->query );
#endif
}

static bool GPUQuery_ReadTimeStamp( ID3D11DeviceContext* context, GPUQuery_s* query )
{
#if V6_GPU_PROFILING == 1
	return context->GetData( query->query, &query->data, sizeof( query->data ), D3D11_ASYNC_GETDATA_DONOTFLUSH ) == S_OK;
#else
	return false;
#endif
}

static float GPUQuery_GetElpasedTime( const GPUQuery_s* queryStart, const GPUQuery_s* queryEnd, const GPUQuery_s* queryDisjoint )
{
	V6_ASSERT( queryStart->data != (core::u64)-1 );
	V6_ASSERT( queryEnd->data != (core::u64)-1 );
	V6_ASSERT( queryDisjoint->data != 0 );
	V6_ASSERT( queryDisjoint->data != (core::u64)-1 );
	return (float)(queryEnd->data - queryStart->data) / queryDisjoint->data;
}

static void GPUQuery_Release( GPUQuery_s* query )
{
#if V6_GPU_PROFILING == 1
	V6_RELEASE_D3D11( query->query );
#endif
}

static void GPUContext_CreateShaders( GPUContext_s* context, core::CFileSystem* fileSystem, core::IStack* stack )
{
	ID3D11Device* device = context->device;
	
	Compute_Create( device, &context->computes[COMPUTE_SAMPLECOLLECT], "sample_collect_cs.cso", fileSystem, stack );
	Compute_Create( device, &context->computes[COMPUTE_BUILDINNER], "octree_build_inner_cs.cso", fileSystem, stack );
	Compute_Create( device, &context->computes[COMPUTE_BUILDLEAF], "octree_build_leaf_cs.cso", fileSystem, stack );
	Compute_Create( device, &context->computes[COMPUTE_FILLLEAF], "octree_fill_leaf_cs.cso", fileSystem, stack );
	Compute_Create( device, &context->computes[COMPUTE_PACKCOLOR], "octree_pack_cs.cso", fileSystem, stack );
	Compute_Create( device, &context->computes[COMPUTE_STREAM_SETBIT4], "stream_setbit_x4_cs.cso", fileSystem, stack );
	Compute_Create( device, &context->computes[COMPUTE_STREAM_SETBIT8], "stream_setbit_x8_cs.cso", fileSystem, stack );
	Compute_Create( device, &context->computes[COMPUTE_STREAM_SETBIT16], "stream_setbit_x16_cs.cso", fileSystem, stack );
	Compute_Create( device, &context->computes[COMPUTE_STREAM_SETBIT32], "stream_setbit_x32_cs.cso", fileSystem, stack );
	Compute_Create( device, &context->computes[COMPUTE_STREAM_SETBIT64], "stream_setbit_x64_cs.cso", fileSystem, stack );
	Compute_Create( device, &context->computes[COMPUTE_STREAM_PREFIX_SUM], "stream_prefix_sum_cs.cso", fileSystem, stack );
	Compute_Create( device, &context->computes[COMPUTE_STREAM_SUMMARIZE], "stream_summarize_cs.cso", fileSystem, stack );
	Compute_Create( device, &context->computes[COMPUTE_STREAM_SCATTER4], "stream_scatter_x4_cs.cso", fileSystem, stack );
	Compute_Create( device, &context->computes[COMPUTE_STREAM_SCATTER8], "stream_scatter_x8_cs.cso", fileSystem, stack );
	Compute_Create( device, &context->computes[COMPUTE_STREAM_SCATTER16], "stream_scatter_x16_cs.cso", fileSystem, stack );
	Compute_Create( device, &context->computes[COMPUTE_STREAM_SCATTER32], "stream_scatter_x32_cs.cso", fileSystem, stack );
	Compute_Create( device, &context->computes[COMPUTE_STREAM_SCATTER64], "stream_scatter_x64_cs.cso", fileSystem, stack );
	Compute_Create( device, &context->computes[COMPUTE_BLOCK_CULL4], "block_cull_x4_cs.cso", fileSystem, stack );
	Compute_Create( device, &context->computes[COMPUTE_BLOCK_CULL8], "block_cull_x8_cs.cso", fileSystem, stack );
	Compute_Create( device, &context->computes[COMPUTE_BLOCK_CULL16], "block_cull_x16_cs.cso", fileSystem, stack );
	Compute_Create( device, &context->computes[COMPUTE_BLOCK_CULL32], "block_cull_x32_cs.cso", fileSystem, stack );
	Compute_Create( device, &context->computes[COMPUTE_BLOCK_CULL64], "block_cull_x64_cs.cso", fileSystem, stack );
	Compute_Create( device, &context->computes[COMPUTE_BLOCK_CULL_STATS4], "block_cull_stats_x4_cs.cso", fileSystem, stack );
	Compute_Create( device, &context->computes[COMPUTE_BLOCK_CULL_STATS8], "block_cull_stats_x8_cs.cso", fileSystem, stack );
	Compute_Create( device, &context->computes[COMPUTE_BLOCK_CULL_STATS16], "block_cull_stats_x16_cs.cso", fileSystem, stack );
	Compute_Create( device, &context->computes[COMPUTE_BLOCK_CULL_STATS32], "block_cull_stats_x32_cs.cso", fileSystem, stack );
	Compute_Create( device, &context->computes[COMPUTE_BLOCK_CULL_STATS64], "block_cull_stats_x64_cs.cso", fileSystem, stack );
	Compute_Create( device, &context->computes[COMPUTE_BLOCK_TRACE_INIT], "block_trace_init_cs.cso", fileSystem, stack );
#if HLSL_ENCODE_DATA == 1
	Compute_Create( device, &context->computes[COMPUTE_BLOCK_TRACE], "block_trace_cs.cso", fileSystem, stack );
	Compute_Create( device, &context->computes[COMPUTE_BLOCK_TRACE_STATS], "block_trace_stats_cs.cso", fileSystem, stack );
#else
	Compute_Create( device, &context->computes[COMPUTE_BLOCK_TRACE4], "block_trace_x4_cs.cso", fileSystem, stack );
	Compute_Create( device, &context->computes[COMPUTE_BLOCK_TRACE8], "block_trace_x8_cs.cso", fileSystem, stack );
	Compute_Create( device, &context->computes[COMPUTE_BLOCK_TRACE16], "block_trace_x16_cs.cso", fileSystem, stack );
	Compute_Create( device, &context->computes[COMPUTE_BLOCK_TRACE32], "block_trace_x32_cs.cso", fileSystem, stack );
	Compute_Create( device, &context->computes[COMPUTE_BLOCK_TRACE64], "block_trace_x64_cs.cso", fileSystem, stack );
	Compute_Create( device, &context->computes[COMPUTE_BLOCK_TRACE_STATS4], "block_trace_stats_x4_cs.cso", fileSystem, stack );
	Compute_Create( device, &context->computes[COMPUTE_BLOCK_TRACE_STATS8], "block_trace_stats_x8_cs.cso", fileSystem, stack );
	Compute_Create( device, &context->computes[COMPUTE_BLOCK_TRACE_STATS16], "block_trace_stats_x16_cs.cso", fileSystem, stack );
	Compute_Create( device, &context->computes[COMPUTE_BLOCK_TRACE_STATS32], "block_trace_stats_x32_cs.cso", fileSystem, stack );
	Compute_Create( device, &context->computes[COMPUTE_BLOCK_TRACE_STATS64], "block_trace_stats_x64_cs.cso", fileSystem, stack );
#endif
	Compute_Create( device, &context->computes[COMPUTE_BLENDPIXEL], "pixel_blend_cs.cso", fileSystem, stack );
	Compute_Create( device, &context->computes[COMPUTE_BLENDPIXEL_OVERDRAW], "pixel_blend_overdraw_cs.cso", fileSystem, stack );
	Compute_Create( device, &context->computes[COMPUTE_COMPOSESURFACE], "surface_compose_cs.cso", fileSystem, stack );

	Shader_Create( device, &context->shaders[SHADER_BASIC], "basic_vs.cso", "basic_ps.cso", VERTEX_FORMAT_POSITION | VERTEX_FORMAT_COLOR, fileSystem, stack );
	Shader_Create( device, &context->shaders[SHADER_FAKE_CUBE], "fake_cube_vs.cso", "fake_cube_ps.cso", 0, fileSystem, stack );
	Shader_Create( device, &context->shaders[SHADER_GENERIC], "generic_vs.cso", "generic_ps.cso", VERTEX_FORMAT_POSITION | VERTEX_FORMAT_USER0_F3 | VERTEX_FORMAT_USER1_F2, fileSystem, stack );
	Shader_Create( device, &context->shaders[SHADER_GENERIC_ALPHA_TEST], "generic_vs.cso", "generic_alpha_test_ps.cso", VERTEX_FORMAT_POSITION | VERTEX_FORMAT_USER0_F3 | VERTEX_FORMAT_USER1_F2, fileSystem, stack );
}

static void GPUContext_ReleaseShaders( GPUContext_s* context )
{
	for ( core::u32 computeID = 0; computeID < COMPUTE_COUNT; ++computeID )
		Compute_Release( &context->computes[computeID] );

	for ( core::u32 shaderID = 0; shaderID < SHADER_COUNT; ++shaderID )
		Shader_Release( &context->shaders[shaderID] );
}

static void GPUContext_Create( GPUContext_s* context, core::u32 width, core::u32 height, HWND hWnd, core::CFileSystem* fileSystem, core::IAllocator* heap, core::IStack* stack )
{
	memset( context, 0, sizeof( *context ) );

	DXGI_SWAP_CHAIN_DESC oSwapChainDesc = {};

	DXGI_MODE_DESC & oModeDesc = oSwapChainDesc.BufferDesc;
	oModeDesc.Width = width * HLSL_EYE_COUNT;
	oModeDesc.Height = height;
	oModeDesc.RefreshRate.Numerator = 60;
	oModeDesc.RefreshRate.Denominator = 1;
	oModeDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	oModeDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
	oModeDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;

	DXGI_SAMPLE_DESC & oSampleDesc = oSwapChainDesc.SampleDesc;
	oSampleDesc.Count = 1;
	oSampleDesc.Quality = 0;

	oSwapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT | DXGI_USAGE_UNORDERED_ACCESS;
	oSwapChainDesc.BufferCount = 2;
	oSwapChainDesc.OutputWindow = hWnd;
	oSwapChainDesc.Windowed = true;
	oSwapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
	oSwapChainDesc.Flags = 0;

	D3D_FEATURE_LEVEL pFeatureLevels[2] = { D3D_FEATURE_LEVEL_11_1 };
	
#if V6_D3D_DEBUG == 1
	const core::u32 createFlags = D3D11_CREATE_DEVICE_DEBUG;		
#else
	const core::u32 createFlags = 0;		
#endif
	V6_ASSERT_D3D11( D3D11CreateDeviceAndSwapChain(
		NULL,
		D3D_DRIVER_TYPE_HARDWARE,
		NULL,
		createFlags | D3D11_CREATE_DEVICE_DISABLE_GPU_TIMEOUT,
		pFeatureLevels,
		1,
		D3D11_SDK_VERSION,
		&oSwapChainDesc,
		&context->swapChain,
		&context->device,
		&context->featureLevel,
		&context->deviceContext) );

	V6_ASSERT( context->featureLevel == D3D_FEATURE_LEVEL_11_1 );	

	ID3D11Device* device = context->device;
	ID3D11DeviceContext* deviceContext = context->deviceContext;

#if 0
	for ( core::u32 sampleCount = 1; sampleCount <= D3D11_MAX_MULTISAMPLE_SAMPLE_COUNT; sampleCount++)
	{
		core::u32 maxQualityLevel;
		HRESULT hr = m_device->CheckMultisampleQualityLevels( DXGI_FORMAT_R8G8B8A8_UNORM, MSAA_SAMPLE_QUALITY, &maxQualityLevel );
		
		if ( hr != S_OK )
			break;
		
		if ( maxQualityLevel > 0 )
			V6_MSG ("MSAA %dX supported with %d quality levels.\n", sampleCount, maxQualityLevel-1 );		
	}
#endif
	
	V6_ASSERT_D3D11( deviceContext->QueryInterface( IID_PPV_ARGS( &context->userDefinedAnnotation ) ) );

	V6_ASSERT_D3D11( context->swapChain->GetBuffer( 0, __uuidof(ID3D11Texture2D), (void **)&context->surfaceBuffer ) );

	V6_ASSERT_D3D11( device->CreateRenderTargetView( context->surfaceBuffer, 0, &context->surfaceView ) );
	V6_ASSERT_D3D11( device->CreateUnorderedAccessView( context->surfaceBuffer, 0, &context->surfaceUAV ) );
	
	for ( core::u32 eye = 0; eye < HLSL_EYE_COUNT; ++eye )
	{
		{
			D3D11_TEXTURE2D_DESC texDesc = {};
			texDesc.Width = width;
			texDesc.Height = height;
			texDesc.MipLevels = 1;
			texDesc.ArraySize = 1;
			texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
			texDesc.SampleDesc.Count = 1;
			texDesc.SampleDesc.Quality = 0;
			texDesc.Usage = D3D11_USAGE_DEFAULT;
			texDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
			texDesc.CPUAccessFlags = 0;
			texDesc.MiscFlags = 0;

			V6_ASSERT_D3D11( device->CreateTexture2D( &texDesc, nullptr, &context->colorBuffers[eye] ) );
			GPUResource_LogMemory( "Texture2D", texDesc.Width * texDesc.Height * texDesc.ArraySize * DXGIFormat_Size( texDesc.Format ) * texDesc.SampleDesc.Count, "mainColor" );
		}

		V6_ASSERT_D3D11( device->CreateRenderTargetView( context->colorBuffers[eye], 0, &context->colorViews[eye] ) );
		V6_ASSERT_D3D11( device->CreateShaderResourceView( context->colorBuffers[eye], 0, &context->colorSRVs[eye] ) );	
		V6_ASSERT_D3D11( device->CreateUnorderedAccessView( context->colorBuffers[eye], 0, &context->colorUAVs[eye] ) );
	}

	{
		D3D11_TEXTURE2D_DESC texDesc;
		texDesc.Width = width;
		texDesc.Height = height;
		texDesc.MipLevels = 1;
		texDesc.ArraySize = 1;
		texDesc.Format = DXGI_FORMAT_D32_FLOAT;	
		texDesc.SampleDesc.Count = 1;
		texDesc.SampleDesc.Quality = 0;
		texDesc.Usage = D3D11_USAGE_DEFAULT;
		texDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
		texDesc.CPUAccessFlags = 0;
		texDesc.MiscFlags = 0;

		V6_ASSERT_D3D11( device->CreateTexture2D( &texDesc, 0, &context->depthStencilBuffer ) );
		GPUResource_LogMemory( "Texture2D", texDesc.Width * texDesc.Height * texDesc.ArraySize * DXGIFormat_Size( texDesc.Format ) * texDesc.SampleDesc.Count, "mainDepth" );
	}

	V6_ASSERT_D3D11( device->CreateDepthStencilView( context->depthStencilBuffer, 0, &context->depthStencilView ) );

	{
		D3D11_TEXTURE2D_DESC texDesc = {};
		texDesc.Width = width;
		texDesc.Height = height;
		texDesc.MipLevels = 1;
		texDesc.ArraySize = 1;
		texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		texDesc.SampleDesc.Count = 8;
		texDesc.SampleDesc.Quality = 16;
		texDesc.Usage = D3D11_USAGE_DEFAULT;
		texDesc.BindFlags = D3D11_BIND_RENDER_TARGET;
		texDesc.CPUAccessFlags = 0;
		texDesc.MiscFlags = 0;
		
		V6_ASSERT_D3D11( device->CreateTexture2D( &texDesc, nullptr, &context->colorBufferMSAA ) );
		GPUResource_LogMemory( "Texture2D", texDesc.Width * texDesc.Height * texDesc.ArraySize * DXGIFormat_Size( texDesc.Format ) * texDesc.SampleDesc.Count, "mainColorMSAA" );
	}
		
	{
		D3D11_RENDER_TARGET_VIEW_DESC viewDesc = {};
		viewDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		viewDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DMS;

		V6_ASSERT_D3D11( device->CreateRenderTargetView( context->colorBufferMSAA, &viewDesc, &context->colorViewMSAA ) );
	}
	
	{
		D3D11_TEXTURE2D_DESC texDesc = {};
		texDesc.Width = width;
		texDesc.Height = height;
		texDesc.MipLevels = 1;
		texDesc.ArraySize = 1;
		texDesc.Format = DXGI_FORMAT_R32_TYPELESS;		
		texDesc.SampleDesc.Count = 8;
		texDesc.SampleDesc.Quality = 16;
		texDesc.Usage = D3D11_USAGE_DEFAULT;
		texDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
		texDesc.CPUAccessFlags = 0;
		texDesc.MiscFlags = 0;

		V6_ASSERT_D3D11( device->CreateTexture2D( &texDesc, nullptr, &context->depthStencilBufferMSAA ) );
		GPUResource_LogMemory( "Texture2D", texDesc.Width * texDesc.Height * texDesc.ArraySize * DXGIFormat_Size( texDesc.Format ) * texDesc.SampleDesc.Count, "mainDepthMSAA" );
	}
	
	{
		D3D11_DEPTH_STENCIL_VIEW_DESC viewDesc = {};
		viewDesc.Format = DXGI_FORMAT_D32_FLOAT;
		viewDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DMS;
		viewDesc.Flags = 0;

		V6_ASSERT_D3D11( device->CreateDepthStencilView( context->depthStencilBufferMSAA, &viewDesc, &context->depthStencilViewMSAA ) );
	}
		
	{
		D3D11_DEPTH_STENCIL_DESC depthStencilDesc = {};
		depthStencilDesc.DepthEnable = false;
		depthStencilDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;

		V6_ASSERT_D3D11( device->CreateDepthStencilState( &depthStencilDesc, &context->depthStencilStateNoZ ) );
	}

	{
		D3D11_DEPTH_STENCIL_DESC depthStencilDesc = {};
		depthStencilDesc.DepthEnable = true;
		depthStencilDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
		depthStencilDesc.DepthFunc = D3D11_COMPARISON_LESS;

		V6_ASSERT_D3D11( device->CreateDepthStencilState( &depthStencilDesc, &context->depthStencilStateZRO ) );
	}

	{
		D3D11_DEPTH_STENCIL_DESC depthStencilDesc = {};
		depthStencilDesc.DepthEnable = true;
		depthStencilDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
		depthStencilDesc.DepthFunc = D3D11_COMPARISON_LESS;

		V6_ASSERT_D3D11( device->CreateDepthStencilState( &depthStencilDesc, &context->depthStencilStateZRW ) );
	}

	{
		D3D11_BLEND_DESC blendState = {};
		blendState.AlphaToCoverageEnable = false;
		blendState.IndependentBlendEnable = false;
		blendState.RenderTarget[0].BlendEnable = FALSE;
		blendState.RenderTarget[0].RenderTargetWriteMask = 0;
		
		V6_ASSERT_D3D11( device->CreateBlendState( &blendState, &context->blendStateNoColor ) );
	}

	{
		D3D11_BLEND_DESC blendState = {};
		blendState.AlphaToCoverageEnable = false;
		blendState.IndependentBlendEnable = false;
		blendState.RenderTarget[0].BlendEnable = FALSE;
		blendState.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
		
		V6_ASSERT_D3D11( device->CreateBlendState( &blendState, &context->blendStateOpaque ) );
	}

	{
		D3D11_BLEND_DESC blendState = {};
		blendState.AlphaToCoverageEnable = true;
		blendState.IndependentBlendEnable = false;
		blendState.RenderTarget[0].BlendEnable = FALSE;
		blendState.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
		
		V6_ASSERT_D3D11( device->CreateBlendState( &blendState, &context->blendStateAlphaCoverage ) );
	}

	{
		D3D11_BLEND_DESC blendState = {};
		blendState.AlphaToCoverageEnable = false;
		blendState.IndependentBlendEnable = false;
		blendState.RenderTarget[0].BlendEnable = TRUE;
		blendState.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
		blendState.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
		blendState.RenderTarget[0].DestBlend = D3D11_BLEND_ONE;
		blendState.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
		blendState.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
		blendState.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ONE;
		blendState.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
		blendState.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
		
		V6_ASSERT_D3D11( device->CreateBlendState( &blendState, &context->blendStateAdditif ) );
	}

	{
		D3D11_RASTERIZER_DESC rasterDesc = {};
		rasterDesc.FillMode = D3D11_FILL_SOLID;
		rasterDesc.CullMode = D3D11_CULL_NONE;
		rasterDesc.FrontCounterClockwise = false;
		rasterDesc.DepthBias = 0;
		rasterDesc.DepthBiasClamp = 0;
		rasterDesc.SlopeScaledDepthBias = 0.0f;
		rasterDesc.DepthClipEnable = true;
		rasterDesc.ScissorEnable = false;
		rasterDesc.MultisampleEnable = false;
		rasterDesc.AntialiasedLineEnable = false;
		
		V6_ASSERT_D3D11( device->CreateRasterizerState( &rasterDesc, &context->rasterState ) );
	}

	{
		D3D11_SAMPLER_DESC samplerDesc = {};
		samplerDesc.Filter = D3D11_FILTER_ANISOTROPIC;
		samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
		samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
		samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
		samplerDesc.MipLODBias = 0.0f;
		samplerDesc.MaxAnisotropy = D3D11_REQ_MAXANISOTROPY;
		samplerDesc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
		samplerDesc.BorderColor[0] = 0;
		samplerDesc.BorderColor[1] = 0;
		samplerDesc.BorderColor[2] = 0;
		samplerDesc.BorderColor[3] = 0;
		samplerDesc.MinLOD = 0;
		samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;

		V6_ASSERT_D3D11( device->CreateSamplerState( &samplerDesc, &context->samplerState ) );		
	}

	ConstantBuffer_Create( device, &context->constantBuffers[CONSTANT_BUFFER_BASIC], sizeof( v6::hlsl::CBBasic ), "basic" );
	ConstantBuffer_Create( device, &context->constantBuffers[CONSTANT_BUFFER_GENERIC], sizeof( v6::hlsl::CBGeneric ), "generic" );
	ConstantBuffer_Create( device, &context->constantBuffers[CONSTANT_BUFFER_SAMPLE], sizeof( v6::hlsl::CBSample ), "sample" );
	ConstantBuffer_Create( device, &context->constantBuffers[CONSTANT_BUFFER_OCTREE], sizeof( v6::hlsl::CBOctree ), "octree" );
	ConstantBuffer_Create( device, &context->constantBuffers[CONSTANT_BUFFER_STREAM], sizeof( v6::hlsl::CBStream ), "stream" );
	ConstantBuffer_Create( device, &context->constantBuffers[CONSTANT_BUFFER_CULL], sizeof( v6::hlsl::CBCull ), "cull" );
	ConstantBuffer_Create( device, &context->constantBuffers[CONSTANT_BUFFER_BLOCK], sizeof( v6::hlsl::CBBlock ), "block" );
	ConstantBuffer_Create( device, &context->constantBuffers[CONSTANT_BUFFER_PIXEL], sizeof( v6::hlsl::CBPixel), "pixel" );
	ConstantBuffer_Create( device, &context->constantBuffers[CONSTANT_BUFFER_COMPOSE], sizeof( v6::hlsl::CBCompose), "compose" );

	GPUContext_CreateShaders( context, fileSystem, stack );	

	for ( core::u32 bufferID = 0; bufferID < 2; ++bufferID )
	{
		for ( core::u32 queryID = 0; queryID < QUERY_COUNT; ++queryID )
		{
			if ( queryID == QUERY_FREQUENCY )
				GPUQuery_CreateTimeStampDisjoint( device, &context->queries[bufferID][queryID] );
			else
				GPUQuery_CreateTimeStamp( device, &context->queries[bufferID][queryID] );
		}
	}
}

void GPUContext_Release( GPUContext_s* context )
{
	context->deviceContext->ClearState();
	
	for ( core::u32 constantBufferID = 0; constantBufferID < CONSTANT_BUFFER_COUNT; ++constantBufferID )
	{
		if ( context->constantBuffers[constantBufferID ].buf )
			ConstantBuffer_Release( &context->constantBuffers[constantBufferID ] );
	}

	GPUContext_ReleaseShaders( context );

	for ( core::u32 bufferID = 0; bufferID < 2; ++bufferID )
	{
		for ( core::u32 queryID = 0; queryID < QUERY_COUNT; ++queryID )
			GPUQuery_Release( &context->queries[bufferID][queryID] );
	}
		
	V6_RELEASE_D3D11( context->surfaceBuffer );
	V6_RELEASE_D3D11( context->surfaceView );
	V6_RELEASE_D3D11( context->surfaceUAV );

	for ( core::u32 eye = 0; eye < HLSL_EYE_COUNT; ++eye )
	{
		V6_RELEASE_D3D11( context->colorBuffers[eye] );
		V6_RELEASE_D3D11( context->colorViews[eye] );
		V6_RELEASE_D3D11( context->colorSRVs[eye] );
		V6_RELEASE_D3D11( context->colorUAVs[eye] );
	}

	V6_RELEASE_D3D11( context->depthStencilBuffer );
	V6_RELEASE_D3D11( context->depthStencilView );

	V6_RELEASE_D3D11( context->colorBufferMSAA );
	V6_RELEASE_D3D11( context->colorViewMSAA );

	V6_RELEASE_D3D11( context->depthStencilBufferMSAA );
	V6_RELEASE_D3D11( context->depthStencilViewMSAA );

	V6_RELEASE_D3D11( context->swapChain );
	V6_RELEASE_D3D11( context->deviceContext );
	V6_RELEASE_D3D11( context->device );
}

static void Cube_Create( ID3D11Device* device, Cube_s* cube, core::u32 size )
{
	{
		D3D11_TEXTURE2D_DESC texDesc = {};
		texDesc.Width = size;
		texDesc.Height = size;
		texDesc.MipLevels = 1;
		texDesc.ArraySize = 1;
		texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		texDesc.SampleDesc.Count = 1;
		texDesc.SampleDesc.Quality = 0;
		texDesc.Usage = D3D11_USAGE_DEFAULT;
		texDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
		texDesc.CPUAccessFlags = 0;
		texDesc.MiscFlags = 0;
		
		V6_ASSERT_D3D11( device->CreateTexture2D( &texDesc, nullptr, &cube->colorBuffer) );
		GPUResource_LogMemory( "Texture2D", texDesc.Width * texDesc.Height * texDesc.ArraySize * DXGIFormat_Size( texDesc.Format ) * texDesc.SampleDesc.Count, "cubeColor" );
	}

	{
		D3D11_SHADER_RESOURCE_VIEW_DESC viewDesc = {};
		viewDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		viewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		viewDesc.Texture2D.MipLevels = 1;
		viewDesc.Texture2D.MostDetailedMip = 0;

		V6_ASSERT_D3D11( device->CreateShaderResourceView( cube->colorBuffer, &viewDesc, &cube->colorSRV ) );
	}

	{
		D3D11_RENDER_TARGET_VIEW_DESC viewDesc = {};
		viewDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		viewDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
		viewDesc.Texture2D.MipSlice = 0;

		V6_ASSERT_D3D11( device->CreateRenderTargetView( cube->colorBuffer, &viewDesc, &cube->colorRTV ) );
	}
	
	{
		D3D11_TEXTURE2D_DESC texDesc = {};
		texDesc.Width = size;
		texDesc.Height = size;
		texDesc.MipLevels = 1;
		texDesc.ArraySize = 1;
		texDesc.Format = DXGI_FORMAT_R32_TYPELESS;		
		texDesc.SampleDesc.Count = 1;
		texDesc.SampleDesc.Quality = 0;
		texDesc.Usage = D3D11_USAGE_DEFAULT;
		texDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
		texDesc.CPUAccessFlags = 0;
		texDesc.MiscFlags = 0;

		V6_ASSERT_D3D11( device->CreateTexture2D( &texDesc, nullptr, &cube->depthBuffer ) );
		GPUResource_LogMemory( "Texture2D", texDesc.Width * texDesc.Height * texDesc.ArraySize * DXGIFormat_Size( texDesc.Format ) * texDesc.SampleDesc.Count, "cubeDepth" );
	}

	{
		D3D11_SHADER_RESOURCE_VIEW_DESC viewDesc = {};
		viewDesc.Format = DXGI_FORMAT_R32_FLOAT;
		viewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		viewDesc.Texture2D.MipLevels = 1;
		viewDesc.Texture2D.MostDetailedMip = 0;

		V6_ASSERT_D3D11( device->CreateShaderResourceView( cube->depthBuffer, &viewDesc, &cube->depthSRV ) );
	}

	{
		D3D11_DEPTH_STENCIL_VIEW_DESC viewDesc = {};
		viewDesc.Format = DXGI_FORMAT_D32_FLOAT;
		viewDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
		viewDesc.Flags = 0;
		viewDesc.Texture2D.MipSlice = 0;

		V6_ASSERT_D3D11( device->CreateDepthStencilView( cube->depthBuffer, &viewDesc, &cube->depthRTV ) );
	}
	
	cube->size = size;
}

static void Cube_Release( Cube_s* cube )
{
	V6_RELEASE_D3D11( cube->colorBuffer );
	V6_RELEASE_D3D11( cube->depthBuffer );

	V6_RELEASE_D3D11( cube->colorSRV );
	V6_RELEASE_D3D11( cube->depthSRV );

	V6_RELEASE_D3D11( cube->colorRTV );
	V6_RELEASE_D3D11( cube->depthRTV );
}

static void Config_Init( Config_s* config, core::u32 screenWidth, core::u32 screenHeight, core::u32 cubeSize, core::u32 gridWidth )
{
	config->screenWidth = screenWidth;
	config->screenHeight = screenHeight;
	config->sampleCount = (core::u32)(cubeSize * cubeSize / AVERAGE_SAMPLE_PER_PIXEL);
	config->leafCount = (core::u32)(gridWidth * gridWidth * 6 * AVERAGE_LAYER_COUNT);
	config->nodeCount = config->leafCount * 3;	
	config->cellCount = config->leafCount * 33 / 16;
	config->blockCount = config->cellCount / 8;
	config->culledBlockCount = config->blockCount / 6;
	config->culledCellCount = config->culledBlockCount * 16;
	config->cellItemCount = (screenWidth * HLSL_EYE_COUNT * screenHeight) * HLSL_CELL_ITEM_PER_PIXEL_MAX_COUNT;
}

static void Config_Log( const Config_s* config )
{
	V6_MSG( "%-20s: %d\n", "config.screenWidth", config->screenWidth );
	V6_MSG( "%-20s: %d\n", "config.screenHeight", config->screenHeight );
	V6_MSG( "%-20s: %13s\n", "config.sample", FormatInteger_Unsafe( config->sampleCount ) );
	V6_MSG( "%-20s: %13s\n", "config.leaf", FormatInteger_Unsafe( config->leafCount ) );
	V6_MSG( "%-20s: %13s\n", "config.node", FormatInteger_Unsafe( config->nodeCount ) );
	V6_MSG( "%-20s: %13s\n", "config.cell", FormatInteger_Unsafe( config->cellCount ) );
	V6_MSG( "%-20s: %13s\n", "config.block", FormatInteger_Unsafe( config->blockCount ) );
	V6_MSG( "%-20s: %13s\n", "config.culledBlock", FormatInteger_Unsafe( config->culledBlockCount ) );
	V6_MSG( "%-20s: %13s\n", "config.culledCell", FormatInteger_Unsafe( config->culledCellCount ) );
	V6_MSG( "%-20s: %13s\n", "config.cellItem", FormatInteger_Unsafe( config->cellItemCount ) );
}

static void Sample_Create( ID3D11Device* device, Sample_s* sample, const Config_s* config, core::IAllocator* heap )
{
	GPUBuffer_CreateStructured( device, &sample->samples, sizeof( hlsl::Sample ), config->sampleCount, 0, "samples" );
	GPUBuffer_CreateIndirectArgs( device, &sample->indirectArgs, sample_all_offset, GPUBUFFER_CREATION_FLAG_READ_BACK, "sampleIndirectArgs" );
}

static void Sample_Release( ID3D11Device* device, Sample_s* sample )
{
	GPUBuffer_Release( device, &sample->samples );
	GPUBuffer_Release( device, &sample->indirectArgs );
}

static void Octree_Create( ID3D11Device* device, Octree_s* octree, const Config_s* config, core::IAllocator* heap )
{
	GPUBuffer_CreateTyped( device, &octree->sampleNodeOffsets, DXGI_FORMAT_R32_UINT, config->sampleCount, 0, "octreeSampleNodeOffsets" );
	GPUBuffer_CreateTyped( device, &octree->firstChildOffsets, DXGI_FORMAT_R32_UINT, config->nodeCount, 0, "octreeFirstChildOffsets" );
	
	{
		D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
		uavDesc.Format = DXGI_FORMAT_R32_UINT;
		uavDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
		uavDesc.Buffer.FirstElement = 0;
		uavDesc.Buffer.NumElements = GRID_COUNT * 8;
		uavDesc.Buffer.Flags = 0;

		V6_ASSERT_D3D11( device->CreateUnorderedAccessView( octree->firstChildOffsets.buf, &uavDesc, &octree->firstChildOffsetsLimitedUAV ) );
	}

	GPUBuffer_CreateStructured( device, &octree->leaves, sizeof( hlsl::OctreeLeaf ), config->leafCount, 0, "octreeLeaves" );
	GPUBuffer_CreateIndirectArgs( device, &octree->indirectArgs, octree_all_offset, GPUBUFFER_CREATION_FLAG_READ_BACK, "octreeIndirectArgs" );
}

static void Octree_Release( ID3D11Device* device, Octree_s* octree )
{
	GPUBuffer_Release( device, &octree->sampleNodeOffsets );
	GPUBuffer_Release( device, &octree->firstChildOffsets );
	V6_RELEASE_D3D11( octree->firstChildOffsetsLimitedUAV );
	GPUBuffer_Release( device, &octree->leaves );
	GPUBuffer_Release( device, &octree->indirectArgs );
}

static void Stream_Create( ID3D11Device* device, Stream_s* stream, const Config_s* config, core::IAllocator* heap, const core::u32 blockCounts[HLSL_BUCKET_COUNT] )
{
	stream->blockCount = 0;
	stream->cellCount = 0;

	core::u32 cellPerBucketCount = 4;
	for ( core::u32 bucket = 0; bucket < CODEC_BUCKET_COUNT; ++bucket, cellPerBucketCount <<= 1 )
	{
		stream->blockCount += blockCounts[bucket];
		stream->cellCount += blockCounts[bucket] * cellPerBucketCount;
	}

#if V6_SORTED_STREAM == 1
	GPUBuffer_CreateTyped( device, &stream->groupBits, DXGI_FORMAT_R32_UINT, HLSL_STREAM_GROUP_SIZE, 0, "streamGroupBits" );
	GPUBuffer_CreateTyped( device, &stream->bits, DXGI_FORMAT_R32_UINT, HLSL_STREAM_SIZE, 0, "streamBits" );
	GPUBuffer_CreateTyped( device, &stream->counts, DXGI_FORMAT_R32_UINT, HLSL_STREAM_SIZE * 2, 0, "streamCounts" );
	GPUBuffer_CreateTyped( device, &stream->addresses, DXGI_FORMAT_R32_UINT, HLSL_STREAM_SIZE, 0, "streamAddresses" );
#endif // #if V6_SORTED_STREAM == 1
	GPUBuffer_CreateTyped( device, &stream->blockPos, DXGI_FORMAT_R32_UINT, core::Max( stream->blockCount, 1u ), GPUBUFFER_CREATION_FLAG_READ_BACK, "streamBlockPositions" );
	GPUBuffer_CreateTyped( device, &stream->blockData, DXGI_FORMAT_R32_UINT, core::Max( stream->cellCount, 1u ), GPUBUFFER_CREATION_FLAG_READ_BACK, "streamBockData" );
}

static void Stream_Release( ID3D11Device* device, Stream_s* stream )
{
#if V6_SORTED_STREAM == 1
	GPUBuffer_Release( device, &stream->groupBits );
	GPUBuffer_Release( device, &stream->bits );
	GPUBuffer_Release( device, &stream->counts );
	GPUBuffer_Release( device, &stream->addresses );
#endif
	GPUBuffer_Release( device, &stream->blockPos );
	GPUBuffer_Release( device, &stream->blockData );
}

static void Block_Create( ID3D11Device* device, Block_s* block, const Config_s* config, core::IAllocator* heap )
{
	GPUBuffer_CreateTyped( device, &block->blockPos, DXGI_FORMAT_R32_UINT, config->blockCount, 0, "blockPositions" );
#if HLSL_ENCODE_DATA == 1
	GPUBuffer_CreateTyped( device, &block->blockData, DXGI_FORMAT_R32_UINT, config->blockCount * 5, 0, "blockData" );
#else
	GPUBuffer_CreateTyped( device, &block->blockData, DXGI_FORMAT_R32_UINT, config->blockCount * 16, 0, "blockData" );
#endif
	GPUBuffer_CreateIndirectArgs( device, &block->blockIndirectArgs, block_all_offset, GPUBUFFER_CREATION_FLAG_READ_BACK, "blockIndirectArgs" );
}

static void Block_CreateFromData( ID3D11Device* device, Block_s* block, const void* blockPos, const void* blockData, const core::u32 blockCounts[HLSL_BUCKET_COUNT] )
{
	core::u32 blockIndirectArgs[block_all_offset] = {};

	core::u32 blockPosCount = 0;
	core::u32 blockDataCount = 0;

	core::u32 cellPerBucketCount = 4;
	for ( core::u32 bucket = 0; bucket < CODEC_BUCKET_COUNT; ++bucket, cellPerBucketCount <<= 1 )
	{
		block_groupCountX( bucket ) = GROUP_COUNT( blockCounts[bucket], HLSL_BLOCK_THREAD_GROUP_SIZE );
		block_groupCountY( bucket ) = 1;
		block_groupCountZ( bucket ) = 1;

		const core::u32 cellCount = blockCounts[bucket] * cellPerBucketCount;

		block_count( bucket ) = blockCounts[bucket];
		block_posOffset( bucket ) = blockPosCount;
		block_dataOffset( bucket ) = blockDataCount;
		block_cellCount( bucket ) = cellCount;

		blockPosCount += blockCounts[bucket];
		blockDataCount += cellCount;
	}

	if ( blockPosCount )
	{
		V6_ASSERT( blockDataCount );
		GPUBuffer_CreateTypedWithStaticData( device, &block->blockPos, blockPos, DXGI_FORMAT_R32_UINT, blockPosCount, 0, "blockPositions" );
		GPUBuffer_CreateTypedWithStaticData( device, &block->blockData, blockData, DXGI_FORMAT_R32_UINT, blockDataCount, 0, "blockData" );
	}
	else
	{
		V6_ASSERT( blockDataCount == 0 );
		GPUBuffer_CreateTyped( device, &block->blockPos, DXGI_FORMAT_R32_UINT, 1, 0, "blockPositions" );
		GPUBuffer_CreateTyped( device, &block->blockData, DXGI_FORMAT_R32_UINT, 1, 0, "blockData" );
	}
	GPUBuffer_CreateIndirectArgsWithStaticData( device, &block->blockIndirectArgs, &blockIndirectArgs, block_all_offset, 0, "blockIndirectArgs" );
}

static void Block_Release( ID3D11Device* device, Block_s* block )
{
	GPUBuffer_Release( device, &block->blockPos );
	GPUBuffer_Release( device, &block->blockData );
	GPUBuffer_Release( device, &block->blockIndirectArgs );
}

static void Trace_Create( ID3D11Device* device, Trace_s* trace, const Config_s* config, core::IAllocator* heap )
{
	GPUBuffer_CreateTyped( device, &trace->traceCell, DXGI_FORMAT_R32_UINT, config->culledCellCount * 2, 0, "traceCell" );
	GPUBuffer_CreateIndirectArgs( device, &trace->traceIndirectArgs, trace_all_offset, GPUBUFFER_CREATION_FLAG_READ_BACK, "traceIndirectArgs" );

	GPUBuffer_CreateStructured( device, &trace->cellItems, sizeof( hlsl::BlockCellItem ), config->cellItemCount, 0, "blockCellItems" );
	GPUBuffer_CreateTyped( device, &trace->cellItemCounters, DXGI_FORMAT_R32_UINT, config->screenWidth * HLSL_EYE_COUNT * config->screenHeight, 0, "blockCellItemCounters" );	
	
	GPUBuffer_CreateStructured( device, &trace->cullStats, sizeof( hlsl::BlockCullStats ), 1, GPUBUFFER_CREATION_FLAG_READ_BACK, "blockCullStats" );
	GPUBuffer_CreateStructured( device, &trace->traceStats, sizeof( hlsl::BlockTraceStats ), 1, GPUBUFFER_CREATION_FLAG_READ_BACK, "blockTraceStats" );

	Texture2D_CreateRW( device, &trace->colors, config->screenWidth, config->screenHeight, "pixelColors" );
}

static void Trace_Release( ID3D11Device* device, Trace_s* trace )
{
	GPUBuffer_Release( device, &trace->traceCell );
	GPUBuffer_Release( device, &trace->traceIndirectArgs );

	GPUBuffer_Release( device, &trace->cellItems );
	GPUBuffer_Release( device, &trace->cellItemCounters );

	GPUBuffer_Release( device, &trace->cullStats );
	GPUBuffer_Release( device, &trace->traceStats );
	
	GPUTexture_Release( &trace->colors );
}

static void Mesh_UpdateVertices( ID3D11DeviceContext* context, GPUMesh_s* mesh, const void* vertices )
{
	V6_ASSERT( mesh->m_vertexBuffer );

	D3D11_BOX box;
	box.left = 0;
	box.right = mesh->m_vertexCount * mesh->m_vertexSize;
	box.front = 0;
	box.back = 1;
	box.top = 0;
	box.bottom = 1;
		
	context->UpdateSubresource( mesh->m_vertexBuffer, 0, &box, vertices, box.right, box.right );
}

static void Mesh_Create( ID3D11Device* device, GPUMesh_s* mesh, const void* vertices, core::u32 vertexCount, core::u32 vertexSize, core::u32 vertexFormat, const void* indices, core::u32 indexCount, core::u32 indexSize, D3D11_PRIMITIVE_TOPOLOGY topology )
{
	mesh->m_vertexBuffer = nullptr;
	mesh->m_vertexCount = vertexCount;
	mesh->m_vertexSize = 0;
	mesh->m_vertexFormat = 0;
	if ( vertexCount > 0 && vertexSize > 0 && vertices != nullptr )
	{	
		D3D11_BUFFER_DESC bufDesc = {};
		bufDesc.Usage = D3D11_USAGE_DEFAULT;
		bufDesc.ByteWidth = vertexSize * vertexCount;
		bufDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		bufDesc.CPUAccessFlags = 0;
		bufDesc.MiscFlags = 0;
		bufDesc.StructureByteStride = 0;

		D3D11_SUBRESOURCE_DATA data = {};
		data.pSysMem = vertices;
		data.SysMemPitch = 0;
		data.SysMemSlicePitch = 0;

		V6_ASSERT_D3D11( device->CreateBuffer( &bufDesc, &data, &mesh->m_vertexBuffer ) );
		GPUResource_LogMemory( "VertexBuffer", bufDesc.ByteWidth, "mesh" );

		mesh->m_vertexSize = vertexSize;
		mesh->m_vertexFormat = vertexFormat;
	}
	
	mesh->m_indexBuffer = nullptr;
	mesh->m_indexCount = 0;
	mesh->m_indexSize = 0;
	if ( indexCount > 0 && indexSize > 0 && indices != nullptr )
	{
		D3D11_BUFFER_DESC bufDesc = {};
		bufDesc.Usage = D3D11_USAGE_DEFAULT;
		bufDesc.ByteWidth = indexSize * indexCount;
		bufDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
		bufDesc.CPUAccessFlags = 0;
		bufDesc.MiscFlags = 0;
		bufDesc.StructureByteStride = 0;

		D3D11_SUBRESOURCE_DATA data = {};
		data.pSysMem = indices;
		data.SysMemPitch = 0;
		data.SysMemSlicePitch = 0;

		V6_ASSERT_D3D11( device->CreateBuffer( &bufDesc, &data, &mesh->m_indexBuffer ) );
		GPUResource_LogMemory( "IndexBuffer", bufDesc.ByteWidth, "mesh" );

		mesh->m_indexCount = indexCount;
		mesh->m_indexSize = indexSize;
	}

	mesh->m_topology = topology;
}

static void Mesh_Release( GPUMesh_s* mesh )
{
	if ( mesh->m_vertexBuffer )
		V6_RELEASE_D3D11( mesh->m_vertexBuffer );
	if ( mesh->m_indexBuffer )
		V6_RELEASE_D3D11( mesh->m_indexBuffer );
}

static void Mesh_CreateTriangle( ID3D11Device* device, GPUMesh_s* mesh )
{
	const BasicVertex_s vertices[3] = 
	{
		{ core::Vec3_Make( 0.0f, 1.0f, 0.0f ), core::Color_Make( 255, 0, 0, 255) },
		{ core::Vec3_Make( 1.0f, -1.0f, 0.0f ), core::Color_Make( 0, 255, 0, 255) },
		{ core::Vec3_Make( -1.0f, -1.0f, 0.0f ), core::Color_Make( 0, 0, 255, 255) } 
	};

	const core::u16 indices[3] = { 0, 1, 2 };

	Mesh_Create( device, mesh, vertices, 3, sizeof( BasicVertex_s ), VERTEX_FORMAT_POSITION | VERTEX_FORMAT_COLOR, indices, 3, sizeof( core::u16 ), D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST );
}

static void Mesh_CreateBox( ID3D11Device* device, GPUMesh_s* mesh, const core::Color_s color, bool wireframe )
{
	const BasicVertex_s vertices[8] = 
	{
		{ core::Vec3_Make( -1.0f, -1.0f,  1.0f ), color },
		{ core::Vec3_Make(  1.0f, -1.0f,  1.0f ), color },
		{ core::Vec3_Make( -1.0f,  1.0f,  1.0f ), color },
		{ core::Vec3_Make(  1.0f,  1.0f,  1.0f ), color },
		{ core::Vec3_Make( -1.0f, -1.0f, -1.0f ), color },
		{ core::Vec3_Make(  1.0f, -1.0f, -1.0f ), color },
		{ core::Vec3_Make( -1.0f,  1.0f, -1.0f ), color },
		{ core::Vec3_Make(  1.0f,  1.0f, -1.0f ), color },
	};

	if ( wireframe )
	{
		const core::u16 indices[24] = { 
			0, 1, 1, 3, 3, 2, 2, 0,
			4, 5, 5, 7, 7, 6, 6, 4,
			1, 5, 0, 4, 3, 7, 2, 6 };

		Mesh_Create( device, mesh, vertices, 8, sizeof( BasicVertex_s ), VERTEX_FORMAT_POSITION | VERTEX_FORMAT_COLOR, indices, 24, sizeof( core::u16 ), D3D11_PRIMITIVE_TOPOLOGY_LINELIST );
	}
	else
	{
		const core::u16 indices[36] = { 
			0, 2, 3,
			0, 3, 1,
			1, 3, 7, 
			1, 7, 5, 
			5, 7, 6,
			5, 6, 4,
			4, 6, 2,
			4, 2, 0, 
			2, 6, 7, 
			2, 7, 3,
			1, 5, 4,
			1, 4, 0 };

		Mesh_Create( device, mesh, vertices, 8, sizeof( BasicVertex_s ), VERTEX_FORMAT_POSITION | VERTEX_FORMAT_COLOR, indices, 36, sizeof( core::u16 ), D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST );
	}
}

static void Mesh_CreateQuad( ID3D11Device* device, GPUMesh_s* mesh, const core::Color_s color )
{
	const BasicVertex_s vertices[8] = 
	{
		{ core::Vec3_Make( -1.0f, -1.0f, 0.0f ), color },
		{ core::Vec3_Make(  1.0f, -1.0f, 0.0f ), color },
		{ core::Vec3_Make( -1.0f,  1.0f, 0.0f ), color },
		{ core::Vec3_Make(  1.0f,  1.0f, 0.0f ), color },
	};

	const core::u16 indices[4] = { 	0, 2, 1, 3 };

	Mesh_Create( device, mesh, vertices, 4, sizeof( BasicVertex_s ), VERTEX_FORMAT_POSITION | VERTEX_FORMAT_COLOR, indices, 4, sizeof( core::u16 ), D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP );
}

static void Mesh_CreateVirtualQuad( ID3D11Device* device, GPUMesh_s* mesh )
{
	Mesh_Create( device, mesh, nullptr, 4, 0, 0, nullptr, 0, 0, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP );
}

static void Mesh_CreateVirtualBox( ID3D11Device* device, GPUMesh_s* mesh )
{
	const core::u16 indices[36] = { 
		0, 2, 3,
		0, 3, 1,
		1, 3, 7, 
		1, 7, 5, 
		5, 7, 6,
		5, 6, 4,
		4, 6, 2,
		4, 2, 0, 
		2, 6, 7, 
		2, 7, 3,
		1, 5, 4,
		1, 4, 0 };

	Mesh_Create( device, mesh, nullptr, 0, 0, 0, indices, 36, sizeof( core::u16 ), D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST );	
}

static void Mesh_CreateFakeCube( ID3D11Device* device, GPUMesh_s* mesh )
{
#if 0
	const core::u16 indices[36] = { 
			0, 2, 3,
			0, 3, 1,
			1, 3, 7, 
			1, 7, 5, 
			5, 7, 6,
			5, 6, 4,
			4, 6, 2,
			4, 2, 0, 
			2, 6, 7, 
			2, 7, 3,
			1, 5, 4,
			1, 4, 0 };

	Mesh_Create( device, mesh, nullptr, 0, 0, 0, indices, 36, sizeof( core::u16 ), D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST );
#else
	const core::u16 indices[5] = { 0, 2, 3, 1, 0 };

	Mesh_Create( device, mesh, nullptr, 0, 0, 0, indices, 5, sizeof( core::u16 ), D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP );
#endif
}

static void Mesh_CreatePoint( ID3D11Device* device, GPUMesh_s* mesh )
{
	Mesh_Create( device, mesh, nullptr, 0, 0, 0, nullptr, 0, sizeof( core::u16 ), D3D11_PRIMITIVE_TOPOLOGY_POINTLIST );
}

static void Mesh_CreateLine( ID3D11Device* device, GPUMesh_s* mesh, const core::Color_s color )
{
	const BasicVertex_s vertices[2] = 
	{
		{ core::Vec3_Make( -1.0f, -1.0f, -1.0f ), color },
		{ core::Vec3_Make(  1.0f,  1.0f,  1.0f ), color },
	};

	Mesh_Create( device, mesh, vertices, 2, sizeof( BasicVertex_s ), VERTEX_FORMAT_POSITION | VERTEX_FORMAT_COLOR, nullptr, 0, sizeof( core::u16 ), D3D11_PRIMITIVE_TOPOLOGY_LINELIST );
}

static void Mesh_Draw( GPUMesh_s* mesh, core::u32 instanceCount, GPUShader_s* shader, ID3D11DeviceContext* ctx, ID3D11Buffer* bufferArgs, core::u32 offsetArgs )
{
	V6_ASSERT( shader->m_vertexFormat == mesh->m_vertexFormat );
	V6_ASSERT( instanceCount > 0 );

	ctx->IASetInputLayout( shader->m_inputLayout );
	ctx->VSSetShader( shader->m_vertexShader, nullptr, 0 );
	ctx->PSSetShader( shader->m_pixelShader, nullptr, 0 );
		
	const core::u32 stride = mesh->m_vertexSize; 
	const core::u32 offset = 0;
			
	ctx->IASetVertexBuffers( 0, mesh->m_vertexBuffer != nullptr ? 1 : 0, &mesh->m_vertexBuffer, &stride, &offset );	
	ctx->IASetPrimitiveTopology( mesh->m_topology );

	if ( mesh->m_indexCount )
	{
		switch ( mesh->m_indexSize )
		{
		case 2:
			ctx->IASetIndexBuffer( mesh->m_indexBuffer, DXGI_FORMAT_R16_UINT, 0 );
			break;
		case 4:
			ctx->IASetIndexBuffer( mesh->m_indexBuffer, DXGI_FORMAT_R32_UINT, 0 );
			break;
		default:
			V6_ASSERT_NOT_SUPPORTED();
		}

		if ( bufferArgs )
			ctx->DrawIndexedInstancedIndirect( bufferArgs, offsetArgs );
		else if ( instanceCount == 1 )
			ctx->DrawIndexed( mesh->m_indexCount, 0, 0 );
		else
			ctx->DrawIndexedInstanced( mesh->m_indexCount, instanceCount, 0, 0, 0 );
	}
	else
	{
		V6_ASSERT( mesh->m_indexBuffer == nullptr );
		ctx->IASetIndexBuffer( nullptr, DXGI_FORMAT_R32_UINT, 0 );
				
		if ( bufferArgs )
			ctx->DrawInstancedIndirect( bufferArgs, offsetArgs );
		else 
		{
			V6_ASSERT( mesh->m_vertexCount > 0 );
			if ( instanceCount == 1 )
				ctx->Draw( mesh->m_vertexCount, 0 );
			else
				ctx->DrawInstanced( mesh->m_vertexCount, instanceCount, 0, 0 );
		}
	}
}

static void Material_DrawBasic( Material_s* material, Entity_s* entity, Scene_s* scene, GPUContext_s* ctx, const RenderingView_s* view, const RenderingSettings_s* settings )
{
#if V6_SIMPLE_SCENE == 0
	if ( settings->isCapturing )
		return;
#endif // #if V6_SIMPLE_SCENE == 0

	v6::hlsl::CBBasic* cbBasic = ConstantBuffer_MapWrite< v6::hlsl::CBBasic >( ctx->deviceContext, &ctx->constantBuffers[CONSTANT_BUFFER_BASIC] );

	core::Mat4x4 worlMatrix;
	core::Mat4x4_Scale( &worlMatrix, entity->scale );
	core::Mat4x4_SetTranslation( &worlMatrix, entity->pos );

	// use this order because one matrix is "from" local space and the other is "to" local space
	core::Mat4x4 objectToViewMatrix;
	core::Mat4x4_Mul( &objectToViewMatrix, view->viewMatrix, worlMatrix );	
	
	cbBasic->c_basicObjectToView = objectToViewMatrix;
	cbBasic->c_basicViewToProj = view->projMatrix;
	core::Mat4x4_Mul( &cbBasic->c_basicObjectToProj, objectToViewMatrix, view->projMatrix );

	ConstantBuffer_UnmapWrite( ctx->deviceContext, &ctx->constantBuffers[CONSTANT_BUFFER_BASIC] );

	ctx->deviceContext->VSSetConstantBuffers( CONSTANT_BUFFER_BASIC, 1, &ctx->constantBuffers[CONSTANT_BUFFER_BASIC].buf );
		
	GPUMesh_s* mesh = &scene->meshes[entity->meshID];
	GPUShader_s* shader = &ctx->shaders[SHADER_BASIC];
	Mesh_Draw( mesh, 1, shader, ctx->deviceContext, nullptr, 0 );
}

static void Material_DrawFakeCube( Material_s* material, Entity_s* entity, Scene_s* scene, GPUContext_s* ctx, const RenderingView_s* view, const RenderingSettings_s* settings )
{
	v6::hlsl::CBBasic* cbBasic = ConstantBuffer_MapWrite< v6::hlsl::CBBasic >( ctx->deviceContext, &ctx->constantBuffers[CONSTANT_BUFFER_BASIC] );

	core::Mat4x4 worlMatrix;
	core::Mat4x4_Scale( &worlMatrix, entity->scale );
	core::Mat4x4_SetTranslation( &worlMatrix, entity->pos );
			
	// use this order because one matrix is "from" local space and the other is "to" local space
	core::Mat4x4_Mul( &cbBasic->c_basicObjectToView, view->viewMatrix, worlMatrix );
	cbBasic->c_basicViewToProj = view->projMatrix;

	ConstantBuffer_UnmapWrite( ctx->deviceContext, &ctx->constantBuffers[CONSTANT_BUFFER_BASIC] );

	ctx->deviceContext->VSSetConstantBuffers( CONSTANT_BUFFER_BASIC, 1, &ctx->constantBuffers[CONSTANT_BUFFER_BASIC].buf );
		
	GPUMesh_s* mesh = &scene->meshes[entity->meshID];
	GPUShader_s* shader = &ctx->shaders[SHADER_FAKE_CUBE];
	Mesh_Draw( mesh, 1, shader, ctx->deviceContext, nullptr, 0 );
}

static void Material_DrawGeneric( Material_s* material, Entity_s* entity, Scene_s* scene, GPUContext_s* ctx, const RenderingView_s* view, const RenderingSettings_s* settings )
{
	v6::hlsl::CBGeneric* cbGeneric = ConstantBuffer_MapWrite< v6::hlsl::CBGeneric >( ctx->deviceContext, &ctx->constantBuffers[CONSTANT_BUFFER_GENERIC] );

	core::Mat4x4 worlMatrix;
	core::Mat4x4_Scale( &worlMatrix, entity->scale );
	core::Mat4x4_SetTranslation( &worlMatrix, entity->pos );
			
	// use this order because one matrix is "from" local space and the other is "to" local space
	const bool useAlbedo = material->textureIDs[TEXTURE_GENERIC_DIFFUSE] != ENTITY_TEXTURE_INVALID;
	const bool useAlpha = material->textureIDs[TEXTURE_GENERIC_ALPHA] != ENTITY_TEXTURE_INVALID;
	cbGeneric->c_genericObjectToWorld = worlMatrix;
	cbGeneric->c_genericWorldToView = view->viewMatrix;
	cbGeneric->c_genericViewToProj = view->projMatrix;
	cbGeneric->c_genericDiffuse = useAlbedo ? core::Vec3_Make( 1.0f, 1.0f, 1.0f ) : material->diffuse;
	cbGeneric->c_genericUseAlbedo = useAlbedo;
	cbGeneric->c_genericUseAlpha = useAlpha;	

	ConstantBuffer_UnmapWrite( ctx->deviceContext, &ctx->constantBuffers[CONSTANT_BUFFER_GENERIC] );

	ctx->deviceContext->VSSetConstantBuffers( CONSTANT_BUFFER_GENERIC, 1, &ctx->constantBuffers[CONSTANT_BUFFER_GENERIC].buf );
	ctx->deviceContext->PSSetConstantBuffers( CONSTANT_BUFFER_GENERIC, 1, &ctx->constantBuffers[CONSTANT_BUFFER_GENERIC].buf );
	
	ctx->deviceContext->PSSetSamplers( HLSL_TRILINEAR_SLOT, 1, &ctx->samplerState );

	if ( material->textureIDs[TEXTURE_GENERIC_DIFFUSE] != ENTITY_TEXTURE_INVALID )
	{
		GPUTexture2D_s* texture = &scene->textures[material->textureIDs[TEXTURE_GENERIC_DIFFUSE]];
		if ( texture->mipmapState == GPUTEXTURE_MIPMAP_STATE_REQUIRED )
		{
			ctx->deviceContext->GenerateMips( texture->srv );
			texture->mipmapState = GPUTEXTURE_MIPMAP_STATE_GENERATED;
		}
		ctx->deviceContext->PSSetShaderResources( HLSL_GENERIC_ALBEDO_SLOT, 1, &texture->srv );
	}

	if ( material->textureIDs[TEXTURE_GENERIC_ALPHA] != ENTITY_TEXTURE_INVALID )
	{
		GPUTexture2D_s* texture = &scene->textures[material->textureIDs[TEXTURE_GENERIC_ALPHA]];
		if ( texture->mipmapState == GPUTEXTURE_MIPMAP_STATE_REQUIRED )
		{
			ctx->deviceContext->GenerateMips( texture->srv );
			texture->mipmapState = GPUTEXTURE_MIPMAP_STATE_GENERATED;
		}
		ctx->deviceContext->PSSetShaderResources( HLSL_GENERIC_ALPHA_SLOT, 1, &texture->srv );
	}
		
	GPUMesh_s* mesh = &scene->meshes[entity->meshID];
	GPUShader_s* shader = (useAlpha && !settings->useAlphaCoverage) ? &ctx->shaders[SHADER_GENERIC_ALPHA_TEST] : &ctx->shaders[SHADER_GENERIC];
	Mesh_Draw( mesh, 1, shader, ctx->deviceContext, nullptr, 0 );

	static const void* nulls[8] = {};
	if ( material->textureIDs[TEXTURE_GENERIC_DIFFUSE] )
		ctx->deviceContext->PSSetShaderResources( HLSL_GENERIC_ALBEDO_SLOT, 1, (ID3D11ShaderResourceView**)nulls );
	if ( material->textureIDs[TEXTURE_GENERIC_ALPHA] )
		ctx->deviceContext->PSSetShaderResources( HLSL_GENERIC_ALPHA_SLOT, 1, (ID3D11ShaderResourceView**)nulls );
}

static void Material_Create( Material_s* material, MaterialDraw_f drawFunction )
{
	material->drawFunction = drawFunction;
	memset( material->textureIDs, 0xFF, sizeof( material->textureIDs ) );
}

static void Material_SetTexture( Material_s* material, core::u32 textureID, core::u32 textureSlot )
{
	V6_ASSERT( textureSlot < ENTITY_TEXTURE_MAX_COUNT );
	material->textureIDs[textureSlot] = textureID;
}

static void Entity_Create( Entity_s* entity, core::u32 materialID, core::u32 meshID, const core::Vec3& pos, float scale )
{
	entity->materialID = materialID;
	entity->meshID = meshID;
	entity->pos = pos;
	entity->scale = scale;
	entity->visible = true;
}

static void Entity_Draw( Entity_s* entity, Scene_s* scene, GPUContext_s* ctx, const RenderingView_s* view, const RenderingSettings_s* settings )
{
	if ( !entity->visible )
		return;

	Material_s* material = &scene->materials[entity->materialID];
	material->drawFunction( material, entity, scene, ctx, view, settings );
}

static void Entity_SetVisible( Entity_s* entity, bool visible )
{
	entity->visible = visible;
}

static void Entity_SetPos( Entity_s* entity, const core::Vec3* pos )
{
	entity->pos = *pos;
}

static void Entity_SetScale( Entity_s* entity, float scale )
{
	entity->scale = scale;
}

void Scene_Create( Scene_s* scene )
{
	memset( scene, 0, sizeof( *scene) );
}

void Scene_SetFilename( Scene_s* scene, const char* filename )
{
	strcpy_s( scene->filename, sizeof( scene->filename ), filename );
}

void Scene_SetInfo( Scene_s* scene, const SceneInfo_s* sceneInfo )
{
	memcpy( &scene->info, sceneInfo, sizeof( scene->info ) );
}

void Scene_SaveInfo( Scene_s* scene )
{
	if ( !scene->filename[0] )
		return;

	V6_ASSERT( !core::FilePath_HasExtension( scene->filename, "info" ) );
	char fileinfo[256];
	core::FilePath_ChangeExtension( fileinfo, sizeof( fileinfo ), scene->filename, "info" );

	SceneInfo_WriteToFile( &scene->info, fileinfo );
}

void Scene_MakeStreamFilename( const Scene_s* scene, char* path, core::u32 maxPathSize, core::u32 frame  )
{
	V6_ASSERT( scene->filename[0] );

	char filepath[256];
	core::FilePath_ExtractPath( filepath, sizeof( filepath ), scene->filename );

	char filename[256];
	core::FilePath_ExtractFilename( filename, sizeof( filename ), scene->filename );

	char filenameWithoutExtension[256];
	core::FilePath_TrimExtension( filenameWithoutExtension, sizeof( filenameWithoutExtension ), filename );

	core::FilePath_Make( path, maxPathSize, filepath, filenameWithoutExtension );
	sprintf_s( path, maxPathSize, "%s_%06u.v6f", path, frame );
}

void Scene_Release( Scene_s* scene )
{
	for ( core::u32 meshID = 0; meshID < scene->meshCount; ++meshID )
		Mesh_Release( &scene->meshes[meshID] );
	for ( core::u32 textureID = 0; textureID < scene->textureCount; ++textureID )
		GPUTexture_Release( &scene->textures[textureID] );
	
	scene->meshCount = 0;
	scene->textureCount = 0;
	scene->materialCount = 0;
	scene->entityCount = 0;
}

struct SceneContext_s
{
	char				filename[256];
	v6::core::IStack*	allocator;
	ObjScene_s			objScene;
	Scene_s*			scene;
	ID3D11Device*		device;
	v6::core::Signal_s	deviceReady;
	v6::core::Signal_s	loadDone;
};

static void SceneContext_Create( SceneContext_s* sceneContext, v6::core::IStack* allocator )
{
	allocator->push();

	memset( sceneContext, 0, sizeof( SceneContext_s ) );
	sceneContext->allocator = allocator;
	core::Signal_Create( &sceneContext->deviceReady );
	core::Signal_Create( &sceneContext->loadDone );
}

static void SceneContext_SetFilename( SceneContext_s* sceneContext, const char* filename )
{
	strcpy_s( sceneContext->filename, sizeof( sceneContext->filename ), filename );
}

static void SceneContext_Release( SceneContext_s* sceneContext )
{
	if ( sceneContext->scene )
		Scene_Release( sceneContext->scene );
	core::Signal_Release( &sceneContext->deviceReady );
	core::Signal_Release( &sceneContext->loadDone );

	sceneContext->allocator->pop();
}

static void SceneContext_SetDevice( SceneContext_s* sceneContext, ID3D11Device* device )
{
	sceneContext->device = device;
	Signal_Emit( &sceneContext->deviceReady );
}

static void SceneContext_Load( SceneContext_s* sceneContext )
{
	V6_MSG( "Load scene\n" );

	V6_ASSERT( !core::FilePath_HasExtension( sceneContext->filename, "info" ) );
	char fileinfo[256];
	core::FilePath_ChangeExtension( fileinfo, sizeof( fileinfo ), sceneContext->filename, "info" );
	
	SceneInfo_s info;
	if ( !SceneInfo_ReadFromFile( &info, fileinfo ) )
		V6_WARNING( "Unable to read info file\n" );
	
	if ( !Obj_ReadObjectFile( &sceneContext->objScene, sceneContext->filename, sceneContext->allocator ) )
	{
		sceneContext->objScene.meshCount = 0;
		V6_ERROR( "Unable to load %s\n", sceneContext->filename );
		core::Signal_Emit( &sceneContext->loadDone );
		return;
	}
	
	V6_MSG( "%d meshes loaded\n",  sceneContext->objScene.meshCount );
	
	core::Signal_Wait( &sceneContext->deviceReady );

	V6_MSG( "Init scene\n" );

	ObjScene_s* objScene = &sceneContext->objScene;
	Scene_s* scene = sceneContext->allocator->newInstance< Scene_s >();
	Scene_Create( scene );
	Scene_SetFilename( scene, sceneContext->filename );
	Scene_SetInfo( scene, &info );

	for ( core::u32 materialID = 0; materialID < objScene->materialCount; ++materialID )
	{
		ObjMaterial_s* objMaterial = &objScene->materials[materialID];
		Material_s* material = &scene->materials[materialID];

		Material_Create( material, Material_DrawGeneric );
		++scene->materialCount;

		sceneContext->allocator->push();

		material->diffuse = objMaterial->kd;
		
		const char* textureFilenames[TEXTURE_GENERIC_COUNT];
		textureFilenames[TEXTURE_GENERIC_DIFFUSE] = objMaterial->mapKd;
		textureFilenames[TEXTURE_GENERIC_ALPHA] = objMaterial->mapD;
		textureFilenames[TEXTURE_GENERIC_NORMAL] = objMaterial->mapBump;

		for ( core::u32 textureSlot = 0; textureSlot < TEXTURE_GENERIC_COUNT; ++textureSlot )
		{
			const char* textureFilename = textureFilenames[textureSlot];
			if ( !*textureFilename )
				continue;

			core::Image_s image = {};
			core::CFileReader fileReader;
			if ( core::FilePath_HasExtension( textureFilename, "tga" ) && fileReader.Open( textureFilename ) && core::Image_ReadTga( &image, &fileReader, sceneContext->allocator ) )
			{
				static const char* textureNames[TEXTURE_GENERIC_COUNT] = { "diffuse", "alpha", "bump" };
				const core::u32 textureID = scene->textureCount;
				Texture2D_Create( sceneContext->device, &scene->textures[scene->textureCount], image.width, image.height, image.pixels, true, textureNames[textureSlot] );
				++scene->textureCount;

				Material_SetTexture( material, textureID, textureSlot );
			}
			else
				V6_WARNING( "Unable to load %s for material %s\n", textureFilename, objMaterial->name );
		}

		sceneContext->allocator->pop();
	}

	float maxDim = 0.0f;

	for ( core::u32 meshID = 0; meshID < objScene->meshCount; ++meshID )
	{
		V6_ASSERT( meshID < MESH_MAX_COUNT );
		V6_ASSERT( meshID < ENTITY_MAX_COUNT );

		sceneContext->allocator->push();

		ObjMesh_s* mesh = &objScene->meshes[meshID];
		
		GenericVertex_s* vertices = sceneContext->allocator->newArray< GenericVertex_s >( mesh->triangleCount * 3 );
		
		ObjTriangle_s* triangle = &objScene->triangles[mesh->firstTriangleID];
		GenericVertex_s* vertex = vertices;
		for ( core::u32 triangleID = 0; triangleID < mesh->triangleCount; ++triangleID, ++triangle, vertex += 3 )
		{
			vertex[0].position = objScene->positions[triangle->vertices[0].posID] * info.worldUnitToCM;
			vertex[1].position = objScene->positions[triangle->vertices[1].posID] * info.worldUnitToCM;
			vertex[2].position = objScene->positions[triangle->vertices[2].posID] * info.worldUnitToCM;

			for ( core::u32 vertexID = 0; vertexID < 3; ++vertexID )
			{
				maxDim = core::Max( maxDim, vertex[vertexID].position.x );
				maxDim = core::Max( maxDim, vertex[vertexID].position.y );
				maxDim = core::Max( maxDim, vertex[vertexID].position.z );
			}

			if ( objScene->normals && triangle->vertices[0].normalID != (core::u32)-1 )
			{
				vertex[0].normal = objScene->normals[triangle->vertices[0].normalID];
				vertex[1].normal = objScene->normals[triangle->vertices[1].normalID];
				vertex[2].normal = objScene->normals[triangle->vertices[2].normalID];
			}
			else
			{
				const core::Vec3 edge1 = vertex[1].position - vertex[0].position;
				const core::Vec3 edge2 = vertex[2].position - vertex[0].position;
				const core::Vec3 normal = core::Cross( edge1, edge2 ).Normalized();
				vertex[0].normal = normal;
				vertex[1].normal = normal;
				vertex[2].normal = normal;
			}

			if ( objScene->uvs && triangle->vertices[0].uvID != (core::u32)-1 )
			{
				vertex[0].uv = objScene->uvs[triangle->vertices[0].uvID];
				vertex[1].uv = objScene->uvs[triangle->vertices[1].uvID];
				vertex[2].uv = objScene->uvs[triangle->vertices[2].uvID];
			}
			else
			{
				vertex[0].uv = core::Vec2_Make( 0.0f, 0.0f );
				vertex[1].uv = core::Vec2_Make( 0.0f, 0.0f );
				vertex[2].uv = core::Vec2_Make( 0.0f, 0.0f );
			}
		}	

		Mesh_Create( sceneContext->device, &scene->meshes[meshID], vertices, mesh->triangleCount * 3, sizeof( GenericVertex_s ), VERTEX_FORMAT_POSITION | VERTEX_FORMAT_USER0_F3 | VERTEX_FORMAT_USER1_F2, nullptr, 0, 0, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST );
		++scene->meshCount;

		Entity_Create( &scene->entities[meshID], mesh->materialID, meshID, core::Vec3_Make( 0.0f, 0.0f, 0.0f ), 1.0f );
		++scene->entityCount;

		sceneContext->allocator->pop();
	}	

	const core::u32 materialID = scene->materialCount;
	Material_Create( &scene->materials[materialID], Material_DrawBasic );
	++scene->materialCount;

	for ( core::u32 boxID = 0; boxID < 3; ++boxID )
	{
		core::u32 meshID = scene->meshCount;
	
		V6_ASSERT( meshID < MESH_MAX_COUNT );
		V6_ASSERT( meshID < ENTITY_MAX_COUNT );

		switch ( boxID )
		{
		case 0:
			{
				Mesh_CreateBox( sceneContext->device, &scene->meshes[meshID], core::Color_Make( 127, 127, 127, 255 ), true );
				++scene->meshCount;
		
				Entity_Create( &scene->entities[meshID], materialID, meshID, core::Vec3_Make( 0.0f, 0.0f, 0.0f), maxDim );
				++scene->entityCount;
			}
			break;
		case 1:
			{
				Mesh_CreateBox( sceneContext->device, &scene->meshes[meshID], core::Color_Make( 255, 0, 0, 255 ), true );
				++scene->meshCount;
		
				Entity_Create( &scene->entities[meshID], materialID, meshID, core::Vec3_Make( 0.0f, 0.0f, 0.0f), GRID_MIN_SCALE );
				++scene->entityCount;
			}
			break;
		case 2:
			{
				Mesh_CreateBox( sceneContext->device, &scene->meshes[meshID], core::Color_Make( 0, 0, 255, 255 ), true );
				++scene->meshCount;
		
				Entity_Create( &scene->entities[meshID], materialID, meshID, core::Vec3_Make( 0.0f, 0.0f, 0.0f), GRID_MAX_SCALE );
				++scene->entityCount;
			}
			break;
		}
	}

	V6_MSG( "%d entities created\n", scene->entityCount );

	GPUResource_LogMemoryUsage();

	sceneContext->scene = scene;
	s_activeScene = scene;

	CameraPath_Load( &s_cameraPath, &info );
	s_yaw = core::DegToRad( info.cameraYaw );
	if ( s_cameraPath.keyCount )
		s_headOffset = s_cameraPath.positions[0];

	core::Signal_Emit( &sceneContext->loadDone );
}

void Scene_CreateDebug( SceneDebug_s* scene, ID3D11Device* device )
{
	Scene_Create( scene );

	scene->meshLineID = scene->meshCount;
	Mesh_CreateLine( device, &scene->meshes[scene->meshCount++], core::Color_Make( 255, 255, 255, 255 ) );
	
	scene->meshGridID = scene->meshCount;
	Mesh_CreateBox( device, &scene->meshes[scene->meshCount++], core::Color_Make( 255, 255, 255, 255 ), true );
		
	Material_Create( &scene->materials[scene->materialCount++], Material_DrawBasic );
	
	core::u32 cellID = 0;
	for ( int z = 0; z < HLSL_CELL_SUPER_SAMPLING_WIDTH; ++z )
	{
		for ( int y = 0; y < HLSL_CELL_SUPER_SAMPLING_WIDTH; ++y )
		{
			for ( int x = 0; x < HLSL_CELL_SUPER_SAMPLING_WIDTH; ++x, ++cellID )
			{						
				for ( core::u32 cellType = 0; cellType < 2; ++cellType )
				{
					scene->meshCellIDs[cellID][cellType] = scene->meshCount;
					Mesh_CreateBox( device, &scene->meshes[scene->meshCount++], core::Color_Make( 0, (cellType == 0) * 255, (cellType == 1) * 255, 255 ), cellType == 0 );

					scene->entityCellIDs[cellID][cellType] = scene->entityCount;
					Entity_s* cellEntity = &scene->entities[scene->entityCount++];
					Entity_Create( cellEntity, 0, scene->meshCellIDs[cellID][cellType], core::Vec3_Zero(), 1.0f );
					Entity_SetVisible( cellEntity, false );
				}
			}
		}
	}

	scene->entityGridID = scene->entityCount;
	Entity_Create( &scene->entities[scene->entityGridID], 0, scene->meshGridID, core::Vec3_Zero(), 1.0f );
	Entity_SetVisible( &scene->entities[scene->entityGridID], false );
	++scene->entityCount;

	scene->entityLineID = scene->entityCount;
	Entity_Create( &scene->entities[scene->entityLineID], 0, scene->meshLineID, core::Vec3_Zero(), 1.0f );
	Entity_SetVisible( &scene->entities[scene->entityLineID], false );
	++scene->entityCount;

	for ( int debugBlockID = 0; debugBlockID < DEBUG_BLOCK_MAX_COUNT; ++debugBlockID )
	{
		for ( core::u32 cellType = 0; cellType < 2; ++cellType )
		{
			core::u32 hashColor = 1 + (debugBlockID%7);
			scene->meshBlockIDs[debugBlockID][cellType] = scene->meshCount;
			Mesh_CreateBox( device, &scene->meshes[scene->meshCount++], core::Color_Make( (hashColor & 1) ? 255 : 0, (hashColor & 2) ? 255 : 0, (hashColor & 4) ? 255 : 0, 255 ), cellType == 0 );

			scene->entityBlockIDs[debugBlockID][cellType] = scene->entityCount;
			Entity_s* blockEntity = &scene->entities[scene->entityCount++];
			Entity_Create( blockEntity, 0, scene->meshBlockIDs[debugBlockID][cellType], core::Vec3_Zero(), 1.0f );
			Entity_SetVisible( blockEntity, false );
		}

		const core::u32 meshTraceID = scene->meshCount;
		Mesh_CreateLine( device, &scene->meshes[scene->meshCount++], core::Color_Make( 255, 255, 255, 255 ) );

		scene->entityTraceIDs[debugBlockID] = scene->entityCount;
		Entity_s* traceEntity = &scene->entities[scene->entityCount++];
		Entity_Create( traceEntity, 0, meshTraceID, core::Vec3_Zero(), 1.0f );
		Entity_SetVisible( traceEntity, false );
	}
}

void Scene_CreateCameraPath( SceneCameraPath_s* scene, ID3D11Device* device )
{
	Scene_Create( scene );

	for ( core::u32 lineRank = 0; lineRank < CameraPath_s::MAX_POINT_COUNT-1; ++lineRank )
	{
		scene->meshLineIDs[lineRank] = scene->meshCount;
		Mesh_CreateLine( device, &scene->meshes[scene->meshCount++], core::Color_Make( 128, 128, 128, 255 ) );
	}

	scene->meshBoxID = scene->meshCount;
	Mesh_CreateBox( device, &scene->meshes[scene->meshCount++], core::Color_Make( 255, 255, 255, 255 ), true );

	scene->meshSelectedBoxID = scene->meshCount;
	Mesh_CreateBox( device, &scene->meshes[scene->meshCount++], core::Color_Make( 255, 0, 0, 255 ), true );

	Material_Create( &scene->materials[scene->materialCount++], Material_DrawBasic );
}

void Scene_UpdateCameraPath( SceneCameraPath_s* scene, const CameraPath_s* cameraPath, ID3D11DeviceContext* context )
{
	scene->entityCount = 0;
	
	for ( core::u32 key = 0; key < (core::u32)cameraPath->keyCount; ++key )
	{
		const bool isSelected = key == s_cameraPath.activeKey;
		Entity_Create( &scene->entities[scene->entityCount++], 0, isSelected ? scene->meshSelectedBoxID : scene->meshBoxID, cameraPath->positions[key], 5.0f );
	}

	int lineRank;
	for ( lineRank = 0; lineRank < cameraPath->keyCount-1; ++lineRank )
	{		
		const core::u32 meshLineID = scene->meshLineIDs[lineRank];
		const BasicVertex_s vertices[2] = 
		{
			{ cameraPath->positions[lineRank], core::Color_Make( 128, 128, 128, 255 ) },
			{ cameraPath->positions[lineRank+1], core::Color_Make( 128, 128, 128, 255 ) },
		};
		Mesh_UpdateVertices( context, &scene->meshes[meshLineID], vertices );
		Entity_Create( &scene->entities[scene->entityCount++], 0, meshLineID, core::Vec3_Zero(), 1.0f );
	}
}

#if V6_SIMPLE_SCENE == 1

void Scene_CreateDefault( Scene_s* scene, ID3D11Device* device )
{
	Scene_Create( scene );

	Mesh_CreateQuad( device, &scene->meshes[MESH_QUAD_RED0], core::Color_Make( 255, 0, 0, 255 ) );
	Mesh_CreateQuad( device, &scene->meshes[MESH_QUAD_RED1], core::Color_Make( 128, 0, 0, 255 ) );
	Mesh_CreateQuad( device, &scene->meshes[MESH_QUAD_RED2], core::Color_Make(  64, 0, 0, 255 ) );
	Mesh_CreateBox( device, &scene->meshes[MESH_BOX_RED], core::Color_Make( 255, 0, 0, 255 ), false );
	Mesh_CreateBox( device, &scene->meshes[MESH_BOX_GREEN], core::Color_Make( 0, 255, 0, 255 ), false );
	Mesh_CreateBox( device, &scene->meshes[MESH_BOX_BLUE], core::Color_Make( 0, 0, 255, 255 ), false );
	Mesh_CreatePoint( device, &scene->meshes[MESH_POINT] );
	Mesh_CreateLine( device, &scene->meshes[MESH_LINE], core::Color_Make( 255, 255, 255, 255 ) );
	Mesh_CreateVirtualBox( device, &scene->meshes[MESH_VIRTUAL_BOX] );

	Material_Create( &scene->materials[MATERIAL_DEFAULT_BASIC], Material_DrawBasic );
	
	const core::u32 screenWidth = HLSL_GRID_WIDTH >> 1;
	//const float depth = -99.0001f;
	const float depth = -100.0001f;
	const float pixelRadius = 0.5f * (200.0f / screenWidth);
	Entity_Create( &scene->entities[scene->entityCount++], MATERIAL_DEFAULT_BASIC, MESH_QUAD_RED0, core::Vec3_Make( 0, 0, depth ), pixelRadius * 16 );
	//Entity_Create( &scene->entities[scene->entityCount++], MATERIAL_DEFAULT_BASIC, MESH_QUAD_RED1, core::Vec3_Make( 0, 0, depth ), pixelRadius * 3 );
	//Entity_Create( &scene->entities[scene->entityCount++], MATERIAL_DEFAULT_BASIC, MESH_QUAD_RED2, core::Vec3_Make( 0, 0, depth ), pixelRadius * 5 );
}

#else

void Scene_CreateDefault( Scene_s* scene, ID3D11Device* device )
{
	Scene_Create( scene );

	Mesh_CreateTriangle( device, &scene->meshes[MESH_TRIANGLE] );	
	Mesh_CreateBox( device, &scene->meshes[MESH_BOX_WIREFRAME], core::Color_Make( 255, 255, 255, 255 ), true );
	Mesh_CreateBox( device, &scene->meshes[MESH_BOX_RED], core::Color_Make( 255, 0, 0, 255 ), false );
	Mesh_CreateBox( device, &scene->meshes[MESH_BOX_GREEN], core::Color_Make( 0, 255, 0, 255 ), false );
	Mesh_CreateBox( device, &scene->meshes[MESH_BOX_BLUE], core::Color_Make( 0, 0, 255, 255 ), false );
	Mesh_CreateVirtualQuad( device, &scene->meshes[MESH_VIRTUAL_QUAD] );
	Mesh_CreateFakeCube( device, &scene->meshes[MESH_FAKE_CUBE] );
	Mesh_CreatePoint( device, &scene->meshes[MESH_POINT] );
	Mesh_CreateLine( device, &scene->meshes[MESH_LINE],  core::Color_Make( 255, 255, 255, 255 ) );
	Mesh_CreateVirtualBox( device, &scene->meshes[MESH_VIRTUAL_BOX] );

	Material_Create( &scene->materials[MATERIAL_DEFAULT_BASIC], Material_DrawBasic );
	Material_Create( &scene->materials[MATERIAL_DEFAULT_FAKE_CUBE], Material_DrawFakeCube );
		
	Entity_Create( &scene->entities[scene->entityCount++], MATERIAL_DEFAULT_BASIC, MESH_BOX_WIREFRAME, core::Vec3_Make( 0.0f, 0.0f, 0.0f), GRID_MAX_SCALE );
	Entity_Create( &scene->entities[scene->entityCount++], MATERIAL_DEFAULT_BASIC, MESH_TRIANGLE, core::Vec3_Make( 0.0f, 0.0f, -GRID_MAX_SCALE ), 5.0f );
	for ( core::u32 randomCubeID = 0; randomCubeID < RANDOM_CUBE_COUNT;  )
	{
		const core::Vec3 center = core::Vec3_Rand() * (GRID_MAX_SCALE - FREE_SCALE);
		const float size = 1.0f + 74.0f * rand() / RAND_MAX;
		if ( 
			center.x - size < FREE_SCALE && center.x + size > -FREE_SCALE &&
			center.y - size < FREE_SCALE && center.y + size > -FREE_SCALE &&
			center.z - size < FREE_SCALE && center.z + size > -FREE_SCALE ) continue;
		if ( fabsf( center.x ) + size > GRID_MAX_SCALE ) continue;
		if ( fabsf( center.y ) + size > GRID_MAX_SCALE ) continue;
		if ( fabsf( center.z ) + size > GRID_MAX_SCALE ) continue;

#if 0
		Entity_Create( &scene->entities[scene->entityCount++], MATERIAL_DEFAULT_BASIC, MESH_BOX_RED + (randomCubeID % 3), center, size );
#else
		Entity_Create( &scene->entities[scene->entityCount++], MATERIAL_DEFAULT_FAKE_CUBE, MESH_FAKE_CUBE, center, size );
#endif
		Entity_Create( &scene->entities[scene->entityCount++], MATERIAL_DEFAULT_BASIC, MESH_BOX_WIREFRAME, center, size );
		
		++randomCubeID;
	}	
}

#endif

core::u32 Grid_Trace( const core::Vec3* gridCenter, float gridScale, core::u32 gridOccupancy, const core::Vec3* rayOrg, const core::Vec3* rayDir, TraceData_s* traceData )
{
	const core::Vec3 invDir = rayDir->Rcp();
	const core::Vec3 alpha = (*gridCenter - *rayOrg) * invDir;
	const core::Vec3 beta = gridScale * invDir;	
	const core::Vec3 t0 = alpha + beta;
	const core::Vec3 t1 = alpha - beta;
	const core::Vec3 tMin = core::Min( t0, t1 );
	const core::Vec3 tMax = core::Max( t0, t1 );
	const float tIn = core::Max( core::Max( tMin.x, tMin.y ), tMin.z );
	const float tOut = core::Min( core::Min( tMax.x, tMax.y ), tMax.z );

	if ( traceData )
	{
		traceData->tIn = tIn;
		traceData->tOut = tOut;
		traceData->hitFoundCoords = core::Vec3i_Zero();
		traceData->hitFailBits = 0;
	}
		
	if ( tIn > tOut )
		return 0;

	const float cellSize = (gridScale * 2.0f) / HLSL_CELL_SUPER_SAMPLING_WIDTH;
	const float scale = 1.0f / cellSize;
	const float offset = HLSL_CELL_SUPER_SAMPLING_WIDTH * 0.5f;
	
	const core::Vec3 tDelta = cellSize * invDir.Abs();	

	const core::Vec3 pIn = *rayOrg + tIn * *rayDir;
	const core::Vec3 coordIn = (pIn - *gridCenter) * scale + offset;
		
	const int x = min( (int)coordIn.x, HLSL_CELL_SUPER_SAMPLING_WIDTH-1 );
	const int y = min( (int)coordIn.y, HLSL_CELL_SUPER_SAMPLING_WIDTH-1 );
	const int z = min( (int)coordIn.z, HLSL_CELL_SUPER_SAMPLING_WIDTH-1 );
	core::Vec3i coords = core::Vec3i_Make( x, y, z );
	const core::Vec3i step = core::Vec3i_Make( rayDir->x < 0.0f ? -1 : 1, rayDir->y < 0.0f ? -1 : 1, rayDir->z < 0.0f ? -1 : 1 );
	core::Vec3 tCur = tMin;
	for ( core::u32 pass = 0; pass < 2; ++pass )
	{
		const core::Vec3 tNext = tCur + tDelta;
		tCur.x = tNext.x < tIn ? tNext.x : tCur.x;
		tCur.y = tNext.y < tIn ? tNext.y : tCur.y;
		tCur.z = tNext.z < tIn ? tNext.z : tCur.z;
	}
	
	core::u32 hitFound = 0;

	for ( ;; )
	{
		const core::u32 cellID = coords.z * HLSL_CELL_SUPER_SAMPLING_WIDTH_SQ + coords.y * HLSL_CELL_SUPER_SAMPLING_WIDTH + coords.x;
		const core::u32 occupancyBit = 1 << cellID;
		hitFound |= (gridOccupancy & occupancyBit);
		if ( hitFound )
		{
			if ( traceData)
				traceData->hitFoundCoords = coords;
			break;
		}

		if ( traceData)
			traceData->hitFailBits |= occupancyBit;

		const core::Vec3 tNext = tCur + tDelta;
		core::u32 nextAxis;
		if ( tNext.x < tNext.y )
			nextAxis = tNext.x < tNext.z ? 0 : 2;
		else
			nextAxis = tNext.y < tNext.z ? 1 : 2;
		
		tCur[nextAxis] = tNext[nextAxis];
		coords[nextAxis] += step[nextAxis];
		
		if ( coords[nextAxis] < 0 || coords[nextAxis] >= HLSL_CELL_SUPER_SAMPLING_WIDTH )
			break;
	}

	return hitFound;
}

void Block_TraceDisplay( GPUContext_s* gpuContext, SceneDebug_s* scene, const core::Vec3* gridCenter, float gridScale, core::u32 gridOccupancy, const core::Vec3* rayOrg, const core::Vec3* rayEnd )
{
	s_gridCenter = *gridCenter;
	s_gridScale = gridScale;
	s_gridOccupancy = gridOccupancy;
	s_rayOrg = *rayOrg;
	s_rayEnd = *rayEnd;

	Entity_SetPos( &scene->entities[scene->entityGridID], gridCenter );
	Entity_SetScale( &scene->entities[scene->entityGridID], gridScale );
	Entity_SetVisible( &scene->entities[scene->entityGridID], true );

	const core::Vec3 rayDir = (*rayEnd - *rayOrg).Normalized();
	
	BasicVertex_s vertices[2];
	vertices[0].position = *rayOrg;
	vertices[0].color = core::Color_Make( 255, 0, 0, 255 );	
	vertices[1].position = *rayOrg + 1000000.0f * rayDir;
	vertices[1].color = core::Color_Make( 0, 255, 0, 255 );
	Mesh_UpdateVertices( gpuContext->deviceContext, &scene->meshes[scene->meshLineID], vertices );
	Entity_SetVisible( &scene->entities[scene->entityLineID], true );
	
	const float cellSize = (gridScale * 2.0f) / HLSL_CELL_SUPER_SAMPLING_WIDTH;
	const core::Vec3 cellOrg = *gridCenter - gridScale + cellSize * 0.5f;
	
	core::u32 cellID = 0;
	for ( int z = 0; z < HLSL_CELL_SUPER_SAMPLING_WIDTH; ++z )
	{
		for ( int y = 0; y < HLSL_CELL_SUPER_SAMPLING_WIDTH; ++y )
		{
			for ( int x = 0; x < HLSL_CELL_SUPER_SAMPLING_WIDTH; ++x, ++cellID )
			{
				const core::Vec3 cellCenter = cellOrg + core::Vec3_Make( (float)x, (float)y, (float)z ) * cellSize;
				
				Entity_s* cellEntity = &scene->entities[scene->entityCellIDs[cellID][0]];
				Entity_SetVisible( cellEntity, (gridOccupancy & (1 << cellID)) != 0 );
				Entity_SetPos( cellEntity, &cellCenter );
				Entity_SetScale( cellEntity, cellSize * 0.5f );
				
				cellEntity = &scene->entities[scene->entityCellIDs[cellID][1]];
				Entity_SetVisible( cellEntity, false );
				Entity_SetPos( cellEntity, &cellCenter );
				Entity_SetScale( cellEntity, cellSize * 0.5f );
			}
		}
	}
	
	const core::u32 hitFound = Grid_Trace( gridCenter, gridScale, gridOccupancy, rayOrg, &rayDir, nullptr );

	for ( core::u32 cellID = 0; cellID < HLSL_CELL_SUPER_SAMPLING_WIDTH_CUBE; ++cellID )
		Entity_SetVisible( &scene->entities[scene->entityCellIDs[cellID][1]], (hitFound & (1 << cellID)) != 0 );
}

static core::Vec3 Block_ComputeGridCenter( const core::Vec3* pos, float gridScale )
{
	const float blockSize = gridScale / HLSL_GRID_BLOCK_HALF_WIDTH;
	const core::Vec3 normalizedPos = *pos * (1.0f / blockSize);
	return core::Vec3_Make( floorf( normalizedPos.x ), floorf( normalizedPos.y ), floorf( normalizedPos.z ) ) * blockSize;
}

class CRenderingDevice
{
public:
	CRenderingDevice();
	~CRenderingDevice();

public:
	void BlendPixel( const RenderingView_s* view );
	void BuildBlock( core::u32 frame );
	core::u32 BuildNode();
	void Capture( const core::Vec3* sampleOffset, core::u32 faceID );
	void ClearNode();
	void Collect( const core::Vec3* sampleOffset, core::u32 faceID );
	bool Create(int nWidth, int nHeight, HWND hWnd, core::CFileSystem* fileSystem, core::IAllocator* heap, core::IStack* stack );
	void CullBlock( const RenderingView_s* views, const core::Vec3* buildOrigin );
	void Draw( float dt );
	void DrawCameraPath( const RenderingView_s* view );
	void DrawDebug( const RenderingView_s* view );
	void DrawScene( Scene_s* scene, const RenderingView_s* view, const RenderingSettings_s* settings );
	void DrawWorld( const RenderingView_s* view );	
	void FillLeaf();
	void MakeRenderingView( RenderingView_s* view, const core::Vec3* org, const core::Vec3* forward, const core::Vec3* up, const core::Vec3* right, core::u32 eye );
	void Output( ID3D11ShaderResourceView* srvLeft, ID3D11ShaderResourceView* srvRight );
	void PackColor( core::u32 blockCounts[HLSL_BUCKET_COUNT] );
	void Present();
	bool ReadStream( core::u32 frame);
	void Release();
	void TraceBlock( const RenderingView_s* views, const core::Vec3* buildOrigin );
	bool WriteStream( const core::u32 blockCounts[HLSL_BUCKET_COUNT], core::u32 frame );

	GPUContext_s		gpuContext;
		
	Config_s			m_config;
	
	core::Vec3			m_sampleOffsets[SAMPLE_MAX_COUNT];
	
	Cube_s				m_cube;
	Sample_s			m_sample;
	Octree_s			m_octree;
	Block_s				m_block;
	Trace_s				m_trace;
	bool				m_blockModeInitialized;

	Scene_s*			m_defaultScene;
	SceneDebug_s*		m_debugScene;
	SceneCameraPath_s*	m_cameraPathScene;

	core::IAllocator*	m_heap;
	core::IStack*		m_stack;

	core::u32			m_width;
	core::u32			m_height;
	float				m_aspectRatio;
};

CRenderingDevice::CRenderingDevice()
{
	memset( this, 0, sizeof( CRenderingDevice ) );
}

CRenderingDevice::~CRenderingDevice()
{
}

bool CRenderingDevice::Create( int nWidth, int nHeight, HWND hWnd, core::CFileSystem* fileSystem, core::IAllocator* heap, core::IStack* stack )
{
	m_heap = heap;
	m_stack = stack;
	core::ScopedStack scopedStack( stack );

	m_width = nWidth;
	m_height = nHeight;
	m_aspectRatio = (float)nWidth / nHeight;	

	GPUContext_Create( &gpuContext, nWidth, nHeight, hWnd, fileSystem, heap, stack );

	m_defaultScene = heap->newInstance< Scene_s >();
	Scene_CreateDefault( m_defaultScene, gpuContext.device );
	s_activeScene = m_defaultScene;

	m_debugScene = heap->newInstance< SceneDebug_s >();
	Scene_CreateDebug( m_debugScene, gpuContext.device );

	m_cameraPathScene = heap->newInstance< SceneCameraPath_s >();
	Scene_CreateCameraPath( m_cameraPathScene, gpuContext.device );

	CameraPath_Init( &s_cameraPath );

	Config_Init( &m_config, m_width, m_height, CUBE_SIZE, HLSL_GRID_WIDTH );
	m_blockModeInitialized = false;

	g_sample = 0;
	m_sampleOffsets[0] = core::Vec3_Make( 0.0f, 0.0f, 0.0f );
	for ( core::u32 sample = 1; sample < SAMPLE_MAX_COUNT; ++sample )
	{
		if ( sample <= 8 )
		{
			core::u32 vertexID = sample-1;
			m_sampleOffsets[sample].x = (vertexID&1) == 0 ? -FREE_SCALE : FREE_SCALE;
			m_sampleOffsets[sample].y = (vertexID&2) == 0 ? -FREE_SCALE : FREE_SCALE;
			m_sampleOffsets[sample].z = (vertexID&4) == 0 ? -FREE_SCALE : FREE_SCALE;
		}
		else
		{
			m_sampleOffsets[sample] = core::Vec3_Rand() * FREE_SCALE;
		}
	}

	GPUResource_LogMemoryUsage();

	Config_Log( &m_config );
	
	return true;
}

void CRenderingDevice::DrawScene( Scene_s* scene, const RenderingView_s* view, const RenderingSettings_s* settings )
{
	for ( core::u32 entityRank = 0; entityRank < scene->entityCount; ++entityRank )
	{
		Entity_s* entity = &scene->entities[entityRank];
		Entity_Draw( entity, scene, &gpuContext, view, settings );		
	}
}

void CRenderingDevice::DrawWorld( const RenderingView_s* view )
{	
	// Rasterization state
	gpuContext.deviceContext->OMSetDepthStencilState( gpuContext.depthStencilStateZRW, 0 );
#if V6_USE_ALPHA_COVERAGE == 1
	gpuContext.deviceContext->OMSetBlendState( gpuContext.blendStateAlphaCoverage, nullptr, 0XFFFFFFFF );	
#else
	gpuContext.deviceContext->OMSetBlendState( gpuContext.blendStateOpaque, nullptr, 0XFFFFFFFF );	
#endif
		
	// Viewport
	{
		D3D11_VIEWPORT viewport = {};
		viewport.TopLeftX = 0;
		viewport.TopLeftY = 0;
		viewport.Width = (float)m_width;
		viewport.Height = (float)m_height;
		viewport.MinDepth = 0.0f;
		viewport.MaxDepth = 1.0f;

		gpuContext.deviceContext->RSSetViewports( 1, &viewport );
		gpuContext.deviceContext->RSSetState( gpuContext.rasterState );
	}
	
	// RT
	if ( g_useMSAA )
		gpuContext.deviceContext->OMSetRenderTargets( 1, &gpuContext.colorViewMSAA, gpuContext.depthStencilViewMSAA );
	else
		gpuContext.deviceContext->OMSetRenderTargets( 1, &view->rtv, gpuContext.depthStencilView );

	// Clear
	float const pRGBA[] = { 0.0f, 0.0f, 0.0f, 0.0f };
	if ( g_useMSAA )
	{
		gpuContext.deviceContext->ClearRenderTargetView( gpuContext.colorViewMSAA, pRGBA );
		gpuContext.deviceContext->ClearDepthStencilView( gpuContext.depthStencilViewMSAA, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0 );
	}
	else
	{
		gpuContext.deviceContext->ClearRenderTargetView( view->rtv, pRGBA );
		gpuContext.deviceContext->ClearDepthStencilView( gpuContext.depthStencilView, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0 );
	}
		
	// Settings
	RenderingSettings_s settings;
#if V6_USE_ALPHA_COVERAGE == 1
	settings.useAlphaCoverage = true;
#else
	settings.useAlphaCoverage = false;
#endif
	settings.isCapturing = false;

	DrawScene( s_activeScene, view, &settings );

	// un RT
	gpuContext.deviceContext->OMSetRenderTargets( 0, nullptr, nullptr );

	if ( g_useMSAA )
		gpuContext.deviceContext->ResolveSubresource( view->texture2D, 0, gpuContext.colorBufferMSAA, 0, DXGI_FORMAT_R8G8B8A8_UNORM );
}

void CRenderingDevice::DrawCameraPath( const RenderingView_s* view )
{
	// Rasterization state
	gpuContext.deviceContext->OMSetDepthStencilState( gpuContext.depthStencilStateZRW, 0 );
	gpuContext.deviceContext->OMSetBlendState( gpuContext.blendStateOpaque, nullptr, 0XFFFFFFFF );

	// Viewport
	{
		D3D11_VIEWPORT viewport = {};
		viewport.TopLeftX = 0;
		viewport.TopLeftY = 0;
		viewport.Width = (float)m_width;
		viewport.Height = (float)m_height;
		viewport.MinDepth = 0.0f;
		viewport.MaxDepth = 1.0f;

		gpuContext.deviceContext->RSSetViewports( 1, &viewport );
		gpuContext.deviceContext->RSSetState( gpuContext.rasterState );
	}

	// RT
	gpuContext.deviceContext->OMSetRenderTargets( 1, &view->rtv, gpuContext.depthStencilView );

	// Settings
	RenderingSettings_s settings;
	settings.useAlphaCoverage = false;
	settings.isCapturing = false;

	DrawScene( m_cameraPathScene, view, &settings );

	// un RT
	gpuContext.deviceContext->OMSetRenderTargets( 0, nullptr, nullptr );
}

void CRenderingDevice::DrawDebug( const RenderingView_s* view )
{	
	if ( g_traceGrid )
	{		
		Block_TraceDisplay( &gpuContext, m_debugScene, &s_gridCenter, s_gridScale, s_gridOccupancy, &s_rayOrg, &s_rayEnd );
		
		g_traceGrid = false;
	}

	// Rasterization state
	gpuContext.deviceContext->OMSetDepthStencilState( g_transparentDebug ? gpuContext.depthStencilStateNoZ : gpuContext.depthStencilStateZRW, 0 );
	gpuContext.deviceContext->OMSetBlendState( gpuContext.blendStateOpaque, nullptr, 0XFFFFFFFF );
		
	// Viewport
	{
		D3D11_VIEWPORT viewport = {};
		viewport.TopLeftX = 0;
		viewport.TopLeftY = 0;
		viewport.Width = (float)m_width;
		viewport.Height = (float)m_height;
		viewport.MinDepth = 0.0f;
		viewport.MaxDepth = 1.0f;

		gpuContext.deviceContext->RSSetViewports( 1, &viewport );
		gpuContext.deviceContext->RSSetState( gpuContext.rasterState );
	}
	
	// RT
	gpuContext.deviceContext->OMSetRenderTargets( 1, &view->rtv, g_transparentDebug ? nullptr : gpuContext.depthStencilView );

	// Clear
	if ( !g_transparentDebug )
		gpuContext.deviceContext->ClearDepthStencilView( gpuContext.depthStencilView, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0 );
				
	// Settings
	RenderingSettings_s settings;
	settings.useAlphaCoverage = false;
	settings.isCapturing = false;

	DrawScene( m_debugScene, view, &settings );

	// un RT
	gpuContext.deviceContext->OMSetRenderTargets( 0, nullptr, nullptr );
}

void CRenderingDevice::Capture( const core::Vec3* samplePos, core::u32 faceID )
{
	// Rasterization state
	gpuContext.deviceContext->OMSetDepthStencilState( gpuContext.depthStencilStateZRW, 0 );
	gpuContext.deviceContext->OMSetBlendState( gpuContext.blendStateOpaque, nullptr, 0XFFFFFFFF );

	// Viewport
	{
		D3D11_VIEWPORT viewport = {};
		viewport.TopLeftX = 0;
		viewport.TopLeftY = 0;
		viewport.Width = (float)m_cube.size;
		viewport.Height = (float)m_cube.size;
		viewport.MinDepth = 0.0f;
		viewport.MaxDepth = 1.0f;

		gpuContext.deviceContext->RSSetViewports( 1, &viewport );
		gpuContext.deviceContext->RSSetState( gpuContext.rasterState );
	}

	gpuContext.userDefinedAnnotation->BeginEvent( L"Capture");
		
	// RT
	gpuContext.deviceContext->OMSetRenderTargets( 1, &m_cube.colorRTV, m_cube.depthRTV );

	// Clear
	float const pRGBA[] = { 0.0f, 0.0f, 0.0f, 0.0f };
	gpuContext.deviceContext->ClearRenderTargetView( m_cube.colorRTV, pRGBA );
	gpuContext.deviceContext->ClearDepthStencilView( m_cube.depthRTV, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0 );						

	// View
	RenderingView_s view;
	Cube_MakeViewMatrix( &view.viewMatrix, *samplePos, (CubeAxis_e)faceID );
	view.projMatrix = core::Mat4x4_Projection( ZNEAR, ZFAR, core::DegToRad( 90.0f ), 1.0f );
		
	// Settings
	RenderingSettings_s settings;
	settings.useAlphaCoverage = false;
	settings.isCapturing = true;

	DrawScene( s_activeScene, &view, &settings );

	// un RT
	gpuContext.deviceContext->OMSetRenderTargets( 0, nullptr, nullptr );
			
	gpuContext.userDefinedAnnotation->EndEvent();
}

void CRenderingDevice::Collect( const core::Vec3* samplePos, core::u32 faceID )
{
	static const void* nulls[8] = {};

	gpuContext.userDefinedAnnotation->BeginEvent( L"Collect");

	// Update buffers
				
	{			
		V6_ASSERT( GRID_COUNT <= HLSL_MIP_MAX_COUNT );

		float gridScales[HLSL_MIP_MAX_COUNT];
		core::Vec3 gridCenters[HLSL_MIP_MAX_COUNT];
		float gridScale = GRID_MIN_SCALE;
		for ( core::u32 gridID = 0; gridID < GRID_COUNT; ++gridID, gridScale *= 2 )
		{
			gridScales[gridID] = gridScale;
			gridCenters[gridID] = Block_ComputeGridCenter( samplePos, gridScale );
		}
		for ( core::u32 gridID = GRID_COUNT; gridID < 16; ++gridID )
		{
			gridScales[gridID] = gridScales[GRID_COUNT-1];
			gridCenters[gridID] = gridCenters[GRID_COUNT-1];
		}

		v6::hlsl::CBSample* cbSample = ConstantBuffer_MapWrite< v6::hlsl::CBSample >( gpuContext.deviceContext, &gpuContext.constantBuffers[CONSTANT_BUFFER_SAMPLE] );

		cbSample->c_sampleDepthLinearScale = -1.0f / ZNEAR;
		cbSample->c_sampleDepthLinearBias = 1.0f / ZNEAR;
		cbSample->c_sampleInvCubeSize.x = 1.0f / CUBE_SIZE;
		cbSample->c_sampleInvCubeSize.y = 1.0f / CUBE_SIZE;
		cbSample->c_samplePos = *samplePos;
		cbSample->c_sampleFaceID = faceID;
		for ( core::u32 gridID = 0; gridID < HLSL_MIP_MAX_COUNT; ++gridID )
			cbSample->c_sampleMipBoundaries[gridID] = core::Vec4_Make( &gridCenters[gridID], gridScales[gridID] * gridScales[gridID] );
		for ( core::u32 gridID = 0; gridID < HLSL_MIP_MAX_COUNT; ++gridID )
			cbSample->c_sampleInvGridScales[gridID] = core::Vec4_Make( 1.0f / gridScales[gridID], 0.0f, 0.0f , 0.0f );

		ConstantBuffer_UnmapWrite( gpuContext.deviceContext, &gpuContext.constantBuffers[CONSTANT_BUFFER_SAMPLE] );		
	}

	core::u32 values[4] = {};
	gpuContext.deviceContext->ClearUnorderedAccessViewUint( m_sample.indirectArgs.uav, values );
		
	gpuContext.deviceContext->CSSetConstantBuffers( v6::hlsl::CBSampleSlot, 1, &gpuContext.constantBuffers[CONSTANT_BUFFER_SAMPLE].buf );
	gpuContext.deviceContext->CSSetShaderResources( HLSL_LCOLOR_SLOT, 1, &m_cube.colorSRV );
	gpuContext.deviceContext->CSSetShaderResources( HLSL_DEPTH_SLOT, 1, &m_cube.depthSRV );
	gpuContext.deviceContext->CSSetUnorderedAccessViews( HLSL_SAMPLE_SLOT, 1, &m_sample.samples.uav, nullptr );
	gpuContext.deviceContext->CSSetUnorderedAccessViews( HLSL_SAMPLE_INDIRECT_ARGS_SLOT, 1, &m_sample.indirectArgs.uav, nullptr );
	gpuContext.deviceContext->CSSetShader( gpuContext.computes[COMPUTE_SAMPLECOLLECT].m_computeShader, nullptr, 0 );
		
	const core::u32 cubeGroupCount = (m_cube.size / HLSL_CELL_SUPER_SAMPLING_WIDTH) >> 3;
	gpuContext.deviceContext->Dispatch( cubeGroupCount, cubeGroupCount, 1 );

	// Unset		
	gpuContext.deviceContext->CSSetShaderResources( HLSL_LCOLOR_SLOT, 1, (ID3D11ShaderResourceView**)nulls );
	gpuContext.deviceContext->CSSetShaderResources( HLSL_DEPTH_SLOT, 1, (ID3D11ShaderResourceView**)nulls );
	gpuContext.deviceContext->CSSetUnorderedAccessViews( HLSL_SAMPLE_SLOT, 1, (ID3D11UnorderedAccessView**)nulls, nullptr );
	gpuContext.deviceContext->CSSetUnorderedAccessViews( HLSL_SAMPLE_INDIRECT_ARGS_SLOT, 1, (ID3D11UnorderedAccessView**)nulls, nullptr );
				
	gpuContext.userDefinedAnnotation->EndEvent();
	
	if ( s_logReadBack )
	{
		// Read back
		const core::u32* collectedIndirectArgs = GPUBUffer_MapReadBack< core::u32 >( gpuContext.deviceContext, &m_sample.indirectArgs );
		
		V6_MSG( "\n" );
		ReadBack_Log( "sample", collectedIndirectArgs[sample_groupCountX_offset], "groupCountX" );
		V6_ASSERT( collectedIndirectArgs[sample_groupCountY_offset] == 1 );
		V6_ASSERT( collectedIndirectArgs[sample_groupCountZ_offset] == 1 );
		ReadBack_Log( "sample", collectedIndirectArgs[sample_count_offset], "count" );		
		V6_ASSERT( collectedIndirectArgs[sample_count_offset] <= m_config.sampleCount );
#if HLSL_DEBUG_COLLECT == 1
		ReadBack_Log( "sample", collectedIndirectArgs[sample_pixelCount_offset], "pixelCount" );
		ReadBack_Log( "sample", collectedIndirectArgs[sample_pixelSampleCount_offset], "pixelSampleCount" );
		ReadBack_Log( "sample", collectedIndirectArgs[sample_out_offset], "out" );
		ReadBack_Log( "sample", collectedIndirectArgs[sample_error_offset], "error" );
#if 0
		ReadBack_Log( "sample", collectedIndirectArgs[sample_occupancy_offset], "occupancy" );
		for ( core::u32 sampleID = 0; sampleID < 144; ++sampleID )
		{
			core::u32 value = collectedIndirectArgs[sample_cellCoords_offset( sampleID )];
			ReadBack_Log( "sample", *((float*)&value), "cellCoords.x" );
		}
#endif
		V6_ASSERT( collectedIndirectArgs[sample_error_offset] == 0 );
#endif // #if HLSL_DEBUG_COLLECT == 1

		GPUBUffer_UnmapReadBack( gpuContext.deviceContext, &m_sample.indirectArgs );
	}
}

void CRenderingDevice::ClearNode()
{
	core::u32 values[4] = {};
	gpuContext.deviceContext->ClearUnorderedAccessViewUint( m_octree.indirectArgs.uav, values );
	gpuContext.deviceContext->ClearUnorderedAccessViewUint( m_octree.firstChildOffsetsLimitedUAV, values );
}

core::u32 CRenderingDevice::BuildNode()
{
	static const void* nulls[8] = {};

	gpuContext.userDefinedAnnotation->BeginEvent( L"BuildNode");

	V6_ASSERT( GRID_COUNT <= HLSL_MIP_MAX_COUNT );	

	gpuContext.deviceContext->CSSetConstantBuffers( v6::hlsl::CBOctreeSlot, 1, &gpuContext.constantBuffers[CONSTANT_BUFFER_OCTREE].buf );
	gpuContext.deviceContext->CSSetShaderResources( HLSL_SAMPLE_SLOT, 1, &m_sample.samples.srv );
	gpuContext.deviceContext->CSSetShaderResources( HLSL_SAMPLE_INDIRECT_ARGS_SLOT, 1, &m_sample.indirectArgs.srv );
	gpuContext.deviceContext->CSSetUnorderedAccessViews( HLSL_OCTREE_SAMPLE_NODE_OFFSET_SLOT, 1, &m_octree.sampleNodeOffsets.uav, nullptr );
	gpuContext.deviceContext->CSSetUnorderedAccessViews( HLSL_OCTREE_FIRST_CHILD_OFFSET_SLOT, 1, &m_octree.firstChildOffsets.uav, nullptr );
	gpuContext.deviceContext->CSSetUnorderedAccessViews( HLSL_OCTREE_LEAF_SLOT, 1, &m_octree.leaves.uav, nullptr );
	gpuContext.deviceContext->CSSetUnorderedAccessViews( HLSL_OCTREE_INDIRECT_ARGS_SLOT, 1, &m_octree.indirectArgs.uav, nullptr );		

	gpuContext.deviceContext->CSSetShader( gpuContext.computes[COMPUTE_BUILDINNER].m_computeShader, nullptr, 0 );

	for ( core::u32 level = 0; level < HLSL_GRID_SHIFT; ++level )
	{
		// Update buffers				
		{
			v6::hlsl::CBOctree* cbOctree = ConstantBuffer_MapWrite< v6::hlsl::CBOctree >( gpuContext.deviceContext, &gpuContext.constantBuffers[CONSTANT_BUFFER_OCTREE] );
			cbOctree->c_octreeCurrentLevel = level;
			cbOctree->c_octreeCurrentBucket = 0;
			ConstantBuffer_UnmapWrite( gpuContext.deviceContext, &gpuContext.constantBuffers[CONSTANT_BUFFER_OCTREE] );
		}

		if ( level == HLSL_GRID_SHIFT-1 )
			gpuContext.deviceContext->CSSetShader( gpuContext.computes[COMPUTE_BUILDLEAF].m_computeShader, nullptr, 0 );

		gpuContext.deviceContext->DispatchIndirect( m_sample.indirectArgs.buf, sample_groupCountX_offset * sizeof( core::u32 ) );
	}

	// Unset
	gpuContext.deviceContext->CSSetShaderResources( HLSL_SAMPLE_SLOT, 1, (ID3D11ShaderResourceView**)nulls);
	gpuContext.deviceContext->CSSetShaderResources( HLSL_SAMPLE_INDIRECT_ARGS_SLOT, 1, (ID3D11ShaderResourceView**)nulls);
	gpuContext.deviceContext->CSSetUnorderedAccessViews( HLSL_OCTREE_SAMPLE_NODE_OFFSET_SLOT, 1, (ID3D11UnorderedAccessView**)nulls, nullptr );
	gpuContext.deviceContext->CSSetUnorderedAccessViews( HLSL_OCTREE_FIRST_CHILD_OFFSET_SLOT, 1, (ID3D11UnorderedAccessView**)nulls, nullptr );
	gpuContext.deviceContext->CSSetUnorderedAccessViews( HLSL_OCTREE_LEAF_SLOT, 1, (ID3D11UnorderedAccessView**)nulls, nullptr );
	gpuContext.deviceContext->CSSetUnorderedAccessViews( HLSL_OCTREE_INDIRECT_ARGS_SLOT, 1, (ID3D11UnorderedAccessView**)nulls, nullptr );
				
	gpuContext.userDefinedAnnotation->EndEvent();

	const core::u32* octreeIndirectArgs = GPUBUffer_MapReadBack< core::u32 >( gpuContext.deviceContext, &m_octree.indirectArgs );	

	if ( s_logReadBack )
	{
		V6_MSG( "\n" );
		ReadBack_Log( "octree", octreeIndirectArgs[octree_nodeCount_offset], "nodeCount" );
		V6_ASSERT( octreeIndirectArgs[octree_nodeCount_offset] <= m_config.nodeCount );
		ReadBack_Log( "octree", octreeIndirectArgs[octree_leafGroupCountX_offset], "leafGroupCountX" );
		V6_ASSERT( octreeIndirectArgs[octree_leafGroupCountY_offset] == 1 );
		V6_ASSERT( octreeIndirectArgs[octree_leafGroupCountZ_offset] == 1 );
		ReadBack_Log( "octree", octreeIndirectArgs[octree_leafCount_offset], "leafCount" );
	}

	const core::u32 leafCount = octreeIndirectArgs[octree_leafCount_offset];
	V6_ASSERT( leafCount <= m_config.leafCount );

	GPUBUffer_UnmapReadBack( gpuContext.deviceContext, &m_octree.indirectArgs );

	return leafCount;
}

void CRenderingDevice::FillLeaf()
{
	static const void* nulls[8] = {};

	gpuContext.userDefinedAnnotation->BeginEvent( L"FillLeaf");

	V6_ASSERT( GRID_COUNT <= HLSL_MIP_MAX_COUNT );

	gpuContext.deviceContext->CSSetConstantBuffers( v6::hlsl::CBOctreeSlot, 1, &gpuContext.constantBuffers[CONSTANT_BUFFER_OCTREE].buf );
	gpuContext.deviceContext->CSSetShaderResources( HLSL_SAMPLE_SLOT, 1, &m_sample.samples.srv );
	gpuContext.deviceContext->CSSetShaderResources( HLSL_SAMPLE_INDIRECT_ARGS_SLOT, 1, &m_sample.indirectArgs.srv );
	gpuContext.deviceContext->CSSetShaderResources( HLSL_OCTREE_SAMPLE_NODE_OFFSET_SLOT, 1, &m_octree.sampleNodeOffsets.srv );
	gpuContext.deviceContext->CSSetShaderResources( HLSL_OCTREE_FIRST_CHILD_OFFSET_SLOT, 1, &m_octree.firstChildOffsets.srv );
	gpuContext.deviceContext->CSSetUnorderedAccessViews( HLSL_OCTREE_LEAF_SLOT, 1, &m_octree.leaves.uav, nullptr );
	gpuContext.deviceContext->CSSetShader( gpuContext.computes[COMPUTE_FILLLEAF].m_computeShader, nullptr, 0 );

	// Update buffers				
	{
		v6::hlsl::CBOctree* cbOctree = ConstantBuffer_MapWrite< v6::hlsl::CBOctree >( gpuContext.deviceContext, &gpuContext.constantBuffers[CONSTANT_BUFFER_OCTREE] );
		cbOctree->c_octreeCurrentLevel = 0;
		cbOctree->c_octreeCurrentBucket = 0;
		ConstantBuffer_UnmapWrite( gpuContext.deviceContext, &gpuContext.constantBuffers[CONSTANT_BUFFER_OCTREE] );		
	}

	gpuContext.deviceContext->DispatchIndirect( m_sample.indirectArgs.buf, sample_groupCountX_offset * sizeof( core::u32 ) );

	// Unset
	gpuContext.deviceContext->CSSetShaderResources( HLSL_SAMPLE_SLOT, 1, (ID3D11ShaderResourceView**)nulls);
	gpuContext.deviceContext->CSSetShaderResources( HLSL_SAMPLE_INDIRECT_ARGS_SLOT, 1, (ID3D11ShaderResourceView**)nulls);
	gpuContext.deviceContext->CSSetShaderResources( HLSL_OCTREE_SAMPLE_NODE_OFFSET_SLOT, 1, (ID3D11ShaderResourceView**)nulls);
	gpuContext.deviceContext->CSSetShaderResources( HLSL_OCTREE_FIRST_CHILD_OFFSET_SLOT, 1, (ID3D11ShaderResourceView**)nulls);
	gpuContext.deviceContext->CSSetUnorderedAccessViews( HLSL_OCTREE_LEAF_SLOT, 1, (ID3D11UnorderedAccessView**)nulls, nullptr );
				
	gpuContext.userDefinedAnnotation->EndEvent();
}

void CRenderingDevice::PackColor( core::u32 blockCounts[HLSL_BUCKET_COUNT] )
{
	static const void* nulls[8] = {};
	
	gpuContext.userDefinedAnnotation->BeginEvent( L"Pack");

	V6_ASSERT( GRID_COUNT <= HLSL_MIP_MAX_COUNT );

	core::u32 values[4] = {};
	gpuContext.deviceContext->ClearUnorderedAccessViewUint( m_block.blockIndirectArgs.uav, values );

	gpuContext.deviceContext->CSSetConstantBuffers( v6::hlsl::CBOctreeSlot, 1, &gpuContext.constantBuffers[CONSTANT_BUFFER_OCTREE].buf );
	gpuContext.deviceContext->CSSetShaderResources( HLSL_OCTREE_FIRST_CHILD_OFFSET_SLOT, 1, &m_octree.firstChildOffsets.srv );
	gpuContext.deviceContext->CSSetShaderResources( HLSL_OCTREE_LEAF_SLOT, 1, &m_octree.leaves.srv );
	gpuContext.deviceContext->CSSetShaderResources( HLSL_OCTREE_INDIRECT_ARGS_SLOT, 1, &m_octree.indirectArgs.srv );
	gpuContext.deviceContext->CSSetUnorderedAccessViews( HLSL_BLOCK_POS_SLOT, 1, &m_block.blockPos.uav, nullptr );
	gpuContext.deviceContext->CSSetUnorderedAccessViews( HLSL_BLOCK_DATA_SLOT, 1, &m_block.blockData.uav, nullptr );
	gpuContext.deviceContext->CSSetUnorderedAccessViews( HLSL_BLOCK_INDIRECT_ARGS_SLOT, 1, &m_block.blockIndirectArgs.uav, nullptr );
	gpuContext.deviceContext->CSSetShader( gpuContext.computes[COMPUTE_PACKCOLOR].m_computeShader, nullptr, 0 );

	for ( core::u32 bucket = 0; bucket < HLSL_BUCKET_COUNT; ++bucket )
	{
		// Update buffers
		{
			v6::hlsl::CBOctree* cbOctree = ConstantBuffer_MapWrite< v6::hlsl::CBOctree >( gpuContext.deviceContext, &gpuContext.constantBuffers[CONSTANT_BUFFER_OCTREE] );
			cbOctree->c_octreeCurrentLevel = 0;
			cbOctree->c_octreeCurrentBucket = bucket;
			ConstantBuffer_UnmapWrite( gpuContext.deviceContext, &gpuContext.constantBuffers[CONSTANT_BUFFER_OCTREE] );		
		}

		gpuContext.deviceContext->DispatchIndirect( m_octree.indirectArgs.buf, octree_leafGroupCountX_offset * sizeof( core::u32 ) );
	}

	// Unset
	gpuContext.deviceContext->CSSetShaderResources( HLSL_OCTREE_FIRST_CHILD_OFFSET_SLOT, 1, (ID3D11ShaderResourceView**)nulls);
	gpuContext.deviceContext->CSSetShaderResources( HLSL_OCTREE_LEAF_SLOT, 1, (ID3D11ShaderResourceView**)nulls);
	gpuContext.deviceContext->CSSetShaderResources( HLSL_OCTREE_INDIRECT_ARGS_SLOT, 1, (ID3D11ShaderResourceView**)nulls);
	gpuContext.deviceContext->CSSetUnorderedAccessViews( HLSL_BLOCK_POS_SLOT, 1, (ID3D11UnorderedAccessView**)nulls, nullptr );
	gpuContext.deviceContext->CSSetUnorderedAccessViews( HLSL_BLOCK_DATA_SLOT, 1, (ID3D11UnorderedAccessView**)nulls, nullptr );
	gpuContext.deviceContext->CSSetUnorderedAccessViews( HLSL_BLOCK_INDIRECT_ARGS_SLOT, 1, (ID3D11UnorderedAccessView**)nulls, nullptr );
				
	gpuContext.userDefinedAnnotation->EndEvent();

	const core::u32* blockIndirectArgs = GPUBUffer_MapReadBack< core::u32 >( gpuContext.deviceContext, &m_block.blockIndirectArgs );

	core::u32 allBlockCount = 0;
	core::u32 allRealCellCount = 0;
	core::u32 allMaxCellCount = 0; 

	for ( core::u32 bucket = 0; bucket < HLSL_BUCKET_COUNT; ++bucket )
	{
		blockCounts[bucket] = block_count( bucket );
		if ( block_count( bucket ) == 0 )
			continue;

		static const core::u32 cellPerBucketCounts[] = { 4, 8, 16, 32, 64 };
		const core::u32 maxCellCount = block_count( bucket ) * cellPerBucketCounts[bucket];

		if ( s_logReadBack )
		{
			V6_MSG( "\n" );
			ReadBack_Log( "block", bucket, "bucket" );
			ReadBack_Log( "block", block_groupCountX( bucket ), "groupCountX" );
			V6_ASSERT( block_groupCountY( bucket ) == 1 );
			V6_ASSERT( block_groupCountZ( bucket ) == 1 );
			ReadBack_Log( "block", block_count( bucket ), "blockCount" );
			ReadBack_Log( "block", block_posOffset( bucket ), "posOffset" );
			ReadBack_Log( "block", block_dataOffset( bucket ), "dataOffset" );
			ReadBack_Log( "block", block_cellCount( bucket ), "realCellCount" );
			ReadBack_Log( "block", maxCellCount, "maxCellCount" );
#if HLSL_DEBUG_OCCUPANCY == 1
			ReadBack_Log( "block", block_uniqueOccupancyCount( bucket ) / (float)block_count( bucket ), "avgOccupancyCount" );
			ReadBack_Log( "block", block_uniqueOccupancyMax( bucket ), "maxOccupancyCount" );
			ReadBack_Log( "block", block_slotOccupancyCount( bucket ) / (float)block_cellCount( bucket ), "avgOccupancySlot" );
#endif // #if HLSL_DEBUG_OCCUPANCY == 1
		}

		allBlockCount += block_count( bucket );
		allRealCellCount += block_cellCount( bucket );
		allMaxCellCount += maxCellCount;
	}		

	V6_MSG( "\n" );
	ReadBack_Log( "packed", allBlockCount, "blockCount" );
	ReadBack_Log( "packed", allRealCellCount, "realCellCount" );
	ReadBack_Log( "packed", allMaxCellCount, "maxCellCount" );
	V6_ASSERT( allMaxCellCount <= m_config.cellCount );

	GPUBUffer_UnmapReadBack( gpuContext.deviceContext, &m_block.blockIndirectArgs );
}

void CRenderingDevice::CullBlock( const RenderingView_s* views, const core::Vec3* buildOrigin )
{		
	static const void* nulls[8] = {};

	gpuContext.userDefinedAnnotation->BeginEvent( L"Cull Blocks");

	core::u32 values[4] = {};
	gpuContext.deviceContext->ClearUnorderedAccessViewUint( m_trace.traceIndirectArgs.uav, values );
	if ( s_logReadBack )
		gpuContext.deviceContext->ClearUnorderedAccessViewUint( m_trace.cullStats.uav, values );

	float gridScale = GRID_MIN_SCALE;

	{
		v6::hlsl::CBCull* cbCull = ConstantBuffer_MapWrite< v6::hlsl::CBCull >( gpuContext.deviceContext, &gpuContext.constantBuffers[CONSTANT_BUFFER_CULL] );

		float gridScale = GRID_MIN_SCALE;
		for ( core::u32 gridID = 0; gridID < HLSL_MIP_MAX_COUNT; ++gridID )
		{
			const float cellScale = gridScale * HLSL_GRID_INV_WIDTH;
			cbCull->c_cullGridScales[gridID] = core::Vec4_Make( gridScale, 0.0f, 0.0f, 0.0f );
			const core::Vec3 center = Block_ComputeGridCenter( buildOrigin, gridScale );
			cbCull->c_cullCenters[gridID] = core::Vec4_Make( &center, 0.0f );
			if ( gridID < GRID_COUNT)
				gridScale *= 2;
		}

		V6_ASSERT( views[LEFT_EYE].forward == views[RIGHT_EYE].forward );
		V6_ASSERT( views[LEFT_EYE].right == views[RIGHT_EYE].right );
		V6_ASSERT( views[LEFT_EYE].up == views[RIGHT_EYE].up );

		const float tanHalfFOVLeft = core::Max( views[LEFT_EYE].tanHalfFOVLeft, views[RIGHT_EYE].tanHalfFOVLeft );
		const float tanHalfFOVRight = core::Max( views[LEFT_EYE].tanHalfFOVRight, views[RIGHT_EYE].tanHalfFOVRight );
		const float tanHalfFOVUp = core::Max( views[LEFT_EYE].tanHalfFOVUp, views[RIGHT_EYE].tanHalfFOVUp );
		const float tanHalfFOVDown = core::Max( views[LEFT_EYE].tanHalfFOVDown, views[RIGHT_EYE].tanHalfFOVDown );

		core::Vec3 leftPlane = (views[ANY_EYE].forward * tanHalfFOVLeft + views[ANY_EYE].right).Normalized();
		core::Vec3 rightPlane = (views[ANY_EYE].forward * tanHalfFOVRight - views[ANY_EYE].right).Normalized();
		core::Vec3 upPlane = (views[ANY_EYE].forward * tanHalfFOVUp - views[ANY_EYE].up).Normalized();
		core::Vec3 bottomPlane = (views[ANY_EYE].forward * tanHalfFOVDown + views[ANY_EYE].up).Normalized();
		
		cbCull->c_cullFrustumPlanes[0] = core::Vec4_Make( &leftPlane, -core::Dot( leftPlane, views[LEFT_EYE].org ) );
		cbCull->c_cullFrustumPlanes[1] = core::Vec4_Make( &rightPlane, -core::Dot( rightPlane, views[RIGHT_EYE].org ) );
		cbCull->c_cullFrustumPlanes[2] = core::Vec4_Make( &upPlane, -core::Dot( upPlane, views[ANY_EYE].org ) );
		cbCull->c_cullFrustumPlanes[3] = core::Vec4_Make( &bottomPlane, -core::Dot( bottomPlane, views[ANY_EYE].org ) );

		ConstantBuffer_UnmapWrite( gpuContext.deviceContext, &gpuContext.constantBuffers[CONSTANT_BUFFER_CULL] );
	}

	// set

	gpuContext.deviceContext->CSSetConstantBuffers( v6::hlsl::CBCullSlot, 1, &gpuContext.constantBuffers[CONSTANT_BUFFER_CULL].buf );	
	gpuContext.deviceContext->CSSetShaderResources( HLSL_BLOCK_POS_SLOT, 1, &m_block.blockPos.srv );
	gpuContext.deviceContext->CSSetShaderResources( HLSL_BLOCK_DATA_SLOT, 1, &m_block.blockData.srv );
	gpuContext.deviceContext->CSSetShaderResources( HLSL_BLOCK_INDIRECT_ARGS_SLOT, 1, &m_block.blockIndirectArgs.srv );
	if ( s_logReadBack )
		gpuContext.deviceContext->CSSetUnorderedAccessViews( HLSL_CULL_STATS_SLOT, 1, &m_trace.cullStats.uav, nullptr );

	for ( core::u32 bucket = 0; bucket < HLSL_BUCKET_COUNT; ++bucket )
	{
		gpuContext.userDefinedAnnotation->BeginEvent( L"Cull Bucket");

		// set
		gpuContext.deviceContext->CSSetUnorderedAccessViews( HLSL_TRACE_CELLS_SLOT, 1, &m_trace.traceCell.uav, nullptr );
		gpuContext.deviceContext->CSSetUnorderedAccessViews( HLSL_TRACE_INDIRECT_ARGS_SLOT, 1, &m_trace.traceIndirectArgs.uav, nullptr );

		// dispach
		if ( s_logReadBack )
			gpuContext.deviceContext->CSSetShader( gpuContext.computes[COMPUTE_BLOCK_CULL_STATS4+bucket].m_computeShader, nullptr, 0 );
		else
			gpuContext.deviceContext->CSSetShader( gpuContext.computes[COMPUTE_BLOCK_CULL4+bucket].m_computeShader, nullptr, 0 );
		gpuContext.deviceContext->DispatchIndirect( m_block.blockIndirectArgs.buf, block_groupCountX_offset( bucket ) * sizeof( core::u32 ) );

		// unset
		gpuContext.deviceContext->CSSetUnorderedAccessViews( HLSL_TRACE_CELLS_SLOT, 1, (ID3D11UnorderedAccessView**)nulls, nullptr );
		gpuContext.deviceContext->CSSetUnorderedAccessViews( HLSL_TRACE_INDIRECT_ARGS_SLOT, 1, (ID3D11UnorderedAccessView**)nulls, nullptr );

		gpuContext.userDefinedAnnotation->EndEvent();
	}	
		// Unset
	gpuContext.deviceContext->CSSetShaderResources( HLSL_BLOCK_POS_SLOT, 1, (ID3D11ShaderResourceView**)nulls );
	gpuContext.deviceContext->CSSetShaderResources( HLSL_BLOCK_DATA_SLOT, 1, (ID3D11ShaderResourceView**)nulls );
	gpuContext.deviceContext->CSSetShaderResources( HLSL_BLOCK_INDIRECT_ARGS_SLOT, 1, (ID3D11ShaderResourceView**)nulls );
	if ( s_logReadBack )
		gpuContext.deviceContext->CSSetUnorderedAccessViews( HLSL_CULL_STATS_SLOT, 1, (ID3D11UnorderedAccessView**)nulls, nullptr );

	gpuContext.userDefinedAnnotation->EndEvent();

	if ( s_logReadBack )
	{
		V6_MSG( "\n" );

		{
			const hlsl::BlockCullStats* blockCullStats = GPUBUffer_MapReadBack< hlsl::BlockCullStats >( gpuContext.deviceContext, &m_trace.cullStats );

			ReadBack_Log( "blockCull", blockCullStats->blockInputCount, "blockInputCount" );
			ReadBack_Log( "blockCull", blockCullStats->blockPassedCount, "blockPassedCount" );
			ReadBack_Log( "blockCull", blockCullStats->cellOutputCount, "cellOutputCount" );

			GPUBUffer_UnmapReadBack( gpuContext.deviceContext, &m_trace.cullStats );
		}
	}
}

void CRenderingDevice::TraceBlock( const RenderingView_s* views, const core::Vec3* buildOrigin )
{		
	static const void* nulls[8] = {};
	
	gpuContext.userDefinedAnnotation->BeginEvent( L"Trace Blocks");

	core::u32 values[4] = {};
	gpuContext.deviceContext->ClearUnorderedAccessViewUint( m_trace.cellItemCounters.uav, values );
	if ( s_logReadBack )
		gpuContext.deviceContext->ClearUnorderedAccessViewUint( m_trace.traceStats.uav, values );

	float gridScale = GRID_MIN_SCALE;

	{
		v6::hlsl::CBBlock* cbBlock = ConstantBuffer_MapWrite< v6::hlsl::CBBlock >( gpuContext.deviceContext, &gpuContext.constantBuffers[CONSTANT_BUFFER_BLOCK] );

		float gridScale = GRID_MIN_SCALE;
		for ( core::u32 gridID = 0; gridID < HLSL_MIP_MAX_COUNT; ++gridID )
		{
			const float cellScale = gridScale * HLSL_GRID_INV_WIDTH;
			cbBlock->c_blockGridScales[gridID] = core::Vec4_Make( gridScale, cellScale, ((1 << 21) - 1) / gridScale, 0.0f );
			const core::Vec3 center = Block_ComputeGridCenter( buildOrigin, gridScale );
			cbBlock->c_blockGridCenters[gridID] = core::Vec4_Make( &center, 0.0f );
			if ( gridID < GRID_COUNT)
				gridScale *= 2;
		}

		const core::Vec2 frameSize = core::Vec2_Make( (float)m_width, (float)m_height );
		cbBlock->c_blockFrameSize = frameSize;
				
		cbBlock->c_blockFrameSize.x = (float)m_config.screenWidth;
		cbBlock->c_blockFrameSize.y = (float)m_config.screenHeight;

		for ( core::u32 eye = 0; eye < HLSL_EYE_COUNT; ++eye )
		{	
			hlsl::BlockPerEye blockPerEye;
			blockPerEye.objectToView = views[eye].viewMatrix;
			blockPerEye.viewToProj = views[eye].projMatrix;

			const float scaleRight = (views[eye].tanHalfFOVLeft + views[eye].tanHalfFOVRight) / m_config.screenWidth;
			const float scaleUp = (views[eye].tanHalfFOVUp + views[eye].tanHalfFOVDown) / m_config.screenHeight;

			const core::Vec3 forward = views[eye].forward;
			const core::Vec3 right = views[eye].right * scaleRight;
			const core::Vec3 up = views[eye].up * scaleUp;
				
			blockPerEye.org = views[eye].org;

			blockPerEye.rayDirBase = forward - views[eye].up * views[eye].tanHalfFOVDown + 0.5f * up - views[eye].right * views[eye].tanHalfFOVLeft + 0.5f * right;
			blockPerEye.rayDirUp = up;
			blockPerEye.rayDirRight = right;

			cbBlock->c_blockEyes[eye] = blockPerEye;
		}

		ConstantBuffer_UnmapWrite( gpuContext.deviceContext, &gpuContext.constantBuffers[CONSTANT_BUFFER_BLOCK] );
	}

	// set

	gpuContext.deviceContext->CSSetConstantBuffers( v6::hlsl::CBBlockSlot, 1, &gpuContext.constantBuffers[CONSTANT_BUFFER_BLOCK].buf );
#if HLSL_ENCODE_DATA == 0
	gpuContext.deviceContext->CSSetShaderResources( HLSL_BLOCK_POS_SLOT, 1, &m_block.blockPos.srv );
	gpuContext.deviceContext->CSSetShaderResources( HLSL_BLOCK_DATA_SLOT, 1, &m_block.blockData.srv );
	gpuContext.deviceContext->CSSetShaderResources( HLSL_BLOCK_INDIRECT_ARGS_SLOT, 1, &m_block.blockIndirectArgs.srv );
#endif // #if HLSL_ENCODE_DATA == 0
	gpuContext.deviceContext->CSSetShaderResources( HLSL_TRACE_CELLS_SLOT, 1, &m_trace.traceCell.srv );
	gpuContext.deviceContext->CSSetUnorderedAccessViews( HLSL_BLOCK_CELL_ITEM_SLOT, 1, &m_trace.cellItems.uav, nullptr );
	gpuContext.deviceContext->CSSetUnorderedAccessViews( HLSL_BLOCK_CELL_ITEM_COUNT_SLOT, 1, &m_trace.cellItemCounters.uav, nullptr );
	if ( s_logReadBack )
		gpuContext.deviceContext->CSSetUnorderedAccessViews( HLSL_TRACE_STATS_SLOT, 1, &m_trace.traceStats.uav, nullptr );
	
	{
		gpuContext.userDefinedAnnotation->BeginEvent( L"Init Trace");

		// set
		gpuContext.deviceContext->CSSetUnorderedAccessViews( HLSL_TRACE_INDIRECT_ARGS_SLOT, 1, &m_trace.traceIndirectArgs.uav, nullptr );
				
		// dispatch
		gpuContext.deviceContext->CSSetShader( gpuContext.computes[COMPUTE_BLOCK_TRACE_INIT].m_computeShader, nullptr, 0 );				
		gpuContext.deviceContext->Dispatch( 1, 1, 1 );

		// unset
		gpuContext.deviceContext->CSSetUnorderedAccessViews( HLSL_TRACE_INDIRECT_ARGS_SLOT, 1, (ID3D11UnorderedAccessView**)nulls, nullptr );

		gpuContext.userDefinedAnnotation->EndEvent();
	}

#if HLSL_ENCODE_DATA == 1
	{
		gpuContext.userDefinedAnnotation->BeginEvent( L"Trace");
		
		// set
		
		gpuContext.deviceContext->CSSetShaderResources( HLSL_TRACE_INDIRECT_ARGS_SLOT, 1, &m_block.traceIndirectArgs.srv );
			
		// dispach
		if ( s_logReadBack )
			gpuContext.deviceContext->CSSetShader( gpuContext.computes[COMPUTE_BLOCK_TRACE_STATS].m_computeShader, nullptr, 0 );
		else
			gpuContext.deviceContext->CSSetShader( gpuContext.computes[COMPUTE_BLOCK_TRACE].m_computeShader, nullptr, 0 );
		gpuContext.deviceContext->DispatchIndirect( m_block.traceIndirectArgs.buf, 0 );

		// unset		
		gpuContext.deviceContext->CSSetShaderResources( HLSL_TRACE_INDIRECT_ARGS_SLOT, 1, (ID3D11ShaderResourceView**)nulls );

		gpuContext.userDefinedAnnotation->EndEvent();
	}
#else
	for ( core::u32 bucket = 0; bucket < HLSL_BUCKET_COUNT; ++bucket )
	{
		gpuContext.userDefinedAnnotation->BeginEvent( L"Trace Bucket");

		// set

		gpuContext.deviceContext->CSSetShaderResources( HLSL_TRACE_INDIRECT_ARGS_SLOT, 1, &m_trace.traceIndirectArgs.srv );

		// dispach
		if ( s_logReadBack )
			gpuContext.deviceContext->CSSetShader( gpuContext.computes[COMPUTE_BLOCK_TRACE_STATS4+bucket].m_computeShader, nullptr, 0 );
		else
			gpuContext.deviceContext->CSSetShader( gpuContext.computes[COMPUTE_BLOCK_TRACE4+bucket].m_computeShader, nullptr, 0 );
		gpuContext.deviceContext->DispatchIndirect( m_trace.traceIndirectArgs.buf, trace_cellGroupCountX_offset( bucket ) * sizeof( core::u32 ) );

		// unset		
		gpuContext.deviceContext->CSSetShaderResources( HLSL_TRACE_INDIRECT_ARGS_SLOT, 1, (ID3D11ShaderResourceView**)nulls );

		gpuContext.userDefinedAnnotation->EndEvent();
	}
#endif

	// Unset
#if HLSL_ENCODE_DATA == 0
	gpuContext.deviceContext->CSSetShaderResources( HLSL_BLOCK_POS_SLOT, 1, (ID3D11ShaderResourceView**)nulls );
	gpuContext.deviceContext->CSSetShaderResources( HLSL_BLOCK_DATA_SLOT, 1, (ID3D11ShaderResourceView**)nulls );
	gpuContext.deviceContext->CSSetShaderResources( HLSL_BLOCK_INDIRECT_ARGS_SLOT, 1, (ID3D11ShaderResourceView**)nulls );
#endif // #if HLSL_ENCODE_DATA == 0
	gpuContext.deviceContext->CSSetShaderResources( HLSL_TRACE_CELLS_SLOT, 1, (ID3D11ShaderResourceView**)nulls );
	gpuContext.deviceContext->CSSetUnorderedAccessViews( HLSL_BLOCK_CELL_ITEM_SLOT, 1, (ID3D11UnorderedAccessView**)nulls, nullptr );
	gpuContext.deviceContext->CSSetUnorderedAccessViews( HLSL_BLOCK_CELL_ITEM_COUNT_SLOT, 1, (ID3D11UnorderedAccessView**)nulls, nullptr );
	if ( s_logReadBack )
		gpuContext.deviceContext->CSSetUnorderedAccessViews( HLSL_TRACE_STATS_SLOT, 1, (ID3D11UnorderedAccessView**)nulls, nullptr );
	
	gpuContext.userDefinedAnnotation->EndEvent();

	if ( s_logReadBack )
	{			
		V6_MSG( "\n" );

		{
			const hlsl::BlockTraceStats* blockTraceStats = GPUBUffer_MapReadBack< hlsl::BlockTraceStats >( gpuContext.deviceContext, &m_trace.traceStats );

			ReadBack_Log( "blockTrace", blockTraceStats->cellInputCount, "cellInputCount" );
			ReadBack_Log( "blockTrace", blockTraceStats->cellProcessedCount, "cellProcessedCount" );
			ReadBack_Log( "blockTrace", blockTraceStats->pixelSampleCount, "pixelSampleCount" );
			ReadBack_Log( "blockTrace", blockTraceStats->cellItemCount, "cellItemCount" );
			ReadBack_Log( "blockTrace", blockTraceStats->cellItemMaxCountPerPixel, "cellItemMaxCountPerPixel" );
			V6_ASSERT( blockTraceStats->cellItemCount < m_config.cellItemCount );

			GPUBUffer_UnmapReadBack( gpuContext.deviceContext, &m_trace.traceStats );
		}
	}
}

void CRenderingDevice::BlendPixel( const RenderingView_s* view )
{
	// Render

	gpuContext.userDefinedAnnotation->BeginEvent( L"Blend Pixels");
	
	// set

	gpuContext.deviceContext->CSSetConstantBuffers( v6::hlsl::CBPixelSlot, 1, &gpuContext.constantBuffers[CONSTANT_BUFFER_PIXEL].buf );

	gpuContext.deviceContext->CSSetShaderResources( HLSL_BLOCK_CELL_ITEM_SLOT, 1, &m_trace.cellItems.srv );
	gpuContext.deviceContext->CSSetShaderResources( HLSL_BLOCK_CELL_ITEM_COUNT_SLOT, 1, &m_trace.cellItemCounters.srv );
	gpuContext.deviceContext->CSSetUnorderedAccessViews( HLSL_LCOLOR_SLOT, 1, &view->uav, nullptr );
	if ( g_showOverdraw	)
		gpuContext.deviceContext->CSSetShader( gpuContext.computes[COMPUTE_BLENDPIXEL_OVERDRAW].m_computeShader, nullptr, 0 );
	else
		gpuContext.deviceContext->CSSetShader( gpuContext.computes[COMPUTE_BLENDPIXEL].m_computeShader, nullptr, 0 );

	{
		v6::hlsl::CBPixel* cbPixel = ConstantBuffer_MapWrite< v6::hlsl::CBPixel >( gpuContext.deviceContext, &gpuContext.constantBuffers[CONSTANT_BUFFER_PIXEL] );

		cbPixel->c_pixelFrameSize.x = m_config.screenWidth;
		cbPixel->c_pixelFrameSize.y = m_config.screenHeight;

		if ( g_randomBackground )
		{
			const float r = core::RandFloat();
			cbPixel->c_pixelBackColor.x = 1.0f; 
			cbPixel->c_pixelBackColor.y = r;
			cbPixel->c_pixelBackColor.z = r;
		}
		else
		{
			cbPixel->c_pixelBackColor.x = 0.0f; 
			cbPixel->c_pixelBackColor.y = 0.0f; 
			cbPixel->c_pixelBackColor.z = 0.0f; 
		}

		cbPixel->c_eye = view->eye;

		ConstantBuffer_UnmapWrite( gpuContext.deviceContext, &gpuContext.constantBuffers[CONSTANT_BUFFER_PIXEL]  );
	}		

	V6_ASSERT( (m_width & 0x7) == 0 );
	V6_ASSERT( (m_height & 0x7) == 0 );
	const core::u32 pixelGroupWidth = m_width >> 3;
	const core::u32 pixelGroupHeight = m_height >> 3;
	gpuContext.deviceContext->Dispatch( pixelGroupWidth, pixelGroupHeight, 1 );

	// unset
	static const void* nulls[8] = {};
	gpuContext.deviceContext->CSSetShaderResources( HLSL_BLOCK_CELL_ITEM_SLOT, 1, (ID3D11ShaderResourceView**)nulls );
	gpuContext.deviceContext->CSSetShaderResources( HLSL_BLOCK_CELL_ITEM_COUNT_SLOT, 1, (ID3D11ShaderResourceView**)nulls );
	gpuContext.deviceContext->CSSetUnorderedAccessViews( HLSL_LCOLOR_SLOT, 1, (ID3D11UnorderedAccessView**)nulls, nullptr );

	gpuContext.userDefinedAnnotation->EndEvent();	
}

void CRenderingDevice::Output( ID3D11ShaderResourceView* srvLeft, ID3D11ShaderResourceView* srvRight )
{
#if HLSL_STEREO == 1
	// Render

	gpuContext.userDefinedAnnotation->BeginEvent( L"Compose Surface" );

	// set

	gpuContext.deviceContext->CSSetConstantBuffers( v6::hlsl::CBComposeSlot, 1, &gpuContext.constantBuffers[CONSTANT_BUFFER_COMPOSE].buf );

	gpuContext.deviceContext->CSSetShaderResources( HLSL_LCOLOR_SLOT, 1, &srvLeft );
	gpuContext.deviceContext->CSSetShaderResources( HLSL_RCOLOR_SLOT, 1, &srvRight );
	gpuContext.deviceContext->CSSetUnorderedAccessViews( HLSL_SURFACE_SLOT, 1, &gpuContext.surfaceUAV, nullptr );
		
	gpuContext.deviceContext->CSSetShader( gpuContext.computes[COMPUTE_COMPOSESURFACE].m_computeShader, nullptr, 0 );

	{
		v6::hlsl::CBCompose* cbCompose = ConstantBuffer_MapWrite< v6::hlsl::CBCompose >( gpuContext.deviceContext, &gpuContext.constantBuffers[CONSTANT_BUFFER_COMPOSE] );
		
		cbCompose->c_composeFrameWidth = m_config.screenWidth;

		ConstantBuffer_UnmapWrite( gpuContext.deviceContext, &gpuContext.constantBuffers[CONSTANT_BUFFER_COMPOSE]  );
	}		

	V6_ASSERT( (m_width & 0x7) == 0 );
	V6_ASSERT( (m_height & 0x7) == 0 );
	const core::u32 pixelGroupWidth = (m_width >> 3) * HLSL_EYE_COUNT;
	const core::u32 pixelGroupHeight = m_height >> 3;
	gpuContext.deviceContext->Dispatch( pixelGroupWidth, pixelGroupHeight, 1 );

	// unset
	static const void* nulls[8] = {};
	gpuContext.deviceContext->CSSetShaderResources( HLSL_LCOLOR_SLOT, 1, (ID3D11ShaderResourceView**)nulls );
	gpuContext.deviceContext->CSSetShaderResources( HLSL_RCOLOR_SLOT, 1, (ID3D11ShaderResourceView**)nulls );
	gpuContext.deviceContext->CSSetUnorderedAccessViews( HLSL_SURFACE_SLOT, 1, (ID3D11UnorderedAccessView**)nulls, nullptr );

	gpuContext.userDefinedAnnotation->EndEvent();	
#else	
	ID3D11Resource* colorBuffer;
	srvLeft->GetResource( &colorBuffer );
	gpuContext.deviceContext->CopyResource( gpuContext.surfaceBuffer, colorBuffer );
#endif
}

bool CRenderingDevice::ReadStream( core::u32 frame )
{
	if ( s_activeScene->filename[0] == 0 )
	{
		V6_ERROR( "Null scene file.\n" );
		return false;
	}

	bool hasError = false;

	char path[256];
	Scene_MakeStreamFilename( s_activeScene, path, sizeof( path ), frame );

	core::CFileReader fileReader;
	if ( fileReader.Open( path ) )
	{
		m_stack->push();

		core::CodecFrameDesc_s frameDesc;
		core::CodecFrameData_s frameData;

		if ( core::Codec_ReadFrame( &fileReader, &frameDesc, &frameData, m_stack ) )
		{
			if ( frameDesc.origin != s_buildOrigin )
			{
				V6_ERROR( "Stream origin is not compatible.\n" );
				hasError = true;
			}
			else if ( frameDesc.frame != frame )
			{
				V6_ERROR( "Stream frame is not compatible.\n" );
				hasError = true;
			}
			else if ( frameDesc.sampleCount != SAMPLE_MAX_COUNT )
			{
				V6_ERROR( "Stream sampleCount is not compatible.\n" );
				hasError = true;
			}
			else if ( frameDesc.gridMacroShift != HLSL_GRID_MACRO_SHIFT )
			{
				V6_ERROR( "Stream gridMacroShift is not compatible.\n" );
				hasError = true;
			}
			else if ( frameDesc.gridScaleMin != GRID_MIN_SCALE )
			{
				V6_ERROR( "Stream gridScaleMin is not compatible.\n" );
				hasError = true;
			}
			else if ( frameDesc.gridScaleMax != GRID_MAX_SCALE )
			{
				V6_ERROR( "Stream gridScaleMax is not compatible.\n" );
				hasError = true;
			}
			else 
			{
				if ( m_blockModeInitialized )
					Block_Release( gpuContext.device, &m_block );

				Block_CreateFromData( gpuContext.device, &m_block, frameData.blockPos, frameData.blockData, frameDesc.blockCounts );
				m_blockModeInitialized = true;

				V6_MSG( "Stream loaded from file %s.\n", path );
			}
		}
		else
		{
			V6_ERROR( "Unable to read file %s.\n", path );
			hasError = true;
		}

		m_stack->pop();
	}
	else
	{
		V6_ERROR( "Unable to open file %s.\n", path );
		hasError = true;
	}

	return !hasError;
}

bool CRenderingDevice::WriteStream( const core::u32 blockCounts[HLSL_BUCKET_COUNT], core::u32 frame )
{
	if ( s_activeScene->filename[0] == 0 )
	{
		V6_ERROR( "Null scene file.\n" );
		return false;
	}

	bool hasError = false;

	Stream_s stream;
	Stream_Create( gpuContext.device, &stream, &m_config, m_heap, blockCounts );

	static const void* nulls[8] = {};

	gpuContext.userDefinedAnnotation->BeginEvent( L"Stream");

#if V6_SORTED_STREAM == 1
		
	for ( core::u32 bucket = 0; bucket < HLSL_BUCKET_COUNT; ++bucket )
	{
		gpuContext.userDefinedAnnotation->BeginEvent( L"Stream Bucket");

		// clear

		core::u32 values[4] = {};
		gpuContext.deviceContext->ClearUnorderedAccessViewUint( stream.groupBits.uav, values );
		gpuContext.deviceContext->ClearUnorderedAccessViewUint( stream.bits.uav, values );
		gpuContext.deviceContext->ClearUnorderedAccessViewUint( stream.counts.uav, values );
		gpuContext.deviceContext->ClearUnorderedAccessViewUint( stream.addresses.uav, values );

		// step #1: Set Bit

		{
			gpuContext.userDefinedAnnotation->BeginEvent( L"Set Bit");
		
			// set
			gpuContext.deviceContext->CSSetShaderResources( HLSL_BLOCK_POS_SLOT, 1, &m_block.blockPos.srv );
			gpuContext.deviceContext->CSSetShaderResources( HLSL_BLOCK_INDIRECT_ARGS_SLOT, 1, &m_block.blockIndirectArgs.srv );
			gpuContext.deviceContext->CSSetUnorderedAccessViews( HLSL_STREAM_GROUP_BITS_SLOT, 1, &stream.groupBits.uav, nullptr );
			gpuContext.deviceContext->CSSetUnorderedAccessViews( HLSL_STREAM_BITS_SLOT, 1, &stream.bits.uav, nullptr );

			// dispach
			gpuContext.deviceContext->CSSetShader( gpuContext.computes[COMPUTE_STREAM_SETBIT4+bucket].m_computeShader, nullptr, 0 );
			gpuContext.deviceContext->DispatchIndirect( m_block.blockIndirectArgs.buf, block_groupCountX_offset( bucket ) * sizeof( core::u32 ) );

			// Unset
			gpuContext.deviceContext->CSSetShaderResources( HLSL_BLOCK_POS_SLOT, 1, (ID3D11ShaderResourceView**)nulls );
			gpuContext.deviceContext->CSSetShaderResources( HLSL_BLOCK_INDIRECT_ARGS_SLOT, 1, (ID3D11ShaderResourceView**)nulls );
			gpuContext.deviceContext->CSSetUnorderedAccessViews( HLSL_STREAM_GROUP_BITS_SLOT, 1, (ID3D11UnorderedAccessView**)nulls, nullptr );
			gpuContext.deviceContext->CSSetUnorderedAccessViews( HLSL_STREAM_BITS_SLOT, 1, (ID3D11UnorderedAccessView**)nulls, nullptr );

			gpuContext.userDefinedAnnotation->EndEvent();
		}

		// step#2: Prefix Sum

		{
			gpuContext.userDefinedAnnotation->BeginEvent( L"Prefix Sum");

			// set
			gpuContext.deviceContext->CSSetShaderResources( HLSL_STREAM_GROUP_BITS_SLOT, 1, &stream.groupBits.srv );
			gpuContext.deviceContext->CSSetShaderResources( HLSL_STREAM_BITS_SLOT, 1, &stream.bits.srv );
			gpuContext.deviceContext->CSSetUnorderedAccessViews( HLSL_STREAM_COUNTS_SLOT, 1, &stream.counts.uav, nullptr );

			gpuContext.deviceContext->CSSetConstantBuffers( v6::hlsl::CBStreamSlot, 1, &gpuContext.constantBuffers[CONSTANT_BUFFER_STREAM].buf );

			core::u32 elementCount = HLSL_STREAM_SIZE;
			core::u32 streamCurrentOffset = 0;
			core::u32 streamLowerOffset = 0;
			for ( core::u32 layer = 0; layer < HLSL_STREAM_LAYER_COUNT; ++layer, elementCount >>= HLSL_STREAM_THREAD_GROUP_SHIFT )
			{
				V6_ASSERT( elementCount > 0 );

				streamCurrentOffset = streamLowerOffset;
				streamLowerOffset += elementCount;

				gpuContext.userDefinedAnnotation->BeginEvent( L"Prefix Layer");

				// init
				{
					v6::hlsl::CBStream* cbStream = ConstantBuffer_MapWrite< v6::hlsl::CBStream >( gpuContext.deviceContext, &gpuContext.constantBuffers[CONSTANT_BUFFER_STREAM] );

					cbStream->c_streamCurrentOffset = streamCurrentOffset;
					cbStream->c_streamLowerOffset = streamLowerOffset;

					ConstantBuffer_UnmapWrite( gpuContext.deviceContext, &gpuContext.constantBuffers[CONSTANT_BUFFER_STREAM] );
				}

				// dispach
				gpuContext.deviceContext->CSSetShader( gpuContext.computes[COMPUTE_STREAM_PREFIX_SUM].m_computeShader, nullptr, 0 );
				gpuContext.deviceContext->Dispatch( GROUP_COUNT( elementCount, HLSL_STREAM_THREAD_GROUP_SIZE ), 1, 1 );

				gpuContext.userDefinedAnnotation->EndEvent();
			}
			V6_ASSERT( elementCount <= 1 );

			// unset
			gpuContext.deviceContext->CSSetShaderResources( HLSL_STREAM_GROUP_BITS_SLOT, 1, (ID3D11ShaderResourceView**)nulls );
			gpuContext.deviceContext->CSSetShaderResources( HLSL_STREAM_BITS_SLOT, 1, (ID3D11ShaderResourceView**)nulls );
			gpuContext.deviceContext->CSSetUnorderedAccessViews( HLSL_STREAM_COUNTS_SLOT, 1, (ID3D11UnorderedAccessView**)nulls, nullptr );

			gpuContext.userDefinedAnnotation->EndEvent();
		}

		// step#3: Summarize

		{
			gpuContext.userDefinedAnnotation->BeginEvent( L"Summarize");

			// set
			gpuContext.deviceContext->CSSetShaderResources( HLSL_STREAM_GROUP_BITS_SLOT, 1, &stream.groupBits.srv );
			gpuContext.deviceContext->CSSetShaderResources( HLSL_STREAM_COUNTS_SLOT, 1, &stream.counts.srv );
			gpuContext.deviceContext->CSSetUnorderedAccessViews( HLSL_STREAM_ADDRESSES_SLOT, 1, &stream.addresses.uav, nullptr );

			// dispach
			gpuContext.deviceContext->CSSetShader( gpuContext.computes[COMPUTE_STREAM_SUMMARIZE].m_computeShader, nullptr, 0 );
			gpuContext.deviceContext->Dispatch( GROUP_COUNT( HLSL_STREAM_SIZE, HLSL_STREAM_THREAD_GROUP_SIZE ), 1, 1 );

			// unset
			gpuContext.deviceContext->CSSetShaderResources( HLSL_STREAM_GROUP_BITS_SLOT, 1, (ID3D11ShaderResourceView**)nulls );
			gpuContext.deviceContext->CSSetShaderResources( HLSL_STREAM_COUNTS_SLOT, 1, (ID3D11ShaderResourceView**)nulls );
			gpuContext.deviceContext->CSSetUnorderedAccessViews( HLSL_STREAM_ADDRESSES_SLOT, 1, (ID3D11UnorderedAccessView**)nulls, nullptr );

			gpuContext.userDefinedAnnotation->EndEvent();
		}

		// step #1: Scatter

		{
			gpuContext.userDefinedAnnotation->BeginEvent( L"Scatter");

			// set
			gpuContext.deviceContext->CSSetShaderResources( HLSL_BLOCK_POS_SLOT, 1, &m_block.blockPos.srv );
			gpuContext.deviceContext->CSSetShaderResources( HLSL_BLOCK_DATA_SLOT, 1, &m_block.blockData.srv );
			gpuContext.deviceContext->CSSetShaderResources( HLSL_BLOCK_INDIRECT_ARGS_SLOT, 1, &m_block.blockIndirectArgs.srv );
			gpuContext.deviceContext->CSSetShaderResources( HLSL_STREAM_BITS_SLOT, 1, &stream.bits.srv );
			gpuContext.deviceContext->CSSetShaderResources( HLSL_STREAM_ADDRESSES_SLOT, 1, &stream.addresses.srv );
			gpuContext.deviceContext->CSSetUnorderedAccessViews( HLSL_STREAM_BLOCK_POS_SLOT, 1, &stream.blockPos.uav, nullptr );
			gpuContext.deviceContext->CSSetUnorderedAccessViews( HLSL_STREAM_BLOCK_DATA_SLOT, 1, &stream.blockData.uav, nullptr );

			// dispach
			gpuContext.deviceContext->CSSetShader( gpuContext.computes[COMPUTE_STREAM_SCATTER4+bucket].m_computeShader, nullptr, 0 );
			gpuContext.deviceContext->DispatchIndirect( m_block.blockIndirectArgs.buf, block_groupCountX_offset( bucket ) * sizeof( core::u32 ) );

			// Unset
			gpuContext.deviceContext->CSSetShaderResources( HLSL_BLOCK_POS_SLOT, 1, (ID3D11ShaderResourceView**)nulls );
			gpuContext.deviceContext->CSSetShaderResources( HLSL_BLOCK_DATA_SLOT, 1, (ID3D11ShaderResourceView**)nulls );
			gpuContext.deviceContext->CSSetShaderResources( HLSL_BLOCK_INDIRECT_ARGS_SLOT, 1, (ID3D11ShaderResourceView**)nulls );
			gpuContext.deviceContext->CSSetShaderResources( HLSL_STREAM_BITS_SLOT, 1, (ID3D11ShaderResourceView**)nulls );
			gpuContext.deviceContext->CSSetShaderResources( HLSL_STREAM_ADDRESSES_SLOT, 1, (ID3D11ShaderResourceView**)nulls );
			gpuContext.deviceContext->CSSetUnorderedAccessViews( HLSL_STREAM_BLOCK_POS_SLOT, 1, (ID3D11UnorderedAccessView**)nulls, nullptr );
			gpuContext.deviceContext->CSSetUnorderedAccessViews( HLSL_STREAM_BLOCK_DATA_SLOT, 1, (ID3D11UnorderedAccessView**)nulls, nullptr );

			gpuContext.userDefinedAnnotation->EndEvent();
		}

		gpuContext.userDefinedAnnotation->EndEvent();
	}

#else // #if V6_SORTED_STREAM == 1

	{		
		D3D11_BOX box = {};
		box.left = 0;
		box.top = 0;
		box.front = 0;
		box.right = stream.blockCount * sizeof( core::u32 );
		box.bottom = 1;
		box.back = 1;
		gpuContext.deviceContext->CopySubresourceRegion( stream.blockPos.buf, 0, 0, 0, 0, m_block.blockPos.buf , 0, &box );
	}

	{		
		D3D11_BOX box = {};
		box.left = 0;
		box.top = 0;
		box.front = 0;
		box.right = stream.cellCount * sizeof( core::u32 );
		box.bottom = 1;
		box.back = 1;
		gpuContext.deviceContext->CopySubresourceRegion( stream.blockData.buf, 0, 0, 0, 0, m_block.blockData.buf , 0, &box );
	}

#endif

	gpuContext.userDefinedAnnotation->EndEvent();

	{
		char path[256];
		Scene_MakeStreamFilename( s_activeScene, path, sizeof( path ), frame );

		core::CFileWriter fileWriter;
		if ( fileWriter.Open( path ) )
		{
			core::CodecFrameDesc_s frameDesc = {};
			frameDesc.origin = s_buildOrigin;
			frameDesc.frame = frame;
			frameDesc.sampleCount = SAMPLE_MAX_COUNT;
			frameDesc.gridMacroShift = HLSL_GRID_MACRO_SHIFT;
			frameDesc.gridScaleMin = GRID_MIN_SCALE;
			frameDesc.gridScaleMax = GRID_MAX_SCALE;
			memcpy( frameDesc.blockCounts, blockCounts, sizeof( frameDesc.blockCounts ) );
			
			core::CodecFrameData_s frameData = {};
			frameData.blockPos = (void*)GPUBUffer_MapReadBack< core::u32 >( gpuContext.deviceContext, &stream.blockPos );
			frameData.blockData = (void*)GPUBUffer_MapReadBack< core::u32 >( gpuContext.deviceContext, &stream.blockData );

			core::Codec_WriteFrame( &fileWriter, &frameDesc, &frameData );
	
			GPUBUffer_UnmapReadBack( gpuContext.deviceContext, &stream.blockPos );
			GPUBUffer_UnmapReadBack( gpuContext.deviceContext, &stream.blockData );

			V6_MSG( "Stream saved to file %s.\n", path );
		}
		else
		{
			V6_ERROR( "Unable to create file %s.\n", path );
			hasError = true;
		}
	}

	Stream_Release( gpuContext.device, &stream );
	
	return !hasError;
}

void CRenderingDevice::BuildBlock( core::u32 frame )
{
	static core::u32 lastSumLeafCount = 0;

	if ( g_sample == 0 )
	{
		s_buildOrigin = s_headOffset;

		if ( m_blockModeInitialized )
		{
			Block_Release( gpuContext.device, &m_block );
			Trace_Release( gpuContext.device, &m_trace );
			m_blockModeInitialized = false;
		}

#if 1
		if ( ReadStream( frame ) )
		{
			Trace_Create( gpuContext.device, &m_trace, &m_config, m_heap );

			g_sample = SAMPLE_MAX_COUNT;
			return;
		}
#endif

		Cube_Create( gpuContext.device, &m_cube, CUBE_SIZE );
		Sample_Create( gpuContext.device, &m_sample, &m_config, m_heap );
		Octree_Create( gpuContext.device, &m_octree, &m_config, m_heap );

		lastSumLeafCount = 0;
		ClearNode();
	}

	V6_MSG( "Capturing sample #%03d...", g_sample );

	const core::Vec3 sampleOffset = m_sampleOffsets[g_sample];
	const core::Vec3 samplePos = s_buildOrigin + sampleOffset;

	core::u32 sumLeafCount = 0;

	for ( core::u32 faceID = 0; faceID < CUBE_AXIS_COUNT; ++faceID )
	{
		Capture( &samplePos, faceID );
		Collect( &samplePos, faceID );					
		sumLeafCount = BuildNode();
		FillLeaf();
	}

	const core::u32 newLeafCount = sumLeafCount - lastSumLeafCount;
	lastSumLeafCount = sumLeafCount;

	V6_MSG( "\r" );
	V6_MSG( "Captured  sample #%03d: %13s cells added\n", g_sample, FormatInteger_Unsafe( newLeafCount ) );

	++g_sample;

	v6::viewer::GPUQuery_WriteTimeStamp( gpuContext.deviceContext, &gpuContext.pendingQueries[v6::viewer::QUERY_T1] );

	if ( g_sample == SAMPLE_MAX_COUNT )
	{
		V6_MSG( "Packing all samples..." );

		Cube_Release( &m_cube );
		Sample_Release( gpuContext.device, &m_sample );

		Block_Create( gpuContext.device, &m_block, &m_config, m_heap );
		m_blockModeInitialized = true;

		core::u32 blockCounts[HLSL_BUCKET_COUNT] = {};
		PackColor( blockCounts );

		Octree_Release( gpuContext.device, &m_octree );

		if ( WriteStream( blockCounts, frame ) )
			ReadStream( frame );

		Trace_Create( gpuContext.device, &m_trace, &m_config, m_heap );

		V6_MSG( "\r" );
		V6_MSG( "Packed  all samples: %13s cells added @ frame %d\n", FormatInteger_Unsafe( sumLeafCount ), frame );
		s_logReadBack = false;
#if 0
		// bake always
		g_sample = 0;
#endif
	}
}

void CRenderingDevice::Draw( float dt )
{
	static int lastKeyPosX = -1;
	static int lastKeyPosZ = -1;
	
	int mouseDeltaX = 0;
	int mouseDeltaY = 0;
	int keyDeltaX = 0;
	int keyDeltaZ = 0;

	if ( g_mousePressed )
	{		
		mouseDeltaX = g_mouseDeltaX;
		mouseDeltaY = g_mouseDeltaY;
		g_mouseDeltaX = 0;
		g_mouseDeltaY = 0;
	}
	else
	{
		mouseDeltaX = 0;
		mouseDeltaY = 0;
	}

	if ( g_keyLeftPressed != g_keyRightPressed )
		keyDeltaX = g_keyLeftPressed ? -1 : 1;

	if ( g_keyDownPressed != g_keyUpPressed )
		keyDeltaZ = g_keyDownPressed ? -1 : 1;
		
	const static float MOUSE_ROTATION_SPEED = 0.5f;
	
	core::Mat4x4 orientationMatrix;
#if V6_USE_HMD
	HmdRenderTarget_s renderTargets[2];
	HmdEyePose_s eyePoses[2];
	s_hmdState = v6::viewer::Hmd_BeginRendering( renderTargets, eyePoses, ZNEAR, ZFAR );
	if ( s_hmdState & HMD_TRACKING_STATE_ON )
	{
		const core::Mat4x4 yawMatrix = core::Mat4x4_RotationY( s_yaw );
		core::Mat4x4_Mul3x3( &orientationMatrix, yawMatrix, eyePoses[0].lookAt );
		core::Mat4x4_Transpose( &orientationMatrix );
	}
	else
#endif // #if V6_USE_HMD
	{
		s_yaw += -mouseDeltaX * MOUSE_ROTATION_SPEED * dt;
		s_pitch += -mouseDeltaY * MOUSE_ROTATION_SPEED * dt;

		const core::Mat4x4 yawMatrix = core::Mat4x4_RotationY( s_yaw );
		const core::Mat4x4 pitchMatrix = core::Mat4x4_RotationX( s_pitch );
		core::Mat4x4_Mul( &orientationMatrix, yawMatrix, pitchMatrix );
		core::Mat4x4_Transpose( &orientationMatrix );
	}

	const core::Vec3 forward = -orientationMatrix.GetZAxis()->Normalized();
	const core::Vec3 right = orientationMatrix.GetXAxis()->Normalized();
	const core::Vec3 up = orientationMatrix.GetYAxis()->Normalized();

	if ( keyDeltaX || keyDeltaZ )
	{
		s_headOffset += right * (float)keyDeltaX * g_translation_speed * dt;
		s_headOffset += forward * (float)keyDeltaZ * g_translation_speed * dt;
	}
	
	if ( g_limit )
	{
		core::Vec3 distanceToCenter = s_headOffset - s_buildOrigin;
		distanceToCenter.x = core::Clamp( distanceToCenter.x, -FREE_SCALE, FREE_SCALE );
		distanceToCenter.y = core::Clamp( distanceToCenter.y, -FREE_SCALE, FREE_SCALE );
		distanceToCenter.z = core::Clamp( distanceToCenter.z, -FREE_SCALE, FREE_SCALE );
		s_headOffset = s_buildOrigin + distanceToCenter;
	}

	if ( s_cameraPath.dirty && (g_showCameraPath || g_playCameraPath) )
	{
		CameraPath_Compute( &s_cameraPath, g_cameraSpeed );
		Scene_UpdateCameraPath( m_cameraPathScene, &s_cameraPath, gpuContext.deviceContext );
	}

	if ( g_playCameraPath )
		s_headOffset = CameraPath_GetPosition( &s_cameraPath, g_playCameraPathTime );

	RenderingView_s views[HLSL_EYE_COUNT];
#if V6_USE_HMD
	if ( s_hmdState & HMD_TRACKING_STATE_ON )
	{
		for ( core::u32 eye = 0; eye < HLSL_EYE_COUNT; ++eye )
		{
			RenderingView_MakeForHMD( &views[eye], &eyePoses[eye], &s_headOffset, s_yaw, eye );
			views[eye].texture2D = (ID3D11Texture2D*)renderTargets[eye].texture2D;
			views[eye].rtv = (ID3D11RenderTargetView*)renderTargets[eye].rtv;
			views[eye].uav = (ID3D11UnorderedAccessView*)renderTargets[eye].uav;
		}
	}
	else
#endif // #if V6_USE_HMD
	{
		for ( core::u32 eye = 0; eye < HLSL_EYE_COUNT; ++eye )
		{
			RenderingView_MakeForStereo( &views[eye], &s_headOffset, &forward, &up, &right, eye, m_aspectRatio );
			views[eye].texture2D = gpuContext.colorBuffers[eye];
			views[eye].rtv = gpuContext.colorViews[eye];
			views[eye].uav = gpuContext.colorUAVs[eye];
		}
	}

	if ( g_drawMode == DRAW_MODE_DEFAULT )
	{
		v6::viewer::GPUQuery_WriteTimeStamp( gpuContext.deviceContext, &gpuContext.pendingQueries[v6::viewer::QUERY_T0] );

		for ( core::u32 eye = 0; eye < HLSL_EYE_COUNT; ++eye )
			DrawWorld( &views[eye] );
		
		v6::viewer::GPUQuery_WriteTimeStamp( gpuContext.deviceContext, &gpuContext.pendingQueries[v6::viewer::QUERY_T1] );
		v6::viewer::GPUQuery_WriteTimeStamp( gpuContext.deviceContext, &gpuContext.pendingQueries[v6::viewer::QUERY_T2] );
		v6::viewer::GPUQuery_WriteTimeStamp( gpuContext.deviceContext, &gpuContext.pendingQueries[v6::viewer::QUERY_T3] );

		if ( g_playCameraPath )
		{
			g_playCameraPathTime += dt;
			++g_playCameraPathFrame;
		}
	}	
	else if ( g_drawMode == DRAW_MODE_BLOCK )
	{		
		if ( g_sample < SAMPLE_MAX_COUNT )
		{
			v6::viewer::GPUQuery_WriteTimeStamp( gpuContext.deviceContext, &gpuContext.pendingQueries[v6::viewer::QUERY_T0] );

			BuildBlock( g_playCameraPath ? g_playCameraPathFrame : 0 );

			v6::viewer::GPUQuery_WriteTimeStamp( gpuContext.deviceContext, &gpuContext.pendingQueries[v6::viewer::QUERY_T2] );
			v6::viewer::GPUQuery_WriteTimeStamp( gpuContext.deviceContext, &gpuContext.pendingQueries[v6::viewer::QUERY_T3] );

			if ( g_playCameraPath && g_sample == SAMPLE_MAX_COUNT )
			{
				g_sample = 0;
				g_playCameraPathTime += 1.0f / HMD_FPS;
				++g_playCameraPathFrame;
			}
		}
		else
		{
			v6::viewer::GPUQuery_WriteTimeStamp( gpuContext.deviceContext, &gpuContext.pendingQueries[v6::viewer::QUERY_T0] );
			
			CullBlock( views, &s_buildOrigin );

			v6::viewer::GPUQuery_WriteTimeStamp( gpuContext.deviceContext, &gpuContext.pendingQueries[v6::viewer::QUERY_T1] );

			TraceBlock( views, &s_buildOrigin );

			v6::viewer::GPUQuery_WriteTimeStamp( gpuContext.deviceContext, &gpuContext.pendingQueries[v6::viewer::QUERY_T2] );

			for ( core::u32 eye = 0; eye < HLSL_EYE_COUNT; ++eye )
				BlendPixel( &views[eye] );

			v6::viewer::GPUQuery_WriteTimeStamp( gpuContext.deviceContext, &gpuContext.pendingQueries[v6::viewer::QUERY_T3] );

			s_logReadBack = false;
			g_mousePicked = false;

			if ( g_playCameraPath )
			{
				g_playCameraPathTime += 1.0f / HMD_FPS;
				++g_playCameraPathFrame;
			}
		}
	}	
	
#if V6_USE_HMD
	if ( s_hmdState & HMD_TRACKING_STATE_ON )
	{
		HmdOuput_s output;
		if ( Hmd_EndRendering( &output ) )
			gpuContext.deviceContext->CopyResource( gpuContext.surfaceBuffer, (ID3D11Texture2D*)output.texture2D );
	}
	else
#endif // #if V6_USE_HMD
	{
		if ( g_showCameraPath )
		{
			for ( core::u32 eye = 0; eye < HLSL_EYE_COUNT; ++eye )
				DrawCameraPath( &views[eye] );
		}

		for ( core::u32 eye = 0; eye < HLSL_EYE_COUNT; ++eye )
			DrawDebug( &views[eye] );

		Output( gpuContext.colorSRVs[LEFT_EYE], gpuContext.colorSRVs[RIGHT_EYE] );
	}

	if ( g_playCameraPath && g_playCameraPathTime >= s_cameraPath.duration || g_playCameraPathFrame >= VIDEO_FRAME_MAX_COUNT )
	{
		g_sample = SAMPLE_MAX_COUNT;
		g_playCameraPath = false;
	}

}

void CRenderingDevice::Present()
{
	gpuContext.swapChain->Present( 0, 0 );
}

void CRenderingDevice::Release()
{
	if ( m_blockModeInitialized )
	{
		Block_Release( gpuContext.device, &m_block );
		Trace_Release( gpuContext.device, &m_trace );
	}

	Scene_Release( m_defaultScene );
	m_heap->deleteInstance( m_defaultScene );

	Scene_Release( m_debugScene );
	m_heap->deleteInstance( m_debugScene );

	Scene_Release( m_cameraPathScene );
	m_heap->deleteInstance( m_cameraPathScene );

	CameraPath_Release( &s_cameraPath );

	GPUContext_Release( &gpuContext );
}

END_V6_VIEWER_NAMESPACE

int main()
{
	V6_MSG( "Viewer 0.1\n" );

	v6::core::CHeap heap;
	v6::core::Stack stack( &heap, 100 * 1024 * 1024 );
	v6::core::CFileSystem filesystem;
		
#if V6_LOAD_EXTERNAL == 1
	v6::core::Stack stackScene( &heap, 400 * 1024 * 1024 );

	const char* filename = "D:/media/obj/crytek-sponza/sponza.obj";
	//const char* filename = "D:/media/obj/dragon/dragon.obj";
	//const char* filename = "D:/media/obj/buddha/buddha.obj";
	//const char* filename = "D:/media/obj/head/head.obj";
	//const char* filename = "D:/media/obj/hairball/hairball.obj"; // 100.0f
	//const char* filename = "D:/media/obj/hairball/hairball_simple.obj"; // 100.0f
	//const char* filename = "D:/media/obj/hairball/hairball_simple2.obj"; // 100.0f
	//const char* filename = "D:/media/obj/sibenik/sibenik.obj"; // 100.0f
	//const char* filename = "D:/media/obj/conference/conference.obj";

	v6::viewer::SceneContext_s sceneContext;
	SceneContext_Create( &sceneContext, &stackScene );
	SceneContext_SetFilename( &sceneContext, filename );

	v6::core::Job_Launch( v6::viewer::SceneContext_Load, &sceneContext );
#endif	

	const char* const title = "V6";

#if V6_USE_HMD
	if ( !v6::viewer::Hmd_Init() )
	{
		V6_ERROR( "Call to Hmd_Init failed!\n" );
		return -1;
	}

	v6::core::Vec2i renterTargerSize = v6::viewer::Hmd_GetRecommendedRenderTargetSize();
	v6::core::u32 maxRenderTargetSize = HLSL_GRID_WIDTH >> 1;
	if ( renterTargerSize.x > (int)maxRenderTargetSize )
	{
		renterTargerSize.y = (renterTargerSize.y * maxRenderTargetSize) / renterTargerSize.x;
		renterTargerSize.x = maxRenderTargetSize;
		
	}
	if ( renterTargerSize.y > (int)maxRenderTargetSize)
	{
		renterTargerSize.x = (renterTargerSize.x  * maxRenderTargetSize) / renterTargerSize.y;
		renterTargerSize.y = maxRenderTargetSize;
	}
	renterTargerSize.x = (renterTargerSize.x + 7) & ~7;
	renterTargerSize.y = (renterTargerSize.y + 7) & ~7;
#else
	v6::core::Vec2i renterTargerSize = v6::core::Vec2i_Make( HLSL_GRID_WIDTH >> 1, HLSL_GRID_WIDTH >> 1 );
#endif // #if V6_USE_HMD

	HWND hWnd = v6::viewer::CreateMainWindow( title, renterTargerSize.x * HLSL_EYE_COUNT, renterTargerSize.y );
	if (!hWnd)
	{
		V6_ERROR( "Call to CreateWindow failed!\n" );
		return -1;
	}

	if ( !v6::viewer::CaptureInputs( hWnd ) )
	{
		V6_ERROR( "Call to CaptureInputs failed!\n" );
		return -1;
	}

	v6::viewer::CRenderingDevice oRenderingDevice;
	if ( !oRenderingDevice.Create( renterTargerSize.x, renterTargerSize.y, hWnd, &filesystem, &heap, &stack ) )
	{
		V6_ERROR( "Call to CRenderingDevice::Create failed!\n" );
		return -1;
	}

#if V6_USE_HMD
	if ( !v6::viewer::Hmd_CreateResources( oRenderingDevice.gpuContext.device, &renterTargerSize ) )
	{
		V6_ERROR( "Call to Hmd_CreateResources failed!\n" );
		return -1;
	}
#endif // #if V6_USE_HMD

#if V6_LOAD_EXTERNAL == 1
	SceneContext_SetDevice( &sceneContext, oRenderingDevice.gpuContext.device );
#endif

	ShowWindow(hWnd, SW_SHOWNORMAL);
	UpdateWindow(hWnd);

	__int64 frameTickLast = GetTickCount(); 
	for ( __int64 frameId = 0; ; ++frameId )
	{
		const v6::core::u32 bufferID = frameId & 1;

		v6::viewer::GPUQuery_s* pendingQueries = oRenderingDevice.gpuContext.queries[bufferID];
		static const int tMaxCount = 64;
		static float dts[tMaxCount] = {};
		static float tfTimes[tMaxCount] = {};
		static float t0Times[tMaxCount] = {};
		static float t1Times[tMaxCount] = {};
		static float t2Times[tMaxCount] = {};
		static int tID = 0;
		
		if ( v6::viewer::GPUQuery_ReadTimeStampDisjoint( oRenderingDevice.gpuContext.deviceContext, &pendingQueries[v6::viewer::QUERY_FREQUENCY] ) )
		{
			for ( v6::core::u32 queryID = v6::viewer::QUERY_FRAME_BEGIN; queryID < v6::viewer::QUERY_COUNT; ++queryID )
				v6::viewer::GPUQuery_ReadTimeStamp( oRenderingDevice.gpuContext.deviceContext, &pendingQueries[queryID] );
			
			tfTimes[tID] = v6::viewer::GPUQuery_GetElpasedTime( &pendingQueries[v6::viewer::QUERY_FRAME_BEGIN], &pendingQueries[v6::viewer::QUERY_FRAME_END], &pendingQueries[v6::viewer::QUERY_FREQUENCY] );
			t0Times[tID] = v6::viewer::GPUQuery_GetElpasedTime( &pendingQueries[v6::viewer::QUERY_T0], &pendingQueries[v6::viewer::QUERY_T1], &pendingQueries[v6::viewer::QUERY_FREQUENCY] );
			t1Times[tID] = v6::viewer::GPUQuery_GetElpasedTime( &pendingQueries[v6::viewer::QUERY_T1], &pendingQueries[v6::viewer::QUERY_T2], &pendingQueries[v6::viewer::QUERY_FREQUENCY] );
			t2Times[tID] = v6::viewer::GPUQuery_GetElpasedTime( &pendingQueries[v6::viewer::QUERY_T2], &pendingQueries[v6::viewer::QUERY_T3], &pendingQueries[v6::viewer::QUERY_FREQUENCY] );
			tID = (tID + 1) & (tMaxCount-1);
		}
		
		oRenderingDevice.gpuContext.pendingQueries = pendingQueries;
		
		const __int64 frameTick = v6::core::GetTickCount();
		
		__int64 frameUpdatedTick = frameTick;
		float dt = 0.0f;
		for (;;)
		{
			const __int64 frameDelta = frameUpdatedTick - frameTickLast;
			dt = v6::core::Min( v6::core::ConvertTicksToSeconds( frameDelta ), 0.1f );
#if !V6_USE_HMD
			if ( dt + 0.0001f >= 1.0f / v6::viewer::HMD_FPS )
				break;
			SwitchToThread();
			frameUpdatedTick = v6::core::GetTickCount();
#endif // #if !V6_USE_HMD
		}
		dts[frameId & (tMaxCount-1)] = dt;

		frameTickLast = frameTick;

		if ( (frameId % 30) == 0 || v6::viewer::IsBakingMode( v6::viewer::g_drawMode ) ) 
		{
			float ifps = 0;
			float tfTime = 0;
			float t0Time = 0;
			float t1Time = 0;
			float t2Time = 0;
				
			for ( int t = 0; t < tMaxCount; ++t )
			{
				ifps += dts[t];
				tfTime += tfTimes[t];
				t0Time += t0Times[t];
				t1Time += t1Times[t];
				t2Time += t2Times[t];
			}

			ifps *= 1.0f / tMaxCount;
			tfTime *= 1.0f / tMaxCount;
			t0Time *= 1.0f / tMaxCount;
			t1Time *= 1.0f / tMaxCount;
			t2Time *= 1.0f / tMaxCount;

			char text[1024];
			sprintf_s( text, sizeof( text ), "%s | fps: %3u | tf: %4u | t0: %4u | t1: %4u | t2: %4u | %s | Hmd %d %s", 
				title, 
				(int)(1.0f / ifps), 
				(int)(tfTime * 1000000.0f),
				(int)(t0Time * 1000000.0f),
				(int)(t1Time * 1000000.0f),
				(int)(t2Time * 1000000.0f),
				v6::viewer::ModeToString( v6::viewer::g_drawMode ),
				v6::viewer::s_hmdState,
				v6::viewer::s_activeScene->info.dirty ? "| *" : "" );
			SetWindowTextA( hWnd, text );
		}
		
		MSG msg;
		while ( PeekMessage(&msg, NULL, 0, 0, PM_REMOVE) )
		{
			TranslateMessage(&msg);
			DispatchMessageA(&msg);

			if ( msg.message == WM_QUIT )
			{
				oRenderingDevice.Release();
#if V6_LOAD_EXTERNAL == 1
				v6::core::Signal_Wait( &sceneContext.loadDone );
				Scene_SaveInfo( v6::viewer::s_activeScene );
				SceneContext_Release( &sceneContext );
#endif
				return 0;
			}
		}

		if ( v6::viewer::g_reloadShaders )
		{
			v6::viewer::GPUContext_ReleaseShaders( &oRenderingDevice.gpuContext );
			v6::viewer::GPUContext_CreateShaders( &oRenderingDevice.gpuContext, &filesystem, &stack );
			v6::viewer::g_reloadShaders = false;
		}
				
		v6::viewer::GPUQuery_BeginTimeStampDisjoint( oRenderingDevice.gpuContext.deviceContext, &pendingQueries[v6::viewer::QUERY_FREQUENCY] );
		v6::viewer::GPUQuery_WriteTimeStamp( oRenderingDevice.gpuContext.deviceContext, &pendingQueries[v6::viewer::QUERY_FRAME_BEGIN] );

		oRenderingDevice.Draw( dt );

		v6::viewer::GPUQuery_WriteTimeStamp( oRenderingDevice.gpuContext.deviceContext, &pendingQueries[v6::viewer::QUERY_FRAME_END] );
		v6::viewer::GPUQuery_EndTimeStampDisjoint( oRenderingDevice.gpuContext.deviceContext, &pendingQueries[v6::viewer::QUERY_FREQUENCY] );

		oRenderingDevice.Present();
	}

#if V6_USE_HMD
	v6::viewer::Hmd_ReleaseResources();

	v6::viewer::Hmd_Shutdown();
#endif // #if V6_USE_HMD
}
