#include <windows.h>
#include "../libretro.h"
#include "glad.h"
#include "gl_render.h"

video g_video;

static const PIXELFORMATDESCRIPTOR pfd =
{
	sizeof(PIXELFORMATDESCRIPTOR),
	1,
	PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER,
	PFD_TYPE_RGBA,
	32,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0,
	32,             // zbuffer
	0,              // stencil!
	0,
	PFD_MAIN_PLANE,
	0, 0, 0, 0
};

static struct {
	GLuint vao;
	GLuint vbo;
	GLuint program;
	GLint i_pos;
	GLint i_coord;
	GLint u_tex;
	GLint u_mvp;
} g_shader = { 0 };

static float g_scale = 2;
static bool g_win = false;

static const char *g_vshader_src =
"#version 330\n"
"in vec2 i_pos;\n"
"in vec2 i_coord;\n"
"out vec2 o_coord;\n"
"uniform mat4 u_mvp;\n"
"void main() {\n"
"o_coord = i_coord;\n"
"gl_Position = vec4(i_pos, 0.0, 1.0) * u_mvp;\n"
"}";

static const char *g_fshader_src =
"#version 330\n"
"in vec2 o_coord;\n"
"uniform sampler2D u_tex;\n"
"void main() {\n"
"gl_FragColor = texture2D(u_tex, o_coord);\n"
"}";




void ortho2d(float m[4][4], float left, float right, float bottom, float top) {
	m[0][0] = 1; m[0][1] = 0; m[0][2] = 0; m[0][3] = 0;
	m[1][0] = 0; m[1][1] = 1; m[1][2] = 0; m[1][3] = 0;
	m[2][0] = 0; m[2][1] = 0; m[2][2] = 1; m[2][3] = 0;
	m[3][0] = 0; m[3][1] = 0; m[3][2] = 0; m[3][3] = 1;

	m[0][0] = 2.0f / (right - left);
	m[1][1] = 2.0f / (top - bottom);
	m[2][2] = -1.0f;
	m[3][0] = -(right + left) / (right - left);
	m[3][1] = -(top + bottom) / (top - bottom);
}
GLuint compile_shader(unsigned type, unsigned count, const char **strings) {
	GLuint shader = glCreateShader(type);
	glShaderSource(shader, count, strings, NULL);
	glCompileShader(shader);

	GLint status;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &status);

	if (status == GL_FALSE) {
		char buffer[4096];
		glGetShaderInfoLog(shader, sizeof(buffer), NULL, buffer);
	}

	return shader;
}


void init_shaders() {
	GLuint vshader = compile_shader(GL_VERTEX_SHADER, 1, &g_vshader_src);
	GLuint fshader = compile_shader(GL_FRAGMENT_SHADER, 1, &g_fshader_src);
	GLuint program = glCreateProgram();

	glAttachShader(program, vshader);
	glAttachShader(program, fshader);
	glLinkProgram(program);

	glDeleteShader(vshader);
	glDeleteShader(fshader);

	glValidateProgram(program);

	GLint status;
	glGetProgramiv(program, GL_LINK_STATUS, &status);

	if (status == GL_FALSE) {
		char buffer[4096];
		glGetProgramInfoLog(program, sizeof(buffer), NULL, buffer);
	}

	g_shader.program = program;
	g_shader.i_pos = glGetAttribLocation(program, "i_pos");
	g_shader.i_coord = glGetAttribLocation(program, "i_coord");
	g_shader.u_tex = glGetUniformLocation(program, "u_tex");
	g_shader.u_mvp = glGetUniformLocation(program, "u_mvp");

	glGenVertexArrays(1, &g_shader.vao);
	glGenBuffers(1, &g_shader.vbo);

	glUseProgram(g_shader.program);

	glUniform1i(g_shader.u_tex, 0);

	float m[4][4];
	if (g_video.hw.bottom_left_origin)
		ortho2d(m, -1, 1, 1, -1);
	else
		ortho2d(m, -1, 1, -1, 1);

	glUniformMatrix4fv(g_shader.u_mvp, 1, GL_FALSE, (float*)m);

	glUseProgram(0);
}


