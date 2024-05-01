#pragma once

#include "qed.h"
#include "types.h"
#include "array.h"

struct Vertex {
    v2 position;
    v2 uv;
    v4 color;
};

struct Render_Group {
    void *texture;
    Array<Vertex> vertices;
    Rect clip_box;
};

struct Render_Target {
    int32 width;
    int32 height;

    void *window_handle;

    Render_Group *current;
    Array<Render_Group> groups;
};

void draw_view(Render_Target *t, View *view);
