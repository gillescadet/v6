/*V6*/

#include <v6/viewer/common.h>

#include <v6/core/algo.h>

#include <windows.h>
#include <d3d11.h>

#pragma comment(lib, "d3d11.lib")

BEGIN_V6_VIEWER_NAMESPACE

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
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
	default:
		return DefWindowProcA(hWnd, message, wParam, lParam);
		break;
	}

	return 0;
}

HWND CreateMainWindow(const char * sTitle, int nWidth, int nHeight)
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

	HWND hWnd = CreateWindowA(
		"v6",
		sTitle,
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, CW_USEDEFAULT,
		nWidth, nHeight,
		NULL,
		NULL,
		NULL,
		NULL
		);

	return hWnd;
}

class CRenderingDevice
{
public:
	bool Create(int nWidth, int nHeight, HWND hWnd);
	void ClearBuffers();
	void Present();
	void Release();

	IDXGISwapChain* m_pSwapChain;
	ID3D11Device* m_pDevice;
	D3D_FEATURE_LEVEL m_nFeatureLevel;
	ID3D11DeviceContext* m_pImmediateContext;
	ID3D11Texture2D* m_pColorBuffer;
	ID3D11Texture2D* m_pDepthStencilBuffer;
	ID3D11RenderTargetView* m_pColorView;
	ID3D11DepthStencilView* m_pDepthStencilView;
};

bool CRenderingDevice::Create(int nWidth, int nHeight, HWND hWnd)
{
	DXGI_SWAP_CHAIN_DESC oSwapChainDesc;

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
	m_pDevice = NULL;
	m_nFeatureLevel = (D3D_FEATURE_LEVEL)0;
	m_pImmediateContext = NULL;

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
			&m_pDevice,
			&m_nFeatureLevel,
			&m_pImmediateContext);

		if (FAILED(hRes))
		{
			V6_ERROR("D3D11CreateDeviceAndSwapChain failed!");
			return false;
		}
	}

	m_pColorBuffer = NULL;

	{
		HRESULT hRes = m_pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void **)&m_pColorBuffer);

		if (FAILED(hRes))
		{
			V6_ERROR("IDXGISwapChain::GetBuffer failed!");
			return false;
		}
	}

	m_pColorView = NULL;

	{
		HRESULT hRes = m_pDevice->CreateRenderTargetView(m_pColorBuffer, 0, &m_pColorView);

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
		HRESULT hRes = m_pDevice->CreateTexture2D(&oDepthStencilDesc, 0, &m_pDepthStencilBuffer);

		if (FAILED(hRes))
		{
			V6_ERROR("ID3D11Device::CreateTexture2D failed!");
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
		HRESULT hRes = m_pDevice->CreateDepthStencilView(m_pDepthStencilBuffer, &oDepthStencilViewDesc, &m_pDepthStencilView);

		if (FAILED(hRes))
		{
			V6_ERROR("ID3D11Device::CreateTexture2D failed!");
			return false;
		}
	}

	m_pImmediateContext->OMSetRenderTargets(1, &m_pColorView, m_pDepthStencilView);

	return true;
}

void CRenderingDevice::ClearBuffers()
{
	float const pRGBA[] = { 0.0f, 0.0f, 0.0f, 0.0f };
	m_pImmediateContext->ClearRenderTargetView(m_pColorView, pRGBA);
	m_pImmediateContext->ClearDepthStencilView(m_pDepthStencilView, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
}

void CRenderingDevice::Present()
{
	m_pSwapChain->Present(0, 0);
}

void CRenderingDevice::Release()
{
	m_pImmediateContext->ClearState();
	
	m_pDepthStencilView->Release();
	m_pDepthStencilBuffer->Release();

	m_pColorView->Release();
	m_pColorBuffer->Release();

	m_pSwapChain->Release();
	m_pImmediateContext->Release();
	m_pDevice->Release();	
}

END_V6_VIEWER_NAMESPACE

void debugInputs(const char * sDebug, int * pValues, int nCount)
{
	printf("%s [ ", sDebug);
	for (int i = 0; i < nCount; ++i)
	{
		printf("%d ", pValues[i]);
	}
	printf("]\n");
}

int main()
{
	V6_LOG("Viewer 0.0");
#if 0
	int const nWidth = 1280;
	int const nHeight = 720;

	HWND hWnd = v6::viewer::CreateMainWindow("V6 Player 0.0", 1280, 720);
	if (!hWnd)
	{
		V6_ERROR("Call to CreateWindow failed!");
		return -1;
	}

	v6::viewer::CRenderingDevice oRenderingDevice;
	if (!oRenderingDevice.Create(nWidth, nHeight, hWnd))
	{
		V6_ERROR("Call to CRenderingDevice::Create failed!");
		return -1;
	}

	ShowWindow(hWnd, SW_SHOWNORMAL);
	UpdateWindow(hWnd);

	for (;;)
	{
		MSG msg;
		if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessageA(&msg);

			if (msg.message == WM_QUIT)
			{
				break;
			}
		}
		else
		{
			oRenderingDevice.ClearBuffers();
			oRenderingDevice.Present();
		}
	}

	oRenderingDevice.Release();
#endif

	int pValues[] = { 2, 3, 3, 2, 4, 4, 5, 5, 6, 7, 7, 6, 0, 1, 0, 1 };
	int nCount = sizeof(pValues) / sizeof(int);
	debugInputs("unsorted", pValues, nCount);
	v6::core::InsertionSort(pValues, nCount);
	debugInputs("  sorted", pValues, nCount);

	return 0;
}