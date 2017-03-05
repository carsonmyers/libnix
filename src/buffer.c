#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>

#include "buffer.h"
#include "lexeme.h"
#include "common.h"
#include "error.h"
#include "position.h"

enum nix_err
nix_buffer__init(struct nix_buffer *out, FILE *in, size_t buffer_size) {
    struct buffer *b = (struct buffer *)out;

    ALLOC(b->buffer, sizeof(uint8_t) * buffer_size * 2);
    b->left = b->buffer;
    b->right = b->buffer + buffer_size;

    // Do a simple encoding check on the file by looking for a BOM
    b->input = in;
    TRY(__read_bom(in, &b->utf16, &b->reverse_order));

    if (b->utf16 == true) {
        b->reader = &__read_utf16;
    } else {
        b->reader = &__read_utf8;
    }

    b->p.buffer_size = buffer_size;

    b->buffer_ready[BUFFER_LEFT] = false;
    b->buffer_ready[BUFFER_RIGHT] = false;

    b->eof = NULL;
    b->at_eof = false;

    TRY(__load_buffer(b, BUFFER_LEFT));

    b->lexeme = b->left;
    b->read = b->left;
    b->peek = b->left;

    TRY(nix_position__construct(&b->p.read));
    TRY(nix_position__construct(&b->p.peek));
    TRY(nix_position__construct(&b->p.lexeme));

    b->last_lexeme = 0;
    b->last_read = 0;
    b->last_peek = 0;

    EXCEPT(err)
    return err;
}

static inline enum nix_err
__read_bom(FILE *input, bool *is_utf16, bool *reverse_order) {
    uint8_t c;

    TRY(__read_bom_byte(input, &c));

    // Detect the FEFF byte-order mark for a UTF-16 document
    if (c == u'\xFE') {
        TRY(__read_bom_byte(input, &c));

        if (c == u'\xFF') {
            *is_utf16 = true;
            *reverse_order = false;
            return NIXERR_NONE;
        }

        goto no_bom;
    }

    // Detect the FFFE reverse byte-order mark for a UTF-16 document
    if (c == u'\xFF') {
        TRY(__read_bom_byte(input, &c));

        if (c == u'\xFE') {
            *is_utf16 = true;
            *reverse_order = true;
            return NIXERR_NONE;
        }

        goto no_bom;
    }

    // Detect the EFBBBF byte-order mark for a UTF-8 document
    if (c == u'\xEF') {
        TRY(__read_bom_byte(input, &c));

        if (c == u'\xBB') {
            TRY(__read_bom_byte(input, &c));

            if (c == u'\xBF') {
                *is_utf16 = false;
                *reverse_order = false;
                return NIXERR_NONE;
            }
        }
    }

    goto no_bom;

    EXCEPT(err)
    CATCH(NIXERR_BUF_EOF) {
        goto no_bom;
    } else {
        return err;
    }

no_bom:
    *is_utf16 = false;
    *reverse_order = false;
    fseek(input, 0, SEEK_SET);

    return NIXERR_NONE;
}

static inline enum nix_err
__read_bom_byte(FILE *input, uint8_t *out) {
    size_t read = fread(out, sizeof(uint8_t), 1, input);
    if (read != 1) {
        if (feof(input)) {
            return NIXERR_BUF_EOF;
        } else {
            return NIXERR_BUF_FILE;
        }
    }

    return NIXERR_NONE;
}

enum nix_err
nix_buffer__construct(struct nix_buffer **out, FILE *in, size_t buffer_size) {
    struct buffer *b = 0;
    ALLOC(b, sizeof(struct buffer));

    TRY(nix_buffer__init((struct nix_buffer *)b, in, buffer_size));
    *out = (struct nix_buffer *)b;
    
    EXCEPT(err)
    CATCH(NIXERR_NOMEMORY)
        nix_buffer__free((struct nix_buffer **)&b);

    return err;
}

