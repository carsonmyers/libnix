#ifndef INCLUDE_common_h__
#define INCLUDE_common_h__

#include <stdlib.h>
#include "libnix/common.h"
#include "libnix/error.h"

#include "error.h"

#define ALLOC(dst, size) if (!(dst = malloc(size))) { \
   __nix_local_err = NIXERR_NOMEMORY; \
   goto except; }

#define FREE(ptr) if (ptr != NULL) free(ptr);

#endif