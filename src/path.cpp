#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <shlwapi.h>
#endif

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "path.h" 
#define IS_SLASH(C) ((C) == '/' || (C) == '\\')

char *path_join(char *left, char *right) {
    assert(left && right);
    uintptr_t len = strlen(left) + strlen(right);
    char *result = (char *)malloc(len + 1);
    result[len] = 0;
    strcpy(result, left);
    strcat(result, right);
    return result;
}

char *path_strip_extension(char *path) {
    char *result = NULL;
    assert(path);
    for (char *ptr = path + strlen(path) - 1; ptr != path; ptr--) {
        switch (*ptr) {
        case '.':
        {
            result = (char *)malloc(path + strlen(path) - ptr + 1);
            strcpy(result, path);
            return result;
        }
        case '/':
        case '\'':
            // No extension for this file
            return NULL;
        }
    }
    return NULL;
}

char *path_strip_dir_name(char *path) {
    assert(path);
    char *ptr = path + strlen(path) - 1;
    while (ptr != path) {
        if (IS_SLASH(*ptr)) {
            break;
        }
        ptr--;
    }

    char *dir_name = NULL;
    size_t len = ptr - path;
    if (len) {
        dir_name = (char *)malloc(len + 1);
        dir_name[len] = 0;
        strncpy(dir_name, path, len);
    }
    return dir_name;
}

char *path_strip_file_name(char *path) {
    assert(path);
    char *ptr = path + strlen(path) - 1;
    if (IS_SLASH(*ptr)) {
        return NULL;
    }
    
    while (ptr != path) {
        if (IS_SLASH(*ptr)) {
            return ptr + 1;
        }
        ptr--;
    }
    return NULL;
}

char *path_normalize(char *path) {
    // @todo should we append current directory if the path is relative?
    // might help with dot2 being clamped to root of path
    assert(path);
    uintptr_t len = 0;
    char *buffer = (char *)malloc(strlen(path) + 1);
    char *stream = path;

    while (*stream) {
        if (IS_SLASH(*stream)) {
            stream++;
            char *start = stream;
            bool dot = false;
            bool dot2 = false;
            if (*stream == '.') {
                stream++;
                if (*stream == '.') {
                    dot2 = true;
                    stream++;
                } else {
                    dot = true;
                }
            }

            // Check if the current dirname is actually '..' or '.' rather than some file like '.foo' or '..foo'
            if (*stream && !IS_SLASH(*stream)) {
                dot = false;
                dot2 = false;
            }

            // If not a dot dirname then just continue on like normal, reset stream to after the seperator
            if (!dot && !dot2) {
                // buffer[len++] = '/';
                stream = start;
            } else if (dot2) {
                // Walk back directory, go until seperator or beginning of stream
                // @todo probably want to just clamp to root
                char *prev = buffer + len - 1;
                while (prev != buffer && !IS_SLASH(*prev)) {
                    prev--;
                }
                len = prev - buffer;
            }
        } else {
            buffer[len++] = *stream++;
        }
    }

    char *normal = (char *)malloc(len + 1);
    strncpy(normal, buffer, len);
    normal[len] = 0;
    free(buffer);
    return normal;
}

#ifdef _WIN32
char *path_home_name() {
    char buffer[MAX_PATH];
    GetEnvironmentVariableA("USERPROFILE", buffer, MAX_PATH);
    char *result = (char *)malloc(strlen(buffer) + 1);
    strcpy(result, buffer);
    return result;
}
#elif defined(__linux__)
char *path_home_name() {
    char *result = NULL;
    if ((result = getenv("HOME")) == NULL) {
        // result = getpwuid(getuid())->pw_dir;
    }
    return result;
}
#endif

#ifdef _WIN32
char *path_current_dir() {
    DWORD length = GetCurrentDirectoryA(0, NULL);
    char *result = (char *)malloc(length);
    DWORD ret = GetCurrentDirectoryA(length, result);
    return result;
}
#elif defined(__linux__)
char *path_current_dir() {
    char *result = getcwd(NULL, 0);
    return result;
}
#endif

#ifdef _WIN32
bool path_file_exists(char *path) {
    return PathFileExistsA(path);
}
#endif

bool path_is_absolute(const char *path) {
    const char *ptr = path;
    switch (*ptr) {
#if defined(__linux__)
    case '~':
#endif
    case '\'':
    case '/':
        return true;
    }

    if (isalpha(*ptr++)) {
        if (*ptr == ':') {
            return true;
        }
    }
    return false;
}

bool path_is_relative(const char *path) {
    return !path_is_absolute(path);
}

void find_first_file() {
//     HANDLE FindFirstFileA(
//   [in]  LPCSTR             lpFileName,
//   [out] LPWIN32_FIND_DATAA lpFindFileData
// );
}
