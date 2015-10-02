/*V6*/

#pragma warning( push, 3 )
#include <windows.h>
#include <Windowsx.h>
#include <d3d11_1.h>
#pragma warning( pop )

#include <v6/viewer/common.h>
#include <v6/viewer/common_shared.h>

#include <v6/core/color.h>
#include <v6/core/filesystem.h>
#include <v6/core/image.h>
#include <v6/core/math.h>
#include <v6/core/mat4x4.h>
#include <v6/core/memory.h>
#include <v6/core/stream.h>
#include <v6/core/time.h>
#include <v6/core/vec2.h>
#include <v6/core/vec3.h>

#pragma comment(lib, "d3d11.lib")

#define V6_D3D_DEBUG			0

#define V6_ASSERT_D3D11( EXP )  { HRESULT hRes = EXP; V6_ASSERT( hRes == S_OK ); }

#define KB( X )					((X) >> 10)
#define MB( X )					((X) >> 20)
#define GB( X )					((X) >> 30)

BEGIN_V6_VIEWER_NAMESPACE

enum DrawMode_e
{
	DRAW_MODE_DEFAULT,
	DRAW_MODE_CUBE,
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
	COMPUTE_SAMPLECOLLECT,
	COMPUTE_BUILDINNER,
	COMPUTE_BUILDLEAF,
	COMPUTE_FILLLEAF,
	COMPUTE_PACKCOLOR,
	COMPUTE_FILTERPIXEL,

	COMPUTE_COUNT
};

enum
{
	SHADER_BASIC,
	SHADER_CUBE_RENDER,
	SHADER_BLOCK_RENDER4,
	SHADER_BLOCK_RENDER8,
	SHADER_BLOCK_RENDER16,
	SHADER_BLOCK_RENDER32,
	SHADER_BLOCK_RENDER64,

	SHADER_COUNT
};

enum
{
	MESH_TRIANGLE,
	MESH_BOX_WIREFRAME,
	MESH_BOX_RED,
	MESH_BOX_BLUE,
	MESH_BOX_GREEN,
	MESH_VIRTUAL_TRIANGLE,
	MESH_CUBE,
	MESH_GRID,
	MESH_POINT,

	MESH_COUNT
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

struct BasicVertex_s
{
	core::Vec3 position;
	core::SColor color;
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

struct GPUTexture2D_s
{
	ID3D11Texture2D*				tex;
	ID3D11ShaderResourceView*		srv;
	ID3D11UnorderedAccessView*		uav;
};

struct GPUConstantBuffer_s
{
	ID3D11Buffer*					buf;
};

struct Compute_s
{
	ID3D11ComputeShader* m_computeShader;
};

struct Shader_s
{
	ID3D11VertexShader* m_vertexShader;
	ID3D11PixelShader* m_pixelShader;

	ID3D11InputLayout* m_inputLayout;

	uint m_vertexFormat;
};

struct Mesh_s
{
	ID3D11Buffer* m_vertexBuffer;
	ID3D11Buffer* m_indexBuffer;	
	uint m_vertexCount;
	uint m_vertexSize;
	uint m_vertexFormat;
	uint m_indexCount;
	uint m_indexSize;
	D3D11_PRIMITIVE_TOPOLOGY m_topology;	
};

struct Entity_s
{
	uint meshID;
	uint shaderID;
	core::Vec3 pos;
	float scale;
};

struct RenderingView_s
{
	core::Mat4x4 viewMatrix;
	core::Mat4x4 projMatrix;
	core::u16 frameWidth;
	core::u16 frameHeight;
};

struct Cube_s
{	
	ID3D11Texture2D* colorBuffer;
	ID3D11ShaderResourceView* colorSRV;
	ID3D11RenderTargetView* colorRTVs[CUBE_AXIS_COUNT];
	
	ID3D11Texture2D* depthBuffer;
	ID3D11ShaderResourceView* depthSRV;
	ID3D11DepthStencilView* depthRTVs[CUBE_AXIS_COUNT];

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
};

struct Sample_s
{
	GPUBuffer_s samples;
	GPUBuffer_s indirectArgs;
};

struct Octree_s
{
	GPUBuffer_s sampleNodeOffsets;
	GPUBuffer_s firstChildOffsets;
	ID3D11UnorderedAccessView* firstChildOffsetsLimitedUAV;
	GPUBuffer_s leaves;
	GPUBuffer_s indirectArgs;
};

struct Block_s
{
	GPUBuffer_s colors;
	GPUBuffer_s indirectArgs;
};

struct Pixel_s
{
	GPUTexture2D_s colors;
};

static const core::u32 ZOOM					= 2;
static const float ZNEAR					= 10.0f;
static const float ZFAR						= 10000.0f;
static const core::u32 CUBE_SIZE			= HLSL_GRID_WIDTH;
static const float GRID_MAX_SCALE			= 1000.0f;
static const float GRID_MIN_SCALE			= 100.0f;
static const core::u32 GRID_COUNT			= 1 + core::u32( ceil( log2f( (float)GRID_MAX_SCALE / GRID_MIN_SCALE ) ) );
static const int SAMPLE_MAX_COUNT			= 16;
static const float FREE_SCALE				= 50.0f;
//static const float FREE_SCALE				= 400.0f;
static const core::u32 RANDOM_CUBE_COUNT	= 1000;

static const uint VERTEX_INPUT_MAX_COUNT	= 6;
static const uint ENTITY_MAX_COUNT			= 16384;

static bool g_mousePressed					= false;
static int g_mousePosX						= 0;
static int g_mousePosY						= 0;
static int g_keyLeftPressed					= false;	
static int g_keyRightPressed				= false;
static int g_keyUpPressed					= false;
static int g_keyDownPressed					= false;

static int g_filterPixel					= true;

static DrawMode_e g_drawMode				= DRAW_MODE_DEFAULT;

static int g_sample							= 0;

static int g_limit							= false; 

static float s_yaw							= 0.0f;
static float s_pitch						= 0.0f;
static core::Vec3 s_headOffset				= core::Vec3_Zero();

static core::u32 gpuMemory					= 0;

static bool s_logReadBack					= false;

LRESULT CALLBACK WndProc( HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam )
{
	switch (message)
	{
	case WM_CHAR:
		switch (wParam)
		{
		case 0x1B:
			DestroyWindow(hWnd);
			break;
		};
		break;
	case WM_DESTROY:
		PostQuitMessage(0);
		break;
	case WM_KEYDOWN:
	case WM_KEYUP:
		{
			const bool pressed = message == WM_KEYDOWN;
			switch( wParam )
			{
			case 'A': g_keyLeftPressed = pressed; break;
			case 'D': g_keyRightPressed = pressed; break;
			case 'S': g_keyDownPressed = pressed; break;
			case 'W': g_keyUpPressed = pressed; break;
			case 'F': g_filterPixel = !pressed; break;
			case 'C': g_drawMode = pressed ? (g_drawMode == DRAW_MODE_CUBE ? DRAW_MODE_DEFAULT : DRAW_MODE_CUBE) : g_drawMode; break;
			case 'B': g_drawMode = pressed ? (g_drawMode == DRAW_MODE_BLOCK ? DRAW_MODE_DEFAULT : DRAW_MODE_BLOCK) : g_drawMode; break;
			case 'R': if ( pressed ) { g_sample = 0; s_logReadBack = true; } break;
			case 'L': g_limit = pressed ? !g_limit : g_limit; break;
			case ' ':
				s_yaw = 0.0f;
				s_pitch = 0.0f;
				s_headOffset = core::Vec3_Zero();
				break;
			}
		}
		break;
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
	case WM_MOUSEMOVE:
		{
			if ( g_mousePressed )
			{
				g_mousePosX = GET_X_LPARAM( lParam ); 
				g_mousePosY = GET_Y_LPARAM( lParam );
			}
		}
		break;
	default:
		return DefWindowProcA(hWnd, message, wParam, lParam);
		break;
	}

	return 0;
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
		V6_ERROR("Call to RegisterClassEx failed!");
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

static const char* ModeToString( DrawMode_e drawMode )
{
	switch ( drawMode )
	{
	case DRAW_MODE_DEFAULT: return "default";
	case DRAW_MODE_CUBE: return "cube";
	case DRAW_MODE_BLOCK: return "block";
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
			V6_ASSERT( "Not supported" );
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

static bool Compute_Create( ID3D11Device* device, Compute_s* compute, const char* cs, core::CFileSystem* fileSystem, core::IStack* stack )
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
			V6_ERROR( "ID3D11Device::CreateComputeShader failed!" );
		}
	}	

	return true;
}

static bool Shader_Create( ID3D11Device* device, Shader_s* shader, const char* vs, const char* ps, uint vertexFormat, core::CFileSystem* fileSystem, core::IStack* stack )
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
			V6_ERROR( "ID3D11Device::CreateVertexShader failed!" );
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
			V6_ERROR( "ID3D11Device::CreatePixelShader failed!" );
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

			stride = 4 * width;
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

			stride = 4 * width;
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

			stride = 4 * width;
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

			stride = 4 * width;
			++inputCount;
		}

		HRESULT hRes = device->CreateInputLayout( idesc, inputCount, vsBytecode, vsBytecodeSize, &shader->m_inputLayout );

		if ( FAILED( hRes) )
		{
			V6_ERROR( "ID3D11Device::CreateInputLayout failed!" );
			return false;
		}

		shader->m_vertexFormat = vertexFormat;
	}

	return true;
}

static core::u32 DXGIFormat_Size( DXGI_FORMAT format )
{
	switch( format )
	{	
	case DXGI_FORMAT_D32_FLOAT:		
	case DXGI_FORMAT_R8G8B8A8_UNORM:
	case DXGI_FORMAT_R32_UINT:
	case DXGI_FORMAT_R32_TYPELESS:
		return 4;
	default:
		V6_ASSERT( !"Not supported" );
		return 0;
	}
}

static void ReadBack_Log( const char* res, core::u32 value, const char* name )
{
	printf( "%-16s %-30s: %8d\n", res, name, value );
}

static void GPUResource_LogMemory( const char* res, core::u32 size, const char* name )
{
	printf( "%-16s %-30s: %8d MB\n", res, name, MB( size ) );
	gpuMemory += size;
}