enum nix_err
nix_buffer__read(struct nix_buffer *buf, uint32_t *out) {
    struct buffer *b = (struct buffer *)buf;

    if (b->at_eof && b->read != b->eof) {
        b->at_eof = false;
    }

    uint8_t *original_ptr = b->read;
    struct nix_position original_position;
    TRY(nix_position__copy(&original_position, b->p.read));

    TRY(__read(b, out, &b->read, b->p.read, b->last_read, true));

    b->last_read = *out;
    b->last_peek = *out;
    TRY(nix_buffer__reset_peek(buf));

    EXCEPT(err)
    CATCH(NIXERR_BUF_EXHAUST) {
        b->read = original_ptr;
        nix_position__copy(b->p.read, &original_position);
    }

    nix_buffer__reset_peek(buf);
    return err;
}

enum nix_err
nix_buffer__peek(struct nix_buffer *buf, uint32_t *out) {
    struct buffer *b = (struct buffer *)buf;

    uint8_t *original_ptr = b->peek;
    struct nix_position original_position;
    TRY(nix_position__copy(&original_position, b->p.peek));

    TRY(__read(b, out, &b->peek, b->p.peek, b->last_peek, true));
    b->last_peek = *out;

    EXCEPT(err)
    CATCH(NIXERR_BUF_EXHAUST) {
        b->peek = original_ptr;
        nix_position__copy(b->p.peek, &original_position);
    }

    return err;
}

enum nix_err
__read(
    struct buffer *b,
    uint32_t *out,
    uint8_t **ptr,
    struct nix_position *ptr_meta,
    uint32_t last_c,
    bool check_bounds)
{
    uint32_t c;
    TRY(b->reader(b, &c, ptr, check_bounds));

    if (c == '\r' || (c == '\n' && last_c != '\r')) {
        ptr_meta->row += 1;
        ptr_meta->col = 0;
    } else {
        ptr_meta->col += 1;
    }

    ptr_meta->abs += 1;

    *out = c;

    EXCEPT(err)
    return err;
}

static inline enum nix_err
__read_byte(struct buffer *b, uint8_t *out, uint8_t **ptr, bool check_bounds) {
    if (check_bounds) {
        TRY(__check_bounds(b, ptr));
    }

    *out = **ptr;
    __increment(b, ptr);

    EXCEPT(err)
    return err;
}

static inline enum nix_err
__check_bounds(struct buffer *b, uint8_t **ptr) {
    enum buffer_side side;
    TRY(__buffer_side(b, &side, *ptr));

    enum buffer_side other_side = BUFFER_RIGHT;
    if (side == BUFFER_RIGHT) {
        other_side = BUFFER_LEFT;
    }

    if (*ptr == b->eof) {
        if (b->at_eof) {
            return NIXERR_BUF_PAST_EOF;
        } else {
            b->at_eof = true;
            return NIXERR_BUF_EOF;
        }
    }

    if (__at_end(b, *ptr) && !b->buffer_ready[other_side]) {
        TRY(__load_buffer(b, other_side));
    }

    EXCEPT(err)
    return err;
}

