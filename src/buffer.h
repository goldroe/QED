#pragma once

#include "platform.h"
#include "types.h"
#include "array.h"

struct Buffer {
    const char *file_name;

    char *text;
    int64 gap_start;
    int64 gap_end;
    int64 size;

    Array<int64> line_starts;

    int64 last_write_time;
};

Buffer *make_buffer_from_file(const char *file_name);

int64 get_line_length(Buffer *buffer, int64 line);
int64 get_line_count(Buffer *buffer);
int64 get_buffer_length(Buffer *buffer);

char buffer_at(Buffer *buffer, int64 position);
void buffer_insert(Buffer *buffer, int64 position, char c);
void buffer_delete_single(Buffer *buffer, int64 position);
void buffer_replace_region(Buffer *buffer, String string, int64 start, int64 end);
void buffer_delete_region(Buffer *buffer, int64 start, int64 end);

Cursor get_cursor_from_position(Buffer *buffer, int64 position);
int64 get_position_from_line(Buffer *buffer, int64 line);
Cursor get_cursor_from_line(Buffer *buffer, int64 line);
