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
    Cgroup(const std::string &path): path(path) {
        backend = new CgroupBackend(path);
    }
    ~Cgroup() { delete backend; }

    int DetectMounts();

private:
    const std::string path;
    CgroupBackend *backend;
    
};

} // namespace mdsd

#endif // __CGROUP_HH__

