#pragma once

#include "simple_math.h"

struct Vertex {
    v2 position;
    v2 uv;
    v4 color;
};

struct Glyph {
    float ax;
    float ay;
    float bx;
    float by;
    float bt;
    float bl;
    float to;
};

struct Face {
    char *font_name;
    Glyph glyphs[256];

    int width;
    int height;
    int max_bmp_height;
    float ascend;
    float descend;
    int bbox_height;
    float glyph_width;
    float glyph_height;
    ID3D11ShaderResourceView *texture;
};

struct Shader {
    char *name;
    ID3D11InputLayout *input_layout;
    ID3D11VertexShader *vertex_shader;
    ID3D11PixelShader *pixel_shader;
};

struct Simple_Constants {
    m4 transform;
};

