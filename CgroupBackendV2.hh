#pragma once
#ifndef __CGROUPBACKENDV2_HH__
#define __CGROUPBACKENDV2_HH__

#include <memory>
#include <string>
#include "CgroupBackend.hh"

namespace mdsd {

class CgroupBackendV2 : public CgroupBackend
{
public:
    CgroupBackendV2(const std::string &placement);
    ~CgroupBackendV2() {};

    virtual void Init();
    virtual int DetectMounts(const char *mntType, const char *mntOpts, const char *mntDir);
    virtual int DetectPlacement(const std::string &path, const std::string &controllers, const std::string &selfpath);
    virtual int ValidatePlacement();
    
    virtual void AddTask(pid_t pid, unsigned int taskflags = CGROUP_TASK_PROCESS);
    virtual bool HasEmptyTasks(int controller = CGROUP_CONTROLLER_NONE);

    virtual void Remove();
    virtual void MakeGroup(unsigned int flags = CGROUP_NONE);
    int EnableSubtreeControllerCgroupV2(int controller);
    int DisableSubtreeControllerCgroupV2(int controller);


    int ParseControllersFile();
    virtual int DetectControllers(int controllers, int alreadyDetected);
    virtual bool HasController(int controller = 0);
    virtual std::string GetPathOfController(int controller, const std::string &key);

    virtual void SetCpuCfsPeriod(unsigned long long cfs_period);
    virtual unsigned long long GetCpuCfsPeriod();

    virtual void SetCpuCfsQuota(long long cfs_quota);
    virtual long long GetCpuCfsQuota();

// helpers
    virtual std::string GetBasePath(int controller = CGROUP_CONTROLLER_NONE);
    virtual bool IsCgroupCreated();
protected:
    virtual void SetMemoryLimitInKB(const std::string &keylimit, unsigned long long kb);
    virtual unsigned long long GetMemoryLimitInKB(const std::string &keylimit);
    virtual std::string GetControllerName(int controller);

private:
    std::string placement;
    int controllers;
};

} // namespace mdsd

#endif // __CGROUPBACKEND_HH__