static void GPUResource_LogMemoryUsage()
{
	printf( "%-16s %-30s: %8d MB\n", "GPU", "total", MB( gpuMemory ) );
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

static void GPUBuffer_CreateTyped( ID3D11Device* device, GPUBuffer_s* buffer, DXGI_FORMAT format, core::u32 count, const char* name )
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

static void GPUBuffer_CreateStructured( ID3D11Device* device, GPUBuffer_s* buffer, core::u32 elementSize, core::u32 count, const char* name )
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
	buffer->buf->Release();
	buffer->srv->Release();
	buffer->uav->Release();
}

template < typename T >
static T* GPUBUffer_MapReadBack( ID3D11DeviceContext* context, GPUBuffer_s* buffer )
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
		GPUResource_LogMemory( "Texture2D", texDesc.Width * texDesc.Height * texDesc.ArraySize * DXGIFormat_Size( texDesc.Format ), name );
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
}

static void Texture2D_Release( ID3D11Device* device, GPUTexture2D_s* tex )
{
	tex->tex->Release();
	tex->srv->Release();
	tex->uav->Release();
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
	buffer->buf->Release();
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

static void Cube_Create( ID3D11Device* device, Cube_s* cube, core::u32 size )
{
	{
		D3D11_TEXTURE2D_DESC texDesc = {};
		texDesc.Width = size;
		texDesc.Height = size;
		texDesc.MipLevels = 1;
		texDesc.ArraySize = 6;
		texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		texDesc.SampleDesc.Count = 1;
		texDesc.SampleDesc.Quality = 0;
		texDesc.Usage = D3D11_USAGE_DEFAULT;
		texDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
		texDesc.CPUAccessFlags = 0;
		texDesc.MiscFlags = 0;
		
		V6_ASSERT_D3D11( device->CreateTexture2D( &texDesc, nullptr, &cube->colorBuffer) );
		GPUResource_LogMemory( "Texture2D", texDesc.Width * texDesc.Height * texDesc.ArraySize * DXGIFormat_Size( texDesc.Format ), "cubeColors" );
	}

	{
		D3D11_SHADER_RESOURCE_VIEW_DESC viewDesc = {};
		viewDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		viewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
		viewDesc.Texture2DArray.MipLevels = 1;
		viewDesc.Texture2DArray.ArraySize = 6;
		viewDesc.Texture2DArray.FirstArraySlice = 0;		
		viewDesc.Texture2DArray.MostDetailedMip = 0;

		V6_ASSERT_D3D11( device->CreateShaderResourceView( cube->colorBuffer, &viewDesc, &cube->colorSRV ) );
	}

	for ( core::u32 faceID = 0; faceID < CUBE_AXIS_COUNT; ++faceID )
	{
		D3D11_RENDER_TARGET_VIEW_DESC viewDesc = {};
		viewDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		viewDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DARRAY;
		viewDesc.Texture2DArray.ArraySize = 1;
		viewDesc.Texture2DArray.FirstArraySlice = faceID;
		viewDesc.Texture2DArray.MipSlice = 0;

		V6_ASSERT_D3D11( device->CreateRenderTargetView( cube->colorBuffer, &viewDesc, &cube->colorRTVs[faceID] ) );
	}
	
	{
		D3D11_TEXTURE2D_DESC texDesc = {};
		texDesc.Width = size;
		texDesc.Height = size;
		texDesc.MipLevels = 1;
		texDesc.ArraySize = 6;
		texDesc.Format = DXGI_FORMAT_R32_TYPELESS;		
		texDesc.SampleDesc.Count = 1;
		texDesc.SampleDesc.Quality = 0;
		texDesc.Usage = D3D11_USAGE_DEFAULT;
		texDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
		texDesc.CPUAccessFlags = 0;
		texDesc.MiscFlags = 0;

		V6_ASSERT_D3D11( device->CreateTexture2D( &texDesc, nullptr, &cube->depthBuffer ) );
		GPUResource_LogMemory( "Texture2D", texDesc.Width * texDesc.Height * texDesc.ArraySize * DXGIFormat_Size( texDesc.Format ), "cubeDepths" );
	}

	{
		D3D11_SHADER_RESOURCE_VIEW_DESC viewDesc = {};
		viewDesc.Format = DXGI_FORMAT_R32_FLOAT;
		viewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
		viewDesc.Texture2DArray.MipLevels = 1;
		viewDesc.Texture2DArray.ArraySize = 6;
		viewDesc.Texture2DArray.FirstArraySlice = 0;		
		viewDesc.Texture2DArray.MostDetailedMip = 0;

		V6_ASSERT_D3D11( device->CreateShaderResourceView( cube->depthBuffer, &viewDesc, &cube->depthSRV ) );
	}

	for ( core::u32 faceID = 0; faceID < CUBE_AXIS_COUNT; ++faceID )
	{
		D3D11_DEPTH_STENCIL_VIEW_DESC viewDesc = {};
		viewDesc.Format = DXGI_FORMAT_D32_FLOAT;
		viewDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DARRAY;
		viewDesc.Flags = 0;
		viewDesc.Texture2DArray.ArraySize = 1;
		viewDesc.Texture2DArray.FirstArraySlice = faceID;
		viewDesc.Texture2DArray.MipSlice = 0;

		V6_ASSERT_D3D11( device->CreateDepthStencilView( cube->depthBuffer, &viewDesc, &cube->depthRTVs[faceID] ) );
	}
	
	cube->size = size;
}

static void Cube_Release( Cube_s* cube )
{
	cube->colorBuffer->Release();
	cube->depthBuffer->Release();

	cube->colorSRV->Release();	
	cube->depthSRV->Release();	

	for ( core::u32 faceID = 0; faceID < CUBE_AXIS_COUNT; ++faceID )
	{
		cube->colorRTVs[faceID]->Release();
		cube->depthRTVs[faceID]->Release();
	}
}

static void Config_Init( Config_s* config, core::u32 screenWidth, core::u32 screenHeight, core::u32 cubeSize, core::u32 gridWidth )
{
	config->screenWidth = screenWidth;
	config->screenHeight = screenHeight;
	config->sampleCount = cubeSize * cubeSize * 6;
	config->leafCount = gridWidth * gridWidth * 6 * 2;
	config->nodeCount = config->leafCount * 2;
	config->cellCount = config->leafCount * 5 / 4;
}

static void Config_Log( const Config_s* config )
{
	printf( "%-16s: %d\n", "config.screenWidth", config->screenWidth );
	printf( "%-16s: %d\n", "config.screenHeight", config->screenHeight );
	printf( "%-16s: %13s\n", "config.sample", FormatInteger_Unsafe( config->sampleCount ) );
	printf( "%-16s: %13s\n", "config.leaf", FormatInteger_Unsafe( config->leafCount ) );
	printf( "%-16s: %13s\n", "config.node", FormatInteger_Unsafe( config->nodeCount ) );
	printf( "%-16s: %13s\n", "config.cell", FormatInteger_Unsafe( config->cellCount ) );
}

static void Sample_Create( ID3D11Device* device, Sample_s* sample, const Config_s* config, core::IHeap* heap )
{
	GPUBuffer_CreateStructured( device, &sample->samples, sizeof( hlsl::Sample ), config->sampleCount, "samples" );
	GPUBuffer_CreateIndirectArgs( device, &sample->indirectArgs, sample_all_offset, GPUBUFFER_CREATION_FLAG_READ_BACK, "sampleIndirectArgs" );
}

static void Sample_Release( ID3D11Device* device, Sample_s* sample )
{
	GPUBuffer_Release( device, &sample->samples );
	GPUBuffer_Release( device, &sample->indirectArgs );
}

static void Octree_Create( ID3D11Device* device, Octree_s* octree, const Config_s* config, core::IHeap* heap )
{
	GPUBuffer_CreateTyped( device, &octree->sampleNodeOffsets, DXGI_FORMAT_R32_UINT, config->sampleCount, "octreeSampleNodeOffsets" );
	GPUBuffer_CreateTyped( device, &octree->firstChildOffsets, DXGI_FORMAT_R32_UINT, config->nodeCount, "octreeFirstChildOffsets" );
	
	{
		D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
		uavDesc.Format = DXGI_FORMAT_R32_UINT;
		uavDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
		uavDesc.Buffer.FirstElement = 0;
		uavDesc.Buffer.NumElements = GRID_COUNT * 8;
		uavDesc.Buffer.Flags = 0;

		V6_ASSERT_D3D11( device->CreateUnorderedAccessView( octree->firstChildOffsets.buf, &uavDesc, &octree->firstChildOffsetsLimitedUAV ) );
	}

	GPUBuffer_CreateStructured( device, &octree->leaves, sizeof( hlsl::OctreeLeaf ), config->leafCount, "octreeLeaves" );
	GPUBuffer_CreateIndirectArgs( device, &octree->indirectArgs, octree_all_offset, GPUBUFFER_CREATION_FLAG_READ_BACK, "octreeIndirectArgs" );
}

static void Octree_Release( ID3D11Device* device, Octree_s* octree )
{
	GPUBuffer_Release( device, &octree->sampleNodeOffsets );
	GPUBuffer_Release( device, &octree->firstChildOffsets );
	GPUBuffer_Release( device, &octree->leaves );
	GPUBuffer_Release( device, &octree->indirectArgs );
}

static void Block_Create( ID3D11Device* device, Block_s* block, const Config_s* config, core::IHeap* heap )
{
	GPUBuffer_CreateTyped( device, &block->colors, DXGI_FORMAT_R32_UINT, config->cellCount, "blockColors" );
	GPUBuffer_CreateIndirectArgs( device, &block->indirectArgs, block_all_offset, GPUBUFFER_CREATION_FLAG_READ_BACK, "blockIndirectArgs" );
}

static void Block_Release( ID3D11Device* device, Block_s* block )
{
	GPUBuffer_Release( device, &block->colors );
	GPUBuffer_Release( device, &block->indirectArgs );
}

static void Pixel_Create( ID3D11Device* device, Pixel_s* pixel, const Config_s* config, core::IHeap* heap )
{
	Texture2D_CreateRW( device, &pixel->colors, config->screenWidth, config->screenHeight, "pixelColors" );
}

static void Pixel_Release( ID3D11Device* device, Pixel_s* pixel )
{
	Texture2D_Release( device, &pixel->colors );
}

static void Mesh_Create( ID3D11Device* device, Mesh_s* mesh, const void* vertices, uint vertexCount, uint vertexSize, uint vertexFormat, const void* indices, uint indexCount, uint indexSize, D3D11_PRIMITIVE_TOPOLOGY topology )
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

static void Mesh_CreateTriangle( ID3D11Device* device, Mesh_s* mesh )
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

static void Mesh_CreateBox( ID3D11Device* device, Mesh_s* mesh, const core::SColor color, bool wireframe )
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

static void Mesh_CreateVirtualTriangle( ID3D11Device* device, Mesh_s* mesh )
{
	Mesh_Create( device, mesh, nullptr, 3, 0, 0, nullptr, 0, 0, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST );	
}

static void Mesh_CreateCube( ID3D11Device* device, Mesh_s* mesh )
{
	CubeVertex_s vertices[24];
	core::u16 indices[36];

	core::u32 vertexID = 0;
	core::u32 indexID = 0;
	for ( core::u32 faceID = 0; faceID < CUBE_AXIS_COUNT; ++faceID )
	{
		core::Vec3 lookAt;
		core::Vec3 up;
		Cube_GetLookAt( lookAt, up, (CubeAxis_e)faceID );
		const core::Vec3 right = core::Cross( lookAt, up );		

		vertices[vertexID+0].pos = (lookAt - right + up) * ZFAR;
		vertices[vertexID+0].uv = core::Vec2_Make( 0.0f, 0.0f );
		vertices[vertexID+1].pos = (lookAt + right + up) * ZFAR;
		vertices[vertexID+1].uv = core::Vec2_Make( 1.0f, 0.0f );
		vertices[vertexID+2].pos = (lookAt - right - up) * ZFAR;
		vertices[vertexID+2].uv = core::Vec2_Make( 0.0f, 1.0f );
		vertices[vertexID+3].pos = (lookAt + right - up) * ZFAR;
		vertices[vertexID+3].uv = core::Vec2_Make( 1.0f, 1.0f );
		
		indices[indexID+0] = vertexID+0;
		indices[indexID+1] = vertexID+1;
		indices[indexID+2] = vertexID+2;

		indices[indexID+3] = vertexID+2;
		indices[indexID+4] = vertexID+1;
		indices[indexID+5] = vertexID+3;

		vertexID += 4;
		indexID += 6;
	}

	Mesh_Create( device, mesh, vertices, 24, sizeof( CubeVertex_s ), VERTEX_FORMAT_POSITION | VERTEX_FORMAT_USER0_F2, indices, 36, sizeof( core::u16 ), D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST );
}

static void Mesh_CreateGrid( ID3D11Device* device, Mesh_s* mesh )
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

	Mesh_Create( device, mesh, nullptr, 4, 0, 0, indices, 36, sizeof( core::u16 ), D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST );
}

