#include <stdlib.h>
#include <time.h>

#include "common.h"
#include "libnix/buffer.h"
#include "libnix/error.h"
#include "unity/src/unity.h"
#include "src/buffer.h"
#include "test_buffer.h"

void __read_unicode_string(FILE*);

void test_init_buffer() {
    FILE *file;
    FILE_FROM_STRING(file, "test_init_buffer", (uint8_t*)"", 0);

    struct nix_buffer *b;
    b = malloc(sizeof(struct buffer));

    enum nix_err r = nix_buffer__init(b, file, 8);
    TEST_ASSERT_MESSAGE(r == NIXERR_NONE, "Could not init buffer");

    fclose(file);
}

void test_construct_buffer() {
    FILE *file;
    FILE_FROM_STRING(file, "test_construct_buffer", (uint8_t*)"", 0);

    struct nix_buffer *b;
    enum nix_err r = nix_buffer__construct(&b, file, 8);
    TEST_ASSERT_MESSAGE(r == NIXERR_NONE, "Could not construct buffer");

    fclose(file);
}

void test_read_bytes() {
    FILE *file;
    FILE_FROM_STRING(file, "test_read_bytes", (uint8_t*)"abc", 3);

    struct nix_buffer *b;
    nix_buffer__construct(&b, file, 8);

    uint32_t c;
    enum nix_err r = nix_buffer__read(b, &c);
    TEST_ASSERT_MESSAGE(r == NIXERR_NONE, "Could not read from buffer");
    TEST_ASSERT_MESSAGE(c == 'a', "Invalid value from buffer");

    r = nix_buffer__read(b, &c);
    TEST_ASSERT_MESSAGE(r == NIXERR_NONE, "Could not read from buffer");
    TEST_ASSERT_MESSAGE(c == 'b', "Invalid value from buffer");

    r = nix_buffer__read(b, &c);
    TEST_ASSERT_MESSAGE(r == NIXERR_NONE, "Could not read from buffer");
    TEST_ASSERT_MESSAGE(c == 'c', "Invalid value from buffer");

    r = nix_buffer__read(b, &c);
    TEST_ASSERT_MESSAGE(r == NIXERR_BUF_EOF, "Buffer did not detect EOF");
    
    r = nix_buffer__read(b, &c);
    TEST_ASSERT_MESSAGE(r == NIXERR_BUF_PAST_EOF,
            "Buffer did not detect reading past EOF");

    fclose(file);
}

void test_read_utf8() {
    uint8_t input[] = {
        0xF0, 0x9F, 0x98, 0x96, // Emoji
        0xC3, 0xA9, // Latin é
        0x65, 0xCC, 0x81 // ASCII e with combining ´
    };

    FILE *file;
    FILE_FROM_STRING(file, "test_read_utf8", input, sizeof(input));

    __read_unicode_string(file);

    fclose(file);
}

void test_read_utf16() {
    uint8_t input[] = {
        0xFE, 0xFF, // BOM
        0xD8, 0x3D, 0xDE, 0x16, // Emoji
        0x00, 0xE9, // Latin é
        0x00, 0x65, 0x03, 0x01 // ASCII e with combining ´
    };

    FILE *file;
    FILE_FROM_STRING(file, "test_read_utf16", input, sizeof(input));

    __read_unicode_string(file);

    fclose(file);
}

void test_read_reverse_utf16() {
    uint8_t input[] = {
        0xFF, 0xFE, // BOM
        0x3D, 0xD8, 0x16, 0xDE, // Emoji
        0xE9, 0x00, // Latin é
        0x65, 0x00, 0x01, 0x03 // ASCII e with combining ´
    };

    FILE *file;
    FILE_FROM_STRING(file, "test_read_reverse_utf16", input, sizeof(input));

    __read_unicode_string(file);

    fclose(file);
}

