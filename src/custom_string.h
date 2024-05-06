#pragma once

#include "types.h"

struct String {
    char *data;
    int64 count;
};

#define STRZ(String) { (String), (int64)strlen(String) }

String string_copy(const char *str);
String string_reserve(int64 count);
String string_copy(String str);
void string_concat(String *str1, String str2);