static void Mesh_CreatePoint( ID3D11Device* device, Mesh_s* mesh )
{
	Mesh_Create( device, mesh, nullptr, 0, 0, 0, nullptr, 0, sizeof( core::u16 ), D3D11_PRIMITIVE_TOPOLOGY_POINTLIST );
}

static void Entity_Create( Entity_s* entity, core::u32 meshID, core::u32 shaderID, const core::Vec3& pos, float scale )
{
	entity->meshID = meshID;
	entity->shaderID = shaderID;
	entity->pos = pos;
	entity->scale = scale;
}

static void Mesh_Draw( Mesh_s* mesh, core::u32 instanceCount, Shader_s* shader, ID3D11DeviceContext* ctx, ID3D11Buffer* bufferArgs, core::u32 offsetArgs )
{
	V6_ASSERT( shader->m_vertexFormat == mesh->m_vertexFormat );
	V6_ASSERT( instanceCount > 0 );

	ctx->IASetInputLayout( shader->m_inputLayout );
	ctx->VSSetShader( shader->m_vertexShader, nullptr, 0 );
	ctx->PSSetShader( shader->m_pixelShader, nullptr, 0 );
		
	const uint stride = mesh->m_vertexSize; 
	const uint offset = 0;
			
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
			V6_ASSERT( !"Not supported");
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
		ctx->IASetIndexBuffer( nullptr, DXGI_FORMAT_R16_UINT, 0 );
				
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

class CRenderingDevice
{
public:
	CRenderingDevice();
	~CRenderingDevice();

public:
	core::u32 BuildNode( bool clear );
	void Capture( const core::Vec3* sampleOffset );
	void Collect( const core::Vec3* sampleOffset );
	bool Create(int nWidth, int nHeight, HWND hWnd, core::CFileSystem* fileSystem, core::IHeap* heap, core::IStack* stack );
	void Draw( float dt );
	void DrawBlock( const core::Mat4x4* viewMatrix );
	void DrawCube( const core::Mat4x4* viewMatrix );	
	void DrawEntities( Entity_s* entities, core::u32 count, const RenderingView_s* view );
	void DrawWorld( const core::Mat4x4* viewMatrix );
	void FillLeaf();
	void PackColor();	
	void PixelFilter();
	void Present();
	void Release();
	void ReleaseObject(IUnknown** unknow);

	IDXGISwapChain* m_pSwapChain;
	ID3D11Device* m_device;
	D3D_FEATURE_LEVEL m_nFeatureLevel;
	ID3D11DeviceContext* m_ctx;
	ID3DUserDefinedAnnotation* m_userDefinedAnnotation;
	ID3D11Texture2D* m_pColorBuffer;
	ID3D11Texture2D* m_pDepthStencilBuffer;
	ID3D11RenderTargetView* m_pColorView;
	ID3D11ShaderResourceView* m_pColorSRV;
	ID3D11DepthStencilView* m_pDepthStencilView;
	ID3D11ShaderResourceView* m_pDepthStencilSRV;
	ID3D11DepthStencilState*  m_depthStencilStateNoZ;
	ID3D11DepthStencilState*  m_depthStencilStateZRO;
	ID3D11DepthStencilState*  m_depthStencilStateZRW;
	ID3D11BlendState* m_blendStateNoColor;
	ID3D11BlendState* m_blendStateOpaque;
	GPUConstantBuffer_s m_viewCBUF;
	GPUConstantBuffer_s m_sampleCBUF;
	GPUConstantBuffer_s m_octreeCBUF;
	GPUConstantBuffer_s m_blockCBUF;
	GPUConstantBuffer_s m_pixelCBUF;

	Config_s m_config;

	Compute_s m_computes[COMPUTE_COUNT];
	Shader_s m_shaders[SHADER_COUNT];
	Mesh_s m_meshes[MESH_COUNT];
	Entity_s m_entities[ENTITY_MAX_COUNT];
	core::Vec3 m_sampleOffsets[SAMPLE_MAX_COUNT];

	uint m_entityCount;

	Cube_s m_cube;
	Sample_s m_sample;
	Octree_s m_octree;
	Block_s m_block;
	Pixel_s m_pixel;

	core::IHeap* m_heap;
	core::IStack* m_stack;

	uint m_width;
	uint m_height;
	float m_aspectRatio;
	core::Mat4x4 m_projMatrix;
	core::Mat4x4 m_cubeProjMatrix;
};

CRenderingDevice::CRenderingDevice()
{
	memset( this, 0, sizeof( CRenderingDevice ) );
}

CRenderingDevice::~CRenderingDevice()
{
}

bool CRenderingDevice::Create( int nWidth, int nHeight, HWND hWnd, core::CFileSystem* fileSystem, core::IHeap* heap, core::IStack* stack )
{
	m_heap = heap;
	m_stack = stack;
	core::ScopedStack scopedStack( stack );

	m_width = nWidth;
	m_height = nHeight;
	m_aspectRatio = (float)nWidth / nHeight;
	m_projMatrix = core::Mat4x4_Projection( ZNEAR, ZFAR, core::DegToRad( 70.0f ), m_aspectRatio );	
	m_cubeProjMatrix = core::Mat4x4_Projection( ZNEAR, ZFAR, core::DegToRad( 90.0f ), 1.0f );

	{
		DXGI_SWAP_CHAIN_DESC oSwapChainDesc = {};

		DXGI_MODE_DESC & oModeDesc = oSwapChainDesc.BufferDesc;
		oModeDesc.Width = nWidth;
		oModeDesc.Height = nHeight;
		oModeDesc.RefreshRate.Numerator = 60;
		oModeDesc.RefreshRate.Denominator = 1;
		oModeDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		oModeDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
		oModeDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;

		DXGI_SAMPLE_DESC & oSampleDesc = oSwapChainDesc.SampleDesc;
		oSampleDesc.Count = 1;
		oSampleDesc.Quality = 0;

		oSwapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT | DXGI_USAGE_SHADER_INPUT;
		oSwapChainDesc.BufferCount = 2;
		oSwapChainDesc.OutputWindow = hWnd;
		oSwapChainDesc.Windowed = true;
		oSwapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
		oSwapChainDesc.Flags = 0;

		D3D_FEATURE_LEVEL pFeatureLevels[2] = { D3D_FEATURE_LEVEL_11_1 };
		m_pSwapChain = NULL;
		m_device = NULL;
		m_nFeatureLevel = (D3D_FEATURE_LEVEL)0;
		m_ctx = NULL;

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
			&m_pSwapChain,
			&m_device,
			&m_nFeatureLevel,
			&m_ctx) );

		V6_ASSERT( m_nFeatureLevel == D3D_FEATURE_LEVEL_11_1 );
	}

#if 0
	for ( core::u32 sampleCount = 1; sampleCount <= D3D11_MAX_MULTISAMPLE_SAMPLE_COUNT; sampleCount++)
	{
		core::u32 maxQualityLevel;
		HRESULT hr = m_device->CheckMultisampleQualityLevels( DXGI_FORMAT_R8G8B8A8_UNORM, MSAA_SAMPLE_QUALITY, &maxQualityLevel );
		
		if ( hr != S_OK )
			break;
		
		if ( maxQualityLevel > 0 )
			printf ("MSAA %dX supported with %d quality levels.\n", sampleCount, maxQualityLevel-1 );		
	}
