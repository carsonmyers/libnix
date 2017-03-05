#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "unity/src/unity.h"

char *test_path;

FILE* file_from_string(char *filename, uint8_t *data, size_t length) {
    FILE *writer = fopen(filename, "w+");
    if (writer == NULL) {
        return NULL;
    }

    fwrite(data, sizeof(uint8_t), length, writer);
    fclose(writer);

    FILE *reader = fopen(filename, "r");
    if (reader == NULL) {
        return NULL;
    }

    return reader;
}

void random_string(size_t length, char *out) {
    static const char charset[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    
    for (size_t i = 0; i < length; i++) {
        size_t key = rand() % (sizeof(charset) - 1);
        out[i] = charset[key];
    }
}

char* get_test_path(int argc, char **argv) {
    if (argc != 2) {
        return NULL;
    }

    return argv[1];
}

char* combine_path(char *part_1, char *part_2) {
    size_t len_1 = strlen(part_1);
    size_t len_2 = strlen(part_2);
    size_t len_slug = 9;
    size_t len = len_1 + len_2 + len_slug + 1;
    char *filename = malloc(sizeof(uint8_t) * len);
    if (filename == NULL) {
        return NULL;
    }

    char *s, *d;

    d = filename;
    s = part_1;

    size_t i;
    for (i = 0; i < len_1; i++) {
        d[i] = s[i];
    }
    
    d[len_1] = '/';
    d = filename + len_1 + 1;
    s = part_2;

    for (i = 0; i < len_2; i++) {
        d[i] = s[i];
    }

    d[len_2] = '_';
    
    d = filename + len_1 + 1 + len_2 + 1;
    random_string(len_slug, d);

    d[len_slug] = '\0';

    return filename;
}