enum nix_err
__read_utf8(struct buffer *b, uint32_t *out, uint8_t **ptr, bool check) {
    uint8_t c = 0;
    TRY(__read_byte(b, &c, ptr, check));

    // The first byte indicates the number of bytes to expect in the
    // encoding, and the rest of the bytes begin with 10
    //
    //                            0uuuuuuu |                   0uuuuuuu
    //                   110xxxxx 10uuuuuu |          00000xxx xxuuuuuu
    //          1110yyyy 10xxxxxx 10uuuuuu |          yyyyxxxx xxuuuuuu
    // 11110zzz 10yyyyyy 10xxxxxx 10uuuuuu | 000zzzyy yyyyxxxx xxuuuuuu

    // If the high-order bit is 0, then this is equivalent to an ASCII byte
    if ((c & 0x80) == 0) {
        *out = c;
        return NIXERR_NONE;
    }

    // Otherwise, this character is encoded in multiple bytes
    size_t byte_count;
    TRY(__count_utf8_encoded_bytes(c, &byte_count));

    uint32_t decoded = 0;

    // The most-significant 3, 4, or 5 bits need to be masked out of the
    // first byte, depending on how many bytes are in this encoding.
    // Start with 0b11111000, shift out the unneeded bits, then
    // complement it to create a mask.
    uint8_t byte_mask = ~(0xF8 << (4 - byte_count));

    // Extract and shift in the character value
    while (byte_count > 0) {
        // Each byte after the first has 6 bits of data - so each chunk of the
        // decoded value should be shifted by a multiple of 6 bits
        uint8_t shift_amt = 6 * (byte_count - 1);
        decoded |= (c & byte_mask) << shift_amt;

        byte_count--;
        if (byte_count > 0) {
            TRY(__read_byte(b, &c, ptr, check));
            // Remaining bytes need the top 2 bits masked out
            byte_mask = 0x3F;
        }
    }

    *out = decoded;

    EXCEPT(err)
    return err;
}

static inline enum nix_err
__count_utf8_encoded_bytes(uint8_t first_byte, size_t *out) {
    // copy the byte since it will be shifted
    uint8_t b = first_byte;

    size_t count = 0;
    bool found_zero = false;
    for (size_t i = 0; i < 5; i++) {
        if ((b & 0x80) != 0) {
            count++;
        } else {
            found_zero = true;
            break;
        }

        b = b << 1;
    }

    // The single-byte case is already handled by now, and the 0x80 byte
    // (which results in a count of 1) means that this byte is in the middle
    // of an encoded character.
    if (found_zero == false || count < 2) {
        return NIXERR_BUF_INVCHAR;
    }

    *out = count;
    return NIXERR_NONE;
}

enum nix_err
__read_utf16(struct buffer *b, uint32_t *out, uint8_t **ptr, bool check) {
    uint16_t c = 0;
    TRY(__read_utf16_data(b, &c, ptr, check));
    
    // UTF-16 is encoded either as a single 16-bit value, or as a
    // surrogate pair of two values. A single value can encode code points
    // 0x0000 - 0xFFFF, except for the range 0xD800 - 0xDFFF. This range is
    // reserved for encoding surrogate pairs.
    //
    // Surrogate pairs are encoded as such:
    //
    // 110110uu uuuuuuuu |
    // 110111xx xxxxxxxx | 00000000 0000uuuu uuuuuuxx xxxxxxxx

    // Single value - just return it
    if (c < 0xD800 || c > 0xDFFF) {
        *out = c;
        return NIXERR_NONE;
    }

    // Check that the top 6 bits are valid for the first component of the
    // surrogate pair
    if ((c & 0xFC00) != 0xD800) {
        return NIXERR_BUF_INVCHAR;
    }

    *out = 0;
    *out = (c & ~0xFC00) << 10;

    // Check that the top 6 bits are valid for the second component
    // of the surrogate pair
    TRY(__read_utf16_data(b, &c, ptr, check));
    if ((c & 0xFC00) != 0xDC00) {
        return NIXERR_BUF_INVCHAR;
    }

    *out |= c & ~0xFC00;
    *out |= 0x10000;

    EXCEPT(err)
    return err;
}

static inline enum nix_err
__read_utf16_data(struct buffer *b, uint16_t *out, uint8_t **ptr, bool check) {
    uint8_t first_byte = 0;
    TRY(__read_byte(b, &first_byte, ptr, check));

    uint8_t second_byte = 0;
    TRY(__read_byte(b, &second_byte, ptr, check));

    *out = 0;
    if (b->reverse_order == false) {
        *out = first_byte << 8 | second_byte;
    } else {
        *out = second_byte << 8 | first_byte;
    }

    EXCEPT(err)
    return err;
}

