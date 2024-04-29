#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <timeapi.h>
#include <windowsx.h>
#include <fileapi.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#undef near
#undef far
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "d3d11.lib")
#endif // _WIN32

#define STB_IMAGE_IMPLEMENTATION
#pragma warning(push)
#pragma warning(disable : 4244)
// #include <stb_image.h>
#pragma warning(pop)

#include <ft2build.h>
#include FT_FREETYPE_H

#include "qed.h"

#include "platform.h"

#define WIDTH  1040
#define HEIGHT 860

static bool window_should_close;

IDXGISwapChain *d3d11_swap_chain = nullptr;
ID3D11Device *d3d11_device = nullptr;
ID3D11DeviceContext *d3d11_context = nullptr;
ID3D11RenderTargetView *d3d11_render_target = nullptr;
ID3D11DepthStencilView *d3d11_depth_stencil_view = nullptr;

Array<Vertex> vertices;
ID3D11Buffer *vertex_buffer;
int vertex_buffer_capacity;

Array<System_Event *> system_events;

Key_Stroke *active_key_stroke;
Text_Input *active_text_input;

View *active_view;
Key_Code keycode_lookup[256];

Key_Map *active_keymap;

struct Shader {
    char *name;
    ID3D11InputLayout *input_layout;
    ID3D11VertexShader *vertex_shader;
    ID3D11PixelShader *pixel_shader;
};

COMMAND(quit_qed) {
    window_should_close = true;
}

COMMAND(quit_selection) {
    active_view->mark_active = false;
}

COMMAND(newline) {
    buffer_insert(active_view->buffer, active_view->cursor.position, '\n');
    active_view->cursor.position++;
    active_view->cursor.col = 0;
    active_view->cursor.line++;
}

COMMAND(self_insert) {
    View *view = active_view;
    if (active_text_input) {
        if (view->mark_active) {
            String string = { active_text_input->text, 1 };
            int64 start = view->cursor.position < view->mark.position ? view->cursor.position : view->mark.position;
            int64 end = view->cursor.position < view->mark.position ? view->mark.position : view->cursor.position;
            buffer_replace_region(view->buffer, string, start, end);
            view->mark_active = false;
            view->cursor = get_cursor_from_position(view->buffer, start);
        } else {
            buffer_insert(view->buffer, view->cursor.position, active_text_input->text[0]);
            view->cursor.col++;
            view->cursor.position++;
        }
    } else {
        assert(false);
    }
}

COMMAND(backward_char) {
    View *view = active_view;
    if (view->cursor.position > 0) {
        Cursor cursor = get_cursor_from_position(view->buffer, view->cursor.position - 1);
        view->cursor = cursor;
    }
}

COMMAND(forward_char) {
    View *view = active_view;
    if (view->cursor.position < get_buffer_length(view->buffer)) {
        Cursor cursor = get_cursor_from_position(view->buffer, view->cursor.position + 1);
        view->cursor = cursor;
    }
}

COMMAND(forward_word) {
    View *view = active_view;
    int64 buffer_length = get_buffer_length(view->buffer);
    char first = buffer_at(view->buffer, view->cursor.position);
    int64 position = view->cursor.position;
    // eat whitespace
    if (isspace(first)) {
        for (; position < buffer_length; position++) {
            char c = buffer_at(view->buffer, position);
            if (!isspace(c)) {
                break;
            }
        }
    }
    for (; position < buffer_length; position++) {
        char c = buffer_at(view->buffer, position);
        if (isspace(c)) {
            break;
        }
    }
    view->cursor = get_cursor_from_position(view->buffer, position);
}

COMMAND(backward_word) {
    View *view = active_view;
    int64 buffer_length = get_buffer_length(view->buffer);
    char first = buffer_at(view->buffer, view->cursor.position);
    int64 position = view->cursor.position;
    // eat whitespace
    if (isspace(first)) {
        for (; position >= 0; position--) {
            char c = buffer_at(view->buffer, position);
            if (!isspace(c)) {
                break;
            }
        }
    }
    for (position = position - 1; position >= 0; position--) {
        char c = buffer_at(view->buffer, position);
        if (isspace(c)) {
            break;
        }
    }
    view->cursor = get_cursor_from_position(view->buffer, position);
}

COMMAND(backward_paragraph) {
    View *view = active_view;
    for (int64 line = view->cursor.line - 1; line > 0; line--) {
        int64 start = view->buffer->line_starts[line - 1];
        int64 end = view->buffer->line_starts[line];
        bool blank_line = true;
        for (int64 position = start; position < end; position++) {
            char c = buffer_at(view->buffer, position);
            if (!isspace(c)) {
                blank_line = false;
                break;
            }
        }
        if (blank_line) {
            view->cursor = get_cursor_from_position(view->buffer, start);
            break;
        }
    }
}

COMMAND(forward_paragraph) {
    View *view = active_view;
    for (int64 line = view->cursor.line + 1; line < get_line_count(view->buffer) - 1; line++) {
        int64 start = view->buffer->line_starts[line];
        int64 end = view->buffer->line_starts[line + 1];
        bool blank_line = true;
        for (int64 position = start; position < end; position++) {
            char c = buffer_at(view->buffer, position);
            if (!isspace(c)) {
                blank_line = false;
                break;
            }
        }
        if (blank_line) {
            view->cursor = get_cursor_from_position(view->buffer, start);
            break;
        }
    }    
}

COMMAND(previous_line) {
    View *view = active_view;
    if (view->cursor.line > 0) {
        int64 position = get_position_from_line(view->buffer, view->cursor.line - 1);
        Cursor cursor = get_cursor_from_position(view->buffer, position);
        view->cursor = cursor;
    }
}

COMMAND(next_line) {
    View *view = active_view;
    if (view->cursor.line < get_line_count(view->buffer) - 1) {
        int64 position = get_position_from_line(view->buffer, view->cursor.line + 1);
        Cursor cursor = get_cursor_from_position(view->buffer, position);
        view->cursor = cursor;
    }
}

