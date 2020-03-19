/*
 * Enum.h: enum value conversion helpers
 *
 */

#pragma once
#ifndef __ENUMTOSTRING_HH__
#define __ENUMTOSTRING_HH__

#include <vector>
#include <string>
using namespace std;

#ifndef G_GNUC_UNUSED
# define G_GNUC_UNUSED __attribute__((__unused__))
#endif

int EnumFromString(const vector<string>& types, const string& type);

string EnumToString(const vector<string>& types, int type);

#define CGROUP_ENUM_IMPL(name, lastVal, ...) \
    static vector<string> name ## TypeList = { __VA_ARGS__ }; \
    string name ## TypeToString(int type) { \
        return EnumToString(name ## TypeList, type); \
    } \
    int name ## TypeFromString(const string& type) { \
        return EnumFromString(name ## TypeList, type); \
    }

#define CGROUP_ENUM_DECL(name) \
    string name ## TypeToString(int type); \
    int name ## TypeFromString(const string& type)

#endif // __ENUMTOSTRING_HH__