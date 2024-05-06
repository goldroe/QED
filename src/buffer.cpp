#include "buffer.h"
#include "types.h"
#include "platform.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DEFAULT_GAP_SIZE 1024

#define PTR_IN_GAP(Ptr, Buffer) (Ptr >= Buffer->text + Buffer->gap_start && Ptr < Buffer->text + Buffer->gap_end)
#define GAP_SIZE(Buffer) (Buffer->gap_end - Buffer->gap_start)
#define BUFFER_SIZE(Buffer) (Buffer->size - GAP_SIZE(Buffer))

void buffer_update_line_starts(Buffer *buffer);

String buffer_to_string(Buffer *buffer) {
    int64 buffer_length = buffer_get_length(buffer);
    String result{};
    result.data = (char *)malloc(buffer_length + 1);
    memcpy(result.data, buffer->text, buffer->gap_start);
    memcpy(result.data + buffer->gap_start, buffer->text + buffer->gap_end, buffer->size - buffer->gap_end);
    result.count = buffer_length;
    result.data[buffer_length] = 0;
    return result;
}

String buffer_to_string_span(Buffer *buffer, Span span) {
    String result{};
    int64 span_length = span.end - span.start;
    assert(span.start >= 0 && span.end >= 0);
    assert(span_length >= 0);
    if (span.end < buffer_get_length(buffer)) {
        result.data = (char *)malloc(span_length + 1);
        result.data[span_length] = 0;
        for (int64 i = 0; i < span_length; i++) {
            result.data[i] = buffer_at(buffer, span.start + i);
        }
    }
    result.count = span_length;
    return result;
}

String buffer_to_string_apply_line_endings(Buffer *buffer) {
    int64 buffer_length = buffer_get_length(buffer); // @todo wrong?
    int64 new_buffer_length = buffer_length + ((buffer->line_ending == LINE_ENDING_CRLF) ? buffer_get_line_count(buffer) : 0);
    String buffer_string;
    buffer_string.data = (char *)malloc(new_buffer_length + 1);
    buffer_string.count = new_buffer_length;
    char *dest = buffer_string.data;
    for (int64 position = 0; position < buffer_length; position++) {
        char c = buffer_at(buffer, position);
        assert(c >= 0 && c <= 127);
        if (c == '\n') {
            switch (buffer->line_ending) {
            case LINE_ENDING_LF:
                *dest++ = '\n';
                break;
            case LINE_ENDING_CR:
                *dest++ = '\r';
                break;
            case LINE_ENDING_CRLF:
                *dest++ = '\r';
                *dest++ = '\n';
                break;
            }
        } else {
            *dest++ = c;
        }
    }
    *dest = 0;
    buffer_string.count = dest - buffer_string.data;
    return buffer_string;
}

int64 buffer_get_line_length(Buffer *buffer, int64 line) {
    int64 length = buffer->line_starts[line + 1] - buffer->line_starts[line] - 1;
    return length;
}

int64 buffer_get_line_count(Buffer *buffer) {
    int64 result = buffer->line_starts.count - 1;
    return result;
}

int64 buffer_get_length(Buffer *buffer) {
    int64 result = BUFFER_SIZE(buffer);
    return result;
}

void remove_crlf(char *data, int64 count, char **out_data, int64 *out_count) {
    char *result = (char *)malloc(count);
    char *src = data;
    char *src_end = data + count;
    char *dest = result;
    while (src < data + count) {
        switch (*src) {
        default:
            *dest++ = *src++;
            break;
        case '\r':
            src++;
            if (src < src_end && *src == '\n') src++;
            *dest++ = '\n';
            break;
        case '\n':
            src++;
            if (src < src_end && *src == '\r') src++;
            *dest++ = '\n';
            break;
        }
    }

    int64 new_count = dest - result;
    result = (char *)realloc(result, new_count);
    if (out_data) *out_data = result;
    if (out_count) *out_count = new_count;
}