void refresh_vertex_data() {

	float bottom = (float)g_video.clip_h / g_video.tex_h;
	float right = (float)g_video.clip_w / g_video.tex_w;

	float vertex_data[] = {
		// pos, coord
		-1.0f, -1.0f, 0.0f, bottom, // left-bottom
		-1.0f, 1.0f, 0.0f, 0.0f,   // left-top
		1.0f, -1.0f, right, bottom,// right-bottom
		1.0f, 1.0f, right, 0.0f,  // right-top
	};

	glBindVertexArray(g_shader.vao);

	glBindBuffer(GL_ARRAY_BUFFER, g_shader.vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertex_data), vertex_data, GL_STREAM_DRAW);

	glEnableVertexAttribArray(g_shader.i_pos);
	glEnableVertexAttribArray(g_shader.i_coord);
	glVertexAttribPointer(g_shader.i_pos, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 4, 0);
	glVertexAttribPointer(g_shader.i_coord, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 4, (void*)(2 * sizeof(float)));

	glBindVertexArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void init_framebuffer(int width, int height)
{
	if (g_video.fbo_id)
		glDeleteFramebuffers(1, &g_video.fbo_id);
	if (g_video.rbo_id)
		glDeleteRenderbuffers(1, &g_video.rbo_id);


	glGenFramebuffers(1, &g_video.fbo_id);
	glBindFramebuffer(GL_FRAMEBUFFER, g_video.fbo_id);

	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, g_video.tex_id, 0);

	if (g_video.hw.depth && g_video.hw.stencil) {
		glGenRenderbuffers(1, &g_video.rbo_id);
		glBindRenderbuffer(GL_RENDERBUFFER, g_video.rbo_id);
		glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);

		glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, g_video.rbo_id);
	}
	else if (g_video.hw.depth) {
		glGenRenderbuffers(1, &g_video.rbo_id);
		glBindRenderbuffer(GL_RENDERBUFFER, g_video.rbo_id);
		glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, width, height);

		glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, g_video.rbo_id);
	}

	if (g_video.hw.depth || g_video.hw.stencil)
		glBindRenderbuffer(GL_RENDERBUFFER, 0);

	glBindRenderbuffer(GL_RENDERBUFFER, 0);

	glCheckFramebufferStatus(GL_FRAMEBUFFER);

	glClearColor(0, 0, 0, 1);
	glClear(GL_COLOR_BUFFER_BIT);

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}


void resize_cb(int w, int h) {
	int32_t vp_width = w;
	int32_t vp_height = h;

	// default to bottom left corner of the window above the status bar
	int32_t vp_x = 0;
	int32_t vp_y = 0;

	int32_t hw = g_video.clip_h * vp_width;
	int32_t wh = g_video.clip_w * vp_height;

	// add letterboxes or pillarboxes if the window has a different aspect ratio
	// than the current display mode
	if (hw > wh) {
		int32_t w_max = wh / g_video.clip_h;
		vp_x += (vp_width - w_max) / 2;
		vp_width = w_max;
	}
	else if (hw < wh) {
		int32_t h_max = hw / g_video.clip_w;
		vp_y += (vp_height - h_max) / 2;
		vp_height = h_max;
	}

	// configure viewport
	glViewport(vp_x, vp_y, vp_width, vp_height);
}

