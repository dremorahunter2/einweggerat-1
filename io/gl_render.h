#ifndef _gl_render_h_
#define _gl_render_h_

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


	GLuint pitch;
	GLint tex_w, tex_h;
	GLuint clip_w, clip_h;

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