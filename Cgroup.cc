#include "Cgroup.hh"
#include "CgroupBackend.hh"
#include "Enum.hh"

#include <unistd.h>
#include <mntent.h>
#include <sys/mount.h>
#include <fstream>
#include <iostream>

#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
// for file open/read
#include <fcntl.h>
#include <sys/file.h>

using namespace mdsd;


int Cgroup::DetectMounts()
{
    FILE *mounts = NULL;
    struct mntent entry;
    char buf[CGROUP_MAX_VAL];
    int ret = -1;
    size_t i;

    mounts = fopen("/proc/mounts", "r");
    if (mounts == NULL) {
        VIR_DEBUG("errno=%d Unable to open /proc/mounts", errno);
        return -1;
    }

    try
    {
        while (getmntent_r(mounts, &entry, buf, sizeof(buf)) != NULL)
        {
            if(backend->DetectMounts(entry.mnt_type, entry.mnt_opts, entry.mnt_dir) < 0)
                break;
        }

        ret = 0;
    }
    catch(const std::exception& e)
    {
        std::cerr << e.what() << std::endl;
    }

    if (mounts)
        fclose(mounts);
    return ret;
}
