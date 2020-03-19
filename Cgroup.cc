#include "Cgroup.hh"
#include "CgroupBackend.hh"
#include "CgroupBackendV2.hh"
#include "CgroupBackendV1.hh"


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
    backend->SetOwner(uid, gid, CGROUP_CONTROLLER_LAST);
}

std::shared_ptr<CgroupBackend> Cgroup::GetCgroupBackend()
{
    return this->backend;
}

void Cgroup::SetCPULimitInPercentage(unsigned int cpu, unsigned int softquota)
{
    CGROUP_DEBUG("CPU: cpu=" << cpu << " softquota=" << softquota);

    unsigned long long period = 100;
    unsigned long long hard = cpu * (1 + double(softquota)/100); // hard

    if (softquota > 0) {
        backend->SetCpuShares(cpu); // soft
    }

    backend->SetCpuCfsQuota(hard * 1000);
    backend->SetCpuCfsPeriod(period * 1000);
}

void Cgroup::SetMemoryLimitInMB(float memory, unsigned int softquota)
{
    CGROUP_DEBUG("MEM: memory=" << memory << " softquota=" << softquota);

    auto hard = memory * (1 + double(softquota)/100);

    backend->SetMemoryHardLimit(CGROUP_MEM_MB_TO_KB(hard));
    if (softquota > 0)
        backend->SetMemorySoftLimit(CGROUP_MEM_MB_TO_KB(memory));
}