void create_window(int width, int height, HWND hwnd) {

	g_video.hwnd = hwnd;
	g_video.hDC = GetDC(hwnd);

	unsigned int	PixelFormat;
	PixelFormat = ChoosePixelFormat(g_video.hDC, &pfd);
	SetPixelFormat(g_video.hDC, PixelFormat, &pfd);
	gladLoadGL();
	if (g_video.hw.context_type == RETRO_HW_CONTEXT_OPENGL_CORE)
	{
		GLint attribs_core[] =
		{
			// Here we ask for OpenGL 4.5
			0x2091, g_video.hw.version_major,
			0x2092, g_video.hw.version_minor,
			// Uncomment this for forward compatibility mode
			//0x2094, 0x0002,
			// Uncomment this for Compatibility profile
			// 0x9126, 0x9126,
			// We are using Core profile here
			0x9126, 0x00000001,
			0
		};
		typedef HGLRC(APIENTRY * PFNWGLCREATECONTEXTATTRIBSARBPROC)(HDC hDC, HGLRC hShareContext, const int *attribList);
		static PFNWGLCREATECONTEXTATTRIBSARBPROC pfnCreateContextAttribsARB = 0;
		HGLRC hTempContext;
		if (!(hTempContext = wglCreateContext(g_video.hDC)))
			return;
		if (!wglMakeCurrent(g_video.hDC, hTempContext))
		{
			wglDeleteContext(hTempContext);
			return;
		}
		pfnCreateContextAttribsARB = reinterpret_cast<PFNWGLCREATECONTEXTATTRIBSARBPROC>(wglGetProcAddress("wglCreateContextAttribsARB"));
		if (!(g_video.hRC = pfnCreateContextAttribsARB(g_video.hDC, 0, attribs_core)))return;
		if (!wglMakeCurrent(g_video.hDC, g_video.hRC))return;
		wglDeleteContext(hTempContext);
	}
	else
	{
		if (!(g_video.hRC = wglCreateContext(g_video.hDC)))return;
		if (!wglMakeCurrent(g_video.hDC, g_video.hRC))return;
	}

	gladLoadGL();
	init_shaders();
	resize_cb(width, height);

	typedef bool (APIENTRY *PFNWGLSWAPINTERVALFARPROC)(int);
	PFNWGLSWAPINTERVALFARPROC wglSwapIntervalEXT = 0;
	wglSwapIntervalEXT =
		(PFNWGLSWAPINTERVALFARPROC)wglGetProcAddress("wglSwapIntervalEXT");
	if (wglSwapIntervalEXT)
		wglSwapIntervalEXT(1);

	if (g_video.hw.context_reset)g_video.hw.context_reset();
	g_win = true;
}


void resize_to_aspect(double ratio, int sw, int sh, int *dw, int *dh) {
	*dw = sw;
	*dh = sh;

	if (ratio <= 0)
		ratio = (double)sw / sh;

	if ((float)sw / sh < 1)
		*dw = *dh * ratio;
	else
		*dh = *dw / ratio;
}

BOOL CenterWindow(HWND hwndWindow)
{
	HWND hwndParent;
	RECT rectWindow, rectParent;

	// make the window relative to its parent
	if ((hwndParent = GetParent(hwndWindow)) != NULL)
	{
		GetWindowRect(hwndWindow, &rectWindow);
		GetWindowRect(hwndParent, &rectParent);

		int nWidth = rectWindow.right - rectWindow.left;
		int nHeight = rectWindow.bottom - rectWindow.top;

		int nX = ((rectParent.right - rectParent.left) - nWidth) / 2 + rectParent.left;
		int nY = ((rectParent.bottom - rectParent.top) - nHeight) / 2 + rectParent.top;

		int nScreenWidth = GetSystemMetrics(SM_CXSCREEN);
		int nScreenHeight = GetSystemMetrics(SM_CYSCREEN);

		// make sure that the dialog box never moves outside of the screen
		if (nX < 0) nX = 0;
		if (nY < 0) nY = 0;
		if (nX + nWidth > nScreenWidth) nX = nScreenWidth - nWidth;
		if (nY + nHeight > nScreenHeight) nY = nScreenHeight - nHeight;

		MoveWindow(hwndWindow, nX, nY, nWidth, nHeight, FALSE);

		return TRUE;
	}

	return FALSE;
}



