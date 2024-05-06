#pragma once

#include "platform.h"
#include "types.h"
#include "array.h"

struct Text_Input;
typedef void (*Self_Insert_Hook)(Text_Input *);

enum Line_Ending {
    LINE_ENDING_NONE,
    LINE_ENDING_LF,
    LINE_ENDING_CR,
    LINE_ENDING_CRLF,
};

enum Edit_Record_Type {
    EDIT_RECORD_INSERT,
    EDIT_RECORD_DELETE,
    EDIT_RECORD_REPLACE,
};

struct Edit_Record {
    Edit_Record_Type type;
    Span span;
    String text;
    Edit_Record *prev;
};

struct Buffer {
    const char *file_name;

    char *text;
    int64 gap_start;
    int64 gap_end;
    int64 size;

    Array<int64> line_starts;

    Line_Ending line_ending;
    int64 last_write_time;
    Self_Insert_Hook post_self_insert_hook;

    Edit_Record *edit_history = nullptr;
};

Buffer *make_buffer(const char *file_name);
Buffer *make_buffer_from_file(const char *file_name);

int64 buffer_get_line_length(Buffer *buffer, int64 line);
int64 buffer_get_line_count(Buffer *buffer);
int64 buffer_get_length(Buffer *buffer);

char buffer_at(Buffer *buffer, int64 position);
void buffer_insert_single(Buffer *buffer, int64 position, char c);
void buffer_insert_text(Buffer *buffer, int64 position, String text);
void buffer_delete_single(Buffer *buffer, int64 position);
void buffer_replace_region(Buffer *buffer, String string, int64 start, int64 end);
void buffer_delete_region(Buffer *buffer, int64 start, int64 end);
void buffer_clear(Buffer *buffer);

Cursor get_cursor_from_position(Buffer *buffer, int64 position);
int64 get_position_from_line(Buffer *buffer, int64 line);
Cursor get_cursor_from_line(Buffer *buffer, int64 line);

String buffer_to_string(Buffer *buffer);
String buffer_to_string_apply_line_endings(Buffer *buffer);
String buffer_to_string_span(Buffer *buffer, Span span);

void buffer_record_insert(Buffer *buffer, int64 position, String text);
void buffer_record_delete(Buffer *buffer, int64 start, int64 end);