COMMAND(backward_delete_char) {
    View *view = active_view;
    if (view->mark_active) {
        if (view->cursor.position != view->mark.position) {
            Cursor start = view->mark.position < view->cursor.position ? view->mark : view->cursor;
            Cursor end = view->mark.position < view->cursor.position ? view->cursor : view->mark;
            buffer_delete_region(view->buffer, start.position, end.position);
            view->cursor = get_cursor_from_position(view->buffer, start.position);
        }
        view->mark_active = false;
    } else if (view->cursor.position > 0) {
        buffer_delete_single(view->buffer, view->cursor.position);
        Cursor cursor = get_cursor_from_position(view->buffer, view->cursor.position - 1);
        view->cursor = cursor;
    }
}

COMMAND(forward_delete_char) {
    View *view = active_view;
    if (view->mark_active) {
        if (view->cursor.position != view->mark.position) {
            Cursor start = view->mark.position < view->cursor.position ? view->mark : view->cursor;
            Cursor end = view->mark.position < view->cursor.position ? view->cursor : view->mark;
            buffer_delete_region(view->buffer, start.position, end.position);
            view->cursor = get_cursor_from_position(view->buffer, start.position);
        }
        view->mark_active = false;
    } else if (view->cursor.position < get_buffer_length(view->buffer)) {
        buffer_delete_single(view->buffer, view->cursor.position + 1);
        Cursor cursor = get_cursor_from_position(view->buffer, view->cursor.position);
        view->cursor = cursor;
    }
}

COMMAND(backward_delete_word) {
    View *view = active_view;
    int64 buffer_length = get_buffer_length(view->buffer);
    char first = buffer_at(view->buffer, view->cursor.position);
    int64 position = view->cursor.position;
    // eat whitespace
    if (isspace(first)) {
        for (; position >= 0; position--) {
            char c = buffer_at(view->buffer, position);
            if (!isspace(c)) {
                break;
            }
        }
    }
    for (position = position - 1; position >= 0; position--) {
        char c = buffer_at(view->buffer, position);
        if (isspace(c)) {
            break;
        }
    }

    position = CLAMP(position, 0, buffer_length);

    buffer_delete_region(view->buffer, position, view->cursor.position);
    view->cursor = get_cursor_from_position(view->buffer, position);
}

COMMAND(forward_delete_word) {
    View *view = active_view;
    int64 buffer_length = get_buffer_length(view->buffer);
    char first = buffer_at(view->buffer, view->cursor.position);
    int64 position = view->cursor.position;
    // eat whitespace
    if (isspace(first)) {
        for (; position < buffer_length; position++) {
            char c = buffer_at(view->buffer, position);
            if (!isspace(c)) {
                break;
            }
        }
    }
    for (; position < buffer_length; position++) {
        char c = buffer_at(view->buffer, position);
        if (isspace(c)) {
            break;
        }
    }

    position = CLAMP(position, 0, buffer_length);

    buffer_delete_region(view->buffer, view->cursor.position, position);
    view->cursor = get_cursor_from_position(view->buffer, view->cursor.position);
}

COMMAND(wheel_scroll_up) {
    View *view = active_view;
    float delta = -2.0f * view->face->glyph_height;
    view->y_off += (int)delta;
}

COMMAND(wheel_scroll_down) {
    View *view = active_view;
    float delta = 2.0f * view->face->glyph_height;
    view->y_off += (int)delta;
}

float rect_width(Rect rect) {
    float result;
    result = rect.x1 - rect.x0;
    return result;
}

float rect_height(Rect rect) {
    float result;
    result = rect.y1 - rect.y0;
    return result;
}

COMMAND(scroll_page_up) {
    View *view = active_view;
    int lines_per_page = (int)(rect_height(view->rect) / view->face->glyph_height);
    int page_line = (int)(view->y_off / view->face->glyph_height);
    float cursor_y = view->face->glyph_height * view->cursor.line;
    int lines_from_top = (int)(((cursor_y  + view->face->glyph_height) - view->y_off) / view->face->glyph_height);

    int64 line = view->cursor.line - lines_from_top;
    line = CLAMP(line, 0, get_line_count(view->buffer) - 1);

    view->y_off = (int)((line - lines_per_page + 1) * view->face->glyph_height);
    view->y_off = view->y_off < 0 ? 0 : view->y_off;
    view->cursor = get_cursor_from_line(view->buffer, line);
}

COMMAND(scroll_page_down) {
    View *view = active_view;
    float cursor_y = view->face->glyph_height * view->cursor.line;

    // @todo "view" space
    int lines_from_bottom = (int)((view->rect.y1 - cursor_y) / view->face->glyph_height);
    printf("%d\n", lines_from_bottom);

    int64 line = view->cursor.line + lines_from_bottom;
    line = CLAMP(line, 0, get_line_count(view->buffer) - 1);

    view->y_off = (int)(line * view->face->glyph_height);
    int max_y_off = (int)((get_line_count(view->buffer) - 4) * view->face->glyph_height);
    view->y_off = view->y_off > max_y_off ? max_y_off : view->y_off;
    view->cursor = get_cursor_from_line(view->buffer, line);
}

