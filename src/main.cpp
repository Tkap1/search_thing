#include "tklib.h"

#pragma warning(push, 0)
#include <gl/GL.h>
#include "external/glcorearb.h"
#include "external/wglext.h"

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ASSERT assert
#include "external/stb_image.h"

#define STB_TRUETYPE_IMPLEMENTATION
#define STBTT_assert assert
#include "external/stb_truetype.h"
#pragma warning(pop)

#include "config.h"
#define m_equals(...) = __VA_ARGS__
#include "shader_shared.h"
#include "math.h"
#include "hashtable.h"
#include "ui.h"
#include "draw.h"
#include "main.h"

#define m_gl_funcs \
X(PFNGLBUFFERDATAPROC, glBufferData) \
X(PFNGLBUFFERSUBDATAPROC, glBufferSubData) \
X(PFNGLGENVERTEXARRAYSPROC, glGenVertexArrays) \
X(PFNGLBINDVERTEXARRAYPROC, glBindVertexArray) \
X(PFNGLGENBUFFERSPROC, glGenBuffers) \
X(PFNGLBINDBUFFERPROC, glBindBuffer) \
X(PFNGLVERTEXATTRIBPOINTERPROC, glVertexAttribPointer) \
X(PFNGLENABLEVERTEXATTRIBARRAYPROC, glEnableVertexAttribArray) \
X(PFNGLCREATESHADERPROC, glCreateShader) \
X(PFNGLSHADERSOURCEPROC, glShaderSource) \
X(PFNGLCREATEPROGRAMPROC, glCreateProgram) \
X(PFNGLATTACHSHADERPROC, glAttachShader) \
X(PFNGLLINKPROGRAMPROC, glLinkProgram) \
X(PFNGLCOMPILESHADERPROC, glCompileShader) \
X(PFNGLVERTEXATTRIBDIVISORPROC, glVertexAttribDivisor) \
X(PFNGLDRAWARRAYSINSTANCEDPROC, glDrawArraysInstanced) \
X(PFNGLDEBUGMESSAGECALLBACKPROC, glDebugMessageCallback) \
X(PFNGLBINDBUFFERBASEPROC, glBindBufferBase) \
X(PFNGLUNIFORM2FVPROC, glUniform2fv) \
X(PFNGLGETUNIFORMLOCATIONPROC, glGetUniformLocation) \
X(PFNGLUSEPROGRAMPROC, glUseProgram) \
X(PFNGLGETSHADERIVPROC, glGetShaderiv) \
X(PFNGLGETSHADERINFOLOGPROC, glGetShaderInfoLog) \
X(PFNGLGENFRAMEBUFFERSPROC, glGenFramebuffers) \
X(PFNGLBINDFRAMEBUFFERPROC, glBindFramebuffer) \
X(PFNGLFRAMEBUFFERTEXTURE2DPROC, glFramebufferTexture2D) \
X(PFNGLCHECKFRAMEBUFFERSTATUSPROC, glCheckFramebufferStatus) \
X(PFNGLACTIVETEXTUREPROC, glActiveTexture) \
X(PFNWGLSWAPINTERVALEXTPROC, wglSwapIntervalEXT) \
X(PFNWGLGETSWAPINTERVALEXTPROC, wglGetSwapIntervalEXT)

#define X(type, name) global type name = null;
m_gl_funcs
#undef X

global int g_width = 0;
global int g_height = 0;
global s_v2 g_window_size = c_base_res;
global s_v2 g_window_center = g_window_size * 0.5f;
global s_sarray<s_transform, 8192> transforms;
global s_input g_input = zero;
global float total_time = 0;
global float delta = 0;
global s_v2 mouse = zero;
global s_carray<s_font, e_font_count> g_font_arr;
global s_carray<s_sarray<s_transform, 8192>, e_font_count> text_arr;

global s_str<1024> thread_str;

global s_sarray<s_str<600>, 4> messages;
global s_lin_arena g_arena = zero;
global s_lin_arena g_frame_arena = zero;

global s_ui ui;
global s_sarray<s_char_event, 64> char_event_arr;

global HWND g_window;
global b8 g_window_shown = false;

global u64 cycle_frequency;

#include "ui.cpp"
#include "draw.cpp"
#include "hashtable.cpp"



