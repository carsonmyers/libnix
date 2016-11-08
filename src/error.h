#ifndef INCLUDE_error_h__
#define INCLUDE_error_h__

#include "libnix/error.h"

static enum nix_err __nix_local_err;

#define TRY(stmt) if ((__nix_local_err = stmt)) goto except;
#define EXCEPT(err) return NIXERR_NONE; \
    enum nix_err err; \
    except: err = __nix_local_err;
#define CATCH(type) if (__nix_local_err == type)

#endif