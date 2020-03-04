#include "Enum.hh"

int
virEnumFromString(const char * const *types,
                  unsigned int ntypes,
                  const char *type)
{
    size_t i;
    if (!type)
        return -1;

    for (i = 0; i < ntypes; i++)
        if (strcmp(types[i], type) == 0)
            return i;

    return -1;
}


const char *
virEnumToString(const char * const *types,
                unsigned int ntypes,
                int type)
{
    if (type < 0 || type >= ntypes)
        return NULL;

    return types[type];
}