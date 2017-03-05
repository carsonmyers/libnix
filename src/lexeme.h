#ifndef INCLUDE_lexeme_h__
#define INCLUDE_lexeme_h__

#include <stdlib.h>
#include "libnix/lexeme.h"

enum nix_err
nix_lexeme__init(
    struct nix_lexeme *out,
    unsigned int *lexeme,
    struct nix_position *start,
    struct nix_position *end);

enum nix_err
nix_lexeme__construct(
    struct nix_lexeme **out,
    unsigned int *lexeme,
    struct nix_position *start,
    struct nix_position *end);

#endif
