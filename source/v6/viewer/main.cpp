/*V6*/

#include <v6/viewer/common.h>
#include <v6/viewer/common_shared.h>

#include <v6/core/memory.h>
#include <v6/core/vec3.h>
#include <v6/core/math.h>
#include <v6/core/mat4x4.h>
#include <v6/core/color.h>
#include <v6/core/filesystem.h>
#include <v6/core/frame_manager.h>
#include <v6/core/stream.h>
#include <v6/core/time.h>

#include <windows.h>
#include <Windowsx.h>
#include <d3d11.h>

#pragma comment(lib, "d3d11.lib")

#define V6_ASSERT_D3D11( EXP )  { HRESULT hRes = EXP; V6_ASSERT( hRes == S_OK ); }

BEGIN_V6_VIEWER_NAMESPACE

static const float zNear = 1.0f;
static const float zFar = 1000.0f;

static bool g_mousePressed = false;
static int g_mousePosX = 0;
static int g_mousePosY = 0;
static int g_keyLeftPressed = false;
static int g_keyRightPressed = false;
static int g_keyUpPressed = false;
static int g_keyDownPressed = false;

static int g_frameLimitation = true;

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
			case 'F': g_frameLimitation = !pressed; break;
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

HWND CreateMainWindow( const char * sTitle, int nWidth, int nHeight )
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

enum
{
	VERTEX_FORMAT_POSITION	= 1 << 0,
	VERTEX_FORMAT_COLOR		= 1 << 1
};

struct BasicVertex
{
	core::Vec3 position;
	core::SColor color;
};

struct Shader
{
	ID3D11VertexShader* m_vertexShader;
	ID3D11PixelShader* m_pixelShader;

	ID3D11InputLayout* m_inputLayout;
};

struct Mesh
{
	ID3D11Buffer* m_vertexBuffer;
	ID3D11Buffer* m_indexBuffer;	
	uint m_vertexCount;
	uint m_vertexSize;
	uint m_indexCount;
	uint m_indexSize;
	D3D11_PRIMITIVE_TOPOLOGY m_topology;
	uint m_shaderID;
};

struct Entity
{
	uint meshID;
	core::Vec3 pos;
	float scale;
};

enum
{
	SHADER_BASIC,
	SHADER_CLOUD,

	SHADER_COUNT
};

enum
{
	MESH_TRIANGLE,
	MESH_BOX_WIREFRAME,
	MESH_BOX_RED,
	MESH_BOX_BLUE,
	MESH_BOX_GREEN,

	MESH_COUNT
};

static const uint VERTEX_INPUT_MAX_COUNT = 2;
static const uint ENTITY_MAX_COUNT = 256;


struct Frame
{	
	ID3D11Texture2D* m_tex[2];
	ID3D11ShaderResourceView* m_srv[2];

	uint width;
	uint height;
};

static bool ShaderCreate( ID3D11Device* device, Shader* shader, const char* vs, const char* ps, uint vertexFormat, core::CFileSystem* fileSystem, core::IStack* stack )
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
			V6_ERROR( "ID3D11Device::CreateVertexShader failed!" );
		}
	}
	
	{
		D3D11_INPUT_ELEMENT_DESC idesc[VERTEX_INPUT_MAX_COUNT] = {};

		int inputCount = 0;
		
		if ( vertexFormat & VERTEX_FORMAT_POSITION )
		{
			V6_ASSERT( inputCount < VERTEX_INPUT_MAX_COUNT );

			idesc[inputCount].SemanticName = "POSITION";
			idesc[inputCount].SemanticIndex = 0;
			idesc[inputCount].Format = DXGI_FORMAT_R32G32B32_FLOAT;
			idesc[inputCount].InputSlot = 0;
			idesc[inputCount].AlignedByteOffset = 0;
			idesc[inputCount].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
			idesc[inputCount].InstanceDataStepRate = 0;

			++inputCount;
		}

		if ( vertexFormat & VERTEX_FORMAT_COLOR )
		{
			V6_ASSERT( inputCount < VERTEX_INPUT_MAX_COUNT );

			idesc[inputCount].SemanticName = "COLOR";
			idesc[inputCount].SemanticIndex = 0;
			idesc[inputCount].Format = DXGI_FORMAT_R8G8B8A8_UNORM;
			idesc[inputCount].InputSlot = 0;
			idesc[inputCount].AlignedByteOffset = 12;
			idesc[inputCount].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
			idesc[inputCount].InstanceDataStepRate = 0;

			++inputCount;
		}

		HRESULT hRes = device->CreateInputLayout( idesc, inputCount, vsBytecode, vsBytecodeSize, &shader->m_inputLayout );

		if ( FAILED( hRes) )
		{
			V6_ERROR( "ID3D11Device::CreateInputLayout failed!" );
			return false;
		}
	}

	return true;
}

