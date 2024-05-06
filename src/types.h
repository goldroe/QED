#pragma once

#include <stdint.h>
typedef uint8_t  uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;
typedef int8_t  int8;
typedef int16_t int16;
typedef int32_t int32;
typedef int64_t int64;
typedef float float32;
typedef double float64;

struct Rect {
    float x0;
    float y0;
    float x1;
    float y1;
};

struct Cursor {
    int64 position;
    int64 line;
    int64 col;
};

struct Span {
    int64 start;
    int64 end;
};

