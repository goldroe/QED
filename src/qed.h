#pragma once

#include "simple_math.h"
#include "array.h"
#include "types.h"
#include "buffer.h" 

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

    void *texture;
};

struct View {
    Rect rect;

    Buffer *buffer;

    Cursor cursor;

    Face *face;
};


void draw_view(View *view);

struct Input {
    bool capture_cursor;
    int cursor_px, cursor_py;
    int last_cursor_px, last_cursor_py;
    int delta_x, delta_y;
};

enum System_Event_Type {
    SYSTEM_EVENT_NONE,
    SYSTEM_EVENT_WINDOW_SIZE,
    SYSTEM_EVENT_KEY_STROKE,
    SYSTEM_EVENT_KEY_RELEASE,
    SYSTEM_EVENT_MOUSEMOVE,
    SYSTEM_EVENT_MOUSECLICK,
};

struct System_Event {
    System_Event_Type type;
};

struct Window_Size_Event : System_Event {
    Window_Size_Event() { type = SYSTEM_EVENT_WINDOW_SIZE; }
    int width;
    int height;
};

struct Text_Event : System_Event {
    char *text;
    int64 count;
};

#define KEYMOD_CONTROL (1 << 11)
#define KEYMOD_ALT     (1 << 10)
#define KEYMOD_SHIFT   (1 << 9)

#define MAX_KEY_COUNT (1 << (8 + 3))
typedef uint16 Key;

struct Key_Map {
    Key keys[MAX_KEY_COUNT];
};

enum Key_Code {
    KEY_A,
    KEY_B,
    KEY_C,
    KEY_D,
    KEY_E,
    KEY_F,
    KEY_G,
    KEY_H,
    KEY_I,
    KEY_J,
    KEY_K,
    KEY_L,
    KEY_M,
    KEY_N,
    KEY_O,
    KEY_P,
    KEY_Q,
    KEY_R,
    KEY_S,
    KEY_T,
    KEY_U,
    KEY_V,
    KEY_W,
    KEY_X,
    KEY_Y,
    KEY_Z,

    KEY_0,
    KEY_1,
    KEY_2,
    KEY_3,
    KEY_4,
    KEY_5,
    KEY_6,
    KEY_7,
    KEY_8,
    KEY_9,

    KEY_SPACE,
    KEY_COMMA,
    KEY_PERIOD,

    KEY_ENTER,
    KEY_BACKSPACE,
    KEY_LEFT,
    KEY_RIGHT,
    KEY_UP,
    KEY_DOWN,

    KEY_LEFTBRACKET,
    KEY_RIGHTBRACKET,
    KEY_SEMICOLON,
    KEY_FORWARDSLASH,
    KEY_BACKSLASH,
    KEY_MINUS,
    KEY_EQUAL,

    KEY_TAB,
    KEY_TICK,
    KEY_HOME,
    KEY_END,
    KEY_PAGEUP,
    KEY_PAGEDOWN,

    KEY_SUPER,
    KEY_CONTROL,
    KEY_ALT,
    KEY_SHFIT,
};


struct Vertex {
    v2 position;
    v2 uv;
    v4 color;
};

struct Simple_Constants {
    m4 transform;
};