#ifdef m_debug
int main(int argc, char** argv)
#else
int APIENTRY WinMain(HINSTANCE instance, HINSTANCE hInstPrev, PSTR cmdline, int cmdshow)
#endif
{
	#ifdef m_debug
	unreferenced(argc);
	unreferenced(argv);
	HINSTANCE instance = GetModuleHandle(null);
	#else
	unreferenced(hInstPrev);
	unreferenced(cmdline);
	unreferenced(cmdshow);
	#endif

	{
		LARGE_INTEGER temp;
		QueryPerformanceFrequency(&temp);
		cycle_frequency = temp.QuadPart;
	}


	// @Note(tkap, 14/05/2023): Prevent multiple instances of this program running at the same time
	CreateEvent(NULL, FALSE, FALSE, "search_thing_event");
	if(GetLastError() == ERROR_ALREADY_EXISTS)
	{
		return 1;
	}

	char* class_name = "search_thing";
	g_arena.init(100 * c_mb);

	g_frame_arena.capacity = 25 * c_mb;
	g_frame_arena.memory = (u8*)g_arena.get(g_frame_arena.capacity);

	PFNWGLCREATECONTEXTATTRIBSARBPROC wglCreateContextAttribsARB = null;
	PFNWGLCHOOSEPIXELFORMATARBPROC wglChoosePixelFormatARB = null;

	// vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv		dummy start		vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv
	{
		WNDCLASSEX window_class = zero;
		window_class.cbSize = sizeof(window_class);
		window_class.style = CS_OWNDC;
		window_class.lpfnWndProc = DefWindowProc;
		window_class.lpszClassName = class_name;
		window_class.hInstance = instance;
		check(RegisterClassEx(&window_class));

		HWND window = CreateWindowEx(
			0,
			class_name,
			"TKCHAT",
			WS_OVERLAPPEDWINDOW,
			CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
			null,
			null,
			instance,
			null
		);
		assert(window != INVALID_HANDLE_VALUE);

		HDC dc = GetDC(window);

		PIXELFORMATDESCRIPTOR pfd = zero;
		pfd.nSize = sizeof(pfd);
		pfd.nVersion = 1;
		pfd.dwFlags = PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER | PFD_DRAW_TO_WINDOW;
		pfd.cColorBits = 24;
		pfd.cDepthBits = 24;
		pfd.iLayerType = PFD_MAIN_PLANE;
		int format = ChoosePixelFormat(dc, &pfd);
		check(DescribePixelFormat(dc, format, sizeof(pfd), &pfd));
		check(SetPixelFormat(dc, format, &pfd));

		HGLRC glrc = wglCreateContext(dc);
		check(wglMakeCurrent(dc, glrc));

		wglCreateContextAttribsARB = (PFNWGLCREATECONTEXTATTRIBSARBPROC)load_gl_func("wglCreateContextAttribsARB");
		wglChoosePixelFormatARB = (PFNWGLCHOOSEPIXELFORMATARBPROC)load_gl_func("wglChoosePixelFormatARB");

		check(wglMakeCurrent(null, null));
		check(wglDeleteContext(glrc));
		check(ReleaseDC(window, dc));
		check(DestroyWindow(window));
		check(UnregisterClass(class_name, instance));

	}
	// ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^		dummy end		^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

	HDC dc = null;
	// vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv		window start		vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv
	{
		WNDCLASSEX window_class = zero;
		window_class.cbSize = sizeof(window_class);
		window_class.style = CS_OWNDC | CS_HREDRAW | CS_VREDRAW;
		window_class.lpfnWndProc = window_proc;
		window_class.lpszClassName = class_name;
		window_class.hInstance = instance;
		window_class.hCursor = LoadCursor(null, IDC_ARROW);
		check(RegisterClassEx(&window_class));

		g_window = CreateWindowEx(
			WS_EX_LAYERED,
			// WS_EX_LAYERED | WS_EX_TOPMOST,
			// WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_TRANSPARENT,
			// 0,
			class_name,
			"Search",
			// WS_OVERLAPPEDWINDOW | WS_VISIBLE,
			// WS_POPUP | WS_VISIBLE,
			WS_POPUP,
			CW_USEDEFAULT, CW_USEDEFAULT, (int)c_base_res.x, (int)c_base_res.y,
			null,
			null,
			instance,
			null
		);
		assert(g_window != INVALID_HANDLE_VALUE);

		SetLayeredWindowAttributes(g_window, RGB(0, 0, 0), 0, LWA_COLORKEY);

		dc = GetDC(g_window);

		int pixel_attribs[] = {
			WGL_DRAW_TO_WINDOW_ARB, GL_TRUE,
			WGL_SUPPORT_OPENGL_ARB, GL_TRUE,
			WGL_DOUBLE_BUFFER_ARB, GL_TRUE,
			WGL_SWAP_METHOD_ARB, WGL_SWAP_COPY_ARB,
			WGL_PIXEL_TYPE_ARB, WGL_TYPE_RGBA_ARB,
			WGL_ACCELERATION_ARB, WGL_FULL_ACCELERATION_ARB,
			WGL_COLOR_BITS_ARB, 32,
			WGL_DEPTH_BITS_ARB, 24,
			WGL_STENCIL_BITS_ARB, 8,
			0
		};

		PIXELFORMATDESCRIPTOR pfd = zero;
		pfd.nSize = sizeof(pfd);
		int format;
		u32 num_formats;
		check(wglChoosePixelFormatARB(dc, pixel_attribs, null, 1, &format, &num_formats));
		check(DescribePixelFormat(dc, format, sizeof(pfd), &pfd));
		SetPixelFormat(dc, format, &pfd);

		int gl_attribs[] = {
			WGL_CONTEXT_MAJOR_VERSION_ARB, 4,
			WGL_CONTEXT_MINOR_VERSION_ARB, 3,
			WGL_CONTEXT_PROFILE_MASK_ARB, WGL_CONTEXT_CORE_PROFILE_BIT_ARB,
			WGL_CONTEXT_FLAGS_ARB, WGL_CONTEXT_DEBUG_BIT_ARB,
			0
		};
		HGLRC glrc = wglCreateContextAttribsARB(dc, null, gl_attribs);
		check(wglMakeCurrent(dc, glrc));

		#define X(type, name) name = (type)load_gl_func(#name);
		m_gl_funcs
		#undef X

		glDebugMessageCallback(gl_debug_callback, null);
		glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);

	}
	// ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^		window end		^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

	// stbi_set_flip_vertically_on_load(true);

	set_window_size_to_monitor_size(g_window);

	RegisterHotKey(g_window, 1, 0, key_f6);

	g_font_arr[e_font_small] = load_font("assets/consola.ttf", 24, &g_frame_arena);
	g_font_arr[e_font_medium] = load_font("assets/consola.ttf", 36, &g_frame_arena);
	g_font_arr[e_font_big] = load_font("assets/consola.ttf", 72, &g_frame_arena);

	u32 vao;
	u32 ssbo;
	u32 program;
	{
		u32 vertex = glCreateShader(GL_VERTEX_SHADER);
		u32 fragment = glCreateShader(GL_FRAGMENT_SHADER);
		char* header = "#version 430 core\n";
		char* vertex_src = read_file_quick("shaders/vertex.vertex", &g_frame_arena);
		char* fragment_src = read_file_quick("shaders/fragment.fragment", &g_frame_arena);
		char* vertex_src_arr[] = {header, read_file_quick("src/shader_shared.h", &g_frame_arena), vertex_src};
		char* fragment_src_arr[] = {header, read_file_quick("src/shader_shared.h", &g_frame_arena), fragment_src};
		glShaderSource(vertex, array_count(vertex_src_arr), vertex_src_arr, null);
		glShaderSource(fragment, array_count(fragment_src_arr), fragment_src_arr, null);
		glCompileShader(vertex);
		char buffer[1024] = zero;
		check_for_shader_errors(vertex, buffer);
		glCompileShader(fragment);
		check_for_shader_errors(fragment, buffer);
		program = glCreateProgram();
		glAttachShader(program, vertex);
		glAttachShader(program, fragment);
		glLinkProgram(program);
		glUseProgram(program);
	}

	glGenVertexArrays(1, &vao);
	glBindVertexArray(vao);

	glGenBuffers(1, &ssbo);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, ssbo);
	glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(transforms.elements), null, GL_DYNAMIC_DRAW);

	string_similarity("cfg.y", "config.py");
	string_similarity("cfg.y", "config.h");

	auto file_name_str = make_input_str<MAX_PATH - 1>();
	auto file_path_str = make_input_str<MAX_PATH - 1>();
	auto file_content_str = make_input_str<MAX_PATH - 1>();
	file_path_str.from_cstr("C:/Users/34687/Desktop/Dev");
	int selected = 0;

	auto files = get_all_files_in_directory(file_path_str.data, &g_arena);
	s_dynamic_array<s_file_with_score> sorted_files = zero;

	for(int file_i = 0; file_i < files.count; file_i++)
	{
		s_file_info info = files[file_i];
		s_file_with_score fws = zero;
		fws.info = info;
		sorted_files.add(fws);
	}

	e_font font_type = e_font_small;

	e_search_type search_type = e_search_type_file_name;

	MSG win_msg = zero;
	b8 running = true;

	// vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv		frame start start		vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv
	while(running)
	{
		// vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv		window messages start		vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv
		while(PeekMessageA(&win_msg, null, 0, 0, PM_REMOVE) != 0)
		// while(GetMessageA(&win_msg, null, 0, 0) != 0)
		{
			if(win_msg.message == WM_QUIT)
			{
				running = false;
				break;
			}
			else if(win_msg.message == WM_HOTKEY)
			{
				if(g_window_shown)
				{
					hide_window();
				}
				else
				{
					show_window();
				}
			}
			TranslateMessage(&win_msg);
			DispatchMessage(&win_msg);
		}
		// ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^		window messages end		^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

		if(!g_window_shown) { Sleep(10); };

		// vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv		app update start		vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv
		{

			e_string_input_result file_name_input_result = zero;
			e_string_input_result file_path_input_result = zero;
			e_string_input_result file_content_input_result = zero;

			if(is_key_pressed(key_tab))
			{
				if(is_key_down(key_left_shift))
				{
					search_type = (e_search_type)circular_index(search_type - 1, e_search_type_count);
				}
				else
				{
					search_type = (e_search_type)circular_index(search_type + 1, e_search_type_count);
				}
			}

			b8 file_name_search_changed = false;
			b8 file_path_search_changed = false;
			b8 file_content_search_changed = false;
			switch(search_type)
			{
				case e_search_type_file_name:
				{
					file_name_input_result = handle_string_input(&file_name_str, &file_name_search_changed);
				} break;

				case e_search_type_file_path:
				{
					file_path_input_result = handle_string_input(&file_path_str, &file_path_search_changed);
				} break;

				case e_search_type_file_content:
				{
					file_content_input_result = handle_string_input(&file_content_str, &file_content_search_changed);
				} break;
			}

			if(file_name_input_result == e_string_input_result_cancel)
			{
				hide_window();
			}

			if(file_path_search_changed)
			{
				m_timed_block("Get all files");
				files = get_all_files_in_directory(file_path_str.data, &g_arena);
				file_name_search_changed = true;
			}

			if(file_content_search_changed)
			{
				file_name_search_changed = true;
			}

			if(file_name_search_changed)
			{
				selected = 0;
				if(sorted_files.elements)
				{
					m_timed_block("Free sorted files");
					sorted_files.free();
				}
				if(files.elements)
				{
					{
						m_timed_block("Calculate file score");
						foreach_raw(file_i, file, files)
						{
							s_file_with_score fws = zero;
							fws.info = file;
							if(file_name_str.len > 0)
							{
								fws.score = string_similarity(file_name_str.data, file.name.cstr());
								if(fws.score <= 0) { continue; }
							}

							if(file_content_str.len > 0)
							{
								g_frame_arena.push();
								char* data = read_file_quick(file.full_path.data, &g_frame_arena);
								int data_len = (int)strlen(data);
								if(data)
								{
									int index = str_find_from_left_fast(data, data_len, file_content_str.data, file_content_str.len);
									if(index == -1)
									{
										g_frame_arena.pop();
										continue;
									}
								}
								g_frame_arena.pop();
							}

							sorted_files.add(fws);
						}
					}

					{
						m_timed_block("Sort files");
						qsort(sorted_files.elements, sorted_files.count, sizeof(*sorted_files.elements), qsort_files);
					}
				}
			}

			float font_size = g_font_arr[font_type].size;
			s_v2 size = g_window_size * v2(0.5f, 0.5f);
			s_v2 pos = g_window_center;
			draw_rect(pos, 0, size, rgb(0x6B4738));

			float panel_top = pos.y - size.y / 2 + font_size;
			float panel_bottom = pos.y + size.y / 2;

			s_v2 search_pos = pos - size / 2;

			draw_search_bar(file_name_str, search_pos, size, font_type, search_type == e_search_type_file_name);
			search_pos.y += font_size;
			draw_search_bar(file_path_str, search_pos, size, font_type, search_type == e_search_type_file_path);
			search_pos.y += font_size;
			draw_search_bar(file_content_str, search_pos, size, font_type, search_type == e_search_type_file_content);

			search_pos.x += 4;

			if(sorted_files.count)
			{
				if(is_key_pressed(key_down))
				{
					selected = circular_index(selected + 1, sorted_files.count);
				}
				if(is_key_pressed(key_up))
				{
					selected = circular_index(selected - 1, sorted_files.count);
				}
			}

			float bottom_of_selected = panel_top + selected * font_size + font_size * 3;
			float diff = bottom_of_selected - panel_bottom;
			int scroll = 0;
			if(diff > 0)
			{
				scroll = ceilfi(diff / font_size);
			}

			s_v2 scroll_v = v2(0.0f, scroll * font_size);

			draw_text(
				format_text("%i", sorted_files.count), pos - v2(0.0f, size.y / 2 + font_size), 2, rgb(0xFFFFFF), font_type, true
			);

			if(sorted_files.elements)
			{
				foreach_raw(fws_i, fws, sorted_files)
				{
					search_pos.y += font_size;
					s_v4 color = rgb(0xD56D3D);
					if(fws_i == selected)
					{
						color = brighter(rgb(0xF49D51), 1.2f);
						if(
							file_name_input_result == e_string_input_result_submit ||
							file_path_input_result == e_string_input_result_submit ||
							file_content_input_result == e_string_input_result_submit
						)
						{
							hide_window();
							s_thread thread;
							thread_str.from_cstr(format_text("\"\"%s\" \"%s\"\"", vscode_path, fws.info.full_path.cstr()));
							thread.init(system_call);
						}
					}
					if(search_pos.y - scroll_v.y < panel_top) { continue; }
					if(search_pos.y - scroll_v.y > panel_bottom) { continue; }

					draw_text(
						fws.info.name.cstr(), search_pos - scroll_v, 2, color, font_type, false,
						{.do_clip = true, .clip_pos = pos - size / 2 + v2(0.0f, font_size * 3), .clip_size = size - v2(0.0f, font_size * 3)}
					);
					s_v2 name_size = get_text_size(fws.info.name.cstr(), font_type);
					s_v2 path_pos = search_pos;
					path_pos.x += name_size.x + 20;
					draw_text(
						format_text("- %s", fws.info.full_path.cstr()), path_pos - scroll_v, 2, rgb(0xADB984), font_type, false,
						{.do_clip = true, .clip_pos = pos - size / 2 + v2(0.0f, font_size * 3), .clip_size = size - v2(0.0f, font_size * 3)}
					);
				}
			}
		}

		soft_reset_ui();
		// ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^		app update end		^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

		// vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv		render start		vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv
		{
			// glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
			glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
			glClearDepth(0.0f);
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
			glViewport(0, 0, g_width, g_height);
			glEnable(GL_DEPTH_TEST);
			glDepthFunc(GL_GREATER);

			int location = glGetUniformLocation(program, "window_size");
			glUniform2fv(location, 1, &g_window_size.x);

			if(transforms.count > 0)
			{
				glEnable(GL_BLEND);
				glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
				glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof(*transforms.elements) * transforms.count, transforms.elements);
				glDrawArraysInstanced(GL_TRIANGLES, 0, 6, transforms.count);
				transforms.count = 0;
			}

			for(int font_i = 0; font_i < e_font_count; font_i++)
			{
				if(text_arr[font_i].count > 0)
				{
					s_font* font = &g_font_arr[font_i];
					glBindTexture(GL_TEXTURE_2D, font->texture.id);
					glEnable(GL_BLEND);
					glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
					glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof(*text_arr[font_i].elements) * text_arr[font_i].count, text_arr[font_i].elements);
					glDrawArraysInstanced(GL_TRIANGLES, 0, 6, text_arr[font_i].count);
					text_arr[font_i].count = 0;
				}
			}
		}
		// ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^		render end		^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

		for(int k_i = 0; k_i < c_max_keys; k_i++)
		{
			g_input.keys[k_i].count = 0;
		}

		char_event_arr.count = 0;

		g_frame_arena.used = 0;

		SwapBuffers(dc);

		// @TODO(tkap, 24/04/2023): Real timing
		Sleep(10);
		delta = 0.01f;
		total_time += delta;
	}
	// ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^		frame start end		^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

	return 0;
}

