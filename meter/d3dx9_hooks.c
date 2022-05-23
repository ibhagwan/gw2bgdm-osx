
#include "core/types.h"
#include "core/debug.h"
#include "hook/hook.h"
#pragma warning (push)
#pragma warning (disable: 4201)
#include "dxsdk\d3dx9.h"
#pragma warning (pop)
#include "meter/resource.h"
#include "meter/logging.h"
#include "meter/d3dx9_hooks.h"
#include "loader/d3d9/d3d9_context.h"


extern HANDLE g_hInstance;
static LPDIRECT3DDEVICE9  g_pD3D = NULL;
static ID3DXEffect *m_pShader;

bool GetEmbeddedResource(int ResID, LPVOID *ppData, LPDWORD pBytes);

/*#define CHECKD3D(s) { HRESULT hr = (s); if (FAILED(hr)) { DBGPRINT(L"%S failed, error 0x%08x", #s, hr); return false; }}
#define CHECKD3DERR(s, err) { HRESULT hr = (s); if (FAILED(hr)) { DBGPRINT(L"%S failed, error 0x%08x: %S", #s, hr, (char*)err->lpVtbl->GetBufferPointer(err)); return false; }}

typedef HRESULT(STDMETHODCALLTYPE* EndScene_t) (LPDIRECT3DDEVICE9);
typedef HRESULT(STDMETHODCALLTYPE* DrawPrimitive_t)(LPDIRECT3DDEVICE9, D3DPRIMITIVETYPE, UINT, UINT);
typedef HRESULT(STDMETHODCALLTYPE* DrawIndexedPrimitive_t)(LPDIRECT3DDEVICE9, D3DPRIMITIVETYPE, INT, UINT, UINT, UINT, UINT);
static EndScene_t _imp_EndScene = 0;
static DrawPrimitive_t _imp_DrawPrimitive = 0;
static DrawIndexedPrimitive_t _imp_DrawIndexedPrimitive = 0;
static uintptr_t g_hkEndScene = 0;
static uintptr_t g_hkDrawPrimitive = 0;
static uintptr_t g_hkDrawIndexedPrimitive = 0;

static HRESULT STDMETHODCALLTYPE hkEndScene(LPDIRECT3DDEVICE9 This)
{
	DBGPRINT(TEXT("IDirect3DDevice9::EndScene(%p) g_pD3D %p"), This, g_pD3D);

	D3DVIEWPORT9 vp;
	if (SUCCEEDED(This->lpVtbl->GetViewport(This, &vp)))
	{
		DBGPRINT(TEXT("w %d  h %d"), (int)vp.Width, (int)vp.Height);
	}

	HRESULT hr = S_OK;
	if (_imp_EndScene)
		hr = _imp_EndScene(This);

	MH_RemoveHook((LPVOID)g_hkEndScene);
	DBGPRINT(TEXT("Successfully unhooked IDirect3DDevice9::EndScene"));

	return hr;
}

static HRESULT STDMETHODCALLTYPE hkDrawPrimitive(LPDIRECT3DDEVICE9 This, D3DPRIMITIVETYPE PrimitiveType, UINT startVertex, UINT primCount)
{
	if (!_imp_DrawPrimitive) return D3D_OK;

	return _imp_DrawPrimitive(This, PrimitiveType, startVertex, primCount);
}

static HRESULT STDMETHODCALLTYPE hkDrawIndexedPrimitive(LPDIRECT3DDEVICE9 This, D3DPRIMITIVETYPE PrimitiveType, INT BaseVertexIndex, UINT MinVertexIndex, UINT NumVertices, UINT startIndex, UINT primCount)
{

	if (!_imp_DrawIndexedPrimitive) return D3D_OK;

	return _imp_DrawIndexedPrimitive(This, PrimitiveType, BaseVertexIndex, MinVertexIndex, NumVertices, startIndex, primCount);;
}

static bool d3dx9_load_shaders()
{
	if (!g_pD3D) return false;
	if (m_pShader) return true;

	ID3DXBuffer *errors = NULL;
	CHECKD3DERR(D3DXCreateEffectFromResource(g_pD3D, (HMODULE)g_hInstance, MAKEINTRESOURCE(IDR_RT_SHADER_DALTONIZE),
		NULL, NULL, D3DXSHADER_OPTIMIZATION_LEVEL3 | D3DXSHADER_PREFER_FLOW_CONTROL, NULL, &m_pShader, &errors), errors);

	return true;
}
*/

int d3dx9_effect_count()
{
	return 1;
}