#endif

	V6_ASSERT_D3D11( m_ctx->QueryInterface( IID_PPV_ARGS( &m_userDefinedAnnotation ) ) );

	V6_ASSERT_D3D11( m_pSwapChain->GetBuffer( 0, __uuidof(ID3D11Texture2D), (void **)&m_pColorBuffer ) );

	V6_ASSERT_D3D11( m_device->CreateShaderResourceView( m_pColorBuffer, 0, &m_pColorSRV ) );	
	V6_ASSERT_D3D11( m_device->CreateRenderTargetView( m_pColorBuffer, 0, &m_pColorView ) );
	
	{
		D3D11_TEXTURE2D_DESC texDesc;
		texDesc.Width = nWidth;
		texDesc.Height = nHeight;
		texDesc.MipLevels = 1;
		texDesc.ArraySize = 1;
		texDesc.Format = DXGI_FORMAT_R32_TYPELESS;	
		texDesc.SampleDesc.Count = 1;
		texDesc.SampleDesc.Quality = 0;
		texDesc.Usage = D3D11_USAGE_DEFAULT;
		texDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
		texDesc.CPUAccessFlags = 0;
		texDesc.MiscFlags = 0;

		V6_ASSERT_D3D11( m_device->CreateTexture2D( &texDesc, 0, &m_pDepthStencilBuffer ) );
		GPUResource_LogMemory( "Texture2D", texDesc.Width * texDesc.Height * texDesc.ArraySize * DXGIFormat_Size( texDesc.Format ), "mainDepths" );
	}
	
	{
		D3D11_SHADER_RESOURCE_VIEW_DESC viewDesc = {};
		viewDesc.Format = DXGI_FORMAT_R32_FLOAT;
		viewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		viewDesc.Texture2D.MipLevels = 1;
		viewDesc.Texture2D.MostDetailedMip = 0;

		V6_ASSERT_D3D11( m_device->CreateShaderResourceView( m_pDepthStencilBuffer, &viewDesc, &m_pDepthStencilSRV ) );
	}

	{
		D3D11_DEPTH_STENCIL_VIEW_DESC oDepthStencilViewDesc = {};
		oDepthStencilViewDesc.Format = DXGI_FORMAT_D32_FLOAT;
		oDepthStencilViewDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
		oDepthStencilViewDesc.Flags = 0;
		oDepthStencilViewDesc.Texture2D.MipSlice = 0;

		V6_ASSERT_D3D11( m_device->CreateDepthStencilView( m_pDepthStencilBuffer, &oDepthStencilViewDesc, &m_pDepthStencilView ) );
	}
		
	{
		D3D11_DEPTH_STENCIL_DESC depthStencilDesc = {};
		depthStencilDesc.DepthEnable = false;
		depthStencilDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;

		V6_ASSERT_D3D11( m_device->CreateDepthStencilState( &depthStencilDesc, &m_depthStencilStateNoZ ) );
	}

	{
		D3D11_DEPTH_STENCIL_DESC depthStencilDesc = {};
		depthStencilDesc.DepthEnable = true;
		depthStencilDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
		depthStencilDesc.DepthFunc = D3D11_COMPARISON_LESS;

		V6_ASSERT_D3D11( m_device->CreateDepthStencilState( &depthStencilDesc, &m_depthStencilStateZRO ) );
	}

	{
		D3D11_DEPTH_STENCIL_DESC depthStencilDesc = {};
		depthStencilDesc.DepthEnable = true;
		depthStencilDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
		depthStencilDesc.DepthFunc = D3D11_COMPARISON_LESS;

		V6_ASSERT_D3D11( m_device->CreateDepthStencilState( &depthStencilDesc, &m_depthStencilStateZRW ) );
	}

	{
		D3D11_BLEND_DESC blendState = {};
		blendState.AlphaToCoverageEnable = false;
		blendState.IndependentBlendEnable = false;
		blendState.RenderTarget[0].BlendEnable = FALSE;
		blendState.RenderTarget[0].RenderTargetWriteMask = 0;
		
		V6_ASSERT_D3D11( m_device->CreateBlendState( &blendState, &m_blendStateNoColor ) );
	}

	{
		D3D11_BLEND_DESC blendState = {};
		blendState.AlphaToCoverageEnable = false;
		blendState.IndependentBlendEnable = false;
		blendState.RenderTarget[0].BlendEnable = FALSE;
		blendState.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
		
		V6_ASSERT_D3D11( m_device->CreateBlendState( &blendState, &m_blendStateOpaque ) );
	}

	ConstantBuffer_Create( m_device, &m_viewCBUF, sizeof( v6::hlsl::CBView ), "view" );
	ConstantBuffer_Create( m_device, &m_sampleCBUF, sizeof( v6::hlsl::CBSample), "sample" );
	ConstantBuffer_Create( m_device, &m_octreeCBUF, sizeof( v6::hlsl::CBOctree ), "octree" );
	ConstantBuffer_Create( m_device, &m_blockCBUF, sizeof( v6::hlsl::CBBlock ), "block" );
	ConstantBuffer_Create( m_device, &m_pixelCBUF, sizeof( v6::hlsl::CBPixel), "pixel" );

	Compute_Create( m_device, &m_computes[COMPUTE_SAMPLECOLLECT], "sample_collect_cs.cso", fileSystem, stack );
	Compute_Create( m_device, &m_computes[COMPUTE_BUILDINNER], "octree_build_inner_cs.cso", fileSystem, stack );
	Compute_Create( m_device, &m_computes[COMPUTE_BUILDLEAF], "octree_build_leaf_cs.cso", fileSystem, stack );
	Compute_Create( m_device, &m_computes[COMPUTE_FILLLEAF], "octree_fill_leaf_cs.cso", fileSystem, stack );
	Compute_Create( m_device, &m_computes[COMPUTE_PACKCOLOR], "octree_pack_cs.cso", fileSystem, stack );
	Compute_Create( m_device, &m_computes[COMPUTE_FILTERPIXEL], "pixel_filter_cs.cso", fileSystem, stack );
		
	Shader_Create( m_device, &m_shaders[SHADER_BASIC], "basic_vs.cso", "basic_ps.cso", VERTEX_FORMAT_POSITION | VERTEX_FORMAT_COLOR, fileSystem, stack );
	Shader_Create( m_device, &m_shaders[SHADER_CUBE_RENDER], "cube_render_vs.cso", "cube_render_ps.cso", VERTEX_FORMAT_POSITION | VERTEX_FORMAT_USER0_F2, fileSystem, stack );
	Shader_Create( m_device, &m_shaders[SHADER_BLOCK_RENDER4], "block_render_x4_vs.cso", "block_render_ps.cso", 0, fileSystem, stack );
	Shader_Create( m_device, &m_shaders[SHADER_BLOCK_RENDER8], "block_render_x8_vs.cso", "block_render_ps.cso", 0, fileSystem, stack );
	Shader_Create( m_device, &m_shaders[SHADER_BLOCK_RENDER16], "block_render_x16_vs.cso", "block_render_ps.cso", 0, fileSystem, stack );
	Shader_Create( m_device, &m_shaders[SHADER_BLOCK_RENDER32], "block_render_x32_vs.cso", "block_render_ps.cso", 0, fileSystem, stack );
	Shader_Create( m_device, &m_shaders[SHADER_BLOCK_RENDER64], "block_render_x64_vs.cso", "block_render_ps.cso", 0, fileSystem, stack );

	Mesh_CreateTriangle( m_device, &m_meshes[MESH_TRIANGLE] );	
	Mesh_CreateBox( m_device, &m_meshes[MESH_BOX_WIREFRAME], core::Color_Make( 255, 255, 255, 255 ), true );
	Mesh_CreateBox( m_device, &m_meshes[MESH_BOX_RED], core::Color_Make( 255, 0, 0, 255 ), false );
	Mesh_CreateBox( m_device, &m_meshes[MESH_BOX_GREEN], core::Color_Make( 0, 255, 0, 255 ), false );
	Mesh_CreateBox( m_device, &m_meshes[MESH_BOX_BLUE], core::Color_Make( 0, 0, 255, 255 ), false );
	Mesh_CreateVirtualTriangle( m_device, &m_meshes[MESH_VIRTUAL_TRIANGLE] );
	Mesh_CreateCube( m_device, &m_meshes[MESH_CUBE] );
	Mesh_CreateGrid( m_device, &m_meshes[MESH_GRID] );
	Mesh_CreatePoint( m_device, &m_meshes[MESH_POINT] );
	
	Entity_Create( &m_entities[m_entityCount++], MESH_BOX_WIREFRAME, SHADER_BASIC, core::Vec3_Make( 0.0f, 0.0f, 0.0f), GRID_MAX_SCALE );
	Entity_Create( &m_entities[m_entityCount++], MESH_TRIANGLE, SHADER_BASIC, core::Vec3_Make( 0.0f, 0.0f, -GRID_MAX_SCALE ), 5.0f );
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
		
		Entity_Create( &m_entities[m_entityCount++], MESH_BOX_RED + (randomCubeID % 3), SHADER_BASIC, center, size );
		Entity_Create( &m_entities[m_entityCount++], MESH_BOX_WIREFRAME, SHADER_BASIC, center, size );

		++randomCubeID;
	}
	Config_Init( &m_config, m_width, m_height, CUBE_SIZE, HLSL_GRID_WIDTH );	
	Cube_Create( m_device, &m_cube, CUBE_SIZE );
	Sample_Create( m_device, &m_sample, &m_config, heap );
	Octree_Create( m_device, &m_octree, &m_config, heap );
	Block_Create( m_device, &m_block, &m_config, heap );
	Pixel_Create( m_device, &m_pixel, &m_config, heap );
	
	g_sample = 0;
	m_sampleOffsets[0] = core::Vec3_Make( 0.0f, 0.0f, 0.0f );
	for ( core::u32 sample = 1; sample < SAMPLE_MAX_COUNT; ++sample )
		m_sampleOffsets[sample] = core::Vec3_Rand() * FREE_SCALE;

	GPUResource_LogMemoryUsage();

	Config_Log( &m_config );
	
	return true;
}

void CRenderingDevice::DrawEntities( Entity_s* entities, core::u32 count, const RenderingView_s* view )
{
	for ( uint entityRank = 0; entityRank < count; ++entityRank )
	{
		Entity_s* entity = &entities[entityRank];
		
		{
			v6::hlsl::CBView* cbView = ConstantBuffer_MapWrite< v6::hlsl::CBView >( m_ctx, &m_viewCBUF );

			core::Mat4x4 worlMatrix;
			core::Mat4x4_Scale( &worlMatrix, entity->scale );
			core::Mat4x4_SetTranslation( &worlMatrix, entity->pos );
			
			// use this order because one matrix is "from" local space and the other is "to" local space
			core::Mat4x4_Mul( &cbView->c_frameObjectToView, view->viewMatrix, worlMatrix );
			cbView->c_frameViewToProj = view->projMatrix;
			cbView->c_frameWidth = view->frameWidth;
			cbView->c_frameHeight = view->frameHeight;

			ConstantBuffer_UnmapWrite( m_ctx, &m_viewCBUF );
		}

		m_ctx->VSSetConstantBuffers( v6::hlsl::CBViewSlot, 1, &m_viewCBUF.buf );
		
		Mesh_s* mesh = &m_meshes[entity->meshID];
		Shader_s* shader = &m_shaders[entity->shaderID];
		Mesh_Draw( mesh, 1, shader, m_ctx, nullptr, 0 );
	}
}

