#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <timeapi.h>
#include <windowsx.h>
#include <fileapi.h>
#undef near
#undef far
#endif // _WIN32

#include "platform.h"
#include "path.h"
#include "qed.h"
#include "draw.h"

#include <stdio.h>

#define WIDTH  1040
#define HEIGHT 860

static bool window_should_close;

Array<System_Event *> system_events;
Key_Code keycode_lookup[256];
Key_Stroke *active_key_stroke;
Text_Input *active_text_input;

View *active_view;

Render_Target render_target;

Find_File_Dialog find_file_dialog;

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

// @todo add options for adjusting view focus (top, center, bottom)
void ensure_cursor_in_view(View *view, Cursor cursor) {
    float cursor_top = cursor.line * view->face->glyph_height - view->y_off;
    float cursor_bottom = cursor_top + view->face->glyph_height;
    if (cursor_top < 0) {
        view->y_off += (int)cursor_top;
    } else if (cursor_bottom >= rect_height(view->rect)) {
        float d = cursor_bottom - rect_height(view->rect);
        view->y_off += (int)d;
    }
}

void view_set_cursor(View *view, Cursor cursor) {
    ensure_cursor_in_view(view, cursor);
    view->cursor = cursor;
}

COMMAND(quit_qed) {
    window_should_close = true;
}

COMMAND(quit_selection) {
    active_view->mark_active = false;
}

COMMAND(newline) {
    View *view = active_view;
    Cursor cursor = view->cursor;
    buffer_insert_single(view->buffer, cursor.position, '\n');
    cursor.position++;
    cursor.col = 0;
    cursor.line++;
    view_set_cursor(view, cursor);
}

String string_copy(const char *str) {
    size_t len = strlen(str);
    String result;
    result.data = (char *)malloc(len + 1);
    memcpy(result.data, str, len);
    result.data[len] = 0;
    result.count = len;
    return result;
}

String string_reserve(int64 count) {
    String result;
    result.data = (char *)malloc(count + 1);
    memset(result.data, 0, count + 1);
    result.count = count;
    return result;
}

String string_copy(String str) {
    String result;
    result.data = (char *)malloc(str.count + 1);
    memcpy(result.data, str.data, str.count + 1);
    result.count = str.count;
    return result;
}

void string_concat(String *str1, String str2) {
    char *temp = str1->data;
    int64 new_count = str1->count + str2.count;
    str1->data = (char *)malloc(new_count + 1);
    memcpy(str1->data, temp, str1->count);
    memcpy(str1->data + str1->count, str2.data, str2.count);
    str1->data[new_count] = 0;
    str1->count += str2.count;
}

void buffer_record_insert(Buffer *buffer, int64 position, String text) {
    Edit_Record *current = buffer->edit_history;
    if (current && current->type == EDIT_RECORD_INSERT && position == current->span.end) {
        string_concat(&current->text, text);
        current->span.end = position + text.count;
    } else {
        Edit_Record *edit = new Edit_Record();
        edit->type = EDIT_RECORD_INSERT;
        edit->span = { position, position + text.count };
        edit->text = text;
        edit->prev = current;
        buffer->edit_history = edit;
    }
}

void buffer_record_delete(Buffer *buffer, int64 start, int64 end) {
    Edit_Record *current = buffer->edit_history;
    {
        Edit_Record *edit = new Edit_Record();
        edit->type = EDIT_RECORD_DELETE;
        edit->span = { start, end };
        edit->text = buffer_to_string_span(buffer, {start, end});
        edit->prev = current;
        buffer->edit_history = edit;
    }
}

COMMAND(self_insert) {
    View *view = active_view;
    String insert_string = string_copy({active_text_input->text, 1});
    if (active_text_input) {
        if (view->mark_active) {
            String string = { active_text_input->text, 1 };
            int64 start = view->cursor.position < view->mark.position ? view->cursor.position : view->mark.position;
            int64 end = view->cursor.position < view->mark.position ? view->mark.position : view->cursor.position;
            buffer_replace_region(view->buffer, string, start, end);
            view->mark_active = false;
            view_set_cursor(view, get_cursor_from_position(view->buffer, start));
        } else {
            buffer_insert_single(view->buffer, view->cursor.position, active_text_input->text[0]);
            buffer_record_insert(view->buffer, view->cursor.position, insert_string);
            view_set_cursor(view, get_cursor_from_position(view->buffer, view->cursor.position + 1));
        }
        if (view->buffer->post_self_insert_hook) view->buffer->post_self_insert_hook(active_text_input);
    } else {
        assert(false);
    }
}

