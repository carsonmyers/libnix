#ifndef INCLUDE_libnix_error_h__
#define INCLUDE_libnix_error_h__

#include "libnix/common.h"

NIX_BEGIN_DECL

enum nix_err {
    NIXERR_NONE = 0,
    NIXERR_NOMEMORY,

    NIXERR_BUF = 0x0100,
    NIXERR_BUF_INVPTR,
    NIXERR_BUF_INVLEN,
    NIXERR_BUF_EXHAUST,
    NIXERR_BUF_INVCHAR,
    NIXERR_BUF_FILE,
    NIXERR_BUF_EOF,
    NIXERR_BUF_PAST_EOF
};

NIX_END_DECL

#endif