enum nix_err
nix_buffer__reset_peek(struct nix_buffer *buf) {
    struct buffer *b = (struct buffer *)buf;

    if (b == NULL || b->read == NULL) {
        return NIXERR_BUF_INVPTR;
    }

    b->peek = b->read;
    b->p.peek->row = b->p.read->row;
    b->p.peek->col = b->p.read->col;
    b->p.peek->abs = b->p.read->abs;

    return NIXERR_NONE;
}

static inline enum nix_err
__buffer_side(struct buffer *b, enum buffer_side *out, uint8_t *ptr) {
    if (ptr >= b->left && ptr < b->right) {
        *out = BUFFER_LEFT;
        return NIXERR_NONE;
    } else if (ptr >= b->right && ptr < b->right + b->p.buffer_size) {
        *out = BUFFER_RIGHT;
        return NIXERR_NONE;
    } else {
        return NIXERR_BUF_INVPTR;
    }
}

static inline bool
__at_end(struct buffer *b, uint8_t *ptr) {
    size_t offset = b->p.buffer_size - 1;
    return ptr == b->left + offset || ptr == b->right + offset;
}

enum nix_err
__load_buffer(struct buffer *b, enum buffer_side side) {
    bool buf_exhausted;
    TRY(__buffer_occupied(b, &buf_exhausted, side));
    if (buf_exhausted) {
        return NIXERR_BUF_EXHAUST;
    }

    uint8_t *target;
    if (side == BUFFER_LEFT) {
        target = b->buffer;
    } else {
        target = b->right;
    }
    
    size_t i_size = sizeof(uint8_t);
    size_t count = b->p.buffer_size;
    size_t result = fread(target, i_size, count, b->input);

    if (result != count * i_size) {
        if (ferror(b->input) || !feof(b->input)) {
            return NIXERR_BUF_FILE;
        } else {
            b->eof = target + result;
        }
    }

    enum buffer_side other_side = BUFFER_LEFT;
    if (side == BUFFER_LEFT) {
        other_side = BUFFER_RIGHT;
    }

    b->buffer_ready[side] = true;
    b->buffer_ready[other_side] = false;

    EXCEPT(err)
    return err;
}

static inline enum nix_err
__buffer_occupied(struct buffer *b, bool *out, enum buffer_side side) {
    enum buffer_side test_side;

    // It's fine if the pointers are invalid, because this function is only
    // checking whether they actually point inside the buffer and is called
    // before the buffer has been completely initialized.
    enum nix_err side_err;
    side_err = __buffer_side(b, &test_side, b->lexeme);
    if (side_err == NIXERR_NONE && test_side == side) {
        *out = true;
        return NIXERR_NONE;
    }

    side_err = __buffer_side(b, &test_side, b->read);
    if (side_err == NIXERR_NONE && test_side == side) {
        *out = true;
        return NIXERR_NONE;
    }

    side_err = __buffer_side(b, &test_side, b->peek);
    if (side_err == NIXERR_NONE && test_side == side) {
        *out = true;
        return NIXERR_NONE;
    }

    *out = false;
    return NIXERR_NONE;
}

static inline void
__increment(struct buffer *b, uint8_t **ptr) {
    (*ptr)++;

    if (*ptr >= b->buffer + b->p.buffer_size * 2) {
        *ptr = b->buffer;
    }
}

enum nix_err
nix_buffer__get_lexeme(
    struct nix_buffer *buf,
    struct nix_lexeme **out,
    size_t exclude)
{
    struct buffer *b = (struct buffer *)buf;

    struct nix_lexeme *lexeme = NULL;
    uint8_t *new_ptr = NULL;

    TRY(__get_lexeme(b, &lexeme, &new_ptr, exclude));

    b->lexeme = new_ptr;
    TRY(nix_position__copy(b->p.lexeme, lexeme->end));

    *out = lexeme;

    EXCEPT(err);
    nix_lexeme__free(&lexeme);
    return err;
}

