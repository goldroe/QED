#include "buffer.h"
#include "platform.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PTR_IN_GAP(Ptr, Buffer) (Ptr >= Buffer->text + Buffer->gap_start && Ptr < Buffer->text + Buffer->gap_end)
#define GAP_SIZE(Buffer) (Buffer->gap_end - Buffer->gap_start)
#define BUFFER_SIZE(Buffer) (Buffer->size - GAP_SIZE(Buffer))

void buffer_update_line_starts(Buffer *buffer);

int64 get_line_length(Buffer *buffer, int64 line) {
    int64 length = buffer->line_starts[line + 1] - buffer->line_starts[line];
    return length;
}

int64 get_line_count(Buffer *buffer) {
    int64 result = buffer->line_starts.count - 1;
    return result;
}

int64 get_buffer_length(Buffer *buffer) {
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

    int64 new_count = src - data;
    result = (char *)realloc(result, new_count);
    if (out_data) *out_data = result;
    if (out_count) *out_count = new_count;
}

Buffer *make_buffer_from_file(const char *file_name) {
    Read_File file = read_entire_file(file_name);
    assert(file.data);
    char *lf_string = nullptr;
    int64 lf_count = 0;
    remove_crlf((char *)file.data, file.count, &lf_string, &lf_count);
    Buffer *buffer = new Buffer();
    buffer->file_name = file_name;
    buffer->text = lf_string;
    buffer->gap_start = 0;
    buffer->gap_end = 0;
    buffer->size = lf_count;
    buffer_update_line_starts(buffer);
    buffer->file_handle = file.handle;
    File_Attributes attribs = get_file_attributes(file_name);
    buffer->last_write_time = attribs.last_write_time;
    free(file.data);
    return buffer;
}

int64 buffer_position_logical(Buffer *buffer, int64 position) {
    if (position > buffer->gap_start) {
        position -= GAP_SIZE(buffer);
    }
    return position;
}

void buffer_update_line_starts(Buffer *buffer) {
    buffer->line_starts.reset_count();
    buffer->line_starts.push(0);
    
    char *text = buffer->text;
    for (;;) {
        if (text > buffer->text + buffer->size) break;
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
        buffer_grow(buffer, 1024);
    }
}

void buffer_insert(Buffer *buffer, int64 position, char c) {
    buffer_ensure_gap(buffer);
    if (buffer->gap_start != position) {
        buffer_shift_gap(buffer, position);
    }
    buffer->text[position] = c;
    buffer->gap_start++;

    buffer_update_line_starts(buffer);
}

void buffer_delete(Buffer *buffer, int64 position) {
    if (buffer->gap_start != position) {
        buffer_shift_gap(buffer, position);
    }

    buffer->gap_start--;
    buffer_update_line_starts(buffer);
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

