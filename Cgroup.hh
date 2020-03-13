#pragma once
#ifndef __CGROUP_HH__
#define __CGROUP_HH__

#include <memory>
#include <string>
#include "CgroupBackend.hh"

namespace mdsd {

#define CGROUP_MAX_VAL 512

class Cgroup
{
public:
    Cgroup(const std::shared_ptr<CgroupBackend>& backend);
    ~Cgroup();


    std::shared_ptr<CgroupBackend> backend;
private:
    const std::string cgpath;
    
};

} // namespace mdsd

#endif // __CGROUP_HH__

