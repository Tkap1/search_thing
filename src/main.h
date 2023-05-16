
enum e_search_type
{
	e_search_type_file_name,
	e_search_type_file_path,
	e_search_type_file_content,
	e_search_type_count,
};

struct s_key
{
	b8 is_down;
	int count;
};


struct s_file_info
{
	s_str_sbuilder<MAX_PATH> full_path;
	s_str_sbuilder<MAX_PATH> name;
};


struct s_file_with_score
{
	int score;
	s_file_info info;

	bool operator>(const s_file_with_score& right)
	{
		return this->score > right.score;
	}

};

struct s_stored_input
{
	b8 is_down;
	int key;
};

struct s_input
{
	s_key keys[c_max_keys];
};

struct s_glyph
{
	int advance_width;
	int width;
	int height;
	int x0;
	int y0;
	int x1;
	int y1;
	s_v2 uv_min;
	s_v2 uv_max;
};

struct s_texture
{
	u32 id;
	s_v2 size;
	s_v2 sub_size;
};

struct s_font
{
	float size;
	float scale;
	int ascent;
	int descent;
	int line_gap;
	s_texture texture;
	s_carray<s_glyph, 1024> glyph_arr;
};

struct s_char_event
{
	b8 is_symbol;
	int c;
};

#ifdef m_debug
func f64 get_ms();
#define m_timed_block(name_) s_auto_timer timer___ = zero; timer___.name = name_; timer___.start = get_ms()

struct s_auto_timer
{
	f64 start;
	char* name;

	~s_auto_timer()
	{
		printf("%s: %f ms\n", name, get_ms() - start);
	}
};
#else
#define m_timed_block(name_)
#endif

LRESULT window_proc(HWND window, UINT msg, WPARAM wparam, LPARAM lparam);

func PROC load_gl_func(char* name);
void gl_debug_callback(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* message, const void* userParam);
func void apply_event_to_input(s_input* in_input, s_stored_input event);
func b8 check_for_shader_errors(u32 id, char* out_error);
func s_v2 get_text_size(char* text, e_font font_id);
func s_font load_font(char* path, float font_size, s_lin_arena* arena);
func s_texture load_texture_from_data(void* data, int width, int height, u32 filtering);
func b8 is_key_down(int key);
func b8 is_key_up(int key);
func b8 is_key_pressed(int key);
func b8 is_key_released(int key);
func s_texture load_texture_from_file(char* path, u32 filtering);
func s_char_event get_char_event();
func s_v2 get_text_size_with_count(char* text, e_font font_id, int count);
func int string_similarity(char* a, char* b);
func void draw_search_bar(s_input_str<MAX_PATH - 1> search, s_v2 pos, s_v2 size, e_font font_type, b8 do_cursor);
DWORD system_call(void* param);
func void show_window();
func void hide_window();
func u64 get_cycles();


template <typename T>
struct s_dynamic_array
{
	int count;
	int capacity;
	T* elements;

	void add(T new_element)
	{
		if(capacity == 0)
		{
			capacity = 64;
			elements = (T*)malloc(sizeof(T) * capacity);
		}
		else if(count >= capacity)
		{
			capacity *= 2;
			elements = (T*)realloc(elements, sizeof(T) * capacity);
		}
		elements[count++] = new_element;
	}

	void free()
	{
		assert(capacity > 0);
		::free(elements);
		capacity = 0;
		count = 0;
		elements = null;
	}

	T& operator[](int index)
	{
		assert(index >= 0);
		assert(index < count);
		return elements[index];
	}
};


func void get_all_files_in_directory_(s_dynamic_array<s_file_info>* dynamic_arr, char* directory_path, s_lin_arena* arena)
{
	WIN32_FIND_DATAA find_data = zero;
	s_str_sbuilder<MAX_PATH> path_with_star;
	path_with_star.add("%s/*", directory_path);
	HANDLE first = FindFirstFileA(path_with_star.cstr(), &find_data);

	if(first == INVALID_HANDLE_VALUE)
	{
		return;
	}

	while(true)
	{
		b8 is_directory = (b8)(find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY);
		if(strcmp(find_data.cFileName, ".") == 0) { goto next; }
		if(strcmp(find_data.cFileName, "..") == 0) { goto next; }

		for(int folder_i = 0; folder_i < array_count(c_folder_blacklist); folder_i++)
		{
			if(strcmp(find_data.cFileName, c_folder_blacklist[folder_i]) == 0) { goto next; }
		}

		if(is_directory)
		{
			s_str_sbuilder<1024> builder;
			builder.add("%s/%s", directory_path, find_data.cFileName);
			get_all_files_in_directory_(dynamic_arr, builder.cstr(), arena);
		}
		else
		{
			// @Note(tkap, 14/05/2023): Let's skip files without an extension or files beginning with a .
			int dot_index = str_find_from_right(find_data.cFileName, ".");
			if(dot_index == -1 || dot_index == 0) { goto next; }
			char* extension = find_data.cFileName + dot_index;

			// @Note(tkap, 14/05/2023): Blacklist
			// for(int extension_i = 0; extension_i < array_count(c_extension_blacklist); extension_i++)
			// {
			// 	if(strcmp(extension, c_extension_blacklist[extension_i]) == 0) { goto next; }
			// }

			// @Note(tkap, 14/05/2023): Whitelist
			b8 found = false;
			for(int extension_i = 0; extension_i < array_count(c_extension_whitelist); extension_i++)
			{
				if(strcmp(extension, c_extension_whitelist[extension_i]) == 0) { found = true; break; }
			}
			if(!found) { goto next; }

			s_file_info info = zero;
			info.full_path.add("%s/%s", directory_path, find_data.cFileName);
			info.name.add("%s", find_data.cFileName);
			dynamic_arr->add(info);
		}

		next:
		if(FindNextFileA(first, &find_data) == 0)
		{
			break;
		}
	}

	FindClose(first);
}

func s_dynamic_array<s_file_info> get_all_files_in_directory(char* directory_path, s_lin_arena* arena)
{
	s_dynamic_array<s_file_info> result = zero;
	get_all_files_in_directory_(&result, directory_path, arena);
	return result;
}

int qsort_files(const void* a, const void* b)
{
	s_file_with_score* aa = (s_file_with_score*)a;
	s_file_with_score* bb = (s_file_with_score*)b;
	if(aa->score > bb->score) { return -1; }
	if(aa->score < bb->score) { return 1; }
	return 0;
}