Line_Ending detect_line_ending(String string) {
    for (int64 i = 0; i < string.count; i++) {
        switch (string.data[i]) {
        case '\r':
            if (i + 1 < string.count && string.data[i + 1] == '\n') {
                return LINE_ENDING_CRLF;
            } else {
                return LINE_ENDING_CR;
            }
        case '\n':
            return LINE_ENDING_LF;
        }
    }
    return LINE_ENDING_LF;
}

Buffer *make_buffer(const char *file_name) {
    Buffer *buffer = new Buffer();
    buffer->file_name = file_name;
    buffer->gap_start = 0;
    buffer->gap_end = DEFAULT_GAP_SIZE;
    buffer->size = buffer->gap_end;
    buffer->text = (char *)malloc(buffer->size);
    buffer_update_line_starts(buffer);
    buffer->post_self_insert_hook = nullptr;
    buffer->line_ending = LINE_ENDING_LF;
    return buffer;
}

Buffer *make_buffer_from_file(const char *file_name) {
    Read_File file = open_entire_file(file_name);
    assert(file.data);
    String buffer_string = { (char *)file.data, file.count };
    Line_Ending line_ending = detect_line_ending(buffer_string);
    if (line_ending != LINE_ENDING_LF) {
        String adjusted{};
        remove_crlf(buffer_string.data, buffer_string.count, &adjusted.data, &adjusted.count);
        free(file.data);
        buffer_string = adjusted;
    }

    Buffer *buffer = new Buffer();
    buffer->file_name = file_name;
    buffer->text = buffer_string.data;
    buffer->gap_start = 0;
    buffer->gap_end = 0;
    buffer->size = buffer_string.count;
    buffer->line_ending = line_ending;
    buffer_update_line_starts(buffer);
    File_Attributes attribs = get_file_attributes(file_name);
    buffer->last_write_time = attribs.last_write_time;
    buffer->post_self_insert_hook = nullptr;
    return buffer;
}

int64 buffer_position_logical(Buffer *buffer, int64 position) {
    if (position > buffer->gap_start) {
        position -= GAP_SIZE(buffer);
    }
    return position;
}

char buffer_at(Buffer *buffer, int64 position) {
    int64 index = position;
    if (index >= buffer->gap_start) {
        index += GAP_SIZE(buffer);
    }
    char c = 0;
    if (buffer_get_length(buffer) > 0) {
       c = buffer->text[index]; 
    }
    return c;
}

void buffer_update_line_starts(Buffer *buffer) {
    buffer->line_starts.reset_count();
    buffer->line_starts.push(0);
    
    char *text = buffer->text;
    for (;;) {
        if (text >= buffer->text + buffer->size) break;
        if (PTR_IN_GAP(text, buffer)) {
            text = buffer->text + buffer->gap_end;
            continue;
        }

        bool newline = false;
        switch (*text) {
        default:
            text++;
            break;
        case '\r':
            text++;
            if (!PTR_IN_GAP(text, buffer) && (text < buffer->text + buffer->size)) {
                if (*text == '\n') text++;
            }
            newline = true;
            break;
        case '\n':
            text++;
            if (!PTR_IN_GAP(text, buffer) && (text < buffer->text + buffer->size)) {
                if (*text == '\r') text++;
            }
            newline = true;
            break;
        }
        if (newline) {
            int64 position = text - buffer->text;
            position = buffer_position_logical(buffer, position);
            buffer->line_starts.push(position);
        }
    }
    buffer->line_starts.push(BUFFER_SIZE(buffer) + 1);
}

void buffer_grow(Buffer *buffer, int64 gap_size) {
    int64 size1 = buffer->gap_start;
    int64 size2 = buffer->size - buffer->gap_end;
    char *data = (char *)calloc(buffer->size + gap_size, 1);
    memcpy(data, buffer->text, buffer->gap_start);
    memset(data + size1, '_', gap_size);
    memcpy(data + buffer->gap_start + gap_size, buffer->text + buffer->gap_end, buffer->size - buffer->gap_end);
    free(buffer->text);
    buffer->text = data;
    buffer->gap_end += gap_size;
    buffer->size += gap_size;
}