void write_buffer(Buffer *buffer) {
    String buffer_string = buffer_to_string(buffer);
    DWORD bytes_written = 0;
    HANDLE file_handle = CreateFileA((LPCSTR)buffer->file_name, GENERIC_WRITE, 0, NULL, TRUNCATE_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file_handle == INVALID_HANDLE_VALUE) {
        DWORD error = GetLastError();
        if (error == ERROR_FILE_NOT_FOUND) {
            file_handle = CreateFileA((LPCSTR)buffer->file_name, GENERIC_WRITE, 0, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        }
    }

    if (file_handle) {
        if (WriteFile(file_handle, buffer_string.data, (DWORD)buffer_string.count, &bytes_written, NULL)) {
        } else {
            DWORD error = GetLastError();
            printf("WriteFile: error writing file '%s'\n", buffer->file_name);
        }
        CloseHandle(file_handle);
    }
    free(buffer_string.data);
}

COMMAND(save_buffer) {
    write_buffer(active_view->buffer);
}

COMMAND(set_mark) {
    View *view = active_view;
    view->mark_active = true;
    view->mark = view->cursor;    
}

COMMAND(goto_beginning_of_line) {
    View *view = active_view;
    int64 position = get_position_from_line(view->buffer, view->cursor.line);
    view->cursor = get_cursor_from_position(view->buffer, position);
}

COMMAND(goto_end_of_line) {
    View *view = active_view;
    int64 position = get_position_from_line(view->buffer, view->cursor.line) + get_line_length(view->buffer, view->cursor.line);
    view->cursor = get_cursor_from_position(view->buffer, position);
}

COMMAND(goto_first_line) {
    View *view = active_view;
    view->cursor = get_cursor_from_position(view->buffer, 0);
}

COMMAND(goto_last_line) {
    View *view = active_view;
    view->cursor = get_cursor_from_position(view->buffer, view->buffer->size - (view->buffer->gap_end - view->buffer->gap_start));
}

void *allocate_system_event(size_t size) {
    void *event = calloc(1, size); 
    return event;
}

Window_Resize *make_window_resize_event(int width, int height) {
    Window_Resize *event = (Window_Resize *)allocate_system_event(sizeof(Window_Resize));
    *event = Window_Resize();
    event->type = SYSTEM_EVENT_WINDOW_RESIZE;
    event->width = width;
    event->height = height;
    return event;
}

Key_Stroke *make_key_stroke_event(Key_Code code, Key_Modifiers modifiers) {
    Key_Stroke *key_stroke = (Key_Stroke *)allocate_system_event(sizeof(Key_Stroke));
    *key_stroke = Key_Stroke();
    key_stroke->code = code;
    key_stroke->modifiers = modifiers;
    return key_stroke;
}

Text_Input *make_text_input(char *text, int64 count) {
    Text_Input *text_input = (Text_Input *)allocate_system_event(sizeof(Text_Input));
    *text_input = Text_Input();
    text_input->text = text;
    text_input->count = count;
    return text_input; 
}

void win32_keycodes_init() {
    for (int i = 0; i < 26; i++) {
        keycode_lookup['A' + i] = (Key_Code)(KEY_A + i);
    }
    for (int i = 0; i < 10; i++) {
        keycode_lookup['0' + i] = (Key_Code)(KEY_0 + i);
    }

    keycode_lookup[VK_HOME] = KEY_HOME;
    keycode_lookup[VK_END] = KEY_END;

    keycode_lookup[VK_SPACE] = KEY_SPACE;
    keycode_lookup[VK_OEM_COMMA] = KEY_COMMA;
    keycode_lookup[VK_OEM_PERIOD] = KEY_PERIOD;

    keycode_lookup[VK_RETURN] = KEY_ENTER;
    keycode_lookup[VK_BACK] = KEY_BACKSPACE;
    keycode_lookup[VK_DELETE] = KEY_DELETE;

    keycode_lookup[VK_LEFT] = KEY_LEFT;
    keycode_lookup[VK_RIGHT] = KEY_RIGHT;
    keycode_lookup[VK_UP] = KEY_UP;
    keycode_lookup[VK_DOWN] = KEY_DOWN;

    keycode_lookup[VK_PRIOR] = KEY_PAGEUP;
    keycode_lookup[VK_NEXT] = KEY_PAGEDOWN;

    keycode_lookup[VK_OEM_4] = KEY_LEFT_BRACKET;
    keycode_lookup[VK_OEM_6] = KEY_RIGHT_BRACKET;
    keycode_lookup[VK_OEM_1] = KEY_SEMICOLON;
    keycode_lookup[VK_OEM_2] = KEY_SLASH;
    keycode_lookup[VK_OEM_5] = KEY_BACKSLASH;
    keycode_lookup[VK_OEM_MINUS] = KEY_MINUS;
    keycode_lookup[VK_OEM_PLUS] = KEY_PLUS;
    keycode_lookup[VK_OEM_3] = KEY_TICK;
    keycode_lookup[VK_OEM_7] = KEY_QUOTE;

    for (int i = 0; i < 12; i++) {
        keycode_lookup[VK_F1 + i] = (Key_Code)(KEY_F1 + i);
    }
}

void win32_get_window_size(HWND hwnd, int *width, int *height) {
    RECT rect;
    GetClientRect(hwnd, &rect);
    if (*width) *width = rect.right - rect.left;
    if (*height) *height = rect.bottom - rect.top;
}

File_Attributes get_file_attributes(const char *file_name) {
    WIN32_FILE_ATTRIBUTE_DATA data{};
    GetFileAttributesExA(file_name, GetFileExInfoStandard, &data);
    File_Attributes attributes{};
    attributes.creation_time = ((uint64)data.ftCreationTime.dwHighDateTime << 32) | (data.ftCreationTime.dwLowDateTime);
    attributes.last_write_time = ((uint64)data.ftLastWriteTime.dwHighDateTime << 32) | (data.ftLastWriteTime.dwLowDateTime);
    attributes.last_access_time = ((uint64)data.ftLastAccessTime.dwHighDateTime << 32) | (data.ftLastAccessTime.dwLowDateTime);
    attributes.file_size = ((uint64)data.nFileSizeHigh << 32) | data.nFileSizeLow;
    return attributes;
}

char *read_file_string(const char *file_name) {
    char *result = NULL;
    HANDLE file_handle = CreateFileA((LPCSTR)file_name, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file_handle != INVALID_HANDLE_VALUE) {
        uint64 bytes_to_read;
        if (GetFileSizeEx(file_handle, (PLARGE_INTEGER)&bytes_to_read)) {
            assert(bytes_to_read <= UINT32_MAX);
            result = (char *)malloc(bytes_to_read + 1);
            result[bytes_to_read] = 0;
            DWORD bytes_read;
            if (ReadFile(file_handle, result, (DWORD)bytes_to_read, &bytes_read, NULL) && (DWORD)bytes_to_read ==  bytes_read) {
            } else {
                // TODO: error handling
                printf("ReadFile: error reading file, %s!\n", file_name);
            }
       } else {
            // TODO: error handling
            printf("GetFileSize: error getting size of file: %s!\n", file_name);
       }
       CloseHandle(file_handle);
    } else {
        // TODO: error handling
        printf("CreateFile: error opening file: %s!\n", file_name);
    }
    return result;
}

Read_File read_entire_file(const char *file_name) {
    Read_File result{};
    HANDLE file_handle = CreateFileA((LPCSTR)file_name, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file_handle != INVALID_HANDLE_VALUE) {
        uint64 bytes_to_read;
        if (GetFileSizeEx(file_handle, (PLARGE_INTEGER)&bytes_to_read)) {
            assert(bytes_to_read <= UINT32_MAX);
            result.data = (char *)malloc(bytes_to_read);
            DWORD bytes_read;
            if (ReadFile(file_handle, result.data, (DWORD)bytes_to_read, &bytes_read, NULL) && (DWORD)bytes_to_read ==  bytes_read) {
                result.count = bytes_read;
                result.handle = (Platform_Handle)file_handle;
            } else {
                // TODO: error handling
                printf("ReadFile: error reading file, %s!\n", file_name);
            }
       } else {

            // TODO: error handling
            printf("GetFileSize: error getting size of file: %s!\n", file_name);
       }
    } else {
        // TODO: error handling
        printf("CreateFile: error opening file: %s!\n", file_name);
    }
    return result;
}

Read_File open_entire_file(const char *file_name) {
    Read_File result{};
    HANDLE file_handle = CreateFileA((LPCSTR)file_name, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file_handle != INVALID_HANDLE_VALUE) {
        uint64 bytes_to_read;
        if (GetFileSizeEx(file_handle, (PLARGE_INTEGER)&bytes_to_read)) {
            assert(bytes_to_read <= UINT32_MAX);
            result.data = (char *)malloc(bytes_to_read);
            DWORD bytes_read;
            if (ReadFile(file_handle, result.data, (DWORD)bytes_to_read, &bytes_read, NULL) && (DWORD)bytes_to_read ==  bytes_read) {
                result.count = bytes_read;
                //result.handle = (Platform_Handle)file_handle;
                CloseHandle(file_handle);
                //if (SetFilePointer(file_handle, 0, 0, FILE_BEGIN) == INVALID_SET_FILE_POINTER) {
                //    printf("SetFilePointer: error rewinding file, '%s'\n", file_name);
                //}
            } else {
                // TODO: error handling
                printf("ReadFile: error reading file, %s!\n", file_name);
            }
       } else {
            // TODO: error handling
            printf("GetFileSize: error getting size of file: %s!\n", file_name);
       }
    } else {
        // TODO: error handling
        printf("CreateFile: error opening file: %s!\n", file_name);
    }
    return result;
}

inline Key_Modifiers make_key_modifiers(bool shift, bool control, bool alt) {
    Key_Modifiers modifiers = KEY_MODIFIER_NONE;
    modifiers = (Key_Modifiers)((int)modifiers | (int)(shift ? (int)KEY_MODIFIER_SHIFT : 0));
    modifiers = (Key_Modifiers)((int)modifiers | (int)(control ? (int)KEY_MODIFIER_CONTROL : 0));
    modifiers = (Key_Modifiers)((int)modifiers | (int)(alt ? (int)KEY_MODIFIER_ALT : 0));
    return modifiers;
}

LRESULT CALLBACK window_proc(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam) {
    static bool shift_down = false;
    static bool control_down = false;
    static bool alt_down = false;
    
    LRESULT result = 0;
    switch (Msg) {
    case WM_MENUCHAR:
    {
        result = (MNC_CLOSE << 16);
        break;
    }
    case WM_SYSKEYDOWN:
    case WM_SYSKEYUP:
    case WM_KEYDOWN:
    case WM_KEYUP:
    {
        bool key_down = (lParam & (1 << 31)) == 0;
        bool was_down = (lParam & (1 << 30)) == 1;
        uint64 vk = wParam;
        if (vk == VK_SHIFT) shift_down = key_down;
        else if (vk == VK_CONTROL) control_down = key_down;
        else if (vk == VK_MENU) alt_down = key_down;

        if (key_down) {
            Key_Code code = keycode_lookup[vk];
            if (code) {
                Key_Modifiers modifiers = make_key_modifiers(shift_down, control_down, alt_down);
                Key_Stroke *key_stroke = make_key_stroke_event(code, modifiers);
                if (active_key_stroke) free(active_key_stroke);
                active_key_stroke = key_stroke;
            }
        }
        break;
    }

    // case WM_LBUTTONUP:
    case WM_LBUTTONDOWN:
    {
        int x = GET_X_LPARAM(lParam); 
        int y = GET_Y_LPARAM(lParam); 
        y += active_view->y_off;
        String string = buffer_to_string(active_view->buffer);
        int line = (int)(y / active_view->face->glyph_height);

        // get cursor position from mouse click
        for (int64 line = 0; line < (int64)active_view->buffer->line_starts.count; line++) {
            float y0 = line * active_view->face->glyph_height;
            float y1 = y0 + active_view->face->glyph_height;
            if (y0 <= y && y <= y1) {
                float x0 = 0.0f;
                for (int64 position = active_view->buffer->line_starts[line]; position < active_view->buffer->line_starts[line + 1] - 1; position++) {
                    char c = string.data[position];
                    Glyph *glyph = &active_view->face->glyphs[c];
                    float x1 = x0 + glyph->ax;
                    if (x0 <= x && x <= x1) {
                        int64 col = position - active_view->buffer->line_starts[line];
                        active_view->cursor = { position, line, col };
                        break;
                    }
                    x0 += glyph->ax;
                }
                break;
            }
        }
        free(string.data);
        break;
    }

    case WM_MOUSEWHEEL:
    {
        float delta = GET_WHEEL_DELTA_WPARAM(wParam);
        if (delta < 0) {
            wheel_scroll_down();
        } else {
            wheel_scroll_up();
        }
        break;
    }

    case WM_CHAR:
    {
        uint16 vk = wParam & 0x0000ffff;
        uint8 c = (uint8)vk;
        if (c == '\r') {
            c = '\n';
        }
        if (c < 127) {
            char *text = (char *)malloc(1);
            text[0] = c;
            Text_Input *text_input = make_text_input(text, 1);
            if (active_key_stroke) {
                active_text_input = text_input;
            }
        }
        break;
    }

    case WM_SIZE:
    {
        UINT width = LOWORD(lParam);
        UINT height = HIWORD(lParam);
        Window_Resize *resize = make_window_resize_event(width, height);
        system_events.push(resize);
        break;
    }

    case WM_CLOSE:
        PostQuitMessage(0);
        break;
    default:
        result = DefWindowProcW(hWnd, Msg, wParam, lParam);
    }
    return result;
}


Face *load_font_face(const char *font_name, int font_height) {
    FT_Library ft_lib;
    int err = FT_Init_FreeType(&ft_lib);
    if (err) {
        printf("Error initializing freetype library: %d\n", err);
    }

    FT_Face ft_face;
    err = FT_New_Face(ft_lib, font_name, 0, &ft_face);
    if (err == FT_Err_Unknown_File_Format) {
        printf("Format not supported\n");
    } else if (err) {
        printf("Font file could not be read\n");
    }

    err = FT_Set_Pixel_Sizes(ft_face, 0, font_height);
    if (err) {
        printf("Error setting pixel sizes of font\n");
    }

    Face *face = new Face();

    int bbox_ymax = FT_MulFix(ft_face->bbox.yMax, ft_face->size->metrics.y_scale) >> 6;
    int bbox_ymin = FT_MulFix(ft_face->bbox.yMin, ft_face->size->metrics.y_scale) >> 6;
    int height = bbox_ymax - bbox_ymin;
    float ascend = ft_face->size->metrics.ascender / 64.f;
    float descend = ft_face->size->metrics.descender / 64.f;
    float bbox_height = (float)(bbox_ymax - bbox_ymin);
    float glyph_height = (float)ft_face->size->metrics.height / 64.f;
    float glyph_width = (float)(ft_face->size->metrics.max_advance) / 64.f;

    int atlas_width = 1; // white pixel
    int atlas_height = 0;
    int max_bmp_height = 0;
    for (uint32 c = 0; c < 256; c++) {
        if (FT_Load_Char(ft_face, c, FT_LOAD_RENDER)) {
            printf("Error loading char %c\n", c);
            continue;
        }

        atlas_width += ft_face->glyph->bitmap.width;
        if (atlas_height < (int)ft_face->glyph->bitmap.rows) {
            atlas_height = ft_face->glyph->bitmap.rows;
        }

        int bmp_height = ft_face->glyph->bitmap.rows + ft_face->glyph->bitmap_top;
        if (max_bmp_height < bmp_height) {
            max_bmp_height = bmp_height;
        }
    }

    int atlas_x = 1; // white pixel

    // Pack glyph bitmaps
    unsigned char *bitmap = (unsigned char *)calloc(atlas_width * atlas_height + 1, 1);
    bitmap[0] = 255;
    for (uint32 c = 0; c < 256; c++) {
        if (FT_Load_Char(ft_face, c, FT_LOAD_RENDER)) {
            printf("Error loading char '%c'\n", c);
        }

        Glyph *glyph = &face->glyphs[c];
        glyph->ax = (float)(ft_face->glyph->advance.x >> 6);
        glyph->ay = (float)(ft_face->glyph->advance.y >> 6);
        glyph->bx = (float)ft_face->glyph->bitmap.width;
        glyph->by = (float)ft_face->glyph->bitmap.rows;
        glyph->bt = (float)ft_face->glyph->bitmap_top;
        glyph->bl = (float)ft_face->glyph->bitmap_left;
        glyph->to = (float)atlas_x / atlas_width;
            
        // Write glyph bitmap to atlas
        for (int y = 0; y < glyph->by; y++) {
            unsigned char *dest = bitmap + y * atlas_width + atlas_x;
            unsigned char *source = ft_face->glyph->bitmap.buffer + y * ft_face->glyph->bitmap.width;
            memcpy(dest, source, ft_face->glyph->bitmap.width);
        }

        atlas_x += ft_face->glyph->bitmap.width;
    }

    face->width = atlas_width;
    face->height = atlas_height;
    face->max_bmp_height = max_bmp_height;
    face->ascend = ascend;
    face->descend = descend;
    face->bbox_height = height;
    face->glyph_width = glyph_width;
    face->glyph_height = glyph_height;

    D3D11_TEXTURE2D_DESC desc{};
    desc.Width = atlas_width;
    desc.Height = atlas_height;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_R8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.SampleDesc.Quality = 0;
    desc.Usage = D3D11_USAGE_IMMUTABLE;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    desc.CPUAccessFlags = 0;
    ID3D11Texture2D *font_texture = nullptr;
    D3D11_SUBRESOURCE_DATA sr_data{};
    sr_data.pSysMem = bitmap;
    sr_data.SysMemPitch = atlas_width;
    sr_data.SysMemSlicePitch = 0;
    HRESULT hr = d3d11_device->CreateTexture2D(&desc, &sr_data, &font_texture);
    assert(SUCCEEDED(hr));
    assert(font_texture != nullptr);
    hr = d3d11_device->CreateShaderResourceView(font_texture, nullptr, (ID3D11ShaderResourceView **)&face->texture);
    assert(SUCCEEDED(hr));

    FT_Done_Face(ft_face);
    FT_Done_FreeType(ft_lib);
    return face;
}

void d3d11_resize_render_target(UINT width, UINT height) {
    // NOTE: Resize render target view
    d3d11_context->OMSetRenderTargets(0, 0, 0);

    // Release all outstanding references to the swap chain's buffers.
    d3d11_render_target->Release();

    // Preserve the existing buffer count and format.
    // Automatically choose the width and height to match the client rect for HWNDs.
    HRESULT hr = d3d11_swap_chain->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0);

    // Get buffer and create a render-target-view.
    ID3D11Texture2D *backbuffer;
    hr = d3d11_swap_chain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void **)&backbuffer);

    hr = d3d11_device->CreateRenderTargetView(backbuffer, NULL, &d3d11_render_target);

    backbuffer->Release();

    ID3D11DepthStencilView *depth_stencil_view = nullptr;
    {
        D3D11_TEXTURE2D_DESC desc{};
        desc.Width = width;
        desc.Height = height;
        desc.MipLevels = 1;
        desc.ArraySize = 1;
        desc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
        desc.SampleDesc.Count = 1;
        desc.SampleDesc.Quality = 0;
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
        desc.CPUAccessFlags = 0;
        desc.MiscFlags = 0;

        ID3D11Texture2D *depth_stencil_buffer = nullptr;
        hr = d3d11_device->CreateTexture2D(&desc, NULL, &depth_stencil_buffer);
        hr = d3d11_device->CreateDepthStencilView(depth_stencil_buffer, NULL, &depth_stencil_view);
    }

    if (d3d11_depth_stencil_view) d3d11_depth_stencil_view->Release();
    d3d11_depth_stencil_view = depth_stencil_view;

    d3d11_context->OMSetRenderTargets(1, &d3d11_render_target, d3d11_depth_stencil_view);
}