COMMAND(backward_char) {
    View *view = active_view;
    if (view->cursor.position > 0) {
        Cursor cursor = get_cursor_from_position(view->buffer, view->cursor.position - 1);
        view_set_cursor(view, cursor);
    }
}

COMMAND(forward_char) {
    View *view = active_view;
    if (view->cursor.position < buffer_get_length(view->buffer)) {
        Cursor cursor = get_cursor_from_position(view->buffer, view->cursor.position + 1);
        view_set_cursor(view, cursor);
    }
}

COMMAND(forward_word) {
    View *view = active_view;
    int64 buffer_length = buffer_get_length(view->buffer);
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

    view_set_cursor(view, get_cursor_from_position(view->buffer, position));
}

COMMAND(backward_word) {
    View *view = active_view;
    int64 buffer_length = buffer_get_length(view->buffer);
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
    view_set_cursor(view, get_cursor_from_position(view->buffer, position));
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
            view_set_cursor(view, get_cursor_from_position(view->buffer, start));
            break;
        }
    }
}

COMMAND(forward_paragraph) {
    View *view = active_view;
    for (int64 line = view->cursor.line + 1; line < buffer_get_line_count(view->buffer) - 1; line++) {
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
            view_set_cursor(view, get_cursor_from_position(view->buffer, start));
            break;
        }
    }    
}

COMMAND(previous_line) {
    View *view = active_view;
    if (view->cursor.line > 0) {
        int64 position = get_position_from_line(view->buffer, view->cursor.line - 1);
        Cursor cursor = get_cursor_from_position(view->buffer, position);
        view_set_cursor(view, cursor);
    }
}

COMMAND(next_line) {
    View *view = active_view;
    if (view->cursor.line < buffer_get_line_count(view->buffer) - 1) {
        int64 position = get_position_from_line(view->buffer, view->cursor.line + 1);
        Cursor cursor = get_cursor_from_position(view->buffer, position);
        view_set_cursor(view, cursor);
    }
}

COMMAND(backward_delete_char) {
    View *view = active_view;
    if (view->mark_active) {
        if (view->cursor.position != view->mark.position) {
            Cursor start = view->mark.position < view->cursor.position ? view->mark : view->cursor;
            Cursor end = view->mark.position < view->cursor.position ? view->cursor : view->mark;
            buffer_delete_region(view->buffer, start.position, end.position);
            view_set_cursor(view, get_cursor_from_position(view->buffer, start.position));
        }
        view->mark_active = false;
    } else if (view->cursor.position > 0) {
        buffer_record_delete(view->buffer, view->cursor.position - 1, view->cursor.position);
        buffer_delete_single(view->buffer, view->cursor.position);
        Cursor cursor = get_cursor_from_position(view->buffer, view->cursor.position - 1);
        view_set_cursor(view, cursor);
    }
}

COMMAND(forward_delete_char) {
    View *view = active_view;
    if (view->mark_active) {
        if (view->cursor.position != view->mark.position) {
            Cursor start = view->mark.position < view->cursor.position ? view->mark : view->cursor;
            Cursor end = view->mark.position < view->cursor.position ? view->cursor : view->mark;
            buffer_delete_region(view->buffer, start.position, end.position);
            view_set_cursor(view, get_cursor_from_position(view->buffer, start.position));
        }
        view->mark_active = false;
    } else if (view->cursor.position < buffer_get_length(view->buffer)) {
        buffer_record_delete(view->buffer, view->cursor.position, view->cursor.position + 1);
        buffer_delete_single(view->buffer, view->cursor.position + 1);
        Cursor cursor = get_cursor_from_position(view->buffer, view->cursor.position);
        view_set_cursor(view, cursor);
    }
}