void buffer_shift_gap(Buffer *buffer, int64 new_gap) {
    char *temp = (char *)malloc(buffer->size);
    int64 gap_start = buffer->gap_start;
    int64 gap_end = buffer->gap_end;
    int64 gap_size = gap_end - gap_start;
    int64 size = buffer->size;
    if (new_gap > gap_start) {
        memcpy(temp, buffer->text, new_gap);
        memcpy(temp + gap_start, buffer->text + gap_end, new_gap - gap_start);
        memcpy(temp + new_gap + gap_size, buffer->text + new_gap + gap_size, size - (new_gap + gap_size));
    } else {
        memcpy(temp, buffer->text, new_gap);
        memcpy(temp + new_gap + gap_size, buffer->text + new_gap, gap_start - new_gap);
        memcpy(temp + new_gap + gap_size + (gap_start - new_gap), buffer->text + gap_end, size - gap_end);
    }
    free(buffer->text);
    buffer->text = temp;
    buffer->gap_start = new_gap;
    buffer->gap_end = new_gap + gap_size;
}

void buffer_ensure_gap(Buffer *buffer) {
    if (buffer->gap_end - buffer->gap_start == 0) {
        buffer_grow(buffer, DEFAULT_GAP_SIZE);
    }
}

void buffer_delete_region(Buffer *buffer, int64 start, int64 end) {
    assert(start < end);
    if (buffer->gap_start != start) {
        buffer_shift_gap(buffer, start);
    }
    buffer->gap_end += (end - start);
    buffer_update_line_starts(buffer);
}

void buffer_delete_single(Buffer *buffer, int64 position) {
    buffer_delete_region(buffer, position - 1, position);
}

void buffer_insert_single(Buffer *buffer, int64 position, char c) {
    buffer_ensure_gap(buffer);
    if (buffer->gap_start != position) {
        buffer_shift_gap(buffer, position);
    }
    buffer->text[position] = c;
    buffer->gap_start++;
    buffer_update_line_starts(buffer);
}

void buffer_insert_text(Buffer *buffer, int64 position, String string) {
    if (GAP_SIZE(buffer) < string.count) {
        buffer_grow(buffer, string.count);
    }
    memcpy(buffer->text + buffer->gap_start, string.data, string.count);
    buffer->gap_start += string.count;
    buffer_update_line_starts(buffer);
}

void buffer_replace_region(Buffer *buffer, String string, int64 start, int64 end) {
    int64 region_size = end - start;
    buffer_delete_region(buffer, start, end);
    if (GAP_SIZE(buffer) < string.count) {
        buffer_grow(buffer, string.count);
    }
    memcpy(buffer->text + buffer->gap_start, string.data, string.count);
    buffer->gap_start += string.count;
    buffer_update_line_starts(buffer);
}

void buffer_clear(Buffer *buffer) {
    buffer->gap_start = 0;
    buffer->gap_end = buffer->size;
}

Cursor get_cursor_from_position(Buffer *buffer, int64 position) {
    Cursor cursor = {};
    for (int line = 0; line < buffer->line_starts.count - 1; line++) {
        int64 start = buffer->line_starts[line];
        int64 end = buffer->line_starts[line + 1];
        if (start <= position && position < end) {
            cursor.line = line;
            cursor.col = position - start;
            break;
        }
    }
    cursor.position = position;
    return cursor;
}

int64 get_position_from_line(Buffer *buffer, int64 line) {
    int64 position = buffer->line_starts[line];
    return position;
}

Cursor get_cursor_from_line(Buffer *buffer, int64 line) {
    int64 position = get_position_from_line(buffer, line);
    Cursor cursor = get_cursor_from_position(buffer, position);
    return cursor;
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