void upload_constants(ID3D11Buffer *constant_buffer, void *constants, int32 constants_size) {
    D3D11_MAPPED_SUBRESOURCE res{};
    if (d3d11_context->Map(constant_buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &res) == S_OK) {
        memcpy(res.pData, constants, constants_size);
        d3d11_context->Unmap(constant_buffer, 0);
    } else {
        fprintf(stderr, "Failed to map constant buffer\n");
    }
}

void update_vertex_buffer(ID3D11Buffer *vertex_buffer, void *vertex_data, int32 vertices_size) {
    D3D11_MAPPED_SUBRESOURCE res{};
    if (d3d11_context->Map(vertex_buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &res) == S_OK) {
        memcpy(res.pData, vertex_data, vertices_size);
        d3d11_context->Unmap(vertex_buffer, 0);
    } else {
        fprintf(stderr, "Failed to map vertex buffer\n");
    }
}

ID3D11Buffer *make_vertex_buffer(void *vertex_data, int32 count, int32 size) {
    ID3D11Buffer *vertex_buffer = nullptr;
    D3D11_BUFFER_DESC desc{};
    desc.ByteWidth = count * size;
    desc.Usage = D3D11_USAGE_DYNAMIC;
    desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    D3D11_SUBRESOURCE_DATA data{};
    data.pSysMem = vertex_data;
    if (d3d11_device->CreateBuffer(&desc, &data, &vertex_buffer) != S_OK) {
        fprintf(stderr, "Failed to create vertex buffer\n");
    }
    return vertex_buffer;
}

