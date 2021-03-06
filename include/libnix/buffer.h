#ifndef INCLUDE_libnix_buffer_h__
#define INCLUDE_libnix_buffer_h__

#include "libnix/common.h"
#include "libnix/lexeme.h"
#include "libnix/position.h"

NIX_BEGIN_DECL

struct nix_buffer {
    size_t buffer_size;

    struct nix_position *lexeme;
    struct nix_position *read;
    struct nix_position *peek;
};

NIX_EXTERN(enum nix_err)
nix_buffer__init(struct nix_buffer *out, FILE *in, size_t buffer_size);

NIX_EXTERN(enum nix_err)
nix_buffer__construct(struct nix_buffer **out, FILE *in, size_t buffer_size);

NIX_EXTERN(enum nix_err)
nix_buffer__read(struct nix_buffer *buf, uint32_t *out);

NIX_EXTERN(enum nix_err)
nix_buffer__peek(struct nix_buffer *buf, uint32_t *out);

NIX_EXTERN(enum nix_err)
nix_buffer__reset_peek(struct nix_buffer *buf);

NIX_EXTERN(enum nix_err)
nix_buffer__get_lexeme(
    struct nix_buffer *buf,
    struct nix_lexeme **out,
    size_t exclude);

NIX_EXTERN(enum nix_err)
nix_buffer__peek_lexeme(
    struct nix_buffer *buf,
    struct nix_lexeme **out,
    size_t exclude);

NIX_EXTERN(enum nix_err)
nix_buffer__discard_lexeme(struct nix_buffer *buf, size_t exclude);

NIX_EXTERN(void)
nix_buffer__free(struct nix_buffer **out);

NIX_END_DECL

#endif