LRESULT window_proc(HWND window, UINT msg, WPARAM wparam, LPARAM lparam)
{
	LRESULT result = 0;

	switch(msg)
	{

		case WM_CLOSE:
		case WM_DESTROY:
		{
			PostQuitMessage(0);
		} break;

		case WM_ACTIVATE:
		{
			DWORD status = LOWORD(wparam);
			if(status == WA_INACTIVE)
			{
				hide_window();
			}
		} break;

		case WM_CHAR:
		{
			char_event_arr.add({.is_symbol = false, .c = (int)wparam});
		} break;

		case WM_SIZE:
		{
			g_width = LOWORD(lparam);
			g_height = HIWORD(lparam);
			g_window_size = v2(g_width, g_height);
			g_window_center = g_window_size * 0.5f;
		} break;

		case WM_LBUTTONDOWN:
		{
			g_input.keys[left_button].is_down = true;
			g_input.keys[left_button].count += 1;
		} break;

		case WM_LBUTTONUP:
		{
			g_input.keys[left_button].is_down = false;
			g_input.keys[left_button].count += 1;
		} break;

		case WM_RBUTTONDOWN:
		{
			g_input.keys[right_button].is_down = true;
			g_input.keys[right_button].count += 1;
		} break;

		case WM_RBUTTONUP:
		{
			g_input.keys[right_button].is_down = false;
			g_input.keys[right_button].count += 1;
		} break;

		case WM_KEYDOWN:
		case WM_KEYUP:
		{
			int key = (int)remap_key_if_necessary(wparam, lparam);
			b8 is_down = !(bool)((HIWORD(lparam) >> 15) & 1);
			if(key < c_max_keys)
			{
				s_stored_input si = zero;
				si.key = key;
				si.is_down = is_down;
				apply_event_to_input(&g_input, si);
			}

			if(is_down)
			{
				char_event_arr.add({.is_symbol = true, .c = (int)wparam});
			}
		} break;

		default:
		{
			result = DefWindowProc(window, msg, wparam, lparam);
		}
	}

	return result;
}