COMMAND(backward_delete_word) {
    View *view = active_view;
    int64 buffer_length = buffer_get_length(view->buffer);
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

    buffer_record_delete(view->buffer, position, view->cursor.position);
    buffer_delete_region(view->buffer, position, view->cursor.position);
    view_set_cursor(view, get_cursor_from_position(view->buffer, position));
}

COMMAND(forward_delete_word) {
    View *view = active_view;
    int64 buffer_length = buffer_get_length(view->buffer);
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

    buffer_record_delete(view->buffer, view->cursor.position, position);
    buffer_delete_region(view->buffer, view->cursor.position, position);
    view_set_cursor(view, get_cursor_from_position(view->buffer, view->cursor.position));
}

COMMAND(kill_line) {
    View *view = active_view;
    int64 start = view->cursor.position;
    int64 end = start + buffer_get_line_length(view->buffer, view->cursor.line);
    if (end - start == 0) end += 1;
    buffer_delete_region(view->buffer, start, end);
    view_set_cursor(view, view->cursor);
}

COMMAND(open_line) {
    View *view = active_view;
    int64 line_begin = get_position_from_line(view->buffer, view->cursor.line + 1);
    buffer_insert_single(view->buffer, line_begin, '\n');
    view_set_cursor(view, get_cursor_from_line(view->buffer, view->cursor.line + 1));
}

COMMAND(undo) {
    View *view = active_view;
    printf("// UNDO //\n");
    Buffer *buffer = active_view->buffer;
    for (Edit_Record *edit = buffer->edit_history; edit; edit = edit->prev) {
        if (edit->type == EDIT_RECORD_INSERT) printf("INSERT ");
        else if (edit->type == EDIT_RECORD_DELETE) printf("DELETE ");
        printf("%lld,%lld ", edit->span.start, edit->span.end);
        printf(": %s\n", edit->text.data);
    }

    Edit_Record *edit = buffer->edit_history;
    if (edit) {
        switch (edit->type) {
        case EDIT_RECORD_INSERT:
            buffer_delete_region(buffer, edit->span.start, edit->span.end);
            view_set_cursor(view, get_cursor_from_position(buffer, edit->span.start));
            break;
        case EDIT_RECORD_DELETE:
            buffer_insert_text(buffer, edit->span.start, edit->text);
            view_set_cursor(view, get_cursor_from_position(buffer, edit->span.end));
            break;
        }

        buffer->edit_history = edit->prev;
        delete edit;
    }
}

// @todo scroll ensure cursor keeps up
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

COMMAND(scroll_page_up) {
    View *view = active_view;
    int lines_per_page = (int)(rect_height(view->rect) / view->face->glyph_height);
    int page_line = (int)(view->y_off / view->face->glyph_height);
    float cursor_y = view->face->glyph_height * view->cursor.line - view->y_off;
    int lines_from_top = (int)(cursor_y / view->face->glyph_height);

    int64 line = view->cursor.line - lines_from_top;
    line = CLAMP(line, 0, buffer_get_line_count(view->buffer) - 1);

    view->y_off = (int)((line - lines_per_page) * view->face->glyph_height);
    view->y_off = view->y_off < 0 ? 0 : view->y_off;
    view_set_cursor(view, get_cursor_from_line(view->buffer, line));
}

COMMAND(scroll_page_down) {
    View *view = active_view;
    float cursor_y = view->face->glyph_height * view->cursor.line - view->y_off;

    int lines_from_bottom = (int)((rect_height(view->rect) - cursor_y) / view->face->glyph_height);
    int64 line = view->cursor.line + lines_from_bottom;
    line = CLAMP(line, 0, buffer_get_line_count(view->buffer) - 1);

    view->y_off = (int)(line * view->face->glyph_height);
    int max_y_off = (int)((buffer_get_line_count(view->buffer) - 4) * view->face->glyph_height);
    view->y_off = view->y_off > max_y_off ? max_y_off : view->y_off;
    view_set_cursor(view, get_cursor_from_line(view->buffer, line));
}