void CRenderingDevice::DrawWorld( const core::Mat4x4* viewMatrix )
{	
	// Rasterization state
	m_ctx->OMSetDepthStencilState( m_depthStencilStateZRW, 0 );
	m_ctx->OMSetBlendState( m_blendStateOpaque, nullptr, 0XFFFFFFFF );
		
	// Viewport
	{
		D3D11_VIEWPORT viewport = {};
		viewport.TopLeftX = 0;
		viewport.TopLeftY = 0;
		viewport.Width = (float)m_width;
		viewport.Height = (float)m_height;
		viewport.MinDepth = 0.0f;
		viewport.MaxDepth = 1.0f;

		m_ctx->RSSetViewports( 1, &viewport );
	}
	
	// RT
	m_ctx->OMSetRenderTargets( 1, &m_pColorView, m_pDepthStencilView );

	// Clear
	float const pRGBA[] = { 0.0f, 0.0f, 0.0f, 0.0f };
	m_ctx->ClearRenderTargetView( m_pColorView, pRGBA );
	m_ctx->ClearDepthStencilView( m_pDepthStencilView, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0 );
		
	// View
	RenderingView_s view;
	view.viewMatrix = *viewMatrix;
	view.projMatrix = m_projMatrix;	
	view.frameWidth = m_width;
	view.frameHeight = m_height;

	DrawEntities( m_entities, m_entityCount, &view );

	// un RT
	m_ctx->OMSetRenderTargets( 0, nullptr, nullptr );
}

void CRenderingDevice::Capture( const core::Vec3* sampleOffset )
{
	// Rasterization state
	m_ctx->OMSetDepthStencilState( m_depthStencilStateZRW, 0 );
	m_ctx->OMSetBlendState( m_blendStateOpaque, nullptr, 0XFFFFFFFF );

	// Viewport
	{
		D3D11_VIEWPORT viewport = {};
		viewport.TopLeftX = 0;
		viewport.TopLeftY = 0;
		viewport.Width = (float)m_cube.size;
		viewport.Height = (float)m_cube.size;
		viewport.MinDepth = 0.0f;
		viewport.MaxDepth = 1.0f;

		m_ctx->RSSetViewports( 1, &viewport );
	}

	m_userDefinedAnnotation->BeginEvent( L"Capture");
		
	for ( core::u32 faceID = 0; faceID < CUBE_AXIS_COUNT; ++faceID )
	{
		m_userDefinedAnnotation->BeginEvent( L"Draw Face");

		// RT
		m_ctx->OMSetRenderTargets( 1, &m_cube.colorRTVs[faceID], m_cube.depthRTVs[faceID] );

		// Clear
		float const pRGBA[] = { 0.0f, 0.0f, 0.0f, 0.0f };
		m_ctx->ClearRenderTargetView( m_cube.colorRTVs[faceID], pRGBA );
		m_ctx->ClearDepthStencilView( m_cube.depthRTVs[faceID], D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0 );						

		// View
		RenderingView_s view;
		Cube_MakeViewMatrix( &view.viewMatrix, *sampleOffset, (CubeAxis_e)faceID );
		view.projMatrix = m_cubeProjMatrix;
		view.frameWidth = m_cube.size;	
		view.frameHeight = m_cube.size;
		
		DrawEntities( m_entities, m_entityCount, &view );

		// un RT
		m_ctx->OMSetRenderTargets( 0, nullptr, nullptr );
			
		m_userDefinedAnnotation->EndEvent();
	}

	m_userDefinedAnnotation->EndEvent();
}

void CRenderingDevice::DrawCube( const core::Mat4x4* viewMatrix )
{
	// Rasterization state
	m_ctx->OMSetDepthStencilState( m_depthStencilStateZRW, 0 );
	m_ctx->OMSetBlendState( m_blendStateOpaque, nullptr, 0XFFFFFFFF );
		
	// Viewport
	{
		D3D11_VIEWPORT viewport = {};
		viewport.TopLeftX = 0;
		viewport.TopLeftY = 0;
		viewport.Width = (float)m_width;
		viewport.Height = (float)m_height;
		viewport.MinDepth = 0.0f;
		viewport.MaxDepth = 1.0f;

		m_ctx->RSSetViewports( 1, &viewport );
	}

	m_userDefinedAnnotation->BeginEvent( L"Render");

	// RT
	m_ctx->OMSetRenderTargets( 1, &m_pColorView, m_pDepthStencilView );

	{
		v6::hlsl::CBView* cbView = ConstantBuffer_MapWrite< v6::hlsl::CBView >( m_ctx, &m_viewCBUF );

		cbView->c_frameObjectToView = *viewMatrix;
		cbView->c_frameViewToProj = m_projMatrix;
		cbView->c_frameWidth = m_cube.size;	
		cbView->c_frameHeight = m_cube.size;

		ConstantBuffer_UnmapWrite( m_ctx, &m_viewCBUF );		
	}

	// Render
	float const pRGBA[] = { 0.0f, 0.0f, 0.0f, 0.0f };
	m_ctx->ClearRenderTargetView( m_pColorView, pRGBA );
	m_ctx->ClearDepthStencilView( m_pDepthStencilView, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0 );
		
	m_ctx->VSSetConstantBuffers( v6::hlsl::CBViewSlot, 1, &m_viewCBUF.buf );
	m_ctx->PSSetConstantBuffers( v6::hlsl::CBViewSlot, 1, &m_viewCBUF.buf );
	m_ctx->PSSetShaderResources( HLSL_COLOR_SLOT, 1, &m_cube.colorSRV );

	Mesh_Draw( &m_meshes[MESH_CUBE], 1, &m_shaders[SHADER_CUBE_RENDER], m_ctx, nullptr, 0 );

	// unset
	static const void* nulls[8] = {};
	m_ctx->PSSetShaderResources( HLSL_COLOR_SLOT, 1, (ID3D11ShaderResourceView**)nulls );
		
	// un RT
	m_ctx->OMSetRenderTargets( 0, nullptr, nullptr );

	m_userDefinedAnnotation->EndEvent();
}

void CRenderingDevice::Collect( const core::Vec3* sampleOffset )
{
	static const void* nulls[8] = {};

	m_userDefinedAnnotation->BeginEvent( L"Collect");

	// Update buffers
				
	{			
		V6_ASSERT( GRID_COUNT < HLSL_MIP_MAX_COUNT );

		float gridScales[HLSL_MIP_MAX_COUNT];
		float gridScale = GRID_MIN_SCALE;
		for ( core::u32 gridID = 0; gridID < GRID_COUNT; ++gridID, gridScale *= 2 )
			gridScales[gridID] = gridScale;
		for ( core::u32 gridID = GRID_COUNT; gridID < HLSL_MIP_MAX_COUNT; ++gridID )
			gridScales[gridID] = gridScales[GRID_COUNT-1];

		v6::hlsl::CBSample* cbSample = ConstantBuffer_MapWrite< v6::hlsl::CBSample >( m_ctx, &m_sampleCBUF );		

		cbSample->c_sampleDepthLinearScale = -1.0f / ZNEAR;
		cbSample->c_sampleDepthLinearBias = 1.0f / ZNEAR;
		cbSample->c_sampleInvCubeSize.x = 1.0f / CUBE_SIZE;
		cbSample->c_sampleInvCubeSize.y = 1.0f / CUBE_SIZE;
		cbSample->c_sampleCubeCenter = *sampleOffset;
		cbSample->c_sampleMipBoundariesA = core::Vec4_Make( gridScales[0], gridScales[1], gridScales[2], gridScales[3] );
		cbSample->c_sampleMipBoundariesB = core::Vec4_Make( gridScales[4], gridScales[5], gridScales[6], gridScales[7] );
		cbSample->c_sampleMipBoundariesC = core::Vec4_Make( gridScales[8], gridScales[9], gridScales[10], gridScales[11] );
		cbSample->c_sampleMipBoundariesD = core::Vec4_Make( gridScales[12], gridScales[13], gridScales[14], gridScales[15] );
		for ( core::u32 gridID = 0; gridID < HLSL_MIP_MAX_COUNT; ++gridID )
			cbSample->c_sampleInvGridScales[gridID] = core::Vec4_Make( 1.0f / gridScales[gridID], 0.0f, 0.0f , 0.0f );

		ConstantBuffer_UnmapWrite( m_ctx, &m_sampleCBUF );		
	}

	core::u32 values[4] = {};
	m_ctx->ClearUnorderedAccessViewUint( m_sample.indirectArgs.uav, values );
		
	m_ctx->CSSetConstantBuffers( v6::hlsl::CBSampleSlot, 1, &m_sampleCBUF.buf );
	m_ctx->CSSetShaderResources( HLSL_COLOR_SLOT, 1, &m_cube.colorSRV );
	m_ctx->CSSetShaderResources( HLSL_DEPTH_SLOT, 1, &m_cube.depthSRV );
	m_ctx->CSSetUnorderedAccessViews( HLSL_SAMPLE_SLOT, 1, &m_sample.samples.uav, nullptr );
	m_ctx->CSSetUnorderedAccessViews( HLSL_SAMPLE_INDIRECT_ARGS_SLOT, 1, &m_sample.indirectArgs.uav, nullptr );
	m_ctx->CSSetShader( m_computes[COMPUTE_SAMPLECOLLECT].m_computeShader, nullptr, 0 );
		
	const core::u32 cubeGroupCount = m_cube.size >> 4;
	m_ctx->Dispatch( cubeGroupCount, cubeGroupCount, CUBE_AXIS_COUNT );

	// Unset		
	m_ctx->CSSetShaderResources( HLSL_COLOR_SLOT, 1, (ID3D11ShaderResourceView**)nulls );
	m_ctx->CSSetShaderResources( HLSL_DEPTH_SLOT, 1, (ID3D11ShaderResourceView**)nulls );
	m_ctx->CSSetUnorderedAccessViews( HLSL_SAMPLE_SLOT, 1, (ID3D11UnorderedAccessView**)nulls, nullptr );
	m_ctx->CSSetUnorderedAccessViews( HLSL_SAMPLE_INDIRECT_ARGS_SLOT, 1, (ID3D11UnorderedAccessView**)nulls, nullptr );
				
	m_userDefinedAnnotation->EndEvent();
	
	if ( s_logReadBack )
	{
		// Read back
		core::u32* collectedIndirectArgs = GPUBUffer_MapReadBack< core::u32 >( m_ctx, &m_sample.indirectArgs );
		
		printf( "\n" );
		ReadBack_Log( "sample", collectedIndirectArgs[sample_groupCountX_offset], "groupCountX" );
		V6_ASSERT( collectedIndirectArgs[sample_groupCountY_offset] == 1 );
		V6_ASSERT( collectedIndirectArgs[sample_groupCountZ_offset] == 1 );
		ReadBack_Log( "sample", collectedIndirectArgs[sample_count_offset], "count" );		
		V6_ASSERT( collectedIndirectArgs[sample_count_offset] <= m_config.sampleCount );
#if HLSL_DEBUG_COLLECT == 1
		ReadBack_Log( "sample", collectedIndirectArgs[sample_out_offset], "out" );
		ReadBack_Log( "sample", collectedIndirectArgs[sample_error_offset], "error" );
		V6_ASSERT( collectedIndirectArgs[sample_error_offset] == 0 );
#endif // #if HLSL_DEBUG_COLLECT == 1

		GPUBUffer_UnmapReadBack( m_ctx, &m_sample.indirectArgs );
	}
}

