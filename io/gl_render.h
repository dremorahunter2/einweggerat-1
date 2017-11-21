#ifndef _gl_render_h_
#define _gl_render_h_
#include <d3d9.h>
#include "glad.h"
void video_deinit();
bool video_set_pixel_format(unsigned format);
void video_refresh(const void *data, unsigned width, unsigned height, unsigned pitch);
void video_configure(const struct retro_game_geometry *geom, HWND hwnd);

typedef struct{
	GLuint tex_id;
	GLuint fbo_id;
	GLuint rbo_id;

	int glmajor;
	int glminor;

	GLuint blit_tex;
	GLuint blit_fbo;

	GLuint pitch;
	GLuint tex_w, tex_h;
	GLuint clip_w, clip_h;

	float ratio;

	GLuint pixfmt;
	GLuint pixtype;
	GLuint bpp;

	HDC   hDC;
	HGLRC hRC;
	HWND gl_hwnd;
	bool alloc_framebuf;
	HWND window_hwnd;
	struct retro_hw_render_callback hw;


	HWND D3D_hwnd;

	IDirect3DDevice9Ex* D3D_device;
	IDirect3DSurface9* D3D_backbuf;
	IDirect3DSurface9* D3D_GLtarget;

	HANDLE D3D_sharehandle;
	HANDLE D3D_sharetexture;
	HANDLE GL_htexture;

}video;
extern video g_video;

#endif