enum nix_err
nix_buffer__peek_lexeme(
    struct nix_buffer *buf,
    struct nix_lexeme **out,
    size_t exclude)
{
    struct buffer *b = (struct buffer *)buf;

    struct nix_lexeme *lexeme = NULL;
    uint8_t *new_ptr = NULL;

    TRY(__get_lexeme(b, &lexeme, &new_ptr, exclude));

    *out = lexeme;

    EXCEPT(err);
    nix_lexeme__free(&lexeme);
    return err;
}

enum nix_err
nix_buffer__discard_lexeme(struct nix_buffer *buf, size_t exclude) {
    struct buffer *b = (struct buffer *)buf;

    if (exclude == 0) {
        b->lexeme = b->read;
        TRY(nix_position__copy(b->p.lexeme, b->p.read));

        return NIXERR_NONE;
    }

    struct nix_lexeme *lexeme = NULL;
    uint8_t *new_ptr = NULL;

    TRY(__get_lexeme(b, &lexeme, &new_ptr, exclude));

    b->lexeme = new_ptr;
    TRY(nix_position__copy(b->p.lexeme, lexeme->end));

    nix_lexeme__free(&lexeme);

    EXCEPT(err)
    nix_lexeme__free(&lexeme);
    return err;
}

enum nix_err
__get_lexeme(
        struct buffer *b,
        struct nix_lexeme **out,
        uint8_t **new_ptr,
        size_t exclude)
{
    if (b == NULL || b->lexeme == NULL || b->read == NULL) {
        return NIXERR_BUF_INVPTR;
    }

    if (b->lexeme == b->read) {
        return NIXERR_BUF_INVLEN;
    }

    uint8_t *buffer_end = b->right + b->p.buffer_size;
    size_t byte_length;

    if (b->read > b->lexeme) {
        size_t byte_start_length = buffer_end - b->lexeme;
        size_t byte_end_length = b->read - b->left;
        byte_length = byte_start_length + byte_end_length;
    } else {
        byte_length = b->read - b->lexeme;
    }

    size_t length = b->p.read->abs - b->p.lexeme->abs;
    if (exclude >= length) {
        return NIXERR_BUF_INVLEN;
    }

    length -= exclude;
    TRY(__read_lexeme(b, out, new_ptr, length));

    EXCEPT(err)
    return err;
}

enum nix_err
__read_lexeme(
        struct buffer *b,
        struct nix_lexeme **out,
        uint8_t **new_ptr,
        size_t length)
{
    uint32_t *text = NULL;
    struct nix_position *start = NULL;
    struct nix_position *end = NULL;
    struct nix_lexeme *lexeme = NULL;

    ALLOC(text, sizeof(uint32_t) * length);

    TRY(nix_position__construct(&start));
    TRY(nix_position__copy(start, b->p.lexeme));

    TRY(nix_position__construct(&end));
    TRY(nix_position__copy(end, b->p.lexeme));

    *new_ptr = b->lexeme;

    for (size_t i = 0; i < length; i++) {
        TRY(__read(b, &text[i], new_ptr, end, b->last_lexeme, false));
        b->last_lexeme = text[i];
    }

    TRY(nix_lexeme__construct(&lexeme, text, start, end));

    *out = lexeme;

    EXCEPT(err)
    FREE(text);
    FREE(start);
    FREE(end);
    nix_lexeme__free(&lexeme);
    return err;
}

void
nix_buffer__free(struct nix_buffer **out) {
    if (*out == NULL) return;

    struct buffer *b = (struct buffer *)*out;

    FREE(b->buffer);
    FREE(b->p.lexeme);
    FREE(b->p.read);
    FREE(b->p.peek);
    FREE(b);

    *out = NULL;
}
