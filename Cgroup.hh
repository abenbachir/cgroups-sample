#pragma once
#ifndef __CGROUP_HH__
#define __CGROUP_HH__

#include <memory>
#include <string>
#include "CgroupBackend.hh"

namespace mdsd {

#define CGROUP_MAX_VAL 512
#define CGROUP_MEM_MB_TO_BYTES(val) val * 1024 * 1024
#define CGROUP_MEM_MB_TO_KB(val) val * 1024
#define CGROUP_MEM_KB_TO_BYTES(val) val * 1024
class Cgroup
{
public:
    Cgroup(const std::shared_ptr<CgroupBackend>& backend);
    ~Cgroup();

    void SetCPULimitInPercentage(unsigned int hard, unsigned int soft = 0);
    void SetMemoryLimitInMB(float hard, float soft = 0);

    void SetOwner(uid_t uid, gid_t gid);
    std::shared_ptr<CgroupBackend> GetCgroupBackend();
    std::shared_ptr<CgroupBackend> backend;
private:
    const std::string cgpath;
    
    
};

} // namespace mdsd

#endif // __CGROUP_HH__

