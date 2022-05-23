
#include "core/debug.h"
#include "hook/hook.h"
#include "dxsdk/d3dx9.h"
#include "reshade-d3d9/d3d9_runtime.hpp"
#include "reshade-d3d9/d3d9_device.hpp"
#include "reshade-d3d9/d3d9_swapchain.hpp"
#include "meter/resource.h"
#include "meter/logging.h"
#include "meter/d3dx9_hooks.h"


extern "C" HANDLE g_hInstance;
static LPDIRECT3DDEVICE9  g_pD3D = NULL;
static D3DVIEWPORT9 g_viewPort = { 0 };
static LPD3DXEFFECT g_shader = NULL;

static Direct3DDevice9* g_deviceProxy = NULL;
static Direct3DSwapChain9* g_swapChainProxy = NULL;

#define SAFE_RELEASE(p) { if (p) { (p)->Release(); (p)=NULL; } }
#define CHECKD3D(s) { HRESULT hr = (s); if (FAILED(hr)) { DBGPRINT(L"%S failed, error 0x%08x", #s, hr); return; }}
#define CHECKD3DERR(s, err) { HRESULT hr = (s); if (FAILED(hr)) { DBGPRINT(L"%S failed, error 0x%08x: %S", #s, hr, (char*)err->GetBufferPointer()); return; }}

void d3dx9_load_shaders()
{
	if (!g_pD3D) return;

	ID3DXBuffer *errors = NULL;
	CHECKD3DERR(D3DXCreateEffectFromResource(g_pD3D, (HMODULE)g_hInstance, MAKEINTRESOURCE(IDR_RT_SHADER_DALTONIZE),
		NULL, NULL, D3DXSHADER_OPTIMIZATION_LEVEL3 | D3DXSHADER_PREFER_FLOW_CONTROL, NULL, &g_shader, &errors), errors);
}

void d3dx9_create(LPDIRECT3DDEVICE9 pD3D, D3DVIEWPORT9* pViewPort, HWND hWnd,
	bool use_software_rendering,
	LPDIRECT3DDEVICE9EX* ppReturnedDeviceInterface,
	LPDIRECT3DSWAPCHAIN9EX* ppReturnedSwapChainInterface)
{
	g_pD3D = pD3D;
	if (!g_pD3D) return;
	if (pViewPort) g_viewPort = *pViewPort;

	IDirect3DSwapChain9 *swapchain = nullptr;
	g_pD3D->GetSwapChain(0, &swapchain);

	assert(swapchain != nullptr);

	D3DPRESENT_PARAMETERS pp;
	swapchain->GetPresentParameters(&pp);

	const auto runtime = std::make_shared<d3d9::d3d9_runtime>(g_pD3D, swapchain);
	if (!runtime->on_init(pp))
	{
		LOG_ERR("Failed to initialize Direct3D 9 runtime environment on runtime %p.", runtime);
	}

	g_deviceProxy = new Direct3DDevice9(g_pD3D);
	g_swapChainProxy = new Direct3DSwapChain9(g_deviceProxy, swapchain, runtime);

	g_deviceProxy->_implicit_swapchain = g_swapChainProxy;
	g_deviceProxy->_use_software_rendering = use_software_rendering;

	*ppReturnedDeviceInterface = g_deviceProxy;
	*ppReturnedSwapChainInterface = g_swapChainProxy;

	if (pp.EnableAutoDepthStencil)
	{
		g_pD3D->GetDepthStencilSurface(&g_deviceProxy->_auto_depthstencil);
		g_deviceProxy->SetDepthStencilSurface(g_deviceProxy->_auto_depthstencil);
	}

	// Upgrade to extended interface if available
	com_ptr<IDirect3DDevice9Ex> deviceex;
	g_deviceProxy->QueryInterface(IID_PPV_ARGS(&deviceex));
}

void d3dx9_destroy(void)
{
	if (g_swapChainProxy) {
		delete g_swapChainProxy;
		g_swapChainProxy = NULL;
	}
	if (g_deviceProxy) {
		delete g_deviceProxy;
		g_deviceProxy = NULL;
	}
	SAFE_RELEASE(g_shader);
}