static void FrameCreate( ID3D11Device* device, Frame* frame, const core::FrameBuffer* frameBuffer )
{
	{
		D3D11_TEXTURE2D_DESC texDesc = {};
		texDesc.Width = frameBuffer->width;
		texDesc.Height = frameBuffer->height;
		texDesc.MipLevels = 1;
		texDesc.ArraySize = 1;
		texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
		texDesc.SampleDesc.Count = 1;
		texDesc.SampleDesc.Quality = 0;
		texDesc.Usage = D3D11_USAGE_DEFAULT;
		texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		texDesc.CPUAccessFlags = 0;
		texDesc.MiscFlags = 0;

		D3D11_SUBRESOURCE_DATA resData = {};
		resData.pSysMem = frameBuffer->colors;
		resData.SysMemPitch = frameBuffer->width * sizeof( core::SColor );
		resData.SysMemSlicePitch = 0;

		V6_ASSERT_D3D11( device->CreateTexture2D( &texDesc, &resData, &frame->m_tex[0] ) );
	}

	{
		D3D11_SHADER_RESOURCE_VIEW_DESC viewDesc = {};
		viewDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
		viewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		viewDesc.Texture2D.MipLevels = -1;
		viewDesc.Texture2D.MostDetailedMip = 0;

		V6_ASSERT_D3D11( device->CreateShaderResourceView( frame->m_tex[0], &viewDesc, &frame->m_srv[0] ) );
	}

	{
		D3D11_TEXTURE2D_DESC texDesc = {};
		texDesc.Width = frameBuffer->width;
		texDesc.Height = frameBuffer->height;
		texDesc.MipLevels = 1;
		texDesc.ArraySize = 1;
		texDesc.Format = DXGI_FORMAT_R32_FLOAT;
		texDesc.SampleDesc.Count = 1;
		texDesc.SampleDesc.Quality = 0;
		texDesc.Usage = D3D11_USAGE_DEFAULT;
		texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		texDesc.CPUAccessFlags = 0;
		texDesc.MiscFlags = 0;

		D3D11_SUBRESOURCE_DATA resData = {};
		resData.pSysMem = frameBuffer->depths;
		resData.SysMemPitch = frameBuffer->width * sizeof( float );
		resData.SysMemSlicePitch = 0;

		V6_ASSERT_D3D11( device->CreateTexture2D( &texDesc, &resData, &frame->m_tex[1] ) );
	}

	{
		D3D11_SHADER_RESOURCE_VIEW_DESC viewDesc = {};
		viewDesc.Format = DXGI_FORMAT_R32_FLOAT;
		viewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		viewDesc.Texture2D.MipLevels = -1;
		viewDesc.Texture2D.MostDetailedMip = 0;

		V6_ASSERT_D3D11( device->CreateShaderResourceView( frame->m_tex[1], &viewDesc, &frame->m_srv[1] ) );
	}

	frame->width = frameBuffer->width;
	frame->height = frameBuffer->height;
}

static void MeshCreate( ID3D11Device* device, Mesh* mesh, const void* vertices, uint vertexCount, uint vertexSize, const void* indices, uint indexCount, uint indexSize, D3D11_PRIMITIVE_TOPOLOGY topology, uint shaderID )
{
	mesh->m_vertexBuffer = nullptr;
	mesh->m_vertexCount = vertexCount;
	mesh->m_vertexSize = 0;
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

		mesh->m_vertexSize = vertexSize;
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

		mesh->m_indexCount = indexCount;
		mesh->m_indexSize = indexSize;
	}

	mesh->m_topology = topology;
	mesh->m_shaderID = shaderID;
}

