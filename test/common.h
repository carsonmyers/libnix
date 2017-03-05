#include <stdlib.h>
#include <stdio.h>

#include "libnix/common.h"
#include "unity/src/unity.h"

NIX_BEGIN_DECL

#ifndef INCLUDE_test_common_h__
#define INCLUDE_test_common_h__

extern char *test_path;

FILE* file_from_string(char*, uint8_t*, size_t);

void random_string(size_t, char*);

char* get_test_path(int, char**);

char* combine_path(char*, char*);

#define TEST_PATH() \
    test_path = get_test_path(argc, argv); \
    if (test_path == NULL) { \
        printf("Invalid test options\n"); \
        return 1; \
    }

#define FILE_FROM_STRING(file, name, data, length) \
    char *filename = combine_path(test_path, name); \
    if (filename == NULL) { \
        TEST_ASSERT_MESSAGE(1 == 0, "Out of memory"); \
    } \
    file = file_from_string(filename, data, length); \
    if (file == NULL) { \
        TEST_ASSERT_MESSAGE(1 == 0, "Failed to open temp file"); \
    } \
    free(filename);

NIX_END_DECL

#endif
