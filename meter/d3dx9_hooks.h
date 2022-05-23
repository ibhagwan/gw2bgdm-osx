#pragma once
#include "loader/d3d9/d3d9_context.h"

#ifdef __cplusplus
extern "C" {
#endif

bool d3dx9_create(LPDIRECT3DDEVICE9 pD3D, D3DVIEWPORT9* pViewPort, HWND hWnd);
bool d3dx9_create_ex(PD3D9Context pD3Dcontext);
void d3dx9_destroy(void);

void d3dx9_unload();
void d3dx9_reset();

int d3dx9_effect_count();
bool d3dx9_effect_get(int idx, const char **p_technique, const char **p_texture, uint8_t** p_bytes, int *p_bytes_len);
bool d3dx9_effect_get_param(int eff_idx, int param_idx, const char** p_name, uint8_t** p_bytes, int *p_bytes_len);
bool d3dx9_effect_enabled(int idx);

#ifdef __cplusplus
}
#endif