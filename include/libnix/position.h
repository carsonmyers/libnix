#ifndef INCLUDE_libnix_position_h__
#define INCLUDE_libnix_position_h__

#include "common.h"

NIX_BEGIN_DECL

struct nix_position {
    unsigned int row;
    unsigned int col;
    unsigned int abs;
};

NIX_EXTERN(enum nix_err)
nix_position__copy(struct nix_position *a, struct nix_position *b);

NIX_END_DECL

#endif