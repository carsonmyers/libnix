#include <stdlib.h>

#include "lexeme.h"
#include "common.h"

enum nix_err
nix_lexeme__init(
    struct nix_lexeme *out,
    unsigned int *text,
    struct nix_position *position,
    size_t length)
{
    out->position = position;
    out->length = length;
    out->text = text;

    return NIXERR_NONE;
}

enum nix_err
nix_lexeme__construct(
    struct nix_lexeme **out,
    unsigned int *text,
    struct nix_position *position,
    size_t length)
{
    struct nix_lexeme *lexeme;
    ALLOC(lexeme, sizeof(struct nix_lexeme));

    TRY(nix_lexeme__init(*out, text, position, length));

    EXCEPT(err)
    return err;
}

void
nix_lexeme__free(struct nix_lexeme **out) {
    if (*out == NULL) return;

    FREE((*out)->position);
    FREE((*out)->text);
    FREE(*out)
    
    *out = NULL;
}
