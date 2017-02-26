#ifndef INCLUDE_buffer_h__
#define INCLUDE_buffer_h__

#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>

#include "libnix/buffer.h"

struct buffer {
    struct nix_buffer p;

    FILE *input;
    bool utf16;
    bool reverse_order;

    enum nix_err (*reader)(struct buffer *, uint32_t *, uint8_t **);

    uint8_t *buffer;
    uint8_t *left;
    uint8_t *right;

    uint8_t *lexeme;
    uint8_t *read;
    uint8_t *peek;
    uint8_t *eof;

    bool at_eof;

    uint32_t last_read;
    uint32_t last_peek;

    bool buffer_ready[2];
};

enum buffer_side {
    BUFFER_LEFT = 0,
    BUFFER_RIGHT = 1
};

static inline enum nix_err
__read_bom(FILE *, bool *, bool *);

static inline enum nix_err
__read_bom_byte(FILE *, uint8_t *);

static inline enum nix_err
__ensure_ptrs_initialized(struct buffer *);

enum nix_err
__read(
    struct buffer *,
    uint32_t *,
    uint8_t **,
    struct nix_position *,
    uint32_t last_c);

static inline enum nix_err
__read_byte(struct buffer *, uint8_t *, uint8_t **);

enum nix_err
__read_utf8(struct buffer *, uint32_t *, uint8_t **);

static inline enum nix_err
__count_utf8_encoded_bytes(uint8_t, size_t *);

enum nix_err
__read_utf16(struct buffer *, uint32_t *, uint8_t **);

static inline enum nix_err
__read_utf16_data(struct buffer *, uint16_t *, uint8_t **);

static inline enum nix_err
__buffer_side(struct buffer *, enum buffer_side *, unsigned int *);

static inline bool
__at_start(struct buffer *, unsigned int *);

static inline bool
__at_end(struct buffer *, unsigned int*);

enum nix_err
__load_buffer(struct buffer *, enum buffer_side);

static inline enum nix_err
__buffer_occupied(struct buffer *, bool *, enum buffer_side);

static inline void
__increment(struct buffer *, unsigned int *);

enum nix_err
__get_lexeme(struct buffer *, struct nix_lexeme **, size_t);

enum nix_err
__reset_lexeme_p(struct buffer *);

#endif