static void MeshCreateTriangle( ID3D11Device* device, Mesh* mesh, uint shaderID )
{
	const BasicVertex vertices[3] = 
	{
		{ core::Vec3_Make( 0.0f, 1.0f, 0.0f ), core::Color_Make( 255, 0, 0, 255) },
		{ core::Vec3_Make( 1.0f, -1.0f, 0.0f ), core::Color_Make( 0, 255, 0, 255) },
		{ core::Vec3_Make( -1.0f, -1.0f, 0.0f ), core::Color_Make( 0, 0, 255, 255) } 
	};

	const core::u16 indices[3] = { 0, 1, 2 };

	MeshCreate( device, mesh, vertices, 3, sizeof( BasicVertex ), indices, 3, sizeof( core::u16 ), D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST, shaderID );
}

static void MeshCreateBox( ID3D11Device* device, Mesh* mesh, uint shaderID, const core::SColor color, bool wireframe )
{
	const BasicVertex vertices[8] = 
	{
		{ core::Vec3_Make( -1.0f, -1.0f, -1.0f ), color },
		{ core::Vec3_Make(  1.0f, -1.0f, -1.0f ), color },
		{ core::Vec3_Make( -1.0f,  1.0f, -1.0f ), color },
		{ core::Vec3_Make(  1.0f,  1.0f, -1.0f ), color },
		{ core::Vec3_Make( -1.0f, -1.0f,  1.0f ), color },
		{ core::Vec3_Make(  1.0f, -1.0f,  1.0f ), color },
		{ core::Vec3_Make( -1.0f,  1.0f,  1.0f ), color },
		{ core::Vec3_Make(  1.0f,  1.0f,  1.0f ), color },
	};

	if ( wireframe )
	{
		const core::u16 indices[24] = { 
			0, 1, 1, 3, 3, 2, 2, 0,
			4, 5, 5, 7, 7, 6, 6, 4,
			1, 5, 0, 4, 3, 7, 2, 6 };

		MeshCreate( device, mesh, vertices, 8, sizeof( BasicVertex ), indices, 24, sizeof( core::u16 ), D3D11_PRIMITIVE_TOPOLOGY_LINELIST, shaderID );
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

		MeshCreate( device, mesh, vertices, 8, sizeof( BasicVertex ), indices, 36, sizeof( core::u16 ), D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST, shaderID );
	}
}

static void MeshCreateCloud( ID3D11Device* device, Mesh* mesh, uint shaderID, const core::FrameBuffer* frameBuffer, core::IStack* stack )
{
	core::ScopedStack scopedStack( stack );

	const uint faceWidth = frameBuffer->width / 3;
	const uint faceHeight = frameBuffer->height / 2;

	const uint vertexCount = frameBuffer->width * frameBuffer->height;

#if 1
	const uint indexCount = 6 * (faceHeight - 1) * ( faceWidth * 2 + 2 ) - 1;

	core::u32* const indices = (core::u32*)stack->alloc( indexCount * sizeof( core::u32 ) );
	core::u32* index = indices;
		
	for ( int faceID = 0; faceID < 6; ++faceID )
	{		
		const uint faceY = faceID / 3;
		const uint faceX = faceID - faceY * 3;
		uint offset = faceY * frameBuffer->width * faceHeight + faceX * faceWidth;
		for ( uint y = 0; y < faceHeight-1; ++y )
		{
			const uint bot = offset + ( faceY ? 0 : frameBuffer->width );
			const uint top = offset + ( faceY ? frameBuffer->width : 0 );
			if ( index != indices )
			{
				*(index++) = bot;
			}
			for ( uint x = 0; x < faceWidth; ++x )
			{
				*(index++) = bot + x;
				*(index++) = top + x;
			}
			*(index++) = top + faceWidth-1;			

			offset += frameBuffer->width;
		}
	}
#else
	const uint indexCount = 6 * faceHeight * faceWidth;

	core::u32* const indices = (core::u32*)stack->alloc( indexCount * sizeof( core::u32 ) );
	core::u32* index = indices;
		
	for ( int vertexID = 0; vertexID < vertexCount; ++vertexID )
	{		
		*(index++) = vertexID;
	}
#endif

	V6_ASSERT( index - indices == indexCount );
	
	MeshCreate( device, mesh, nullptr, vertexCount, 0, indices, indexCount, sizeof( core::u32 ), D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP, shaderID );
	// RenderableCreate( device, renderable, nullptr, vertexCount, 0, indices, indexCount, sizeof( core::u32 ), D3D11_PRIMITIVE_TOPOLOGY_POINTLIST, shaderID );
	//RenderableCreate( device, renderable, nullptr, vertexCount, 0, nullptr, 0, 0, D3D11_PRIMITIVE_TOPOLOGY_POINTLIST, shaderID );
}

