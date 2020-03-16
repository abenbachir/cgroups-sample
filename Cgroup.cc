#include "Cgroup.hh"
#include "CgroupBackend.hh"
#include "CgroupBackendV2.hh"
#include "CgroupBackendV1.hh"
#include "Enum.hh"

#include <unistd.h>
#include <mntent.h>
#include <sys/mount.h>
#include <fstream>
#include <iostream>

#include <exception>

// for file open/read
#include <fcntl.h>
#include <sys/file.h>

using namespace mdsd;


Cgroup::Cgroup(const std::shared_ptr<CgroupBackend>& backend): backend(backend)
{
    backend->Init();
}

Cgroup::~Cgroup()
{
}

void Cgroup::SetOwner(uid_t uid, gid_t gid)
{
    backend->SetOwner(uid, gid);
}

std::shared_ptr<CgroupBackend> Cgroup::GetCgroupBackend()
{
    return this->backend;
}

void Cgroup::SetCPULimitInPercentage(unsigned int hard, unsigned int soft)
{
    unsigned long long period = 100;
    unsigned long long quota = hard;
    backend->SetCpuCfsQuota(quota * 1000);
    backend->SetCpuCfsPeriod(period * 1000);

    // backend->SetCpuShares(soft);
}

void Cgroup::SetMemoryLimitInMB(float hard, float soft)
{
    backend->SetMemoryHardLimit(CGROUP_MEM_MB_TO_KB(hard));
    if (soft > 0)
        backend->SetMemorySoftLimit(CGROUP_MEM_MB_TO_KB(soft));
}