func PROC load_gl_func(char* name)
{
	PROC result = wglGetProcAddress(name);
	if(!result)
	{
		printf("Failed to load %s\n", name);
		assert(false);
	}
	return result;
}

void gl_debug_callback(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* message, const void* userParam)
{
	unreferenced(userParam);
	unreferenced(length);
	unreferenced(id);
	unreferenced(type);
	unreferenced(source);
	if(severity >= GL_DEBUG_SEVERITY_HIGH)
	{
		printf("GL ERROR: %s\n", message);
		assert(false);
	}
}

func void apply_event_to_input(s_input* in_input, s_stored_input event)
{
	in_input->keys[event.key].is_down = event.is_down;
	in_input->keys[event.key].count += 1;
}

func b8 check_for_shader_errors(u32 id, char* out_error)
{
	int compile_success;
	char info_log[1024];
	glGetShaderiv(id, GL_COMPILE_STATUS, &compile_success);

	if(!compile_success)
	{
		glGetShaderInfoLog(id, 1024, null, info_log);
		printf("Failed to compile shader:\n%s", info_log);

		if(out_error)
		{
			strcpy(out_error, info_log);
		}

		return false;
	}
	return true;
}


func s_v2 get_text_size_with_count(char* text, e_font font_id, int count)
{
	assert(count >= 0);
	if(count <= 0) { return zero; }
	s_font* font = &g_font_arr[font_id];

	s_v2 size = zero;
	size.y = font->size;

	for(int char_i = 0; char_i < count; char_i++)
	{
		char c = text[char_i];
		s_glyph glyph = font->glyph_arr[c];
		if(char_i == count - 1 && c != ' ')
		{
			size.x += glyph.width;
		}
		else
		{
			size.x += glyph.advance_width * font->scale;
		}
	}

	return size;
}

