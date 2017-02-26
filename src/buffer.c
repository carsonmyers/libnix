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
        b->reader = __read_utf16;
    } else {
        b->reader = __read_utf8;
    }

    b->p.buffer_size = buffer_size;

    b->lexeme = NULL;
    b->read = NULL;
    b->peek = NULL;
    b->eof = NULL;

    b->at_eof = false;
    
    b->last_read = 0;
    b->last_peek = 0;

    b->p.lexeme = NULL;
    b->p.read = NULL;
    b->p.peek = NULL;

    b->buffer_ready[BUFFER_LEFT] = false;
    b->buffer_ready[BUFFER_RIGHT] = false;

    EXCEPT(err)
    return err;
}

static inline enum nix_err
__read_bom(FILE *input, bool *is_utf16, bool *reverse_order) {
    uint8_t c;

    TRY(__read_bom_byte(input, &c));

    // Detect the FEFF byte-order mark for a UTF-16 document
    if (c == '\xFE') {
        TRY(__read_bom_byte(input, &c));

        if (c == '\xFF') {
            *is_utf16 = true;
            *reverse_order = false;
            return NIXERR_NONE;
        }

        goto no_bom;
    }

    // Detect the FFFE reverse byte-order mark for a UTF-16 document
    if (c == '\xFF') {
        TRY(__read_bom_byte(input, &c));

        if (c == '\xFE') {
            *is_utf16 = true;
            *reverse_order = true;
            return NIXERR_NONE;
        }

        goto no_bom;
    }

    // Detect the EFBBBF byte-order mark for a UTF-8 document
    if (c == '\xEF') {
        TRY(__read_bom_byte(input, &c));

        if (c == '\xBB') {
            TRY(__read_bom_byte(input, &c));

            if (c == '\xBF') {
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
    size_t read = fread(&out, sizeof(uint8_t), 1, input);
    if (read != 1) {
        if (feof(input)) {
            return NIXERR_BUF_EOF;
        } else {
            return NUXERR_BUF_FILE;
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

    TRY(__ensure_ptrs_initialized(b));

    if (b->at_eof && b->read != b->eof) {
        b->at_eof = false;
    }

    bool consume_buffer = __at_start(b, b->read);

    TRY(__read(b, out, &b->read, b->p.read, b->last_read));

    if (consume_buffer) {
        enum buffer_side side;
        TRY(__buffer_side(b, &side, b->read));

        b->buffer_ready[side] = false;
    }

    b->last_read = *out;
    b->last_peek = *out;
    TRY(nix_buffer__reset_peek(buf));

    EXCEPT(err)
    CATCH(NIXERR_BUF_EOF) {
        TRY(nix_buffer__reset_peek(buf));
    }

    return err;
}

enum nix_err
nix_buffer__peek(struct nix_buffer *buf, uint32_t *out) {
    struct buffer *b = (struct buffer *)buf;

    TRY(__ensure_ptrs_initialized(b));
    TRY(__read(b, out, &b->peek, b->p.peek, b->last_peek));
    b->last_peek = *out;

    EXCEPT(err)
    return err;
}

static inline enum nix_err
__ensure_ptrs_initialized(struct buffer *b) {
    if (b == NULL) {
        return NIXERR_BUF_INVPTR;
    }

    if (!b->read) {
        b->read = b->left;
        b->lexeme = b->read;
        b->peek = b->read;

        TRY(nix_position__construct(&b->p.read));
        TRY(nix_position__construct(&b->p.peek));
        TRY(nix_position__construct(&b->p.lexeme));
    }

    EXCEPT(err)
    return err;
}

enum nix_err
__read(
    struct buffer *b,
    uint32_t *out,
    uint8_t **ptr,
    struct nix_position *ptr_meta,
    uint32_t last_c)
{
    uint32_t c;
    TRY(b->reader(b, &c, ptr));

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

static inline num nix_err
__read_byte(struct buffer *b, uint8_t *out, uint8_t **ptr) {
    enum buffer_side side;
    TRY(__buffer_side(b, &side, *ptr));

    if (__at_start(b, *ptr) && !b->buffer_ready[side]) {
        TRY(__load_buffer(b, side));
    }

    if (*ptr == b->eof) {
        if (b->at_eof) {
            return NIXERR_BUF_PAST_EOF;
        } else {
            b->at_eof = true;
            return NIXERR_BUF_EOF;
        }
    }

    *out = **ptr;
    __increment(b, *ptr);

    EXCEPT(err)
    return err;
}

enum nix_err
__read_utf8(struct buffer *b, uint32_t *out, uint8_t **ptr) {
    uint8_t c;
    TRY(__read_byte(b, &c, ptr));

    // The first byte indicates the number of bytes to expect in the
    // encoding, and the rest of the bytes begin with 10
    //
    // 0uuuuuuu                             | 0uuuuuuu
    // 110uuuuu 10xxxxxx                    | 00000uuu uuxxxxxx
    // 1110uuuu 10xxxxxx 10yyyyyy           | uuuuxxxx xxyyyyyy
    // 11110uuu 10xxxxxx 10yyyyyy 10zzzzzz  | 000uuuxx xxxxyyyy yyzzzzzz

    // If the high-order bit is 0, then this is equivalent to an ASCII byte
    if (c & 0x80 == 0) {
        *out = c;
        return NIXERR_NONE;
    }

    // Otherwise, this character is encoded in multiple bytes
    size_t byte_count;
    TRY(count_utf8_encoded_bytes(c, &byte_count));

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
            TRY(__read_byte(b, &c, ptr));
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
        if (b & 0x80 != 0) {
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
__read_utf16(struct buffer *b, uint32_t *out, uint8_t **ptr) {
    uint16_t c;
    TRY(__read_utf16_data(b, &c, ptr));
    
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
    if (c & 0xFC00 != 0xD800) {
        return NIXERR_BUF_INVCHAR;
    }

    *out = 0;
    *out = c & ~0xFC00 << 10;

    // Check that the top 6 bits are valid for the second component
    // of the surrogate pair
    TRY(__read_utf16_data(b, &c, ptr));
    if (c & 0xFC00 != 0xDC00) {
        return NIXERR_BUF_INVCHAR;
    }

    *out |= c & ~0xFC00;

    EXCEPT(err)
    return err;
}

static inline enum nix_err
__read_utf16_data(struct buffer *b, uint16_t *out, uint8_t **ptr) {
    uint16_t first_byte;
    TRY(__read_byte(b, &first_byte, ptr));

    uint16_t second_byte;
    TRY(__read_byte(b, &second_byte, ptr));

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
__buffer_side(struct buffer *b, enum buffer_side *out, unsigned int *ptr) {
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
__at_start(struct buffer *b, unsigned int *ptr) {
    return ptr == b->left || ptr == b->right;
}

static inline bool
__at_end(struct buffer *b, unsigned int *ptr) {
    return (
        ptr == b->left + b->p.buffer_size - 1 ||
        ptr == b->right + b->p.buffer_size - 1
    );
}

enum nix_err
__load_buffer(struct buffer *b, enum buffer_side side) {
    bool buf_exhausted;
    TRY(__buffer_occupied(b, &buf_exhausted, side));
    if (buf_exhausted) {
        return NIXERR_BUF_EXHAUST;
    }

    unsigned int *target;
    if (side == BUFFER_LEFT) {
        target = b->buffer;
    } else {
        target = b->right;
    }
    
    size_t i_size = sizeof(unsigned int);
    size_t count = b->p.buffer_size;
    size_t result = fread(target, i_size, count, b->input);

    if (result != count * i_size) {
        if (ferror(b->input) || !feof(b->input)) {
            return NIXERR_BUF_FILE;
        } else {
            b->eof = target + result;
        }
    }

    b->buffer_ready[side] = true;

    EXCEPT(err)
    return err;
}

static inline enum nix_err
__buffer_occupied(struct buffer *b, bool *out, enum buffer_side side) {
    enum buffer_side test_side;

    TRY(__buffer_side(b, &test_side, b->lexeme));
    if (test_side == side) {
        *out = true;
        return NIXERR_NONE;
    }

    TRY(__buffer_side(b, &test_side, b->read));
    if (test_side == side) {
        *out = true;
        return NIXERR_NONE;
    }

    TRY(__buffer_side(b, &test_side, b->peek));
    if (test_side == side) {
        *out = true;
        return NIXERR_NONE;
    }

    *out = false;

    EXCEPT(err)
    return err;
}

static inline void
__increment(struct buffer *b, unsigned int *ptr) {
    ptr++;

    if (ptr >= b->buffer + b->p.buffer_size * 2) {
        ptr = b->buffer;
    }
}

enum nix_err
nix_buffer__get_lexeme(
    struct nix_buffer *buf,
    struct nix_lexeme **out,
    size_t exclude)
{
    struct buffer *b = (struct buffer *)buf;

    struct nix_lexeme *lexeme;
    TRY(__get_lexeme(b, &lexeme, exclude));
    TRY(__reset_lexeme_p(b));

    *out = lexeme;

    EXCEPT(err);
    return err;
}

enum nix_err
nix_buffer__peek_lexeme(
    struct nix_buffer *buf,
    struct nix_lexeme **out,
    size_t exclude)
{
    struct buffer *b = (struct buffer *)buf;

    struct nix_lexeme *lexeme;
    TRY(__get_lexeme(b, &lexeme, exclude));

    *out = lexeme;

    EXCEPT(err);
    return err;
}

enum nix_err
nix_buffer__discard_lexeme(struct nix_buffer *buf, size_t exclude) {
    struct buffer *b = (struct buffer *)buf;

    TRY(__reset_lexeme_p(b));

    EXCEPT(err)
    return err;
}

enum nix_err
__get_lexeme(struct buffer *b, struct nix_lexeme **out, size_t exclude) {
    if (b == NULL || b->lexeme == NULL || b->read == NULL) {
        return NIXERR_BUF_INVPTR;
    }

    if (b->lexeme == b->read) {
        return NIXERR_BUF_INVLEN;
    }

    struct nix_position *lexeme_pos = NULL;
    TRY(nix_position__construct(&lexeme_pos));
    TRY(nix_position__copy(lexeme_pos, b->p.lexeme))
    
    unsigned int *buffer_end = b->right + b->p.buffer_size;
    size_t length;
    size_t start_length;
    size_t end_length;

    if (b->read > b->lexeme) {
        start_length = buffer_end - b->lexeme;
        end_length = b->read - b->left;
        length = start_length + end_length;
    } else {
        start_length = b->read - b->lexeme;
        end_length = 0;
        length = start_length;
    }

    if (exclude >= length) {
        return NIXERR_BUF_INVLEN;
    } else if (exclude > end_length) {
        start_length -= exclude - end_length;
        end_length = 0;
    } else {
        end_length -= exclude;
    }

    length -= exclude;

    unsigned int *text;
    ALLOC(text, length);

    unsigned int *l = text;
    unsigned int *r = b->lexeme;

    for (size_t i = 0; i < start_length; i++) {
        *l++ = *r++;
    }

    r = b->left;
    for (size_t i = 0; i < end_length; i++) {
        *l++ = *r++;
    }

    struct nix_lexeme *lexeme;
    TRY(nix_lexeme__construct(&lexeme, text, lexeme_pos, length));

    *out = lexeme;

    EXCEPT(err)
    return err;
}

enum nix_err
__reset_lexeme_p(struct buffer *b) {
    if (b == NULL || b->p.read == NULL) {
        return NIXERR_BUF_INVPTR;
    }

    TRY(nix_position__copy(b->p.lexeme, b->p.read));

    EXCEPT(err)
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
