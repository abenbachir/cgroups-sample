#pragma once
#ifndef __CGROUP_HH__
#define __CGROUP_HH__

#include <memory>
#include <string>
#include "CgroupBackend.hh"
#include "CgroupDef.hh"

namespace mdsd {

class Cgroup
{
public:
    Cgroup(const std::shared_ptr<CgroupBackend>& backend);
    ~Cgroup();

    void SetCPULimitInPercentage(unsigned int cpu, unsigned int softquota = 0);
    void SetMemoryLimitInMB(float memory, unsigned int softquota = 0);
    void SetOwner(uid_t uid, gid_t gid);
    std::shared_ptr<CgroupBackend> GetCgroupBackend();
    std::shared_ptr<CgroupBackend> backend;

    unsigned long long GetMemoryInMB()
    {
        return CGROUP_MEM_KB_TO_MB(backend->GetMemoryHardLimit());
    }

    unsigned int GetCPUInPercent()
    {
        return 100 * double(backend->GetCpuCfsQuota())/backend->GetCpuCfsPeriod();
    }
private:
    
};

} // namespace mdsd

#endif // __CGROUP_HH__