func s_v2 get_text_size(char* text, e_font font_id)
{
	return get_text_size_with_count(text, font_id, (int)strlen(text));
}

func s_font load_font(char* path, float font_size, s_lin_arena* arena)
{
	s_font font = zero;
	font.size = font_size;

	u8* file_data = (u8*)read_file_quick(path, arena);
	assert(file_data);

	stbtt_fontinfo info = zero;
	stbtt_InitFont(&info, file_data, 0);

	stbtt_GetFontVMetrics(&info, &font.ascent, &font.descent, &font.line_gap);

	font.scale = stbtt_ScaleForPixelHeight(&info, font_size);
	constexpr int max_chars = 128;
	s_sarray<u8*, max_chars> bitmap_arr;
	constexpr int padding = 10;
	int total_width = padding;
	int total_height = 0;
	for(int char_i = 0; char_i < max_chars; char_i++)
	{
		s_glyph glyph = zero;
		u8* bitmap = stbtt_GetCodepointBitmap(&info, 0, font.scale, char_i, &glyph.width, &glyph.height, 0, 0);
		stbtt_GetCodepointBox(&info, char_i, &glyph.x0, &glyph.y0, &glyph.x1, &glyph.y1);
		stbtt_GetGlyphHMetrics(&info, char_i, &glyph.advance_width, null);

		total_width += glyph.width + padding;
		total_height = max(glyph.height + padding * 2, total_height);

		font.glyph_arr[char_i] = glyph;
		bitmap_arr.add(bitmap);
	}

	// @Fixme(tkap, 21/04/2023): Use arena
	u8* gl_bitmap = (u8*)calloc(1, sizeof(u8) * 4 * total_width * total_height);

	int current_x = padding;
	for(int char_i = 0; char_i < max_chars; char_i++)
	{
		s_glyph* glyph = &font.glyph_arr[char_i];
		u8* bitmap = bitmap_arr[char_i];
		for(int y = 0; y < glyph->height; y++)
		{
			for(int x = 0; x < glyph->width; x++)
			{
				u8 src_pixel = bitmap[x + y * glyph->width];
				u8* dst_pixel = &gl_bitmap[((current_x + x) + (padding + y) * total_width) * 4];
				dst_pixel[0] = src_pixel;
				dst_pixel[1] = src_pixel;
				dst_pixel[2] = src_pixel;
				dst_pixel[3] = src_pixel;
			}
		}

		glyph->uv_min.x = current_x / (float)total_width;
		glyph->uv_max.x = (current_x + glyph->width) / (float)total_width;

		glyph->uv_min.y = padding / (float)total_height;
		glyph->uv_max.y = (padding + glyph->height) / (float)total_height;

		current_x += glyph->width + padding;
	}

	font.texture = load_texture_from_data(gl_bitmap, total_width, total_height, GL_LINEAR);

	foreach_raw(bitmap_i, bitmap, bitmap_arr)
	{
		stbtt_FreeBitmap(bitmap, null);
	}

	free(gl_bitmap);

	return font;
}

