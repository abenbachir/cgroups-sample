#pragma once
#ifndef __CGROUPBACKENDV1_HH__
#define __CGROUPBACKENDV1_HH__

#include <memory>
#include <string>
#include "CgroupBackend.hh"

namespace mdsd {

struct _CgroupBackendV1Controller {
    std::string mountPoint;
    std::string linkPoint;
    std::string placement;
    CgroupController controller;

    bool PlacementExist() { 
        fs::path path(mountPoint);
        path /= placement;
        return fs::exists(path);
    }

    bool Enabled() { return placement != "" && placement != "/"; }
    void Print() { CGROUP_DEBUG("controller=" << controller<< " mountPoint="<<mountPoint << " placement=" << placement); }
};
typedef struct _CgroupBackendV1Controller CgroupBackendV1Controller;

class CgroupBackendV1 : public CgroupBackend
{
public:
    CgroupBackendV1(const std::string &placement);
    ~CgroupBackendV1() {};

    virtual void Init();
    virtual int DetectMounts(const char *mntType, const char *mntOpts, const char *mntDir);
    virtual int DetectPlacement(const std::string &path, const std::string &controllers, const std::string &selfpath);
    virtual int ValidatePlacement();
    
    virtual void AddTask(pid_t pid, unsigned int taskflags = CGROUP_TASK_PROCESS);
    virtual bool HasEmptyTasks(int controller = CGROUP_CONTROLLER_NONE);

    virtual void Remove();
    virtual void MakeGroup(unsigned int flags = CGROUP_NONE);
    virtual void SetOwner(uid_t uid, gid_t gid, int controllers);

    virtual int DetectControllers(int controllers, int alreadyDetected = CGROUP_CONTROLLER_NONE);
    virtual bool HasController(int controller = 0);
    virtual std::string GetPathOfController(int controller, const std::string &key);

    virtual void SetCpuCfsPeriod(unsigned long long cfs_period);
    virtual unsigned long long GetCpuCfsPeriod();

    virtual void SetCpuCfsQuota(long long cfs_quota);
    virtual long long GetCpuCfsQuota();

// helpers
    virtual std::string GetBasePath(int controller = CGROUP_CONTROLLER_NONE);
    virtual std::string GetRelativeBasePath(int controller = CGROUP_CONTROLLER_NONE);

protected:
    virtual void SetMemoryLimitInKB(const std::string &keylimit, unsigned long long kb);
    virtual unsigned long long GetMemoryLimitInKB(const std::string &keylimit);
    void MemoryInit();
    int ResolveMountLink(const char *mntDir, const std::string& typeStr, CgroupBackendV1Controller *controller);
    int MountOptsMatchController(const std::string &mntOpts, const std::string& typeStr);

    virtual std::string GetControllerName(int controller);

private:
    std::string placement;
    unsigned long long int memoryUnlimitedKB = CGROUP_MEMORY_PARAM_UNLIMITED;
    CgroupBackendV1Controller controllers[CGROUP_CONTROLLER_LAST];
};

} // namespace mdsd

#endif // __CGROUPBACKENDV1_HH__