bool d3dx9_effect_get(int idx, const char **p_technique, const char **p_texture, uint8_t** p_bytes, int *p_bytes_len)
{
	if (idx != 0) return false;

	static const char* technique = "PostProcess";
	static const char* texture = "g_txSrcColor";

	DWORD dwBytes = 0;
	LPVOID pSrcData = NULL;
	if (GetEmbeddedResource(IDR_RT_SHADER_DALTONIZE, &pSrcData, &dwBytes))
	{
		if (p_bytes) *p_bytes = pSrcData;
		if (p_bytes_len) *p_bytes_len = dwBytes;
		if (p_technique) *p_technique = technique;
		if (p_texture) *p_texture = texture;
		return true;
	}
	return false;
}

extern int imgui_colorblind_mode;

bool d3dx9_effect_get_param(int eff_idx, int param_idx, const char** p_name, uint8_t** p_bytes, int *p_bytes_len)
{
	if (eff_idx != 0 || param_idx != 0) return false;

	static const char* param = "g_Type";

	if (p_name) *p_name = param;
	if (p_bytes) *p_bytes = (uint8_t*)&imgui_colorblind_mode;
	if (p_bytes_len) *p_bytes_len = sizeof(imgui_colorblind_mode);

	return true;
}

bool d3dx9_effect_enabled(int idx)
{
	if (idx != 0) return false;
	if (imgui_colorblind_mode > -1 && imgui_colorblind_mode < 3) return true;
	return false;
}

void d3dx9_unload()
{
	if (!g_pD3D || !m_pShader) return;
}

void d3dx9_reset()
{
	if (!g_pD3D || !m_pShader) return;
}

bool d3dx9_create(LPDIRECT3DDEVICE9 pD3D, D3DVIEWPORT9* pViewPort, HWND hWnd)
{
	g_pD3D = pD3D;
	if (!g_pD3D) return false;

	/*if (!d3dx9_load_shaders()) return false;
	if (!m_pShader) return false;
	CHECKD3D(m_pShader->lpVtbl->SetTechnique(m_pShader, "SpriteBatch"));

	if (MH_CreateHook((LPVOID)pD3D->lpVtbl->DrawPrimitive, (LPVOID)&hkDrawPrimitive, (LPVOID*)&_imp_DrawPrimitive) == MH_OK)
	{
		if (MH_EnableHook((LPVOID)pD3D->lpVtbl->DrawIndexedPrimitive) == MH_OK)
		{
			DBGPRINT(TEXT("IDirect3DDevice9::DrawPrimitive hook created [0x%016llX]"), pD3D->lpVtbl->DrawPrimitive);
		}
		g_hkDrawPrimitive = (uintptr_t)pD3D->lpVtbl->DrawPrimitive;
	}
	else
	{
		DBGPRINT(TEXT("IDirect3DDevice9::DrawPrimitive hook [p_cbt_log_result=0x%016llX]"), pD3D->lpVtbl->DrawPrimitive);
	}


	if (MH_CreateHook((LPVOID)pD3D->lpVtbl->DrawIndexedPrimitive, (LPVOID)&hkDrawIndexedPrimitive, (LPVOID*)&_imp_DrawIndexedPrimitive) == MH_OK)
	{
		if (MH_EnableHook((LPVOID)pD3D->lpVtbl->DrawIndexedPrimitive) == MH_OK)
		{
			DBGPRINT(TEXT("IDirect3DDevice9::DrawIndexedPrimitive hook created [0x%016llX]"), pD3D->lpVtbl->DrawIndexedPrimitive);
		}
		g_hkDrawIndexedPrimitive = (uintptr_t)pD3D->lpVtbl->DrawIndexedPrimitive;
	}
	else
	{
		DBGPRINT(TEXT("IDirect3DDevice9::DrawIndexedPrimitive hook [p_cbt_log_result=0x%016llX]"), pD3D->lpVtbl->DrawIndexedPrimitive);
	}
	*/

	return true;
}

bool d3dx9_create_ex(PD3D9Context pD3Dcontext)
{
	return true;
}

void d3dx9_destroy(void)
{
	//if (g_hkEndScene) MH_RemoveHook((LPVOID)g_hkEndScene);
	//if (g_hkDrawPrimitive) MH_RemoveHook((LPVOID)g_hkDrawPrimitive);
	//if (g_hkDrawIndexedPrimitive) MH_RemoveHook((LPVOID)g_hkDrawIndexedPrimitive);
	d3dx9_unload();
}