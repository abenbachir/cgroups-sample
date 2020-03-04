#pragma once
#ifndef __CGROUPBACKEND_HH__
#define __CGROUPBACKEND_HH__

#include <memory>
#include <string>

namespace mdsd {

#define CGROUP_MAX_VAL 512

#define VIR_DEBUG printf

enum {
    VIR_CGROUP_CONTROLLER_CPU,
    VIR_CGROUP_CONTROLLER_CPUACCT,
    VIR_CGROUP_CONTROLLER_CPUSET,
    VIR_CGROUP_CONTROLLER_MEMORY,
    VIR_CGROUP_CONTROLLER_DEVICES,
    VIR_CGROUP_CONTROLLER_FREEZER,
    VIR_CGROUP_CONTROLLER_BLKIO,
    VIR_CGROUP_CONTROLLER_NET_CLS,
    VIR_CGROUP_CONTROLLER_PERF_EVENT,
    VIR_CGROUP_CONTROLLER_SYSTEMD,

    VIR_CGROUP_CONTROLLER_LAST
} CgroupController;

typedef enum {
    VIR_CGROUP_NONE = 0, /* create subdir under each cgroup if possible. */
    VIR_CGROUP_MEM_HIERACHY = 1 << 0, /* call virCgroupSetMemoryUseHierarchy
                                       * before creating subcgroups and
                                       * attaching tasks
                                       */
    VIR_CGROUP_THREAD = 1 << 1, /* cgroup v2 handles threads differently */
    VIR_CGROUP_SYSTEMD = 1 << 2, /* with systemd and cgroups v2 we cannot
                                  * manually enable controllers that systemd
                                  * doesn't know how to delegate */
} CgroupBackendFlags;

typedef enum {
    /* Adds a whole process with all threads to specific cgroup except
     * to systemd named controller. */
    VIR_CGROUP_TASK_PROCESS = 1 << 0,

    /* Same as VIR_CGROUP_TASK_PROCESS but it also adds the task to systemd
     * named controller. */
    VIR_CGROUP_TASK_SYSTEMD = 1 << 1,

    /* Moves only specific thread into cgroup except to systemd
     * named controller. */
    VIR_CGROUP_TASK_THREAD = 1 << 2,
} CgroupBackendTaskFlags;

typedef enum {
    VIR_CGROUP_BACKEND_TYPE_V2 = 0,
    VIR_CGROUP_BACKEND_TYPE_V1,
    VIR_CGROUP_BACKEND_TYPE_LAST,
} CgroupBackendType;

class CgroupBackend
{
public:
    CgroupBackend(const std::string &mountPath): mountPoint(mountPath) {};
    ~CgroupBackend() {};

    bool Available();
    int DetectMounts(const char *mntType, const char *mntOpts, const char *mntDir);
    int DetectPlacement(const std::string &path, const std::string &controllers, const std::string &selfpath);

    int SetCpuShares(unsigned long long shares);
    int GetCpuShares(unsigned long long *shares);

    bool HasController(int controller);
    int GetPathOfController(int controller, const std::string &key, std::string *path);

    int SetCgroupValueU64(int controller, const std::string &key, unsigned long long int value);
    int SetCgroupValueI64(int controller, const std::string &key, long long int value);
    int SetCgroupValueStr(int controller, const std::string &key, const std::string& value);
    int SetCgroupValueRaw(const std::string &path, const std::string& value);

    int GetCgroupValueU64(int controller, const std::string &key, unsigned long long int *value);
    int GetCgroupValueI64(int controller, const std::string &key, long long int *value);
    int GetCgroupValueStr(int controller, const std::string &key, std::string* value);
    int GetCgroupValueRaw(const std::string &path, std::string* value);

    std::string FileReadAll(const std::string &path);
    int FileWriteStr(const std::string &path, const std::string &buffer);

private:
    std::string mountPoint;
    std::string placement;
    int controllers = VIR_CGROUP_CONTROLLER_CPU || VIR_CGROUP_CONTROLLER_MEMORY;
};

} // namespace mdsd

#endif // __CGROUPBACKEND_HH__