ID3D11Buffer *make_constant_buffer(unsigned int size) {
    ID3D11Buffer *constant_buffer = nullptr;
    D3D11_BUFFER_DESC desc{};
    desc.ByteWidth = size;
    desc.Usage = D3D11_USAGE_DYNAMIC;
    desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    if (d3d11_device->CreateBuffer(&desc, nullptr, &constant_buffer) != S_OK) {
        fprintf(stderr, "Failed to create constant buffer\n");
    }
    return constant_buffer;
}

Shader *make_shader(const char *file_name, const char *vs_entry, const char *ps_entry, D3D11_INPUT_ELEMENT_DESC *items, int item_count) {
    Shader *shader = new Shader();
    //shader->name = copy_string(path_strip_file_name(file_name));
    printf("Compiling shader '%s'... ", file_name);

    char *source = read_file_string(file_name);
    ID3DBlob *vertex_blob, *pixel_blob, *vertex_error_blob, *pixel_error_blob;
    UINT flags = 0;
    #if _DEBUG
    flags |= D3DCOMPILE_DEBUG;
    #endif

    bool compilation_success = true;
    if (D3DCompile(source, strlen(source), file_name, NULL, D3D_COMPILE_STANDARD_FILE_INCLUDE, vs_entry, "vs_5_0", flags, 0, &vertex_blob, &vertex_error_blob) != S_OK) {
        compilation_success = false;
    }
    if (D3DCompile(source, strlen(source), file_name, NULL, D3D_COMPILE_STANDARD_FILE_INCLUDE, ps_entry, "ps_5_0", flags, 0, &pixel_blob, &pixel_error_blob) != S_OK) {
        compilation_success = false;
    }

    if (compilation_success) {
        printf("SUCCESS\n");
    } else {
        printf("FAILED\n");
        if (vertex_error_blob) printf("%s", (char *)vertex_error_blob->GetBufferPointer());
        // Errors are the same since compiling from the same source file, might be useful if we used different source files for the pixel and vertex shaders
        // if (pixel_error_blob) log_info("%s", (char *)pixel_error_blob->GetBufferPointer());
    }

    if (compilation_success) {
        d3d11_device->CreateVertexShader(vertex_blob->GetBufferPointer(), vertex_blob->GetBufferSize(), NULL, &shader->vertex_shader);
        d3d11_device->CreatePixelShader(pixel_blob->GetBufferPointer(), pixel_blob->GetBufferSize(), NULL, &shader->pixel_shader);
        HRESULT hr = d3d11_device->CreateInputLayout(items, item_count, vertex_blob->GetBufferPointer(), vertex_blob->GetBufferSize(), &shader->input_layout);
        // assert(hr == S_OK);
    }

    if (vertex_error_blob) vertex_error_blob->Release();
    if (pixel_error_blob)  pixel_error_blob->Release();
    if (vertex_blob) vertex_blob->Release();
    if (pixel_blob) pixel_blob->Release();
    return shader;
}