core::u32 CRenderingDevice::BuildNode( bool clear )
{
	static const void* nulls[8] = {};

	m_userDefinedAnnotation->BeginEvent( L"BuildNode");

	V6_ASSERT( GRID_COUNT < HLSL_MIP_MAX_COUNT );

	if ( clear )
	{
		core::u32 values[4] = {};
		m_ctx->ClearUnorderedAccessViewUint( m_octree.indirectArgs.uav, values );
		m_ctx->ClearUnorderedAccessViewUint( m_octree.firstChildOffsetsLimitedUAV, values );
	}

	m_ctx->CSSetConstantBuffers( v6::hlsl::CBOctreeSlot, 1, &m_octreeCBUF.buf );
	m_ctx->CSSetShaderResources( HLSL_SAMPLE_SLOT, 1, &m_sample.samples.srv );
	m_ctx->CSSetShaderResources( HLSL_SAMPLE_INDIRECT_ARGS_SLOT, 1, &m_sample.indirectArgs.srv );
	m_ctx->CSSetUnorderedAccessViews( HLSL_OCTREE_SAMPLE_NODE_OFFSET_SLOT, 1, &m_octree.sampleNodeOffsets.uav, nullptr );
	m_ctx->CSSetUnorderedAccessViews( HLSL_OCTREE_FIRST_CHILD_OFFSET_SLOT, 1, &m_octree.firstChildOffsets.uav, nullptr );
	m_ctx->CSSetUnorderedAccessViews( HLSL_OCTREE_LEAF_SLOT, 1, &m_octree.leaves.uav, nullptr );
	m_ctx->CSSetUnorderedAccessViews( HLSL_OCTREE_INDIRECT_ARGS_SLOT, 1, &m_octree.indirectArgs.uav, nullptr );		

	m_ctx->CSSetShader( m_computes[COMPUTE_BUILDINNER].m_computeShader, nullptr, 0 );

	for ( core::u32 level = 0; level < HLSL_GRID_SHIFT; ++level )
	{
		// Update buffers				
		{
			v6::hlsl::CBOctree* cbOctree = ConstantBuffer_MapWrite< v6::hlsl::CBOctree >( m_ctx, &m_octreeCBUF );
			cbOctree->c_octreeCurrentLevel = level;
			cbOctree->c_octreeCurrentBucket = 0;
			ConstantBuffer_UnmapWrite( m_ctx, &m_octreeCBUF );
		}

		if ( level == HLSL_GRID_SHIFT-1 )
			m_ctx->CSSetShader( m_computes[COMPUTE_BUILDLEAF].m_computeShader, nullptr, 0 );

		m_ctx->DispatchIndirect( m_sample.indirectArgs.buf, sample_groupCountX_offset * sizeof( core::u32 ) );
	}

	// Unset
	m_ctx->CSSetShaderResources( HLSL_SAMPLE_SLOT, 1, (ID3D11ShaderResourceView**)nulls);
	m_ctx->CSSetShaderResources( HLSL_SAMPLE_INDIRECT_ARGS_SLOT, 1, (ID3D11ShaderResourceView**)nulls);
	m_ctx->CSSetUnorderedAccessViews( HLSL_OCTREE_SAMPLE_NODE_OFFSET_SLOT, 1, (ID3D11UnorderedAccessView**)nulls, nullptr );
	m_ctx->CSSetUnorderedAccessViews( HLSL_OCTREE_FIRST_CHILD_OFFSET_SLOT, 1, (ID3D11UnorderedAccessView**)nulls, nullptr );
	m_ctx->CSSetUnorderedAccessViews( HLSL_OCTREE_LEAF_SLOT, 1, (ID3D11UnorderedAccessView**)nulls, nullptr );
	m_ctx->CSSetUnorderedAccessViews( HLSL_OCTREE_INDIRECT_ARGS_SLOT, 1, (ID3D11UnorderedAccessView**)nulls, nullptr );
				
	m_userDefinedAnnotation->EndEvent();

	core::u32* octreeIndirectArgs = GPUBUffer_MapReadBack< core::u32 >( m_ctx, &m_octree.indirectArgs );	

	if ( s_logReadBack )
	{
		printf( "\n" );
		ReadBack_Log( "octree", octreeIndirectArgs[octree_nodeCount_offset], "nodeCount" );
		V6_ASSERT( octreeIndirectArgs[octree_nodeCount_offset] <= m_config.nodeCount );
		ReadBack_Log( "octree", octreeIndirectArgs[octree_leafGroupCountX_offset], "leafGroupCountX" );
		V6_ASSERT( octreeIndirectArgs[octree_leafGroupCountY_offset] == 1 );
		V6_ASSERT( octreeIndirectArgs[octree_leafGroupCountZ_offset] == 1 );
		ReadBack_Log( "octree", octreeIndirectArgs[octree_leafCount_offset], "leafCount" );
	}

	const core::u32 leafCount = octreeIndirectArgs[octree_leafCount_offset];
	V6_ASSERT( leafCount <= m_config.leafCount );

	GPUBUffer_UnmapReadBack( m_ctx, &m_octree.indirectArgs );

	return leafCount;
}

void CRenderingDevice::FillLeaf()
{
	static const void* nulls[8] = {};

	m_userDefinedAnnotation->BeginEvent( L"FillLeaf");

	V6_ASSERT( GRID_COUNT < HLSL_MIP_MAX_COUNT );

	m_ctx->CSSetConstantBuffers( v6::hlsl::CBOctreeSlot, 1, &m_octreeCBUF.buf );
	m_ctx->CSSetShaderResources( HLSL_SAMPLE_SLOT, 1, &m_sample.samples.srv );
	m_ctx->CSSetShaderResources( HLSL_SAMPLE_INDIRECT_ARGS_SLOT, 1, &m_sample.indirectArgs.srv );
	m_ctx->CSSetShaderResources( HLSL_OCTREE_SAMPLE_NODE_OFFSET_SLOT, 1, &m_octree.sampleNodeOffsets.srv );
	m_ctx->CSSetShaderResources( HLSL_OCTREE_FIRST_CHILD_OFFSET_SLOT, 1, &m_octree.firstChildOffsets.srv );
	m_ctx->CSSetUnorderedAccessViews( HLSL_OCTREE_LEAF_SLOT, 1, &m_octree.leaves.uav, nullptr );
	m_ctx->CSSetShader( m_computes[COMPUTE_FILLLEAF].m_computeShader, nullptr, 0 );

	// Update buffers				
	{
		v6::hlsl::CBOctree* cbOctree = ConstantBuffer_MapWrite< v6::hlsl::CBOctree >( m_ctx, &m_octreeCBUF );
		cbOctree->c_octreeCurrentLevel = 0;
		cbOctree->c_octreeCurrentBucket = 0;
		ConstantBuffer_UnmapWrite( m_ctx, &m_octreeCBUF );		
	}

	m_ctx->DispatchIndirect( m_sample.indirectArgs.buf, sample_groupCountX_offset * sizeof( core::u32 ) );

	// Unset
	m_ctx->CSSetShaderResources( HLSL_SAMPLE_SLOT, 1, (ID3D11ShaderResourceView**)nulls);
	m_ctx->CSSetShaderResources( HLSL_SAMPLE_INDIRECT_ARGS_SLOT, 1, (ID3D11ShaderResourceView**)nulls);
	m_ctx->CSSetShaderResources( HLSL_OCTREE_SAMPLE_NODE_OFFSET_SLOT, 1, (ID3D11ShaderResourceView**)nulls);
	m_ctx->CSSetShaderResources( HLSL_OCTREE_FIRST_CHILD_OFFSET_SLOT, 1, (ID3D11ShaderResourceView**)nulls);
	m_ctx->CSSetUnorderedAccessViews( HLSL_OCTREE_LEAF_SLOT, 1, (ID3D11UnorderedAccessView**)nulls, nullptr );
				
	m_userDefinedAnnotation->EndEvent();
}

