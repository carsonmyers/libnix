#include <stdlib.h>

#include "lexeme.h"
#include "common.h"

enum nix_err
nix_lexeme__init(
    struct nix_lexeme *out,
    uint32_t *text,
    struct nix_position *start,
    struct nix_position *end)
{
    out->start = start;
    out->end = end;
    out->text = text;

    return NIXERR_NONE;
}

enum nix_err
nix_lexeme__construct(
    struct nix_lexeme **out,
    uint32_t *text,
    struct nix_position *start,
    struct nix_position *end)
{
    struct nix_lexeme *lexeme;
    ALLOC(lexeme, sizeof(struct nix_lexeme));

    TRY(nix_lexeme__init(*out, text, start, end));

    EXCEPT(err)
    return err;
}

void
nix_lexeme__free(struct nix_lexeme **out) {
    if (*out == NULL) return;

    FREE((*out)->start);
    FREE((*out)->end);
    FREE((*out)->text);
    FREE(*out)
    
    *out = NULL;
}