uint16 key_stroke_to_key(Key_Stroke *key_stroke) {
    uint16 result = (uint8)key_stroke->code;
    if (key_stroke->modifiers & KEY_MODIFIER_SHIFT) result |= KEYMOD_SHIFT;
    if (key_stroke->modifiers & KEY_MODIFIER_CONTROL) result |= KEYMOD_CONTROL;
    if (key_stroke->modifiers & KEY_MODIFIER_ALT) result |= KEYMOD_ALT;
    return result;
}

Key_Command make_key_command(const char *name, Command_Proc procedure) {
    Key_Command command;
    command.name = name;
    command.procedure = procedure;
    return command;
}

void set_key_command(Key_Map *key_map, uint16 key, Key_Command command) {
    key_map->commands[key] = command;
}

Key_Map *default_key_map() {
    Key_Map *key_map = (Key_Map *)calloc(1, sizeof(Key_Map));

    Key_Command self_insert_command = make_key_command("self_insert", self_insert);
    for (unsigned char c = 0; c < 128; c++) {
        uint16 vk = VkKeyScanA(c);
        vk = vk & 0x00ff; // discard high byte
        Key_Code key = keycode_lookup[vk];
        if (isprint(c) && key) {
            key_map->commands[key] = self_insert_command;
            key_map->commands[KEYMOD_SHIFT|key] = self_insert_command;
        }
    }

    // Default keybindings
    set_key_command(key_map, KEYMOD_ALT | KEY_F4, make_key_command("quit", quit_qed));
    set_key_command(key_map, KEYMOD_CONTROL | KEY_S, make_key_command("save_buffer", save_buffer));

    set_key_command(key_map, KEY_LEFT, make_key_command("backward_char", backward_char));
    set_key_command(key_map, KEY_RIGHT, make_key_command("forward_char", forward_char));
    set_key_command(key_map, KEYMOD_CONTROL | KEY_LEFT, make_key_command("backward_word", backward_word));
    set_key_command(key_map, KEYMOD_CONTROL | KEY_RIGHT, make_key_command("forward_word", forward_word));
    set_key_command(key_map, KEYMOD_CONTROL | KEY_UP, make_key_command("backward_paragraph", backward_paragraph));
    set_key_command(key_map, KEYMOD_CONTROL | KEY_DOWN, make_key_command("forward_paragraph", forward_paragraph));
    set_key_command(key_map, KEY_UP, make_key_command("previous_line", previous_line));
    set_key_command(key_map, KEY_DOWN, make_key_command("next_line", next_line));

    set_key_command(key_map, KEYMOD_SHIFT | KEY_BACKSPACE, make_key_command("backward_delete_char", backward_delete_char));
    set_key_command(key_map, KEY_BACKSPACE, make_key_command("backward_delete_char", backward_delete_char));
    set_key_command(key_map, KEYMOD_SHIFT | KEY_DELETE, make_key_command("forward_delete_char", forward_delete_char));
    set_key_command(key_map, KEY_DELETE, make_key_command("forward_delete_char", forward_delete_char));
    set_key_command(key_map, KEYMOD_CONTROL | KEY_BACKSPACE, make_key_command("backward_delete_word", backward_delete_word));
    set_key_command(key_map, KEYMOD_CONTROL | KEYMOD_SHIFT | KEY_BACKSPACE, make_key_command("backward_delete_word", backward_delete_word));
    set_key_command(key_map, KEYMOD_CONTROL | KEY_DELETE, make_key_command("forward_delete_word", forward_delete_word));
    set_key_command(key_map, KEYMOD_CONTROL | KEYMOD_SHIFT | KEY_DELETE, make_key_command("forward_delete_word", forward_delete_word));

    set_key_command(key_map, KEY_HOME, make_key_command("goto_beginning_of_line", goto_beginning_of_line));
    set_key_command(key_map, KEY_END, make_key_command("goto_end_of_line", goto_end_of_line));
    set_key_command(key_map, KEYMOD_CONTROL | KEY_HOME, make_key_command("goto_first_line", goto_first_line));
    set_key_command(key_map, KEYMOD_CONTROL | KEY_END, make_key_command("goto_last_line", goto_last_line));

    set_key_command(key_map, KEY_ENTER, make_key_command("newline", newline));

    set_key_command(key_map, KEY_PAGEUP, make_key_command("scroll_page_up", scroll_page_up));
    set_key_command(key_map, KEY_PAGEDOWN, make_key_command("scroll_page_down", scroll_page_down));

    // Emacs keybindings
    set_key_command(key_map, KEYMOD_CONTROL | KEY_J, make_key_command("newline", newline));

    set_key_command(key_map, KEYMOD_CONTROL | KEY_A, make_key_command("goto_beginning_of_line", goto_beginning_of_line));
    set_key_command(key_map, KEYMOD_CONTROL | KEY_E, make_key_command("goto_end_of_line", goto_end_of_line));

    set_key_command(key_map, KEYMOD_CONTROL | KEY_SPACE, make_key_command("set_mark", set_mark));
    set_key_command(key_map, KEYMOD_CONTROL | KEY_G, make_key_command("quit_selection", quit_selection));
    return key_map;
}

