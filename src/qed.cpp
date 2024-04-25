#include "qed.h"
#include "buffer.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

extern Array<Vertex> vertices;

int64 get_line_length(Buffer *buffer, int64 line) {
    int64 length = buffer->line_starts[line + 1] - buffer->line_starts[line];
    return length;
}

float get_string_width(Face *face, char *str, int64 count) {
    float result = 0.0f;
    for (int64 i = 0; i < count; i++) {
        char c = str[i];
        Glyph *glyph = &face->glyphs[c];
        result += glyph->ax;
    }
    return result;
}

void draw_vertex(float x, float y, float u, float v, v4 color) {
    Vertex vertex;
    vertex.position.x = x;
    vertex.position.y = y;
    vertex.uv.x = u;
    vertex.uv.y = v;
    vertex.color = color;
    vertices.push(vertex);
}

void draw_rectangle(Rect rect, v4 color) {
    draw_vertex(rect.x0, rect.y0, 0.0f, 0.0f, color);
    draw_vertex(rect.x0, rect.y1, 0.0f, 0.0f, color);
    draw_vertex(rect.x1, rect.y1, 0.0f, 0.0f, color);
    draw_vertex(rect.x0, rect.y0, 0.0f, 0.0f, color);
    draw_vertex(rect.x1, rect.y1, 0.0f, 0.0f, color);
    draw_vertex(rect.x1, rect.y0, 0.0f, 0.0f, color);
}

void draw_string(Face *face, char *string, int64 count, v4 text_color) {
    v2 cursor = V2(0.0f, 0.0f);
    for (int64 i = 0; i < count; i++) {
        char c = string[i];
        if (c == '\n') {
            cursor.x = 0.0f;
            cursor.y += face->glyph_height;
            continue;
        }

        Glyph *glyph = face->glyphs + c;
        float x0 = cursor.x + glyph->bl;
        float x1 = x0 + glyph->bx;
        float y0 = cursor.y - glyph->bt + face->ascend;
        float y1 = y0 + glyph->by; 

        float tw = glyph->bx / (float)face->width;
        float th = glyph->by / (float)face->height;
        float tx = glyph->to;
        float ty = 0.0f;

        draw_vertex(x0, y1, tx,      ty + th, text_color);
        draw_vertex(x0, y0, tx,      ty,      text_color);
        draw_vertex(x1, y0, tx + tw, ty,      text_color);
        draw_vertex(x0, y1, tx,      ty + th, text_color);
        draw_vertex(x1, y0, tx + tw, ty,      text_color);
        draw_vertex(x1, y1, tx + tw, ty + th, text_color);

        cursor.x += glyph->ax;
    }
}

void draw_view(View *view) {
    v4 bg_color = V4(0.12f, 0.12f, 0.12f, 1.0f);
    v4 text_color = V4(0.73f, 0.69f, 0.72f, 1.0f);
    v4 cursor_color = V4(1.0f, 1.0f, 1.0f, 1.0f);
    draw_rectangle(view->rect, bg_color);

    int64 count = view->buffer->size - (view->buffer->gap_end - view->buffer->gap_start);
    char *string = (char *)malloc(count);
    memcpy(string, view->buffer->text, view->buffer->gap_start);
    memcpy(string + view->buffer->gap_start, view->buffer->text + view->buffer->gap_end, view->buffer->size - view->buffer->gap_end);

    draw_string(view->face, string, count, text_color);

    float cw = get_string_width(view->face, string + view->cursor.position, 1);
    float cx = get_string_width(view->face, string + view->buffer->line_starts[view->cursor.line], view->cursor.col);
    float cy = view->cursor.line * view->face->glyph_height;
    Rect rc = { cx, cy, cx + cw, cy + view->face->glyph_height };
    draw_rectangle(rc, cursor_color);

    free(string);
}
