#pragma once
#ifndef __CGROUPBACKEND_FACTORY_HH__
#define __CGROUPBACKEND_FACTORY_HH__

#include <memory>
#include <string>
#include "Cgroup.hh"
#include "CgroupBackend.hh"

namespace mdsd {

class CgroupBackendFactory
{
public:
    CgroupBackendFactory();
    ~CgroupBackendFactory();

    CgroupBackendType DetectMountedCgroupBackend();
    std::shared_ptr<CgroupBackend> GetCgroupBackend(const std::string& path);
    std::shared_ptr<Cgroup> GetCgroup(const std::string& path);
private:
    
};

} // namespace mdsd

#endif // __CGROUPBACKEND_FACTORY_HH__