void CRenderingDevice::PackColor()
{
	static const void* nulls[8] = {};

	m_userDefinedAnnotation->BeginEvent( L"Pack");

	V6_ASSERT( GRID_COUNT < HLSL_MIP_MAX_COUNT );

	core::u32 values[4] = {};
	m_ctx->ClearUnorderedAccessViewUint( m_block.indirectArgs.uav, values );

	m_ctx->CSSetConstantBuffers( v6::hlsl::CBOctreeSlot, 1, &m_octreeCBUF.buf );
	m_ctx->CSSetShaderResources( HLSL_OCTREE_FIRST_CHILD_OFFSET_SLOT, 1, &m_octree.firstChildOffsets.srv );
	m_ctx->CSSetShaderResources( HLSL_OCTREE_LEAF_SLOT, 1, &m_octree.leaves.srv );
	m_ctx->CSSetShaderResources( HLSL_OCTREE_INDIRECT_ARGS_SLOT, 1, &m_octree.indirectArgs.srv );
	m_ctx->CSSetUnorderedAccessViews( HLSL_BLOCK_COLOR_SLOT, 1, &m_block.colors.uav, nullptr );
	m_ctx->CSSetUnorderedAccessViews( HLSL_BLOCK_INDIRECT_ARGS_SLOT, 1, &m_block.indirectArgs.uav, nullptr );
	m_ctx->CSSetShader( m_computes[COMPUTE_PACKCOLOR].m_computeShader, nullptr, 0 );

	for ( core::u32 bucket = 0; bucket < HLSL_BUCKET_COUNT; ++bucket )
	{
		// Update buffers
		{
			v6::hlsl::CBOctree* cbOctree = ConstantBuffer_MapWrite< v6::hlsl::CBOctree >( m_ctx, &m_octreeCBUF );
			cbOctree->c_octreeCurrentLevel = 0;
			cbOctree->c_octreeCurrentBucket = bucket;
			ConstantBuffer_UnmapWrite( m_ctx, &m_octreeCBUF );		
		}

		m_ctx->DispatchIndirect( m_octree.indirectArgs.buf, octree_leafGroupCountX_offset * sizeof( core::u32 ) );
	}

	// Unset
	m_ctx->CSSetShaderResources( HLSL_OCTREE_FIRST_CHILD_OFFSET_SLOT, 1, (ID3D11ShaderResourceView**)nulls);
	m_ctx->CSSetShaderResources( HLSL_OCTREE_LEAF_SLOT, 1, (ID3D11ShaderResourceView**)nulls);
	m_ctx->CSSetShaderResources( HLSL_OCTREE_INDIRECT_ARGS_SLOT, 1, (ID3D11ShaderResourceView**)nulls);
	m_ctx->CSSetUnorderedAccessViews( HLSL_BLOCK_COLOR_SLOT, 1, (ID3D11UnorderedAccessView**)nulls, nullptr );
	m_ctx->CSSetUnorderedAccessViews( HLSL_BLOCK_INDIRECT_ARGS_SLOT, 1, (ID3D11UnorderedAccessView**)nulls, nullptr );
				
	m_userDefinedAnnotation->EndEvent();

	if ( s_logReadBack )
	{
		core::u32* blockIndirectArgs = GPUBUffer_MapReadBack< core::u32 >( m_ctx, &m_block.indirectArgs );

		core::u32 allRealCellCount = 0;
		core::u32 allMaxCellCount = 0; 

		for ( core::u32 bucket = 0; bucket < HLSL_BUCKET_COUNT; ++bucket )
		{
			if ( blockIndirectArgs[block_count_offset( bucket )] == 0 )
				continue;

			static const core::u32 cellPerBucketCounts[] = { 4, 8, 16, 32, 64 };
			const core::u32 maxCellCount = blockIndirectArgs[block_count_offset( bucket )] * cellPerBucketCounts[bucket];

			printf( "\n" );
			ReadBack_Log( "block", bucket, "bucket" );
			V6_ASSERT( blockIndirectArgs[block_vertexCountPerInstance_offset( bucket )] == 1 );
			ReadBack_Log( "block", blockIndirectArgs[block_renderInstanceCount_offset( bucket )], "renderInstanceCount" );
			V6_ASSERT( blockIndirectArgs[block_startVertexLocation_offset( bucket )] == 0 );
			V6_ASSERT( blockIndirectArgs[block_renderInstanceLocation_offset( bucket )] == 0 );
			ReadBack_Log( "block", blockIndirectArgs[block_count_offset( bucket )], "blockCount" );
			ReadBack_Log( "block", blockIndirectArgs[block_packedOffset_offset( bucket )], "packedOffset" );
			ReadBack_Log( "block", blockIndirectArgs[block_cellCount_offset( bucket )], "realCellCount" );
			ReadBack_Log( "block", maxCellCount, "maxCellCount" );

			allRealCellCount += blockIndirectArgs[block_cellCount_offset( bucket )];
			allMaxCellCount += maxCellCount;
		}		

		if ( allMaxCellCount )
		{
			printf( "\n" );
			ReadBack_Log( "packed_all", allRealCellCount, "realCellCount" );
			ReadBack_Log( "packed_all", allMaxCellCount, "maxCellCount" );
		}

		GPUBUffer_UnmapReadBack( m_ctx, &m_block.indirectArgs );
	}
}


void CRenderingDevice::DrawBlock( const core::Mat4x4* viewMatrix )
{
	// Rasterization state
	m_ctx->OMSetDepthStencilState( m_depthStencilStateZRW, 0 );
	m_ctx->OMSetBlendState( m_blendStateOpaque, nullptr, 0XFFFFFFFF );
		
	// Viewport
	{
		D3D11_VIEWPORT viewport = {};
		viewport.TopLeftX = 0;
		viewport.TopLeftY = 0;
		viewport.Width = (float)m_width;
		viewport.Height = (float)m_height;
		viewport.MinDepth = 0.0f;
		viewport.MaxDepth = 1.0f;

		m_ctx->RSSetViewports( 1, &viewport );
	}

	// RT
	m_ctx->OMSetRenderTargets( 1, &m_pColorView, m_pDepthStencilView );

	{
		v6::hlsl::CBView* cbView = ConstantBuffer_MapWrite< v6::hlsl::CBView >( m_ctx, &m_viewCBUF );

		cbView->c_frameObjectToView = *viewMatrix;
		cbView->c_frameViewToProj = m_projMatrix;

		ConstantBuffer_UnmapWrite( m_ctx, &m_viewCBUF );		
	}

	// Render

	m_userDefinedAnnotation->BeginEvent( L"Draw Blocks");

	float const pRGBA[] = { 0.0f, 0.0f, 0.0f, 0.0f };
	m_ctx->ClearRenderTargetView( m_pColorView, pRGBA );
	m_ctx->ClearDepthStencilView( m_pDepthStencilView, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0 );

	m_ctx->VSSetConstantBuffers( v6::hlsl::CBViewSlot, 1, &m_viewCBUF.buf );
	m_ctx->VSSetConstantBuffers( v6::hlsl::CBBlockSlot, 1, &m_blockCBUF.buf );

	// set
	m_ctx->VSSetShaderResources( HLSL_BLOCK_COLOR_SLOT, 1, &m_block.colors.srv );
	m_ctx->VSSetShaderResources( HLSL_BLOCK_INDIRECT_ARGS_SLOT, 1, &m_block.indirectArgs.srv );

	float gridScale = GRID_MIN_SCALE;

	{
		v6::hlsl::CBBlock* cbBlock = ConstantBuffer_MapWrite< v6::hlsl::CBBlock >( m_ctx, &m_blockCBUF );

		float gridScale = GRID_MIN_SCALE;
		for ( core::u32 gridID = 0; gridID < GRID_COUNT; ++gridID, gridScale *= 2 )
			cbBlock->c_blockGridScales[gridID] = core::Vec4_Make( gridScale, 0.0f, 0.0f, 0.0f );
		for ( core::u32 gridID = GRID_COUNT; gridID < HLSL_MIP_MAX_COUNT; ++gridID )
			cbBlock->c_blockGridScales[gridID] = core::Vec4_Make( 0.0f, 0.0f, 0.0f, 0.0f );

		ConstantBuffer_UnmapWrite( m_ctx, &m_blockCBUF );
	}		

	for ( core::u32 bucket = 0; bucket < HLSL_BUCKET_COUNT; ++bucket )
	{
		m_userDefinedAnnotation->BeginEvent( L"Draw Bucket");
			
		Mesh_Draw( &m_meshes[MESH_POINT], -1, &m_shaders[SHADER_BLOCK_RENDER4+bucket], m_ctx, m_block.indirectArgs.buf, block_vertexCountPerInstance_offset( bucket ) * sizeof( core::u32 ) );
			
		m_userDefinedAnnotation->EndEvent();
	}

	// unset
	static const void* nulls[8] = {};
	m_ctx->VSSetShaderResources( HLSL_BLOCK_COLOR_SLOT, 1, (ID3D11ShaderResourceView**)nulls );
	m_ctx->VSSetShaderResources( HLSL_BLOCK_INDIRECT_ARGS_SLOT, 1, (ID3D11ShaderResourceView**)nulls );

	m_userDefinedAnnotation->EndEvent();

	// un RT
	m_ctx->OMSetRenderTargets( 0, nullptr, nullptr );
}

void CRenderingDevice::PixelFilter()
{
	// Render

	m_userDefinedAnnotation->BeginEvent( L"Filter Pixels");
	
	// set

	m_ctx->CSSetConstantBuffers( v6::hlsl::CBPixelSlot, 1, &m_pixelCBUF.buf );

	m_ctx->CSSetShaderResources( HLSL_COLOR_SLOT, 1, &m_pColorSRV );
	m_ctx->CSSetShaderResources( HLSL_DEPTH_SLOT, 1, &m_pDepthStencilSRV );	
	m_ctx->CSSetUnorderedAccessViews( HLSL_PIXEL_COLOR_SLOT, 1, &m_pixel.colors.uav, nullptr );
	m_ctx->CSSetShader( m_computes[COMPUTE_FILTERPIXEL].m_computeShader, nullptr, 0 );

	float gridScale = GRID_MIN_SCALE;

	{
		v6::hlsl::CBPixel* cbPixel = ConstantBuffer_MapWrite< v6::hlsl::CBPixel >( m_ctx, &m_pixelCBUF );
				
		cbPixel->c_pixelDepthLinearScale = -1.0f / ZNEAR;
		cbPixel->c_pixelDepthLinearBias = 1.0f / ZNEAR;
		float gridScale = GRID_MIN_SCALE;
		for ( core::u32 gridID = 0; gridID < GRID_COUNT; ++gridID, gridScale *= 2 )
			cbPixel->c_pixelInvCellSizes[gridID] = core::Vec4_Make( HLSL_GRID_WIDTH / (gridScale * 2.0f), 0.0f, 0.0f, 0.0f );

		ConstantBuffer_UnmapWrite( m_ctx, &m_pixelCBUF  );
	}		

	V6_ASSERT( (m_width & 0xF) == 0 );
	V6_ASSERT( (m_height & 0xF) == 0 );
	const core::u32 pixelGroupWidth = m_width >> 4;
	const core::u32 pixelGroupHeight = m_height >> 4;
	m_ctx->Dispatch( pixelGroupWidth, pixelGroupHeight, 1 );

	// unset
	static const void* nulls[8] = {};
	m_ctx->CSSetShaderResources( HLSL_COLOR_SLOT, 1, (ID3D11ShaderResourceView**)nulls );
	m_ctx->CSSetShaderResources( HLSL_DEPTH_SLOT, 1, (ID3D11ShaderResourceView**)nulls );	
	m_ctx->CSSetUnorderedAccessViews( HLSL_PIXEL_COLOR_SLOT, 1, (ID3D11UnorderedAccessView**)nulls, nullptr );

	// Blit

	m_ctx->CopyResource( m_pColorBuffer, m_pixel.colors.tex );

	m_userDefinedAnnotation->EndEvent();	
}