func s_texture load_texture_from_data(void* data, int width, int height, u32 filtering)
{
	assert(data);
	u32 id;
	glGenTextures(1, &id);
	glBindTexture(GL_TEXTURE_2D, id);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filtering);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filtering);

	s_texture texture = zero;
	texture.id = id;
	texture.size = v2(width, height);
	return texture;
}

func s_texture load_texture_from_file(char* path, u32 filtering)
{
	int width, height, num_channels;
	void* data = stbi_load(path, &width, &height, &num_channels, 4);
	s_texture texture = load_texture_from_data(data, width, height, filtering);
	stbi_image_free(data);
	return texture;
}

func b8 is_key_down(int key)
{
	assert(key < c_max_keys);
	return g_input.keys[key].is_down;
}

func b8 is_key_up(int key)
{
	assert(key < c_max_keys);
	return !g_input.keys[key].is_down;
}

func b8 is_key_pressed(int key)
{
	assert(key < c_max_keys);
	return (g_input.keys[key].is_down && g_input.keys[key].count == 1) || g_input.keys[key].count > 1;
}

func b8 is_key_released(int key)
{
	assert(key < c_max_keys);
	return (!g_input.keys[key].is_down && g_input.keys[key].count == 1) || g_input.keys[key].count > 1;
}

