/*
 * Enum.h: enum value conversion helpers
 *
 */

#pragma once
#ifndef __ENUM_HH__
#define __ENUM_HH__

#include <vector>
#include <string>
using namespace std;

#define G_N_ELEMENTS(Array) (sizeof(Array) / sizeof(*(Array)))
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

int EnumFromString(const vector<string>& types, const string& type);

string EnumToString(const vector<string>& types, int type);

// int
// virEnumFromString(const char * const *types,
//                   unsigned int ntypes,
//                   const char *type);

// const char *
// virEnumToString(const char * const *types,
//                 unsigned int ntypes,
//                 int type);

// // __VA_ARGS__
#define CGROUP_ENUM_IMPL(name, lastVal, ...) \
    static vector<string> name ## TypeList = { __VA_ARGS__ }; \
    string name ## TypeToString(int type) { \
        return EnumToString(name ## TypeList, type); \
    } \
    int name ## TypeFromString(const string& type) { \
        return EnumFromString(name ## TypeList, type); \
    }
    // G_STATIC_ASSERT(name ## TypeList.size() == lastVal)

#define CGROUP_ENUM_DECL(name)
// #define CGROUP_ENUM_DECL(name) \
//     string name ## TypeToString(int type); \
//     int name ## TypeFromString(const string& type)


// CGROUP_ENUM_IMPL(CgroupV1Controller,
//               CGROUP_CONTROLLER_LAST,
//               "", "cpu", "cpuacct", "cpuset", "memory", "devices",
//               "freezer", "blkio", "net_cls", "pids", "rdma", "perf_event", "name=systemd",
// );

#endif // __CGROUPBACKEND_HH__