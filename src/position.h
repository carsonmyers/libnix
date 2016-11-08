#ifndef INCLUDE_position_h
#define INCLUDE_position_h

#include "libnix/position.h"
#include "common.h"

enum nix_err
nix_position__init(struct nix_position *out);

enum nix_err
nix_position__construct(struct nix_position **out);

#endif