void EntityCreate( Entity* entity, core::u32 meshID, const core::Vec3& pos, float scale )
{
	entity->meshID = meshID;
	entity->pos = pos;
	entity->scale = scale;
}

class CRenderingDevice
{
public:
	CRenderingDevice();
	~CRenderingDevice();

public:
	bool Create(int nWidth, int nHeight, HWND hWnd, core::FrameBuffer* frameBuffer, core::CFileSystem* fileSystem, core::IStack* stack );
	void Draw( float dt );
	void ClearBuffers();	
	void Present();
	void Release();
	void ReleaseObject(IUnknown** unknow);

	IDXGISwapChain* m_pSwapChain;
	ID3D11Device* m_device;
	D3D_FEATURE_LEVEL m_nFeatureLevel;
	ID3D11DeviceContext* m_ctx;
	ID3D11Texture2D* m_pColorBuffer;
	ID3D11Texture2D* m_pDepthStencilBuffer;
	ID3D11RenderTargetView* m_pColorView;
	ID3D11DepthStencilView* m_pDepthStencilView;

	Shader m_shaders[SHADER_COUNT];
	Mesh m_meshes[MESH_COUNT];
	Entity m_entities[ENTITY_MAX_COUNT];
	uint m_entityCount;

	Frame m_frame;

	ID3D11Buffer* m_cbuffer;

	uint m_width;
	uint m_height;
	float m_aspectRatio;
	core::Mat4x4 m_projMatrix;
};

CRenderingDevice::CRenderingDevice()
{
	memset( this, 0, sizeof( CRenderingDevice ) );
}

CRenderingDevice::~CRenderingDevice()
{
}

