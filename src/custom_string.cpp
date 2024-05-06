#include "custom_string.h"

#include <string.h>
#include <stdlib.h>

String string_copy(const char *str) {
    size_t len = strlen(str);
    String result;
    result.data = (char *)malloc(len + 1);
    memcpy(result.data, str, len);
    result.data[len] = 0;
    result.count = len;
    return result;
}

String string_reserve(int64 count) {
    String result;
    result.data = (char *)malloc(count + 1);
    memset(result.data, 0, count + 1);
    result.count = count;
    return result;
}

String string_copy(String str) {
    String result;
    result.data = (char *)malloc(str.count + 1);
    memcpy(result.data, str.data, str.count + 1);
    result.count = str.count;
    return result;
}

void string_concat(String *str1, String str2) {
    char *temp = str1->data;
    int64 new_count = str1->count + str2.count;
    str1->data = (char *)malloc(new_count + 1);
    memcpy(str1->data, temp, str1->count);
    memcpy(str1->data + str1->count, str2.data, str2.count);
    str1->data[new_count] = 0;
    str1->count += str2.count;
    free(temp);
}