void video_configure(const struct retro_game_geometry *geom, HWND hwnd) {
	int nwidth = 0, nheight = 0;

	resize_to_aspect(geom->aspect_ratio, geom->base_width * 1, geom->base_height * 1, &nwidth, &nheight);

	nwidth *= g_scale;
	nheight *= g_scale;

	if (!g_win)
		create_window(nwidth, nheight, hwnd);

	if (g_video.tex_id)
		glDeleteTextures(1, &g_video.tex_id);


	g_video.tex_id = 0;

	if (!g_video.pixfmt)
		g_video.pixfmt = GL_UNSIGNED_SHORT_5_5_5_1;

	int screenWidth = GetSystemMetrics(SM_CXSCREEN);
	int screenHeight = GetSystemMetrics(SM_CYSCREEN);
	CenterWindow(hwnd);

	glGenTextures(1, &g_video.tex_id);

	g_video.pitch = geom->base_width * g_video.bpp;

	glBindTexture(GL_TEXTURE_2D, g_video.tex_id);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, geom->max_width, geom->max_height, 0,
		g_video.pixtype, g_video.pixfmt, NULL);

	glBindTexture(GL_TEXTURE_2D, 0);

	init_framebuffer(geom->base_width, geom->base_height);

	g_video.tex_w = geom->max_width;
	g_video.tex_h = geom->max_height;
	g_video.clip_w = geom->base_width;
	g_video.clip_h = geom->base_height;

	refresh_vertex_data();

	if (g_video.hw.context_reset)g_video.hw.context_reset();
}


bool video_set_pixel_format(unsigned format) {
	switch (format) {
	case RETRO_PIXEL_FORMAT_0RGB1555:
		g_video.pixfmt = GL_UNSIGNED_SHORT_5_5_5_1;
		g_video.pixtype = GL_BGRA;
		g_video.bpp = sizeof(uint16_t);
		break;
	case RETRO_PIXEL_FORMAT_XRGB8888:
		g_video.pixfmt = GL_UNSIGNED_INT_8_8_8_8_REV;
		g_video.pixtype = GL_BGRA;
		g_video.bpp = sizeof(uint32_t);
		break;
	case RETRO_PIXEL_FORMAT_RGB565:
		g_video.pixfmt = GL_UNSIGNED_SHORT_5_6_5;
		g_video.pixtype = GL_RGB;
		g_video.bpp = sizeof(uint16_t);
		break;
	default:
		break;
	}

	return true;
}


void video_refresh(const void *data, unsigned width, unsigned height, unsigned pitch) {
	if (g_video.clip_w != width || g_video.clip_h != height)
	{
		g_video.clip_h = height;
		g_video.clip_w = width;

		refresh_vertex_data();
	}
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glBindTexture(GL_TEXTURE_2D, g_video.tex_id);

	if (pitch != g_video.pitch) {
		g_video.pitch = pitch;
		glPixelStorei(GL_UNPACK_ROW_LENGTH, g_video.pitch / g_video.bpp);
	}

	if (data && data != RETRO_HW_FRAME_BUFFER_VALID) {
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height,
			g_video.pixtype, g_video.pixfmt, data);
	}

	RECT clientRect;

	GetClientRect(g_video.hwnd, &clientRect);
	resize_cb(clientRect.right, clientRect.bottom);

	glClear(GL_COLOR_BUFFER_BIT);

	glUseProgram(g_shader.program);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, g_video.tex_id);


	glBindVertexArray(g_shader.vao);
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
	glBindVertexArray(0);

	glUseProgram(0);

	SwapBuffers(g_video.hDC);
}

static void video_deinit() {
	if (g_video.tex_id)
	{
		glDeleteTextures(1, &g_video.tex_id);
		g_video.tex_id = 0;

	}
	if (g_video.fbo_id)
	{
		glDeleteFramebuffers(1, &g_video.fbo_id);
		g_video.fbo_id = 0;
	}


	if (g_video.rbo_id)
	{
		glDeleteRenderbuffers(1, &g_video.rbo_id);
		g_video.rbo_id = 0;
	}

	glDisableVertexAttribArray(1);
	glDeleteBuffers(1, &g_shader.vbo);
	glDeleteVertexArrays(1, &g_shader.vbo);

	if (g_video.hRC)
	{
		wglMakeCurrent(0, 0);
		wglDeleteContext(g_video.hRC);
	}

	if (g_video.hDC) ReleaseDC(g_video.hwnd, g_video.hDC);

}