int main(int argc, char **argv) {
    // UINT desired_scheduler_ms = 1;
    // timeBeginPeriod(desired_scheduler_ms);
    argc--;
    argv++;
    if (argc == 0) {
        puts("QED");
        puts("Usage: Qed [filename]");
        exit(0);
    }

    char *file_name = argv[0];

    win32_keycodes_init();

    active_keymap = default_key_map();

#define CLASSNAME L"QED_WINDOW_CLASS"
    HINSTANCE hinstance = GetModuleHandle(NULL);
    WNDCLASSW window_class{};
    window_class.style = CS_HREDRAW | CS_VREDRAW;
    window_class.lpfnWndProc = window_proc;
    window_class.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    window_class.lpszClassName = CLASSNAME;
    window_class.hInstance = hinstance;
    window_class.hCursor = LoadCursorW(NULL, IDC_ARROW);
    if (!RegisterClassW(&window_class)) {
        fprintf(stderr, "RegisterClassA failed, err:%d\n", GetLastError());
    }

    HWND window = CreateWindowW(CLASSNAME, L"Qed", WS_OVERLAPPEDWINDOW | WS_VISIBLE, CW_USEDEFAULT, CW_USEDEFAULT, WIDTH, HEIGHT, NULL, NULL, hinstance, NULL);

    HRESULT hr = S_OK;

    // INITIALIZE SWAP CHAIN
    DXGI_SWAP_CHAIN_DESC swapchain_desc{};
    {
        DXGI_MODE_DESC buffer_desc{};
        buffer_desc.Width = WIDTH;
        buffer_desc.Height = HEIGHT;
        buffer_desc.RefreshRate.Numerator = 60;
        buffer_desc.RefreshRate.Denominator = 1;
        buffer_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        buffer_desc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
        buffer_desc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;

        swapchain_desc.BufferDesc = buffer_desc;
        swapchain_desc.SampleDesc.Count = 1;
        swapchain_desc.SampleDesc.Quality = 0;
        swapchain_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        swapchain_desc.BufferCount = 1;
        swapchain_desc.OutputWindow = window;
        swapchain_desc.Windowed = TRUE;
        swapchain_desc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
    }

    hr = D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, D3D11_CREATE_DEVICE_DEBUG, NULL, NULL, D3D11_SDK_VERSION, &swapchain_desc, &d3d11_swap_chain, &d3d11_device, NULL, &d3d11_context);

    ID3D11Texture2D *backbuffer;
    hr = d3d11_swap_chain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void **)&backbuffer);

    hr = d3d11_device->CreateRenderTargetView(backbuffer, NULL, &d3d11_render_target);
    backbuffer->Release();

    {
        RECT rc = {0, 0, WIDTH, HEIGHT};
        AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);
        SetWindowPos(window, HWND_NOTOPMOST, 0, 0, rc.right - rc.left, rc.bottom - rc.top, SWP_NOMOVE|SWP_NOZORDER);
    }

    // DEPTH-STENCIL STATE
    ID3D11DepthStencilState *depth_stencil_state = nullptr;
    {
        D3D11_DEPTH_STENCIL_DESC desc{};
        desc.DepthEnable = false;
        desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
        desc.DepthFunc = D3D11_COMPARISON_ALWAYS;
        desc.StencilEnable = false;
        desc.FrontFace.StencilFailOp = desc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
        desc.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
        desc.BackFace = desc.FrontFace;
        d3d11_device->CreateDepthStencilState(&desc, &depth_stencil_state);
    }

    // BLEND STATE
    ID3D11BlendState *blend_state = nullptr;
    {
        D3D11_BLEND_DESC desc{};
        desc.AlphaToCoverageEnable = false;
        desc.RenderTarget[0].BlendEnable = true;
        desc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
        desc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
        desc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
        desc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
        desc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
        desc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
        desc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
        d3d11_device->CreateBlendState(&desc, &blend_state);
    }

    // RASTERIZER STATE
    ID3D11RasterizerState *rasterizer_state = nullptr;
    {
        D3D11_RASTERIZER_DESC desc{};
        desc.FillMode = D3D11_FILL_SOLID;
        desc.CullMode = D3D11_CULL_NONE;
        desc.ScissorEnable = false;
        desc.DepthClipEnable = false;
        d3d11_device->CreateRasterizerState(&desc, &rasterizer_state);
    }

    // FONT SAMPLER
    ID3D11SamplerState *font_sampler = nullptr;
    {
        D3D11_SAMPLER_DESC desc{};
        desc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
        desc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
        desc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
        desc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
        desc.ComparisonFunc = D3D11_COMPARISON_NEVER;
        HRESULT hr = d3d11_device->CreateSamplerState(&desc, &font_sampler);
        assert(SUCCEEDED(hr));
    }

    D3D11_INPUT_ELEMENT_DESC simple_layout[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(Vertex, position), D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(Vertex, uv), D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, offsetof(Vertex, color), D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };
    Shader *simple_shader = make_shader("simple.hlsl", "vs_main", "ps_main", simple_layout, ARRAYSIZE(simple_layout));

    Face *face = load_font_face("fonts/consolas.ttf", 20);

    ID3D11Buffer *simple_constant_buffer = make_constant_buffer(sizeof(Simple_Constants));

    Simple_Constants simple_constants{};

    active_view = new View();
    active_view->rect = { 0.0f, 0.0f, (float)WIDTH, (float)HEIGHT };
    active_view->buffer = make_buffer_from_file(file_name);
    active_view->cursor = {};
    active_view->face = face;
    active_view->y_off = 0;
    
    while (!window_should_close) {
        MSG message;
        while (PeekMessageW(&message, NULL, 0, 0, PM_REMOVE)) {
            if (message.message == WM_QUIT) {
                window_should_close = true;
            }
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }

        for (int i = 0; i < system_events.count; i++) {
            System_Event *event = system_events[i];
            switch (event->type) {
            case SYSTEM_EVENT_WINDOW_RESIZE:
            {
                Window_Resize *resize_event = static_cast<Window_Resize *>(event);
                d3d11_resize_render_target(resize_event->width, resize_event->height);
                free(event);
                break;
            }
            }
        }
        system_events.reset_count();

        if (active_key_stroke) {
            uint16 key = key_stroke_to_key(active_key_stroke);
            assert(key < MAX_KEY_COUNT);
            Key_Command *command = &active_keymap->commands[key];
            if (command->procedure) {
                // printf("executing '%s'\n", command->name);
                command->procedure();
            }

            free(active_key_stroke);
            active_key_stroke = nullptr;

            free(active_text_input);
            active_text_input = nullptr;
        }

        int width, height;
        win32_get_window_size(window, &width, &height);
        v2 render_dim = V2((float)width, (float)height);

        active_view->rect = { 0.0f, 0.0f, render_dim.x, render_dim.y };

        vertices.reset_count();

        draw_view(active_view);

        if (!vertex_buffer && vertices.count > 0 || vertex_buffer_capacity < vertices.count) {
            if (vertex_buffer) {
                vertex_buffer->Release();
                vertex_buffer = nullptr;
            }
            vertex_buffer_capacity = (int)vertices.capacity;
            vertex_buffer = make_vertex_buffer(vertices.data, vertex_buffer_capacity, sizeof(Vertex));
        }

        if (vertices.count > 0) {
            update_vertex_buffer(vertex_buffer, vertices.data, (int)vertices.count * sizeof(Vertex));
        }

        m4 projection = ortho_rh_zo(0.0f, render_dim.x, render_dim.y, 0.0f, 0.0f, 1.0f);

        float bg_color[4] = {0, 0, 0, 1};
        d3d11_context->ClearRenderTargetView(d3d11_render_target, bg_color);

        D3D11_VIEWPORT viewport{};
        viewport.TopLeftX = viewport.TopLeftY = 0;
        viewport.Width = render_dim.x;
        viewport.Height = render_dim.y;
        viewport.MinDepth = 0.0f;
        viewport.MaxDepth = 1.0f;

        d3d11_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        d3d11_context->IASetInputLayout(simple_shader->input_layout);

        UINT stride = sizeof(Vertex), offset = 0;
        d3d11_context->IASetVertexBuffers(0, 1, &vertex_buffer, &stride, &offset);
        
        simple_constants.transform = projection;
        upload_constants(simple_constant_buffer, &simple_constants, sizeof(Simple_Constants));

        d3d11_context->VSSetConstantBuffers(0, 1, &simple_constant_buffer);

        d3d11_context->VSSetShader(simple_shader->vertex_shader, 0, 0);
        d3d11_context->PSSetShader(simple_shader->pixel_shader, 0, 0);

        d3d11_context->PSSetSamplers(0, 1, &font_sampler);

        d3d11_context->PSSetShaderResources(0, 1, (ID3D11ShaderResourceView **)&face->texture);
        
        d3d11_context->RSSetState(rasterizer_state);
        d3d11_context->RSSetViewports(1, &viewport);

        float blend_factor[4] = {0.0f, 0.0f, 0.0f, 0.0f};
        d3d11_context->OMSetBlendState(blend_state, blend_factor, 0xffffffff); 
        d3d11_context->OMSetDepthStencilState(depth_stencil_state, 0);

        d3d11_context->Draw((UINT)vertices.count, 0);

        d3d11_swap_chain->Present(1, 0);
    }

    return 0;
}