bool CRenderingDevice::Create( int nWidth, int nHeight, HWND hWnd, core::FrameBuffer* frameBuffer, core::CFileSystem* fileSystem, core::IStack* stack )
{
	core::ScopedStack scopedStack( stack );

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

	oSwapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	oSwapChainDesc.BufferCount = 2;
	oSwapChainDesc.OutputWindow = hWnd;
	oSwapChainDesc.Windowed = true;
	oSwapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
	oSwapChainDesc.Flags = 0;

	D3D_FEATURE_LEVEL pFeatureLevels[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_11_1 };
	m_pSwapChain = NULL;
	m_device = NULL;
	m_nFeatureLevel = (D3D_FEATURE_LEVEL)0;
	m_ctx = NULL;

	{
		HRESULT hRes = D3D11CreateDeviceAndSwapChain(
			NULL,
			D3D_DRIVER_TYPE_HARDWARE,
			NULL,
			0,
			pFeatureLevels,
			2,
			D3D11_SDK_VERSION,
			&oSwapChainDesc,
			&m_pSwapChain,
			&m_device,
			&m_nFeatureLevel,
			&m_ctx);

		if (FAILED(hRes))
		{
			V6_ERROR("D3D11CreateDeviceAndSwapChain failed!");
			return false;
		}
	}

	m_pColorBuffer = NULL;

	{
		HRESULT hRes = m_pSwapChain->GetBuffer( 0, __uuidof(ID3D11Texture2D), (void **)&m_pColorBuffer );

		if (FAILED(hRes))
		{
			V6_ERROR("IDXGISwapChain::GetBuffer failed!");
			return false;
		}
	}

	m_pColorView = NULL;

	{
		HRESULT hRes = m_device->CreateRenderTargetView( m_pColorBuffer, 0, &m_pColorView );

		if (FAILED(hRes))
		{
			V6_ERROR("ID3D11Device::CreateRenderTargetView failed!");
			return false;
		}
	}

	D3D11_TEXTURE2D_DESC oDepthStencilDesc;
	oDepthStencilDesc.Width = nWidth;
	oDepthStencilDesc.Height = nHeight;
	oDepthStencilDesc.MipLevels = 1;
	oDepthStencilDesc.ArraySize = 1;
	oDepthStencilDesc.Format = DXGI_FORMAT_D32_FLOAT;
	oDepthStencilDesc.SampleDesc.Count = 1;
	oDepthStencilDesc.SampleDesc.Quality = 0;
	oDepthStencilDesc.Usage = D3D11_USAGE_DEFAULT;
	oDepthStencilDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
	oDepthStencilDesc.CPUAccessFlags = 0;
	oDepthStencilDesc.MiscFlags = 0;

	m_pDepthStencilBuffer = 0;

	{
		HRESULT hRes = m_device->CreateTexture2D( &oDepthStencilDesc, 0, &m_pDepthStencilBuffer );

		if ( FAILED( hRes ) )
		{
			V6_ERROR( "ID3D11Device::CreateTexture2D failed!" );
			return false;
		}
	}

	D3D11_DEPTH_STENCIL_VIEW_DESC oDepthStencilViewDesc;
	oDepthStencilViewDesc.Format = DXGI_FORMAT_D32_FLOAT;
	oDepthStencilViewDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
	oDepthStencilViewDesc.Flags = 0;
	oDepthStencilViewDesc.Texture2D.MipSlice = 0;

	m_pDepthStencilView = 0;

	{
		HRESULT hRes = m_device->CreateDepthStencilView( m_pDepthStencilBuffer, &oDepthStencilViewDesc, &m_pDepthStencilView );

		if ( FAILED( hRes ) )
		{
			V6_ERROR( "ID3D11Device::CreateTexture2D failed!" );
			return false;
		}
	}

	m_ctx->OMSetRenderTargets( 1, &m_pColorView, m_pDepthStencilView );

	{
		D3D11_VIEWPORT viewport = {};
		viewport.TopLeftX = 0;
		viewport.TopLeftY = 0;
		viewport.Width = (float)nWidth;
		viewport.Height = (float)nHeight;

		m_ctx->RSSetViewports( 1, &viewport );
	}
	
	ShaderCreate( m_device, &m_shaders[SHADER_BASIC], "basic_vs.cso", "basic_ps.cso", VERTEX_FORMAT_POSITION | VERTEX_FORMAT_COLOR, fileSystem, stack );
	ShaderCreate( m_device, &m_shaders[SHADER_CLOUD], "cloud_vs.cso", "cloud_ps.cso", 0, fileSystem, stack );

	MeshCreateTriangle( m_device, &m_meshes[MESH_TRIANGLE], SHADER_BASIC );	
	MeshCreateBox( m_device, &m_meshes[MESH_BOX_WIREFRAME], SHADER_BASIC, core::Color_Make( 255, 255, 255, 255 ), true );
	MeshCreateBox( m_device, &m_meshes[MESH_BOX_RED], SHADER_BASIC, core::Color_Make( 255, 0, 0, 255 ), false );
	MeshCreateBox( m_device, &m_meshes[MESH_BOX_GREEN], SHADER_BASIC, core::Color_Make( 0, 255, 0, 255 ), false );
	MeshCreateBox( m_device, &m_meshes[MESH_BOX_BLUE], SHADER_BASIC, core::Color_Make( 0, 0, 255, 255 ), false );
	
	//RenderableCreateCloud( m_device, &m_renderables[m_renderableCount++], SHADER_CLOUD, frameBuffer, stack );

	EntityCreate( &m_entities[m_entityCount++], MESH_BOX_WIREFRAME, core::Vec3_Make( 0.0f, 0.0f, 0.0f), 500.0f );
	EntityCreate( &m_entities[m_entityCount++], MESH_TRIANGLE, core::Vec3_Make( 0.0f, 0.0f, -100.0f), 5.0f );
	EntityCreate( &m_entities[m_entityCount++], MESH_BOX_RED, core::Vec3_Make( -190.0f, 100.0f, -200.0f), 20.0f );
	EntityCreate( &m_entities[m_entityCount++], MESH_BOX_WIREFRAME, core::Vec3_Make( -190.0f, 100.0f, -200.0f), 20.0f );
	EntityCreate( &m_entities[m_entityCount++], MESH_BOX_GREEN, core::Vec3_Make( 110.0f, 200.0f, -120.0f), 100.0f );
	EntityCreate( &m_entities[m_entityCount++], MESH_BOX_WIREFRAME, core::Vec3_Make( 110.0f, 200.0f, -120.0f), 100.0f );
	EntityCreate( &m_entities[m_entityCount++], MESH_BOX_BLUE, core::Vec3_Make( 10.0f, -300.0f, -300.0f), 50.0f );
	EntityCreate( &m_entities[m_entityCount++], MESH_BOX_WIREFRAME, core::Vec3_Make( 10.0f, -300.0f, -300.0f), 50.0f );
	EntityCreate( &m_entities[m_entityCount++], MESH_BOX_GREEN, core::Vec3_Make( -120.0f, -150.0f, -50.0f), 80.0f );
	EntityCreate( &m_entities[m_entityCount++], MESH_BOX_WIREFRAME, core::Vec3_Make( -120.0f, -150.0f, -50.0f), 80.0f );
	EntityCreate( &m_entities[m_entityCount++], MESH_BOX_BLUE, core::Vec3_Make( 300.0f, 0.0f, 400.0f), 120.0f );
	EntityCreate( &m_entities[m_entityCount++], MESH_BOX_WIREFRAME, core::Vec3_Make( 300.0f, 0.0f, 400.0f), 120.0f );


	FrameCreate( m_device, &m_frame, frameBuffer );

	{
		D3D11_BUFFER_DESC bufDesc = {};	
		bufDesc.Usage = D3D11_USAGE_DYNAMIC;
		bufDesc.ByteWidth = sizeof( v6::hlsl::CBView );
		bufDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		bufDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		bufDesc.MiscFlags = 0;
		bufDesc.StructureByteStride = 0;

		HRESULT hRes = m_device->CreateBuffer( &bufDesc, nullptr, &m_cbuffer );
		
		if ( FAILED( hRes ) )
		{
			V6_ERROR( "ID3D11Device::CreateBuffer failed!" );
			return false;
		}
	}

	m_width = nWidth;
	m_height = nHeight;
	m_aspectRatio = (float)nWidth / nHeight;
	m_projMatrix = core::Mat4x4_Projection( zNear, zFar, core::DegToRad( 70.0f ), m_aspectRatio );
	
	return true;
}

