#pragma once

#include "simple_math.h"

enum Keycode {
    Key_Begin = 0,
    
    Key_Enter  = 0x0D,
    Key_Shift  = 0x10,
    Key_Ctrl   = 0x11,
    Key_Alt    = 0x12,

    Key_Escape = 0x1B,

    Key_Left = 0x25,
    Key_Up,
    Key_Right,
    Key_Down,

    Key_0 = 0x30,
    Key_1,
    Key_2,
    Key_3,
    Key_4,
    Key_5,
    Key_6,
    Key_7,
    Key_8,
    Key_9,

    Key_A = 0x41,
    Key_B,
    Key_C,
    Key_D,
    Key_E,
    Key_F,
    Key_G,
    Key_H,
    Key_I,
    Key_J,
    Key_K,
    Key_L,
    Key_M,
    Key_N,
    Key_O,
    Key_P,
    Key_Q,
    Key_R,
    Key_S,
    Key_T,
    Key_U,
    Key_V,
    Key_W,
    Key_X,
    Key_Y,
    Key_Z,

    Key_Last,
};

struct Read_File {
    void *data;
    int64 count;
    HANDLE handle;
};

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

struct Rect {
    float x0;
    float y0;
    float x1;
    float y1;
};

struct Buffer {
    const char *file_name;
    char *text;

    int64 gap;
    int64 gap_end;
    int64 end;

    HANDLE file_handle;
    int64 last_write_time;
};

struct View {
    Rect rect;

    Buffer *buffer;
    int64 cursor;

    Face *face;
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

enum Input_Event_Type {
    INPUT_EVENT_NONE,
    INPUT_EVENT_RESIZE,
    INPUT_EVENT_KEY_STROKE,
    INPUT_EVENT_KEY_RELEASE,
    INPUT_EVENT_MOUSEMOVE,
    INPUT_EVENT_MOUSECLICK,
};

struct Input {
    bool capture_cursor;
    int cursor_px, cursor_py;
    int last_cursor_px, last_cursor_py;
    int delta_x, delta_y;
};

struct Input_Event {
    Input_Event_Type type;
};

struct Key_Event : Input_Event {
    Keycode key;
    int modifiers;
};

struct Text_Event : Input_Event {
    char *text;
    int64 count;
};

#define KEYCODE_NAMES() \
    KEY(Key_A, "A"), \
    KEY(Key_B, "B"), \
    KEY(Key_C, "C"), \
    KEY(Key_D, "D"), \
    KEY(Key_E, "E"), \
    KEY(Key_F, "F"), \
    KEY(Key_G, "G"), \
    KEY(Key_H, "H"), \
    KEY(Key_I, "I"), \
    KEY(Key_J, "J"), \
    KEY(Key_K, "K"), \
    KEY(Key_L, "L"), \
    KEY(Key_M, "M"), \
    KEY(Key_N, "N"), \
    KEY(Key_O, "O"), \
    KEY(Key_P, "P"), \
    KEY(Key_Q, "Q"), \
    KEY(Key_R, "R"), \
    KEY(Key_S, "S"), \
    KEY(Key_T, "T"), \
    KEY(Key_U, "U"), \
    KEY(Key_V, "V"), \
    KEY(Key_W, "W"), \
    KEY(Key_X, "X"), \
    KEY(Key_Y, "Y"), \
    KEY(Key_Z, "Z"), \
    \
    KEY(Key_0, "0"), \
    KEY(Key_1, "1"), \
    KEY(Key_2, "2"), \
    KEY(Key_3, "3"), \
    KEY(Key_4, "4"), \
    KEY(Key_5, "5"), \
    KEY(Key_6, "6"), \
    KEY(Key_7, "7"), \
    KEY(Key_8, "8"), \
    KEY(Key_9, "9"), \
    \
    KEY(Key_Space, " "), \
    KEY(Key_Comma, ","), \
    KEY(Key_Period, "."), \
    KEY(Key_Asterisk, "*"), \
