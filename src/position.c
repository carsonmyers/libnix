#include "position.h"
#include "common.h"

enum nix_err
nix_position__init(struct nix_position *out) {
    out->row = 1;
    out->col = 1;
    out->abs = 0;

    return NIXERR_NONE;
}

enum nix_err
nix_position__construct(struct nix_position **out) {
    struct nix_position *p = NULL;
    ALLOC(p, sizeof(struct nix_position));

    TRY(nix_position__init(p))

    *out = p;

    EXCEPT(err)
    return err;
}

enum nix_err
nix_position__copy(struct nix_position *a, struct nix_position *b) {
    if (a == NULL || b == NULL) {
        return NIXERR_BUF_INVPTR;
    }

    a->row = b->row;
    a->col = b->col;
    a->abs = b->abs;

    return NIXERR_NONE;
}