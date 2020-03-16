#include "CgroupBackendFactory.hh"
#include "Cgroup.hh"
#include "CgroupBackend.hh"
#include "CgroupBackendV2.hh"
// #include "CgroupBackendV1.hh"

#include <unistd.h>
#include <mntent.h>
#include <sys/mount.h>
#include <fstream>
#include <iostream>

#include <exception>

// for file open/read
#include <fcntl.h>
#include <sys/file.h>
#include <string.h>

using namespace mdsd;


CgroupBackendFactory::CgroupBackendFactory()
{
}

CgroupBackendFactory::~CgroupBackendFactory()
{
}

CgroupBackendType CgroupBackendFactory::DetectMountedCgroupBackend()
{
    const char * CGROUPV1_NAME = "cgroup";
    const char * CGROUPV2_NAME = "cgroup2";
    const char * PROC_MOUNTS_PATH = "/proc/mounts";

    FILE *mounts = NULL;
    struct mntent entry;
    char buf[CGROUP_MAX_VAL];

    if (!(mounts = fopen(PROC_MOUNTS_PATH, "r")))
        return CGROUP_BACKEND_NONE;
    try
    {    
        while (getmntent_r(mounts, &entry, buf, sizeof(buf)) != NULL)
        {
            if (strcmp(entry.mnt_type, CGROUPV1_NAME) == 0)
                return CGROUP_BACKEND_TYPE_V1;
            
            if (strcmp(entry.mnt_type, CGROUPV2_NAME) == 0)
                return CGROUP_BACKEND_TYPE_V2;
        }
    }
    catch(const std::exception& e)
    {
        CGROUP_ERROR(e.what());
    }

    if (mounts)
        fclose(mounts);
    
    return CGROUP_BACKEND_NONE;
}

std::shared_ptr<CgroupBackend> CgroupBackendFactory::GetCgroupBackend(const std::string& path)
{
    std::shared_ptr<CgroupBackend> backend;
    auto type = this->DetectMountedCgroupBackend();
    switch (type)
    {
        case CGROUP_BACKEND_TYPE_V1:
            // backend = std::make_shared<CgroupBackendV1>(path);
            break;
        case CGROUP_BACKEND_TYPE_V2:
            backend = std::make_shared<CgroupBackendV2>(path);
            break;
        default:
            throw std::runtime_error("Cannot found the cgroup mount, make sure your have mounted the cgroup on your system");
            break;
    }

    return backend;
}


std::shared_ptr<Cgroup> CgroupBackendFactory::GetCgroup(const std::string& path)
{
    return std::make_shared<Cgroup>(this->GetCgroupBackend(path));
}
