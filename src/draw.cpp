#include "draw.h"
#include "types.h"
#include "qed.h"

float get_string_width(Face *face, char *str, int64 count) {
    float result = 0.0f;
    for (int64 i = 0; i < count; i++) {
        char c = str[i];
        Glyph *glyph = &face->glyphs[c];
        result += glyph->ax;
    }
    return result;
}

void draw__begin_group(Render_Target *t) {
    Render_Group g{};
    t->groups.push(g);
    t->current = &t->groups[t->groups.count - 1];
}

void draw__set_texture(Render_Target *t, void *texture) {
    if (!t->current) {
        draw__begin_group(t);
    }

    Render_Group *group = t->current;
    group->texture = texture;
}

void draw_push_vertex(Render_Target *t, Vertex v) {
    if (!t->current) {
        draw__begin_group(t);
    }
    t->current->vertices.push(v);
}

void draw_vertex(Render_Target *t, float x, float y, float u, float v, v4 color) {
    Vertex vertex;
    vertex.position.x = x;
    vertex.position.y = y;
    vertex.uv.x = u;
    vertex.uv.y = v;
    vertex.color = color;
    draw_push_vertex(t, vertex);
}

void draw_rectangle(Render_Target *t, Rect rect, v4 color) {
    draw_vertex(t, rect.x0, rect.y0, 0.0f, 0.0f, color);
    draw_vertex(t, rect.x0, rect.y1, 0.0f, 0.0f, color);
    draw_vertex(t, rect.x1, rect.y1, 0.0f, 0.0f, color);
    draw_vertex(t, rect.x0, rect.y0, 0.0f, 0.0f, color);
    draw_vertex(t, rect.x1, rect.y1, 0.0f, 0.0f, color);
    draw_vertex(t, rect.x1, rect.y0, 0.0f, 0.0f, color);
}

void draw_string(Render_Target *t, Face *face, v2 offset, char *string, int64 count, v4 text_color) {
    draw__set_texture(t, face->texture);
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
        float y0 = cursor.y - glyph->bt + face->ascend - offset.y;
        float y1 = y0 + glyph->by; 

        float tw = glyph->bx / (float)face->width;
        float th = glyph->by / (float)face->height;
        float tx = glyph->to;
        float ty = 0.0f;

        draw_vertex(t, x0, y1, tx,      ty + th, text_color);
        draw_vertex(t, x0, y0, tx,      ty,      text_color);
        draw_vertex(t, x1, y0, tx + tw, ty,      text_color);
        draw_vertex(t, x0, y1, tx,      ty + th, text_color);
        draw_vertex(t, x1, y0, tx + tw, ty,      text_color);
        draw_vertex(t, x1, y1, tx + tw, ty + th, text_color);

        cursor.x += glyph->ax;
    }
}

inline v4 argb_to_v4(uint32 argb) {
    v4 result;
    result.x = (float)(argb >> 24 & 0xFF) / 255.0f;
    result.y = (float)(argb >> 16 & 0xFF) / 255.0f;
    result.z = (float)(argb >> 8 & 0xFF) / 255.0f;
    result.w = (float)(argb & 0xFF) / 255.0f;
    return result;
}

inline v4 theme_color(Theme *theme, Theme_Color color) {
    v4 result = argb_to_v4(theme->colors[color]);
    return result;
}

void draw_view(Render_Target *t, View *view) {
    draw_rectangle(t, view->rect, theme_color(view->theme, THEME_COLOR_BACKGROUND));

    if (view->mark_active) {
        Cursor start = view->mark;
        Cursor end = view->cursor;
        if (start.position > end.position) {
            Cursor temp = start;
            start = end;
            end = temp;
        }

        float line_height = view->face->glyph_height;

        float line_x = 0.0f;
        float line_y = line_height * start.line - view->y_off;

        // draw first line
        int64 line_end = view->buffer->line_starts[start.line + 1];
        float line_width = 0.0f;
        for (int64 p = view->buffer->line_starts[start.line]; p < start.position; p++) {
            char c = buffer_at(view->buffer, p);
            Glyph *g = &view->face->glyphs[c];
            line_x += g->ax;
        }
        if (start.line < end.line) {
            for (int64 position = start.position ; position < line_end; position++) {
                char c = buffer_at(view->buffer, position);
                Glyph *g = &view->face->glyphs[c];
                line_width += g->ax;
            }
            Rect line_rect = { line_x, line_y, line_x + line_width, line_y + line_height };
            draw_rectangle(t, line_rect, theme_color(view->theme, THEME_COLOR_REGION));
        }

        // draw whole lines
        for (int64 line = start.line + 1; line < end.line; line++) {
            line_width = 0.0f;
            line_y = line_height * line - view->y_off;
            line_end = view->buffer->line_starts[line + 1];
            for (int64 position = view->buffer->line_starts[line]; position < line_end; position++) {
                char c = buffer_at(view->buffer, position);
                Glyph *g = &view->face->glyphs[c];
                line_width += g->ax;
            }
            Rect line_rect = { 0.0f, line_y, line_width, line_y + line_height };
            draw_rectangle(t, line_rect, theme_color(view->theme, THEME_COLOR_REGION));
        }

        // draw remainder line
        line_width = 0.0f;
        line_y = line_height * end.line - view->y_off;
        for (int64 position = view->buffer->line_starts[end.line]; position < end.position; position++) {
            char c = buffer_at(view->buffer, position);
            Glyph *g = &view->face->glyphs[c];
            line_width += g->ax;
        }
        Rect line_rect = { 0.0f, line_y, line_width, line_y + line_height };
        draw_rectangle(t, line_rect, theme_color(view->theme, THEME_COLOR_REGION));
    }

    String buffer_string = buffer_to_string(view->buffer);

    draw_string(t, view->face, V2(0.0f, (float)view->y_off), buffer_string.data, buffer_string.count, theme_color(view->theme, THEME_COLOR_DEFAULT));

    float cw = get_string_width(view->face, buffer_string.data + view->cursor.position, 1);
    if (cw == 0) cw = view->face->glyph_width;
    float cx = get_string_width(view->face, buffer_string.data + view->buffer->line_starts[view->cursor.line], view->cursor.col);
    float cy = view->cursor.line * view->face->glyph_height - view->y_off;
    Rect rc = { cx, cy, cx + cw, cy + view->face->glyph_height };
    draw_rectangle(t, rc, theme_color(view->theme, THEME_COLOR_CURSOR));

    free(buffer_string.data);
}
