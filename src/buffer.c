#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "buffer.h"
#include "common.h"
#include "error.h"
#include "position.h"

enum nix_err
nix_buffer__init(struct nix_buffer *out, FILE *in, size_t buffer_size) {
    struct buffer *b = (struct buffer *)out;

    ALLOC(b->buffer, sizeof(unsigned int) * buffer_size * 2);
    b->left = b->buffer;
    b->right = b->buffer + buffer_size;

    b->input = in;

    b->p->buffer_size = buffer_size;

    b->lexeme = b->left;
    b->read = b->left;
    b->peek = b->left;
    b->eof = NULL;

    b->eof = false;
    
    b->last_read = 0;
    b->last_peek = 0;

    b->p->lexeme = NULL;
    b->p->read = NULL;
    b->p->peek = NULL;

    b->buffer_ready[BUFFER_LEFT] = false;
    b->buffer_ready[BUFFER_RIGHT] = false;

    EXCEPT(err)
    return err;
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
nix_buffer__read(
    struct nix_buffer *buf,
    unsigned int *out,
    size_t *read,
    size_t count)
{
    struct buffer *b = (struct buffer *)buf;

    TRY(__ensure_ptrs_initialized(b));

    if (b->at_eof && b->read != b->eof) {
        b->at_eof = false;
    }

    bool consume_buffer = __at_start(b, b->read);

    unsigned int *cursor = out;
    *read = 0;
    for (size_t i = 0; i < count; i++) {
        TRY(__read(b, cursor, &b->read, b->p->read, b->last_read));
        (*read)++;
        cursor++;

        if (consume_buffer) {
            enum buffer_side side;
            TRY(__buffer_side(b, &side, b->read));

            b->buffer_ready[side] = false;
        }
    }

    TRY(nix_buffer__reset_peek(buf));

    EXCEPT(err)
    CATCH(NIXERR_BUF_EOF) {
        TRY(nix_buffer__reset_peek(buf));
    }

    return err;
}

enum nix_err
nix_buffer__peek(
    struct nix_buffer *buf,
    unsigned int *out,
    size_t *read,
    size_t count)
{
    struct buffer *b = (struct buffer *)buf;

    TRY(__ensure_ptrs_initialized(b));

    unsigned int *cursor = out;
    *read = 0;
    for (size_t i = 0; i < count; i++) {
        TRY(__read(b, cursor, &b->peek, b->p->peek, b->last_peek));
        (*read)++;
        cursor++;
    }

    EXCEPT(err)
    return err;
}

inline enum nix_err
__ensure_ptrs_initialized(struct buffer *b) {
    if (b == NULL) {
        return NIXERR_BUF_INVPTR;
    }

    if (!b->read) {
        b->read = b->buffer;
        b->lexeme = b->read;
        b->peek = b->read;

        TRY(nix_position__construct(&b->p->read));
        TRY(nix_position__construct(&b->p->peek));
        TRY(nix_position__construct(&b->p->lexeme));
    }

    EXCEPT(err)
    return err;
}

inline enum nix_err
__read(
    struct buffer *b,
    unsigned int *out,
    unsigned int **ptr,
    struct nix_position *ptr_meta,
    unsigned int last_c)
{
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

    unsigned int c = **ptr;

    if (c == '\r' || (c == '\n' && last_c != '\r')) {
        ptr_meta->row += 1;
        ptr_meta->col = 0;
    } else {
        ptr_meta->col += 1;
    }

    ptr_meta->abs += 1;
    __increment(b, *ptr);

    EXCEPT(err)
    return err;
}

inline enum nix_err
nix_buffer__reset_peek(struct nix_buffer *buf) {
    struct buffer *b = (struct buffer *)buf;

    if (b == NULL || b->read == NULL) {
        return NIXERR_BUF_INVPTR;
    }

    b->peek = b->read;
    b->p->peek->row = b->p->read->row;
    b->p->peek->col = b->p->read->col;
    b->p->peek->abs = b->p->read->abs;

    return NIXERR_NONE;
}

enum nix_err
__buffer_side(struct buffer *b, enum buffer_side *out, unsigned int *ptr) {
    if (ptr >= b->left && ptr < b->right) {
        *out = BUFFER_LEFT;
        return NIXERR_NONE;
    } else if (ptr >= b->right && ptr < b->right + b->p->buffer_size) {
        *out = BUFFER_RIGHT;
        return NIXERR_NONE;
    } else {
        return NIXERR_BUF_INVPTR;
    }
}

bool
__at_start(struct buffer *b, unsigned int *ptr) {
    return ptr == b->left || ptr == b->right;
}

bool
__at_end(struct buffer *b, unsigned int *ptr) {
    return (
        ptr == b->left + b->p->buffer_size - 1 ||
        ptr == b->right + b->p->buffer_size - 1
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
    size_t count = b->p->buffer_size;
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

enum nix_err
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

void
__increment(struct buffer *b, unsigned int *ptr) {
    ptr++;

    if (ptr >= b->buffer + b->p->buffer_size * 2) {
        ptr = b->buffer;
    }
}

enum nix_err
nix_buffer__get_lexeme(struct nix_buffer *buf, unsigned int **out) {
    struct buffer *b = (struct buffer *)buf;

    unsigned int *lexeme;
    TRY(__get_lexeme(b, &lexeme));
    TRY(__reset_lexeme_p(b));

    *out = lexeme;

    EXCEPT(err);
    return err;
}

enum nix_err
nix_buffer__peek_lexeme(struct nix_buffer *buf, unsigned int **out) {
    struct buffer *b = (struct buffer *)buf;

    unsigned int *lexeme;
    TRY(__get_lexeme(b, &lexeme));

    *out = lexeme;

    EXCEPT(err);
    return err;
}

enum nix_err
nix_buffer__discard_lexeme(struct nix_buffer *buf) {
    struct buffer *b = (struct buffer *)buf;

    TRY(__reset_lexeme_p(b));

    EXCEPT(err)
    return err;
}

enum nix_err
__get_lexeme(struct buffer *b, unsigned int **out) {
    if (b == NULL || b->lexeme == NULL || b->read == NULL) {
        return NIXERR_BUF_INVPTR;
    }

    size_t length;
    unsigned int *buffer_end = b->right + b->p->buffer_size;

    if (b->read > b->lexeme) {
        length = (
            (buffer_end - b->read) +
            (b->lexeme - b->left)
        );
    } else {
        length = b->read - b->lexeme;
    }

    unsigned int *lexeme;
    ALLOC(lexeme, length);

    unsigned int *l = lexeme;
    unsigned int *r = b->read;

    if (b->read > b->lexeme) {
        while (l < buffer_end) {
            *l++ = *r++;
        }

        l = r = b->left;
    }

    while (l < b->read) {
        *l++ = *r++;
    }

    *out = lexeme;

    EXCEPT(err)
    return err;
}

enum nix_err
__reset_lexeme_p(struct buffer *b) {
    if (b == NULL || b->p == NULL || b->p->read == NULL) {
        return NIXERR_BUF_INVPTR;
    }

    TRY(nix_position__copy(b->p->lexeme, b->p->read));

    EXCEPT(err)
    return err;
}

void
nix_buffer__free(struct nix_buffer **out) {
    if (out == NULL) return;

    struct buffer *b = (struct buffer *)*out;

    FREE(b->buffer);
    FREE(b->p->lexeme);
    FREE(b->p->read);
    FREE(b->p->peek);
    FREE(b->p);
    FREE(b);

    *out = NULL;
}