func s_char_event get_char_event()
{
	if(!char_event_arr.is_empty())
	{
		return char_event_arr.remove_and_shift(0);
	}
	return zero;
}

func int string_similarity(char* needle, char* haystack)
{
	int needle_len = (int)strlen(needle);
	int haystack_len = (int)strlen(haystack);

	if(needle_len <= 0) { return 0; }
	if(haystack_len <= 0) { return 0; }

	int score = 0;
	int sequence_bonus = 0;
	int needle_index = 0;
	int haystack_index = 0;
	int matches = 0;

	// @Note(tkap, 16/05/2023): Bonus if first character matches
	if(tolower(needle[0] == tolower(haystack[0]))) { score += 1; }

	while(needle_index < needle_len)
	{
		char needle_c = (char)tolower(needle[needle_index]);
		while(haystack_index < haystack_len)
		{
			char haystack_c = (char)tolower(haystack[haystack_index]);
			if(needle_c == haystack_c)
			{
				matches += 1;
				score += 2 + sequence_bonus;
				sequence_bonus += 7;
				needle_index += 1;
				haystack_index += 1;
				break;
			}
			else
			{
				haystack_index += 1;
				sequence_bonus = 0;
				score -= 1;
			}
		}
		if(haystack_index >= haystack_len) { break; }
	}

	assert(matches <= needle_len);

	if(needle_index >= needle_len && haystack_index < haystack_len)
	{
		score -= haystack_len - needle_len;
	}

	if(matches == needle_len) { score = at_least(1, score); }
	else if(matches != needle_len) { score = 0; }

	return score;
}

