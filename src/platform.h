#pragma once

#include "types.h"
typedef int64 Platform_Handle;

struct Read_File {
    void *data;
    int64 count;
    Platform_Handle handle;
};

struct File_Attributes {
    uint64 creation_time;
    uint64 last_access_time;
    uint64 last_write_time;
    uint64 file_size;
};

char *read_file_string(const char *file_name);
Read_File read_entire_file(const char *file_name);
Read_File open_entire_file(const char *file_name);
File_Attributes get_file_attributes(const char *file_name);
