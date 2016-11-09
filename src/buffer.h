#ifndef INCLUDE_buffer_h__
#define INCLUDE_buffer_h__

#include <stdlib.h>
#include <stdio.h>

#include "libnix/buffer.h"
#include "libnix/position.h"

struct buffer {
    struct nix_buffer *p;

    FILE *input;

    unsigned int *buffer;
    unsigned int *left;
    unsigned int *right;

    unsigned int *lexeme;
    unsigned int *read;
    unsigned int *peek;
    unsigned int *eof;

    bool at_eof;
   
    unsigned int last_read;
    unsigned int last_peek;

    bool buffer_ready[2];    
};

enum buffer_side {
    BUFFER_LEFT = 0,
    BUFFER_RIGHT = 1
};

inline enum nix_err
__ensure_ptrs_initialized(struct buffer *);

inline enum nix_err
__read(
    struct buffer *,
    unsigned int *,
    unsigned int **,
    struct nix_position *,
    unsigned int last_c);

enum nix_err
__buffer_side(struct buffer *, enum buffer_side *, unsigned int *);

bool
__at_start(struct buffer *, unsigned int *);

bool
__at_end(struct buffer *, unsigned int*);

enum nix_err
__load_buffer(struct buffer *, enum buffer_side);

enum nix_err
__buffer_occupied(struct buffer *, bool *, enum buffer_side);

void
__increment(struct buffer *, unsigned int *);

enum nix_err
__get_lexeme(struct buffer *, unsigned int **, size_t);

enum nix_err
__reset_lexeme_p(struct buffer *);

#endif