void CRenderingDevice::ClearBuffers()
{
	float const pRGBA[] = { 0.0f, 0.0f, 0.0f, 0.0f };
	m_ctx->ClearRenderTargetView( m_pColorView, pRGBA );
	m_ctx->ClearDepthStencilView( m_pDepthStencilView, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0 );
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

	static float yaw = 0.0f;
	static float pitch = 0.0f;
	static float headOffsetX = 0.0f;
	static float headOffsetZ = 0.0f;
	const static float MOUSE_ROTATION_SPEED = 0.5f;
	const static float KEY_TRANSLATION_SPEED = 1.0f;
	
	yaw += -mouseDeltaX * MOUSE_ROTATION_SPEED * dt;
	pitch += -mouseDeltaY * MOUSE_ROTATION_SPEED * dt;

	if ( keyDeltaX )
	{
		headOffsetX += keyDeltaX * KEY_TRANSLATION_SPEED * dt;
	}
	else if ( fabs( headOffsetX - 0.0f ) > 0.001f )
	{
		headOffsetX += (headOffsetX > 0 ? -1 : 1) * KEY_TRANSLATION_SPEED * dt;
	}
	if ( keyDeltaZ )
	{
		headOffsetZ += -keyDeltaZ * KEY_TRANSLATION_SPEED * dt;
	}
	else if ( fabs( headOffsetZ - 0.0f ) > 0.001f )
	{
		headOffsetZ += (headOffsetZ > 0 ? -1 : 1) * KEY_TRANSLATION_SPEED * dt;
	}

	const core::Mat4x4 yawMatrix = core::Mat4x4_RotationY( yaw );
	const core::Mat4x4 pitchMatrix = core::Mat4x4_RotationX( pitch );
	core::Mat4x4 viewMatrix;
	core::Mat4x4_Mul( &viewMatrix, yawMatrix, pitchMatrix );
	core::Mat4x4_SetTranslation( &viewMatrix, core::Vec3_Make( headOffsetX, 0.0, headOffsetZ ) );
	core::Mat4x4_AffineInverse( &viewMatrix );
		
	m_ctx->VSSetShaderResources( HLSL_FIRST_SLOT, 2, m_frame.m_srv );
	m_ctx->PSSetShaderResources( HLSL_FIRST_SLOT, 2, m_frame.m_srv );

	for ( uint entityID = 0; entityID < m_entityCount; ++entityID )
	{
		Entity* entity = &m_entities[entityID];
		
		{
			D3D11_MAPPED_SUBRESOURCE res;
			V6_ASSERT_D3D11( m_ctx->Map( m_cbuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &res ) );

			v6::hlsl::CBView* cbView = (v6::hlsl::CBView*)res.pData;

			core::Mat4x4 worlMatrix;
			core::Mat4x4_Scale( &worlMatrix, entity->scale );
			core::Mat4x4_SetTranslation( &worlMatrix, entity->pos );
			
			// use this order because one matrix is "from" local space and the other is "to" local space
			core::Mat4x4_Mul( &cbView->objectToView, viewMatrix, worlMatrix );
			cbView->viewToProj = m_projMatrix;
			cbView->frameWidth = m_frame.width;
			cbView->frameHeight = m_frame.height;
			cbView->zFar = zFar;

			m_ctx->Unmap( m_cbuffer, 0 );
		}

		m_ctx->VSSetConstantBuffers( v6::hlsl::CBViewSlot, 1, &m_cbuffer );
		
		Mesh* mesh = &m_meshes[entity->meshID];
		Shader* shader = &m_shaders[mesh->m_shaderID];

		m_ctx->IASetInputLayout( shader->m_inputLayout );
		m_ctx->VSSetShader( shader->m_vertexShader, nullptr, 0 );
		m_ctx->PSSetShader( shader->m_pixelShader, nullptr, 0 );
		
		{
			const uint stride = mesh->m_vertexSize; 
			const uint offset = 0;
			
			m_ctx->IASetVertexBuffers( 0, 1, &mesh->m_vertexBuffer, &stride, &offset );
			switch ( mesh->m_indexSize )
			{
			case 1:
				m_ctx->IASetIndexBuffer( mesh->m_indexBuffer, DXGI_FORMAT_R8_UINT, 0 );
				break;
			case 2:
				m_ctx->IASetIndexBuffer( mesh->m_indexBuffer, DXGI_FORMAT_R16_UINT, 0 );
				break;
			case 4:
				m_ctx->IASetIndexBuffer( mesh->m_indexBuffer, DXGI_FORMAT_R32_UINT, 0 );
				break;
			default:
				V6_ASSERT( !"Not supprted");
			}
			m_ctx->IASetPrimitiveTopology( mesh->m_topology );
		}

		if ( mesh->m_indexCount )
		{
			m_ctx->DrawIndexed( mesh->m_indexCount, 0, 0 );
		}
		else
		{
			V6_ASSERT( mesh->m_vertexCount > 0 );
			m_ctx->Draw( mesh->m_vertexCount, 0 );
		}
	}
}

