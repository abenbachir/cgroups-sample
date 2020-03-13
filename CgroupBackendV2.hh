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
    
    virtual int AddTask(pid_t pid, unsigned int taskflags = CGROUP_TASK_PROCESS);
    virtual int HasEmptyTasks(int controller = CGROUP_CONTROLLER_NONE);

    virtual int SetOwner(uid_t uid, gid_t gid, int controllers = 0);
    virtual int Remove();
    virtual int MakeGroup(unsigned int flags = CGROUP_NONE);
    int EnableSubtreeControllerCgroupV2(int controller);
    int DisableSubtreeControllerCgroupV2(int controller);


    int ParseControllersFile();
    virtual int DetectControllers(int controllers, int alreadyDetected);
    virtual bool HasController(int controller = 0);
    virtual int GetPathOfController(int controller, const std::string &key, std::string *path);

    virtual int SetCpuShares(unsigned long long shares);
    virtual int SetCpuCfsPeriod(unsigned long long cfs_period);
    virtual int SetCpuCfsQuota(long long cfs_quota);
    virtual unsigned long long GetCpuShares();
    virtual unsigned long long GetCpuCfsPeriod();
    virtual long long GetCpuCfsQuota();

    virtual int SetMemory(unsigned long long kb);
    virtual int GetMemoryStat(unsigned long long *cache,
                         unsigned long long *activeAnon,
                         unsigned long long *inactiveAnon,
                         unsigned long long *activeFile,
                         unsigned long long *inactiveFile,
                         unsigned long long *unevictable);
    virtual int GetMemoryUsage(unsigned long *kb);
    virtual int SetMemoryHardLimit(unsigned long long kb);
    virtual int GetMemoryHardLimit(unsigned long long *kb);
    virtual int SetMemorySoftLimit(unsigned long long kb);
    virtual int GetMemorySoftLimit(unsigned long long *kb);
    virtual int SetMemSwapHardLimit(unsigned long long kb);
    virtual int GetMemSwapHardLimit(unsigned long long *kb);
    virtual int GetMemSwapUsage(unsigned long long *kb);    

// helpers
    virtual std::string GetBasePath(int controller = CGROUP_CONTROLLER_NONE);
    virtual bool IsCgroupCreated();
protected:
    virtual int SetMemoryLimit(const std::string &keylimit, unsigned long long kb);
    virtual int GetMemoryLimit(const std::string &keylimit, unsigned long long *kb);

private:
    std::string placement;
    int controllers;
};

} // namespace mdsd

#endif // __CGROUPBACKEND_HH__