void __read_unicode_string(FILE *file) {
    struct nix_buffer *b;
    nix_buffer__construct(&b, file, 10);

    uint32_t c;
    enum nix_err r;

    r = nix_buffer__read(b, &c);
    TEST_ASSERT_MESSAGE(r == NIXERR_NONE, "Could not read from buffer");
    TEST_ASSERT_MESSAGE(c == 0x1F616, "Invalid value read from buffer");

    r = nix_buffer__read(b, &c);
    TEST_ASSERT_MESSAGE(r == NIXERR_NONE, "Could not read from buffer");
    TEST_ASSERT_MESSAGE(c == 0xE9, "Invalid value read from buffer");
    
    r = nix_buffer__read(b, &c);
    TEST_ASSERT_MESSAGE(r == NIXERR_NONE, "Could not read from buffer");
    TEST_ASSERT_MESSAGE(c == 'e', "Invalid value read from buffer");

    r = nix_buffer__read(b, &c);
    TEST_ASSERT_MESSAGE(r == NIXERR_NONE, "Could not read from buffer");
    TEST_ASSERT_MESSAGE(c == 0x301, "Invalid value read from buffer");

    r = nix_buffer__read(b, &c);
    TEST_ASSERT_MESSAGE(r == NIXERR_BUF_EOF, "Buffer did not detect EOF");
}

void test_discard_lexeme() {
    FILE *file;
    FILE_FROM_STRING(file, "test_discard_lexeme", (uint8_t*)"ab", 2);

    struct nix_buffer *buf;
    nix_buffer__construct(&buf, file, 8);

    struct buffer *b = (struct buffer*)buf;

    uint32_t c;
    enum nix_err r;

    nix_buffer__read(buf, &c);
    TEST_ASSERT_MESSAGE(c == 'a', "Invalid value read from buffer");
    TEST_ASSERT_MESSAGE(b->read - b->left == 1,
            "Read pointer is not 1 byte from left");
    TEST_ASSERT_MESSAGE(b->lexeme == b->left, "Lexeme pointer is not on left");

    r = nix_buffer__discard_lexeme(buf, 0);
    TEST_ASSERT_MESSAGE(r == NIXERR_NONE, "Error resetting peek pointer");
    TEST_ASSERT_MESSAGE(b->lexeme == b->read, "Lexeme pointer is not on read");
    TEST_ASSERT_MESSAGE(b->lexeme - b->left == 1,
            "Lexeme pointer is not 1 byte from left");

    fclose(file);
}

void test_read_over_buffer() {
    FILE *file;
    FILE_FROM_STRING(file, "test_read_over_buffer", (uint8_t*)"abcdef", 6);

    struct nix_buffer *buf;
    nix_buffer__construct(&buf, file, 2);

    struct buffer *b = (struct buffer*)buf;

    uint32_t c;
    enum nix_err r;

    nix_buffer__read(buf, &c);
    TEST_ASSERT_MESSAGE(c == 'a', "Invalid value read from buffer");
    TEST_ASSERT_MESSAGE(b->buffer_ready[BUFFER_LEFT] == true,
            "Buffer not ready when it should be");
    TEST_ASSERT_MESSAGE(b->buffer_ready[BUFFER_RIGHT] == false,
            "Buffer ready when it shouldn't be");

    r = nix_buffer__read(buf, &c);
    TEST_ASSERT_MESSAGE(r == NIXERR_NONE, "Could not read from buffer");
    TEST_ASSERT_MESSAGE(c == 'b', "Invalid value read from buffer");
    TEST_ASSERT_MESSAGE(b->buffer_ready[BUFFER_LEFT] == false,
            "Buffer ready when it shouldn't be");
    TEST_ASSERT_MESSAGE(b->buffer_ready[BUFFER_RIGHT] == true,
            "Buffer not ready when it should be");

    nix_buffer__read(buf, &c);
    TEST_ASSERT_MESSAGE(c == 'c', "Invalid value read from buffer");

    r = nix_buffer__read(buf, &c);
    TEST_ASSERT_MESSAGE(r == NIXERR_BUF_EXHAUST, "Buffer should be exhausted");

    nix_buffer__discard_lexeme(buf, 0);
    r = nix_buffer__read(buf, &c);
    TEST_ASSERT_MESSAGE(r == NIXERR_NONE, "Buffer should be freed up again");
    TEST_ASSERT_MESSAGE(c == 'd', "Invalid value read from buffer");

    fclose(file);
}

int main(int argc, char **argv) {
    TEST_PATH();

    srand(time(NULL));

    UNITY_BEGIN();
    RUN_TEST(test_init_buffer);
    RUN_TEST(test_construct_buffer);
    RUN_TEST(test_read_bytes);
    RUN_TEST(test_read_utf8);
    RUN_TEST(test_read_utf16);
    RUN_TEST(test_read_reverse_utf16);
    RUN_TEST(test_discard_lexeme);
    RUN_TEST(test_read_over_buffer);
    return UNITY_END();
}