void CRenderingDevice::Present()
{
	m_pSwapChain->Present( 0, 0 );
}

void CRenderingDevice::Release()
{
	m_ctx->ClearState();

	m_cbuffer->Release();

	for ( uint texID = 0; texID < 2; ++texID )
	{
		m_frame.m_tex[texID]->Release();
		m_frame.m_srv[texID]->Release();
	}

	for ( uint meshID = 0; meshID < MESH_COUNT; ++meshID )
	{
		Mesh* mesh = &m_meshes[meshID];
		if ( mesh->m_vertexBuffer )
		{
			mesh->m_vertexBuffer->Release();
		}
		if ( mesh->m_indexBuffer )
		{
			mesh->m_indexBuffer->Release();
		}
	}

	for ( uint shaderID = 0; shaderID < SHADER_CLOUD; ++shaderID )
	{
		Shader* shader = &m_shaders[shaderID];
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
	V6_LOG("Viewer 0.0");

	v6::core::CHeap heap;
	v6::core::Stack stack( &heap, 100 * 1024 * 1024 );
	v6::core::CFileSystem filesystem;
	v6::core::FrameManager frameManager( &heap );
	v6::core::FrameBuffer* frameBuffer = nullptr;

	{
		stack.push();
		void* data;
		const int nSize = filesystem.ReadFile( "d:/data/v6/frameBuffer0.frm", &data, &stack );
		if ( nSize == -1 )
		{
			V6_ERROR( "Bad file" );
			return false;
		}

		if ( nSize < 4 + sizeof(v6::core::FrameDesc) )
		{
			V6_ERROR( "Bad header file size" );
			return false;
		}

		v6::core::CBufferReader bufferReader( data, nSize );
		char magic[5] = {};
		bufferReader.Read( 4, magic );
		if ( magic[0] != 'V' || magic[1] != '6' || magic[2] != 'F' || magic[3] != '0')
		{
			V6_ERROR( "Bad magic: %s", magic );
			return false;
		}

		v6::core::FrameDesc frameDesc;
		bufferReader.Read( sizeof( v6::core::FrameDesc ), &frameDesc );

		if ( bufferReader.GetRamaining() != v6::core::FrameManager::GetFrameBufferColorSize( &frameDesc ) + v6::core::FrameManager::GetFrameBufferDepthSize( &frameDesc ) )
		{
			V6_ERROR( "Bad data file size" );
			return false;
		}
		
		frameBuffer = frameManager.CreateFrameBuffer( &frameDesc );
		bufferReader.Read( v6::core::FrameManager::GetFrameBufferColorSize( &frameDesc ), frameBuffer->colors );
		bufferReader.Read( v6::core::FrameManager::GetFrameBufferDepthSize( &frameDesc ), frameBuffer->depths );
				
		stack.pop();
	}

	const int nWidth = 1280;
	const int nHeight = 720;	

	const char* const title = "V6 Player | version: 0.1";

	HWND hWnd = v6::viewer::CreateMainWindow( title, nWidth, nHeight );
	if (!hWnd)
	{
		V6_ERROR("Call to CreateWindow failed!");
		return -1;
	}

	v6::viewer::CRenderingDevice oRenderingDevice;
	if ( !oRenderingDevice.Create( nWidth, nHeight, hWnd, frameBuffer, &filesystem, &stack ) )
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
		while ( dt < 0.0095f && v6::viewer::g_frameLimitation )
		{
			Sleep( 1 );
			frameTick = v6::core::GetTickCount(); 
			frameDelta = frameTick - frameTickLast;
			dt = v6::core::ConvertTicksToSeconds( frameDelta );
		}

		frameTickLast = frameTick;

		if ( (frameId % 100) == 0 )
		{
			static __int64 fpsTickLast = frameTick;
			const __int64 fpsDelta = frameTick - fpsTickLast;
			fpsTickLast = frameTick;
			if ( fpsDelta > 0.0f )
			{
				const float fps = 100 / v6::core::ConvertTicksToSeconds( fpsDelta );
				char text[256];
				sprintf_s( text, sizeof( text ), "%s | fps: %.1f", title, fps );
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
			
		oRenderingDevice.ClearBuffers();
		oRenderingDevice.Draw( dt );
		oRenderingDevice.Present();
	}
}