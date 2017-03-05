#ifndef INCLUDE_libnix_position_h__
#define INCLUDE_libnix_position_h__

#include <stdlib.h>

#include "libnix/common.h"

NIX_BEGIN_DECL

struct nix_position {
    size_t row;
    size_t col;
    size_t abs;
};

NIX_EXTERN(enum nix_err)
nix_position__copy(struct nix_position *a, struct nix_position *b);

NIX_END_DECL

#endif
