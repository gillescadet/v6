/*V6*/

#include <v6/viewer/common.h>
#include <v6/viewer/basic_shared.h>

#include <v6/core/memory.h>
#include <v6/core/vec3.h>
#include <v6/core/math.h>
#include <v6/core/mat4x4.h>
#include <v6/core/color.h>
#include <v6/core/filesystem.h>
#include <v6/core/time.h>

#include <windows.h>
#include <Windowsx.h>
#include <d3d11.h>

#pragma comment(lib, "d3d11.lib")

#define V6_ASSERT_D3D11( EXP )  { HRESULT hRes = EXP; V6_ASSERT( hRes == S_OK ); }

BEGIN_V6_VIEWER_NAMESPACE

static bool g_mouseDown = false;
static int g_mousePosX = 0;
static int g_mousePosY = 0;

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
	case WM_LBUTTONDOWN:
	case WM_LBUTTONUP:
		{
			g_mouseDown = message == WM_LBUTTONDOWN;
			g_mousePosX = GET_X_LPARAM( lParam ); 
			g_mousePosY = GET_Y_LPARAM( lParam );

			if ( g_mouseDown )
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
			if ( g_mouseDown )
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

struct BasicVertex
{
	core::Vec3 position;
	core::SColor color;
};

struct Renderable
{
	ID3D11Buffer* m_vertexBuffer;
	ID3D11Buffer* m_indexBuffer;	
	uint m_indexCount;
	D3D11_PRIMITIVE_TOPOLOGY m_topology;
};

static const int RENDERABLE_MAX_COUNT = 16;


static void RenderableCreate( ID3D11Device* device, Renderable* renderable, const BasicVertex* vertices, uint vertexCount, const core::u16* indices, uint indexCount, D3D11_PRIMITIVE_TOPOLOGY topology  )
{
	{	
		D3D11_BUFFER_DESC bufDesc = {};
		bufDesc.Usage = D3D11_USAGE_DEFAULT;
		bufDesc.ByteWidth = sizeof( *vertices ) * vertexCount;
		bufDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		bufDesc.CPUAccessFlags = 0;
		bufDesc.MiscFlags = 0;
		bufDesc.StructureByteStride = 0;

		D3D11_SUBRESOURCE_DATA data = {};
		data.pSysMem = vertices;
		data.SysMemPitch = 0;
		data.SysMemSlicePitch = 0;

		V6_ASSERT_D3D11( device->CreateBuffer( &bufDesc, &data, &renderable->m_vertexBuffer ) );
	}

	{
		D3D11_BUFFER_DESC bufDesc = {};
		bufDesc.Usage = D3D11_USAGE_DEFAULT;
		bufDesc.ByteWidth = sizeof( *indices ) * indexCount;
		bufDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
		bufDesc.CPUAccessFlags = 0;
		bufDesc.MiscFlags = 0;
		bufDesc.StructureByteStride = 0;

		D3D11_SUBRESOURCE_DATA data = {};
		data.pSysMem = indices;
		data.SysMemPitch = 0;
		data.SysMemSlicePitch = 0;

		V6_ASSERT_D3D11( device->CreateBuffer( &bufDesc, &data, &renderable->m_indexBuffer ) );
	}

	renderable->m_indexCount = indexCount;

	renderable->m_topology = topology;
}

static void RenderableCreateTriangle( ID3D11Device* device, Renderable* renderable )
{
	const BasicVertex vertices[3] = 
	{
		{ core::Vec3_Make( 0.0f, 5.0f, 100.0f ), core::Color_Make( 255, 0, 0, 255) },
		{ core::Vec3_Make( 5.7f, -5.0f, 100.0f ), core::Color_Make( 0, 255, 0, 255) },
		{ core::Vec3_Make( -5.7f, -5.0f, 100.0f ), core::Color_Make( 0, 0, 255, 255) } 
	};

	const core::u16 indices[3] = { 0, 1, 2 };

	RenderableCreate( device, renderable, vertices, 3, indices, 3, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST );
}

static void RenderableCreateBox( ID3D11Device* device, Renderable* renderable )
{
	const core::SColor colorFront = core::Color_Make( 0, 200, 0, 255);
	const core::SColor colorBack = core::Color_Make( 200, 0, 0, 255);

	const float size = 500.0f;

	const BasicVertex vertices[8] = 
	{
		{ core::Vec3_Make( -size, -size, -size ), colorBack },
		{ core::Vec3_Make(  size, -size, -size ), colorBack },
		{ core::Vec3_Make( -size,  size, -size ), colorBack },
		{ core::Vec3_Make(  size,  size, -size ), colorBack },
		{ core::Vec3_Make( -size, -size,  size ), colorFront },
		{ core::Vec3_Make(  size, -size,  size ), colorFront },
		{ core::Vec3_Make( -size,  size,  size ), colorFront },
		{ core::Vec3_Make(  size,  size,  size ), colorFront },
	};

	const core::u16 indices[24] = { 
		0, 1, 1, 3, 3, 2, 2, 0,
		4, 5, 5, 7, 7, 6, 6, 4,
		1, 5, 0, 4, 3, 7, 2, 6 };

	RenderableCreate( device, renderable, vertices, 8, indices, 24, D3D11_PRIMITIVE_TOPOLOGY_LINELIST );
}

class CRenderingDevice
{
public:
	CRenderingDevice();
	~CRenderingDevice();

public:
	bool Create(int nWidth, int nHeight, HWND hWnd, core::CFileSystem* fileSystem, core::IStack* stack );
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

	ID3D11VertexShader* m_vertexShader;
	ID3D11PixelShader* m_pixelShader;

	ID3D11InputLayout* m_inputLayout;

	Renderable m_renderables[RENDERABLE_MAX_COUNT];
	uint m_renderableCount;

	ID3D11Buffer* m_cbuffer;

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

bool CRenderingDevice::Create( int nWidth, int nHeight, HWND hWnd, core::CFileSystem* fileSystem, core::IStack* stack )
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
	
	void* vsBytecode = nullptr;
	const int vsBytecodeSize = fileSystem->ReadFile( "basic_vs.cso", &vsBytecode, stack );
	if ( vsBytecodeSize <= 0 )
	{
		return false;
	}	

	{
		HRESULT hRes = m_device->CreateVertexShader( vsBytecode, vsBytecodeSize, nullptr, &m_vertexShader );

		if ( FAILED( hRes) )
		{
			V6_ERROR( "ID3D11Device::CreateVertexShader failed!" );
		}
	}

	void* psBytecode = nullptr;
	const int psBytecodeSize = fileSystem->ReadFile( "basic_ps.cso", &psBytecode, stack );
	if ( psBytecodeSize <= 0 )
	{
		return false;
	}

	{
		HRESULT hRes = m_device->CreatePixelShader( psBytecode, psBytecodeSize, nullptr, &m_pixelShader );

		if ( FAILED( hRes) )
		{
			V6_ERROR( "ID3D11Device::CreateVertexShader failed!" );
		}
	}
	
	{
		D3D11_INPUT_ELEMENT_DESC idesc[2] = {};
		
		idesc[0].SemanticName = "POSITION";
		idesc[0].SemanticIndex = 0;
		idesc[0].Format = DXGI_FORMAT_R32G32B32_FLOAT;
		idesc[0].InputSlot = 0;
		idesc[0].AlignedByteOffset = 0;
		idesc[0].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
		idesc[0].InstanceDataStepRate = 0;
		
		idesc[1].SemanticName = "COLOR";
		idesc[1].SemanticIndex = 0;
		idesc[1].Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		idesc[1].InputSlot = 0;
		idesc[1].AlignedByteOffset = 12;
		idesc[1].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
		idesc[1].InstanceDataStepRate = 0;

		HRESULT hRes = m_device->CreateInputLayout( idesc, 2, vsBytecode, vsBytecodeSize, &m_inputLayout );

		if ( FAILED( hRes) )
		{
			V6_ERROR( "ID3D11Device::CreateInputLayout failed!" );
			return false;
		}
	}

	RenderableCreateTriangle( m_device, &m_renderables[m_renderableCount++] );
	RenderableCreateBox( m_device, &m_renderables[m_renderableCount++] );

	{
		D3D11_BUFFER_DESC bufDesc = {};	
		bufDesc.Usage = D3D11_USAGE_DYNAMIC;
		bufDesc.ByteWidth = sizeof( v6::hlsl::CBBasicView );
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

	m_aspectRatio = (float)nWidth / nHeight;
	m_projMatrix = core::Mat4x4_Projection( 1.0f, 1000.0f, core::DegToRad( 70.0f ), m_aspectRatio );
	
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
	
	int mouseDeltaX = 0;
	int mouseDeltaY = 0;

	if ( g_mouseDown )
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

	static float yaw = 0.0f;
	static float pitch = 0.0f;
	const static float MOUSE_ROTATION_SPEED = 0.5f;
	
	yaw += -mouseDeltaX * MOUSE_ROTATION_SPEED * dt;
	pitch += -mouseDeltaY * MOUSE_ROTATION_SPEED * dt;

	const core::Mat4x4 yawMatrix = core::Mat4x4_RotationY( yaw );
	const core::Mat4x4 pitchMatrix = core::Mat4x4_RotationX( pitch );
	core::Mat4x4 viewMatrix;
	core::Mat4x4_Mul( &viewMatrix, yawMatrix, pitchMatrix );
	core::Mat4x4_Transpose( &viewMatrix );
	
	{
		m_ctx->IASetInputLayout( m_inputLayout );
		m_ctx->VSSetShader( m_vertexShader, nullptr, 0 );
		m_ctx->PSSetShader( m_pixelShader, nullptr, 0 );
	}

	{
		D3D11_MAPPED_SUBRESOURCE res;
		V6_ASSERT_D3D11( m_ctx->Map( m_cbuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &res ) );

		v6::hlsl::CBBasicView* basicView = (v6::hlsl::CBBasicView*)res.pData;

		basicView->worldToView = viewMatrix;
		basicView->viewToProj = m_projMatrix;

		m_ctx->Unmap( m_cbuffer, 0 );

		m_ctx->VSSetConstantBuffers( v6::hlsl::CBBasicViewSlot, 1, &m_cbuffer );
	}

	for ( uint renderableID = 0; renderableID < m_renderableCount; ++renderableID )
	{
		Renderable* renderable = &m_renderables[renderableID];
		{
			const uint stride = sizeof( BasicVertex ); 
			const uint offset = 0;
    
			m_ctx->IASetVertexBuffers( 0, 1, &renderable->m_vertexBuffer, &stride, &offset );
			m_ctx->IASetIndexBuffer( renderable->m_indexBuffer, DXGI_FORMAT_R16_UINT, 0 );
			m_ctx->IASetPrimitiveTopology( renderable->m_topology );
		}

		m_ctx->DrawIndexed( renderable->m_indexCount, 0, 0 );
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

	for ( uint renderableID = 0; renderableID < m_renderableCount; ++renderableID )
	{
		Renderable* renderable = &m_renderables[renderableID];
		renderable->m_vertexBuffer->Release();
		renderable->m_indexBuffer->Release();
	}

	m_inputLayout->Release();

	m_pixelShader->Release();
	m_vertexShader->Release();
	
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
	v6::core::Stack stack( &heap );
	v6::core::CFileSystem filesystem;

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
	if ( !oRenderingDevice.Create( nWidth, nHeight, hWnd, &filesystem, &stack ) )
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