func void draw_search_bar(s_input_str<MAX_PATH - 1> search, s_v2 pos, s_v2 size, e_font font_type, b8 do_cursor)
{
	float font_size = g_font_arr[font_type].size;
	draw_rect(pos, 1, v2(size.x, font_size), darker(rgb(0x6B4738), 1.2f), {.origin_offset = c_origin_topleft});
	pos.x += 4;

	if(search.len > 0)
	{
		draw_text(search.data, pos, 2, rgb(0xCBA678), font_type, false);
	}

	// @Note(tkap, 14/05/2023): Draw cursor
	if(do_cursor)
	{
		if(search.cursor >= 0)
		{
			s_v2 text_size = get_text_size_with_count(search.data, font_type, search.cursor);
			s_v2 cursor_pos = v2(
				pos.x + text_size.x,
				pos.y
			);
			s_v2 cursor_size = v2(11.0f, g_font_arr[font_type].size);
			draw_rect(cursor_pos, 10, cursor_size, rgb(0xABC28F), {.origin_offset = c_origin_topleft});
		}
	}
}

DWORD WINAPI system_call(void* param)
{
	unreferenced(param);
	system(thread_str.data);
	return 0;
}

func void show_window()
{
	g_window_shown = true;

	// @Note(tkap, 14/05/2023): https://stackoverflow.com/a/34414846
	HWND hCurWnd = GetForegroundWindow();
	DWORD dwMyID = GetCurrentThreadId();
	DWORD dwCurID = GetWindowThreadProcessId(hCurWnd, NULL);
	AttachThreadInput(dwCurID, dwMyID, TRUE);
	SetWindowPos(g_window, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE);
	SetWindowPos(g_window, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_SHOWWINDOW | SWP_NOSIZE | SWP_NOMOVE);
	SetForegroundWindow(g_window);
	SetFocus(g_window);
	SetActiveWindow(g_window);
	AttachThreadInput(dwCurID, dwMyID, FALSE);
}

func void hide_window()
{
	g_window_shown = false;
	ShowWindow(g_window, SW_HIDE);
}

func u64 get_cycles()
{
	LARGE_INTEGER temp;
	QueryPerformanceCounter(&temp);
	return temp.QuadPart;
}

func f64 get_ms()
{
	return get_cycles() * 1000.0 / cycle_frequency;
}

// @Note(tkap, 16/05/2023): https://stackoverflow.com/a/15977613
func WPARAM remap_key_if_necessary(WPARAM vk, LPARAM lparam)
{
	WPARAM new_vk = vk;
	UINT scancode = (lparam & 0x00ff0000) >> 16;
	int extended  = (lparam & 0x01000000) != 0;

	switch(vk)
	{
		case VK_SHIFT:
		{
			new_vk = MapVirtualKey(scancode, MAPVK_VSC_TO_VK_EX);
		} break;

		case VK_CONTROL:
		{
			new_vk = extended ? VK_RCONTROL : VK_LCONTROL;
		} break;

		case VK_MENU:
		{
			new_vk = extended ? VK_RMENU : VK_LMENU;
		} break;

		default:
		{
			new_vk = vk;
		} break;
	}

	return new_vk;
}