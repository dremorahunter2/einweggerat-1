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
	int last_h;
	int last_w;


	GLuint blit_tex;
	GLuint blit_fbo;

	GLuint pitch;
	GLint tex_w, tex_h;
	GLuint base_w, base_h;
	float aspect;

	GLuint pixfmt;
	GLuint pixtype;
	GLuint bpp;
	HDC   hDC;
	HGLRC hRC;
	HWND hwnd;
	struct retro_hw_render_callback hw;

}video;
extern video g_video;

#endif