void write_buffer(Buffer *buffer) {
    // String buffer_string = buffer_to_string(buffer);
    String buffer_string = buffer_to_string_apply_line_endings(buffer);
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
    view_set_cursor(view, get_cursor_from_position(view->buffer, position));
}

COMMAND(goto_end_of_line) {
    View *view = active_view;
    int64 position = get_position_from_line(view->buffer, view->cursor.line) + buffer_get_line_length(view->buffer, view->cursor.line);
    view_set_cursor(view, get_cursor_from_position(view->buffer, position));
}

COMMAND(goto_first_line) {
    View *view = active_view;
    view_set_cursor(view, get_cursor_from_position(view->buffer, 0));
}

COMMAND(goto_last_line) {
    View *view = active_view;
    view_set_cursor(view, get_cursor_from_position(view->buffer, view->buffer->size - (view->buffer->gap_end - view->buffer->gap_start)));
}

COMMAND(find_file) {
    find_file_dialog.last_active = active_view;
    find_file_dialog.is_active = true;
    active_view = find_file_dialog.view;
}

COMMAND(find_file_enter) {
    find_file_dialog.is_active = false;

    String file_name = buffer_to_string(find_file_dialog.view->buffer);
    printf("Opening %s\n", file_name.data);

    View *view = find_file_dialog.last_active;
    Buffer *buffer = make_buffer_from_file(file_name.data);
    view->buffer = buffer;
    view->mark_active = false;
    view->cursor = {};
    view->mark = {};
    view->y_off = 0;
    active_view = view;
    buffer_clear(find_file_dialog.view->buffer);
}

