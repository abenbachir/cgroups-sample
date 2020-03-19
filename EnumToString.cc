#include "EnumToString.hh"

#include <vector>
#include <string>
using namespace std;


int EnumFromString(const vector<string>& types, const string& type)
{
    for (size_t i = 0; i < types.size(); i++)
        if (types[i] == type)
            return i;

    return -1;
}

string EnumToString(const vector<string>& types, int type)
{
    if (type < 0 || type >= types.size())
        return NULL;

    return types[type];
}

