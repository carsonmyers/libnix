#ifndef INCLUDE_libnix_common_h__
#define INCLUDE_libnix_common_h__

#ifdef __cplusplus
    #define NIX_BEGIN_DECL extern "c" {
    #define NIX_END_DECL }
#else
    #define NIX_BEGIN_DECL /* empty */
    #define NIX_END_DECL /* empty */
#endif

#if __GNUC__ >= 4
    #define NIX_EXTERN(type) extern \
        __attribute__((visibility("default"))) \
        type
#elif defined(_MSC_VER)
    #define NIX_EXTERN(type) __declspec(dllexport) type
#else
    #define NIX_EXTERN(type) extern type
#endif

#endif