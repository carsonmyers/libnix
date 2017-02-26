#ifndef INCLUDE_libnix_lexeme_h__
#define INCLUDE_libnix_lexeme_h__

#include <stdlib.h>

#include "libnix/common.h"
#include "libnix/position.h"

NIX_BEGIN_DECL

struct nix_lexeme {
    struct nix_position *position;
    size_t length;

    unsigned int *text;
};

NIX_EXTERN(void)
nix_lexeme__free(struct nix_lexeme **out);

NIX_END_DECL

#endif