void CRenderingDevice::Draw( float dt )
{
	static int lastMousePosX = -1;
	static int lastMousePosY = -1;
	static int lastKeyPosX = -1;
	static int lastKeyPosZ = -1;
	
	int mouseDeltaX = 0;
	int mouseDeltaY = 0;
	int keyDeltaX = 0;
	int keyDeltaZ = 0;

	if ( g_mousePressed )
	{		
		mouseDeltaX = lastMousePosX < 0 ? 0 : g_mousePosX - lastMousePosX;
		mouseDeltaY = lastMousePosY < 0 ? 0 : g_mousePosY - lastMousePosY;
		lastMousePosX = g_mousePosX;
		lastMousePosY = g_mousePosY;
	}
	else
	{
		mouseDeltaX = 0;
		mouseDeltaY = 0;
		lastMousePosX = -1;
		lastMousePosY = -1;
	}

	if ( g_keyLeftPressed != g_keyRightPressed )
	{
		keyDeltaX = g_keyLeftPressed ? -1 : 1;
	}

	if ( g_keyDownPressed != g_keyUpPressed )
	{
		keyDeltaZ = g_keyDownPressed ? -1 : 1;
	}
		
	const static float MOUSE_ROTATION_SPEED = 0.5f;
	const static float KEY_TRANSLATION_SPEED = 100.0f;
	
	s_yaw += -mouseDeltaX * MOUSE_ROTATION_SPEED * dt;
	s_pitch += -mouseDeltaY * MOUSE_ROTATION_SPEED * dt;

	const core::Mat4x4 yawMatrix = core::Mat4x4_RotationY( s_yaw );
	const core::Mat4x4 pitchMatrix = core::Mat4x4_RotationX( s_pitch );
	core::Mat4x4 orientationMatrix;
	core::Mat4x4_Mul( &orientationMatrix, yawMatrix, pitchMatrix );	
	core::Mat4x4_Transpose( &orientationMatrix );

	if ( keyDeltaX || keyDeltaZ )
	{
		s_headOffset += *orientationMatrix.GetXAxis() * (float)keyDeltaX * KEY_TRANSLATION_SPEED * dt;
		s_headOffset += -*orientationMatrix.GetZAxis() * (float)keyDeltaZ * KEY_TRANSLATION_SPEED * dt;
	}
	else if ( g_limit && s_headOffset.LengthSq() > 0.001f )
	{
		s_headOffset -= s_headOffset * core::Min( 0.5f, KEY_TRANSLATION_SPEED * dt );
	}

	if ( g_limit )
	{
		s_headOffset.x = core::Clamp( s_headOffset.x, -FREE_SCALE, FREE_SCALE );
		s_headOffset.y = core::Clamp( s_headOffset.y, -FREE_SCALE, FREE_SCALE );
		s_headOffset.z = core::Clamp( s_headOffset.z, -FREE_SCALE, FREE_SCALE );
	}

	core::Mat4x4 viewMatrix;
	core::Mat4x4_Mul( &viewMatrix, yawMatrix, pitchMatrix );
	core::Mat4x4_SetTranslation( &viewMatrix, s_headOffset );
	core::Mat4x4_AffineInverse( &viewMatrix );
	
	if ( g_drawMode == DRAW_MODE_DEFAULT )
	{
		DrawWorld( &viewMatrix );
	}
	else if ( g_drawMode == DRAW_MODE_CUBE )
	{
		Capture( &s_headOffset );

		DrawCube( &viewMatrix );
	}
	else if ( g_drawMode == DRAW_MODE_BLOCK )
	{		
		if ( g_sample < SAMPLE_MAX_COUNT )
		{
			printf( "Capturing sample #%03d...", g_sample );

			const core::Vec3 sampleOffset = m_sampleOffsets[g_sample];

			Capture( &sampleOffset );
		
			Collect( &sampleOffset );
			static core::u32 sumLeafCount = 0;
			if ( g_sample == 0 )
				sumLeafCount = 0;
			const core::u32 newLeafCount = BuildNode( g_sample == 0 ) - sumLeafCount;
			sumLeafCount += newLeafCount;
			FillLeaf();			

			printf( "\rCapturing sample #%03d: %13s cells added\n", g_sample, FormatInteger_Unsafe( newLeafCount ) );
			
			++g_sample;

			if ( g_sample == SAMPLE_MAX_COUNT )
			{
				PackColor();
				printf( "          all samples: %13s cells added\n", FormatInteger_Unsafe( sumLeafCount ) );
			}
		}

		DrawBlock( &viewMatrix );
		if ( g_filterPixel )
			PixelFilter();

		s_logReadBack = false;
	}
}

void CRenderingDevice::Present()
{
	m_pSwapChain->Present( 0, 0 );
}

void CRenderingDevice::Release()
{
	m_ctx->ClearState();
	
	Cube_Release( &m_cube );
	Sample_Release( m_device, &m_sample );
	Octree_Release( m_device, &m_octree );
	Block_Release( m_device, &m_block );
	Pixel_Release( m_device, &m_pixel );

	ConstantBuffer_Release( &m_viewCBUF );
	ConstantBuffer_Release( &m_sampleCBUF );
	ConstantBuffer_Release( &m_octreeCBUF );
	ConstantBuffer_Release( &m_blockCBUF );
	ConstantBuffer_Release( &m_pixelCBUF );

	for ( uint meshID = 0; meshID < MESH_COUNT; ++meshID )
	{
		Mesh_s* mesh = &m_meshes[meshID];
		if ( mesh->m_vertexBuffer )
			mesh->m_vertexBuffer->Release();
		if ( mesh->m_indexBuffer )
			mesh->m_indexBuffer->Release();
	}

	for ( uint computeID = 0; computeID < COMPUTE_COUNT; ++computeID )
	{
		Compute_s* compute = &m_computes[computeID];
		compute->m_computeShader->Release();		
	}

	for ( uint shaderID = 0; shaderID < SHADER_COUNT; ++shaderID )
	{
		Shader_s* shader = &m_shaders[shaderID];
		shader->m_vertexShader->Release();
		shader->m_pixelShader->Release();
		shader->m_inputLayout->Release();
	}
		
	m_pDepthStencilView->Release();
	m_pDepthStencilBuffer->Release();

	m_pColorView->Release();
	m_pColorBuffer->Release();

	m_pSwapChain->Release();
	m_ctx->Release();
	m_device->Release();	
}

void CRenderingDevice::ReleaseObject(IUnknown** unknow)
{
	const ULONG refCount = (*unknow)->Release();
	V6_ASSERT(refCount == 0);
	*unknow = nullptr;
}

END_V6_VIEWER_NAMESPACE

int main()
{
	V6_LOG( "Viewer 0.1" );

	v6::core::CHeap heap;
	v6::core::Stack stack( &heap, 100 * 1024 * 1024 );
	v6::core::CFileSystem filesystem;

	const int nWidth = (HLSL_GRID_WIDTH >> 1) * v6::viewer::ZOOM / 2;
	const int nHeight = (HLSL_GRID_WIDTH >> 1) * v6::viewer::ZOOM / 2;	

	const char* const title = "V6 Player | version: 0.1";

	HWND hWnd = v6::viewer::CreateMainWindow( title, nWidth, nHeight );
	if (!hWnd)
	{
		V6_ERROR("Call to CreateWindow failed!");
		return -1;
	}

	v6::viewer::CRenderingDevice oRenderingDevice;
	if ( !oRenderingDevice.Create( nWidth, nHeight, hWnd, &filesystem, &heap, &stack ) )
	{
		V6_ERROR("Call to CRenderingDevice::Create failed!");
		return -1;
	}

	ShowWindow(hWnd, SW_SHOWNORMAL);
	UpdateWindow(hWnd);

	__int64 frameTickLast = GetTickCount(); 
	for ( __int64 frameId = 0; ; ++frameId )
	{
		__int64 frameTick = v6::core::GetTickCount(); 
		__int64 frameDelta = frameTick - frameTickLast;

		float dt = v6::core::Min( v6::core::ConvertTicksToSeconds( frameDelta ), 0.1f );
		while ( dt < 0.0095f )
		{
			Sleep( 1 );
			frameTick = v6::core::GetTickCount(); 
			frameDelta = frameTick - frameTickLast;
			dt = v6::core::ConvertTicksToSeconds( frameDelta );
		}

		frameTickLast = frameTick;

		if ( (frameId % 10) == 0 )
		{
			static __int64 fpsTickLast = frameTick;
			const __int64 fpsDelta = frameTick - fpsTickLast;
			fpsTickLast = frameTick;
			if ( fpsDelta > 0.0f )
			{
				const float fps = 10 / v6::core::ConvertTicksToSeconds( fpsDelta );
				char text[256];
				sprintf_s( text, sizeof( text ), "%s | fps: %.1f | %s mode", title, fps, v6::viewer::ModeToString( v6::viewer::g_drawMode ) );
				SetWindowTextA( hWnd, text );
			}
		}
		
		MSG msg;
		while ( PeekMessage(&msg, NULL, 0, 0, PM_REMOVE) )
		{
			TranslateMessage(&msg);
			DispatchMessageA(&msg);

			if (msg.message == WM_QUIT)
			{
				oRenderingDevice.Release();
				return 0;
			}
		}
			
		oRenderingDevice.Draw( dt );
		oRenderingDevice.Present();
	}
}
