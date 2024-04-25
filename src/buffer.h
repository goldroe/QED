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

    Platform_Handle file_handle;
    int64 last_write_time;
};

void buffer_insert(Buffer *buffer, int64 position, char c);
void buffer_delete(Buffer *buffer, int64 position);
Buffer *make_buffer_from_file(const char *file_name);
int64 get_line_length(Buffer *buffer, int64 line);
int64 get_line_count(Buffer *buffer);