COMMAND(find_file_escape) {
    find_file_dialog.is_active = false;
    active_view = find_file_dialog.last_active;
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

    keycode_lookup[VK_ESCAPE] = KEY_ESCAPE;

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

String read_file_string(const char *file_name) {
    String result{};
    HANDLE file_handle = CreateFileA((LPCSTR)file_name, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file_handle != INVALID_HANDLE_VALUE) {
        uint64 bytes_to_read;
        if (GetFileSizeEx(file_handle, (PLARGE_INTEGER)&bytes_to_read)) {
            assert(bytes_to_read <= UINT32_MAX);
            result.data = (char *)malloc(bytes_to_read + 1);
            result.data[bytes_to_read] = 0;
            DWORD bytes_read;
            if (ReadFile(file_handle, result.data, (DWORD)bytes_to_read, &bytes_read, NULL) && (DWORD)bytes_to_read ==  bytes_read) {
                result.count = bytes_read;
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
        CloseHandle(file_handle);
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
        CloseHandle(file_handle);
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

Key_Map *make_default_key_map() {
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

    set_key_command(key_map, KEYMOD_CONTROL|KEY_O, make_key_command("find_file", find_file));

    set_key_command(key_map, KEYMOD_CONTROL | KEY_Z, make_key_command("undo", undo));

    // Emacs keybindings
    set_key_command(key_map, KEYMOD_CONTROL | KEY_A, make_key_command("goto_beginning_of_line", goto_beginning_of_line));
    set_key_command(key_map, KEYMOD_CONTROL | KEY_E, make_key_command("goto_end_of_line", goto_end_of_line));

    set_key_command(key_map, KEYMOD_CONTROL | KEY_SPACE, make_key_command("set_mark", set_mark));
    set_key_command(key_map, KEYMOD_CONTROL | KEY_G, make_key_command("quit_selection", quit_selection));

    set_key_command(key_map, KEYMOD_CONTROL | KEY_K, make_key_command("kill_line", kill_line));
    set_key_command(key_map, KEYMOD_CONTROL | KEY_J, make_key_command("open_line", open_line));

    return key_map;
}

Key_Map *make_find_file_key_map() {
    Key_Map *key_map = (Key_Map *)calloc(sizeof(Key_Map), 1);
    Key_Command self_insert_command = make_key_command("self_insert", self_insert);
    for (unsigned char c = 0; c < 128; c++) {
        uint16 vk = VkKeyScanA(c);
        vk = vk & 0x00ff; // discard high byte
        Key_Code key = keycode_lookup[vk];
        if (isprint(c) && key) {
            set_key_command(key_map, key, self_insert_command);
            set_key_command(key_map, KEYMOD_SHIFT|key, self_insert_command);
        }
    }

    set_key_command(key_map, KEY_ENTER, make_key_command("find_file_enter", find_file_enter));
    set_key_command(key_map, KEY_ESCAPE, make_key_command("find_file_escape", find_file_escape));
    return key_map;
}

void find_file_post_self_insert_hook(Text_Input *input) {
    WIN32_FIND_DATAA file_data;
    char c = input->text[0];
    if (c == '/' || c == '\\') {
        // String string = buffer_to_string(find_file_dialog.view->buffer);
        // find_file_dialog.current_path = string.data;
    }

    HANDLE file_handle = FindFirstFileA(find_file_dialog.current_path, &file_data);
    if (file_handle != INVALID_HANDLE_VALUE) {
        do {
            printf("%s\n", file_data.cFileName);
        } while (FindNextFileA(file_handle, &file_data));
        FindClose(file_handle);
    }
}

void default_post_self_insert_hook(Text_Input *input) {
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

    Key_Map *default_key_map = make_default_key_map();

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
    {
        RECT rc = {0, 0, WIDTH, HEIGHT};
        AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);
        SetWindowPos(window, HWND_NOTOPMOST, 0, 0, rc.right - rc.left, rc.bottom - rc.top, SWP_NOMOVE|SWP_NOZORDER);
    }

    void d3d11_initialize_devices(uint32 width, uint32 height, HWND window_handle);
    d3d11_initialize_devices(WIDTH, HEIGHT, window);

    Theme *load_theme(const char *file_name);
    Theme *theme = load_theme("themes/gruvbox.qed-theme");

    View *view = new View();
    view->rect = { 0.0f, 0.0f, (float)WIDTH, (float)HEIGHT };
    view->buffer = make_buffer_from_file(file_name);
    view->buffer->post_self_insert_hook = default_post_self_insert_hook;
    view->cursor = {};
    view->face = load_font_face("fonts/JetBrainsMono.ttf", 18);
    view->y_off = 0;
    view->theme = theme;
    view->key_map = default_key_map;

    View *find_file_view = new View();
    find_file_view->rect = { 0.15 * WIDTH, 0.1f * HEIGHT, 0.85f * WIDTH, 0.5f * HEIGHT };
    find_file_view->buffer = make_buffer("find_file");
    String current_dir = STRZ(path_current_dir());
    buffer_insert_text(find_file_view->buffer, 0, current_dir);
    find_file_view->cursor = get_cursor_from_position(find_file_view->buffer, buffer_get_length(find_file_view->buffer));
    find_file_view->buffer->post_self_insert_hook = find_file_post_self_insert_hook;
    find_file_view->face = load_font_face("fonts/SegUI.ttf", 20);
    find_file_view->y_off = 0;
    find_file_view->theme = load_theme("themes/gruvbox.qed-theme");
    find_file_view->key_map = make_find_file_key_map();
    find_file_dialog.view = find_file_view;

    active_view = view;
 
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
                //d3d11_resize_render_target(resize_event->width, resize_event->height);
                free(event);
                break;
            }
            }
        }
        system_events.reset_count();

        if (active_key_stroke) {
            uint16 key = key_stroke_to_key(active_key_stroke);
            assert(key < MAX_KEY_COUNT);
            Key_Command *command = &active_view->key_map->commands[key];
            if (command->procedure) {
                command->procedure();
            }

            free(active_key_stroke);
            active_key_stroke = nullptr;

            if (active_text_input) free(active_text_input);
            active_text_input = nullptr;
        }

        int width, height;
        win32_get_window_size(window, &width, &height);
        v2 render_dim = V2((float)width, (float)height);

        render_target.width = width;
        render_target.height = height;
        render_target.current = nullptr;
        render_target.groups.reset_count();

        draw_view(&render_target, view);
        draw_find_file_dialog(&render_target, &find_file_dialog);

        void d3d11_render(Render_Target *target);
        d3d11_render(&render_target);
    }

    return 0;
}
