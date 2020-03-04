#pragma once
#ifndef __CGROUPBACKEND_HH__
#define __CGROUPBACKEND_HH__

#include <memory>
#include <string>

namespace mdsd {

#define CGROUP_MAX_VAL 512
#define CGROUP_MEMORY_PARAM_UNLIMITED 9007199254740991LL /* = INT64_MAX >> 10 */
#define VIR_DEBUG printf
#define CGROUP_DEBUG(log) std::cout << log << std::endl
#define CGROUP_ERROR(error) std::cerr << error << std::endl

enum {
    CGROUP_CONTROLLER_CPU,
    CGROUP_CONTROLLER_CPUACCT,
    CGROUP_CONTROLLER_CPUSET,
    CGROUP_CONTROLLER_MEMORY,
    CGROUP_CONTROLLER_DEVICES,
    CGROUP_CONTROLLER_FREEZER,
    CGROUP_CONTROLLER_BLKIO,
    CGROUP_CONTROLLER_NET_CLS,
    CGROUP_CONTROLLER_PIDS,
    CGROUP_CONTROLLER_RDMA,
    CGROUP_CONTROLLER_PERF_EVENT,
    CGROUP_CONTROLLER_SYSTEMD,

    CGROUP_CONTROLLER_LAST
} CgroupController;

typedef enum {
    CGROUP_NONE = 0, /* create subdir under each cgroup if possible. */
    CGROUP_MEM_HIERACHY = 1 << 0, /* call virCgroupSetMemoryUseHierarchy
                                       * before creating subcgroups and
                                       * attaching tasks
                                       */
    CGROUP_THREAD = 1 << 1, /* cgroup v2 handles threads differently */
    CGROUP_SYSTEMD = 1 << 2, /* with systemd and cgroups v2 we cannot
                                  * manually enable controllers that systemd
                                  * doesn't know how to delegate */
} CgroupBackendFlags;

typedef enum {
    /* Adds a whole process with all threads to specific cgroup except
     * to systemd named controller. */
    CGROUP_TASK_PROCESS = 1 << 0,

    /* Same as CGROUP_TASK_PROCESS but it also adds the task to systemd
     * named controller. */
    CGROUP_TASK_SYSTEMD = 1 << 1,

    /* Moves only specific thread into cgroup except to systemd
     * named controller. */
    CGROUP_TASK_THREAD = 1 << 2,
} CgroupBackendTaskFlags;

typedef enum {
    CGROUP_BACKEND_TYPE_V2 = 0,
    CGROUP_BACKEND_TYPE_V1,
    CGROUP_BACKEND_TYPE_LAST,
} CgroupBackendType;

class CgroupBackend
{
public:
    CgroupBackend();
    CgroupBackend(const std::string &placement);
    ~CgroupBackend() {};

    void Init();
    bool Available();
    int DetectMounts();
    int DetectMounts(const char *mntType, const char *mntOpts, const char *mntDir);
    int ValidatePlacement();
    int DetectPlacement(const std::string &path, const std::string &controllers, const std::string &selfpath);

    int SetCpuShares(unsigned long long shares);
    int GetCpuShares(unsigned long long *shares);

    int ParseControllersFile();
    int DetectControllers(int controllers, int alreadyDetected);
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

    // std::string FileReadAll(const std::string &path);
    int FileReadAll(const std::string &path, std::string &output);
    int FileWriteStr(const std::string &path, const std::string &buffer);


    int SetMemory(unsigned long long kb);
    int GetMemoryStat(unsigned long long *cache,
                         unsigned long long *activeAnon,
                         unsigned long long *inactiveAnon,
                         unsigned long long *activeFile,
                         unsigned long long *inactiveFile,
                         unsigned long long *unevictable);
    int GetMemoryUsage(unsigned long *kb);
    int SetMemoryHardLimit(unsigned long long kb);
    int GetMemoryHardLimit(unsigned long long *kb);
    int SetMemorySoftLimit(unsigned long long kb);
    int GetMemorySoftLimit(unsigned long long *kb);
    int SetMemSwapHardLimit(unsigned long long kb);
    int GetMemSwapHardLimit(unsigned long long *kb);
    int GetMemSwapUsage(unsigned long long *kb);

private:
    const char* PROC_MOUNTS_PATH = "/proc/mounts";
    std::string mountPoint;
    std::string placement;
    int controllers;
};

} // namespace mdsd

#endif // __CGROUPBACKEND_HH__

