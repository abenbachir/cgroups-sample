/*
 * Enum.h: enum value conversion helpers
 *
 */

#pragma once
#ifndef __ENUM_HH__
#define __ENUM_HH__

#include <string.h> 

#define G_N_ELEMENTS(Array) (sizeof(Array) / sizeof(*(Array)))
#define STREQ(a, b) (strcmp(a, b) == 0)
#define NULLSTR(s) ((s) ? (s) : "<null>")

#if (4 < __GNUC__ + (6 <= __GNUC_MINOR__) \
     && (201112L <= __STDC_VERSION__  || !defined __STRICT_ANSI__) \
     && !defined __cplusplus)
# define G_STATIC_ASSERT(cond) _Static_assert(cond, "verify (" #cond ")")
#else
# define G_STATIC_ASSERT(cond)
#endif

#ifndef G_GNUC_UNUSED
# define G_GNUC_UNUSED __attribute__((__unused__))
#endif

int
virEnumFromString(const char * const *types,
                  unsigned int ntypes,
                  const char *type);

const char *
virEnumToString(const char * const *types,
                unsigned int ntypes,
                int type);

#define VIR_ENUM_IMPL(name, lastVal, ...) \
    static const char *const name ## TypeList[] = { __VA_ARGS__ }; \
    const char *name ## TypeToString(int type) { \
        return virEnumToString(name ## TypeList, \
                               G_N_ELEMENTS(name ## TypeList), \
                               type); \
    } \
    int name ## TypeFromString(const char *type) { \
        return virEnumFromString(name ## TypeList, \
                                 G_N_ELEMENTS(name ## TypeList), \
                                 type); \
    } \
    G_STATIC_ASSERT(G_N_ELEMENTS(name ## TypeList) == lastVal)

#define VIR_ENUM_DECL(name) \
    const char *name ## TypeToString(int type); \
    int name ## TypeFromString(const char*type)


#endif // __CGROUPBACKEND_HH__