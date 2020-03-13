#pragma once
#ifndef __CGROUPBACKEND_HH__
#define __CGROUPBACKEND_HH__

#include <memory>
#include <string>

#include <experimental/filesystem> // TODO: remove 'experimental'

namespace fs = std::experimental::filesystem;

namespace mdsd {

#define CGROUP_MAX_VAL 512
#define CGROUP_MEMORY_PARAM_UNLIMITED 9007199254740991LL /* = INT64_MAX >> 10 */
#define CGROUP_DEBUG(log) std::cout << log << std::endl
#define CGROUP_ERROR(error) std::cerr << "ERROR: " << error << std::endl

enum {
    CGROUP_CONTROLLER_NONE = 0,
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

    CGROUP_CONTROLLER_LAST,
} CgroupController;

typedef enum {
    CGROUP_NONE = 0, /* create subdir under each cgroup if possible. */
    CGROUP_MEM_HIERACHY = 1 << 0, /* call SetMemoryUseHierarchy
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
    CGROUP_BACKEND_NONE = 0,
    CGROUP_BACKEND_TYPE_V2,
    CGROUP_BACKEND_TYPE_V1,
    CGROUP_BACKEND_TYPE_LAST,
} CgroupBackendType;

class CgroupBackend
{
public:
    CgroupBackend(CgroupBackendType type, const std::string &placement);
    ~CgroupBackend() {};

    virtual void Init();
    virtual bool Available();
    virtual int DetectMounts();

    virtual std::string GetBackendName();
    virtual CgroupBackendType GetBackendType();

    virtual int DetectMounts(const char *mntType, const char *mntOpts, const char *mntDir)  = 0;
    virtual int DetectPlacement(const std::string &path, const std::string &controllers, const std::string &selfpath)  = 0;
    virtual int ValidatePlacement()  = 0;

    virtual int AddTask(pid_t pid, unsigned int taskflags = CGROUP_TASK_PROCESS) = 0;
    virtual int HasEmptyTasks(int controller = CGROUP_CONTROLLER_NONE);

    virtual int SetOwner(uid_t uid, gid_t gid, int controllers = 0) = 0;
    virtual int Remove() = 0;
    virtual int MakeGroup(unsigned int flags = CGROUP_NONE) = 0;


    int ParseControllersFile();
    int DetectControllers(int controllers, int alreadyDetected);
    virtual bool HasController(int controller = CGROUP_CONTROLLER_NONE) = 0;
    virtual int GetPathOfController(int controller, const std::string &key, std::string *path) = 0;

    int SetCgroupValueU64(int controller, const std::string &key, unsigned long long int value);
    int SetCgroupValueI64(int controller, const std::string &key, long long int value);
    int SetCgroupValueStr(int controller, const std::string &key, const std::string& value);
    int SetCgroupValueRaw(const std::string &path, const std::string& value);

    int GetCgroupValueU64(int controller, const std::string &key, unsigned long long int *value);
    int GetCgroupValueI64(int controller, const std::string &key, long long int *value);
    int GetCgroupValueStr(int controller, const std::string &key, std::string* value);
    int GetCgroupValueRaw(const std::string &path, std::string* value);

    int FileReadAll(const std::string &path, std::string &output);
    int FileWriteStr(const std::string &path, const std::string &buffer);

    virtual int SetCpuShares(unsigned long long shares) = 0;
    virtual int SetCpuCfsPeriod(unsigned long long cfs_period) = 0;
    virtual int SetCpuCfsQuota(long long cfs_quota) = 0;
    virtual unsigned long long GetCpuShares() = 0;
    virtual unsigned long long GetCpuCfsPeriod() = 0;
    virtual long long GetCpuCfsQuota() = 0;

    virtual int SetMemory(unsigned long long kb) = 0;
    virtual int GetMemoryStat(unsigned long long *cache,
                         unsigned long long *activeAnon,
                         unsigned long long *inactiveAnon,
                         unsigned long long *activeFile,
                         unsigned long long *inactiveFile,
                         unsigned long long *unevictable) = 0;
    virtual int GetMemoryUsage(unsigned long *kb) = 0;
    virtual int SetMemoryHardLimit(unsigned long long kb) = 0;
    virtual int GetMemoryHardLimit(unsigned long long *kb) = 0;
    virtual int SetMemorySoftLimit(unsigned long long kb) = 0;
    virtual int GetMemorySoftLimit(unsigned long long *kb) = 0;
    virtual int SetMemSwapHardLimit(unsigned long long kb) = 0;
    virtual int GetMemSwapHardLimit(unsigned long long *kb) = 0;
    virtual int GetMemSwapUsage(unsigned long long *kb) = 0; 

// helpers
    virtual std::string GetBasePath(int controller = CGROUP_CONTROLLER_NONE) = 0;

protected:
        // trim from start (in place)
    virtual void ltrim(std::string &s);
    // trim from end (in place)
    virtual void rtrim(std::string &s);
    // trim from both ends (in place)
    virtual void trim(std::string &s);

    virtual void splitstring(const std::string& str,
        std::vector<std::string>& container, const std::string& delims = " ");

    virtual std::string serialize_fileperms(const fs::perms &p);

protected:
    const CgroupBackendType backenType = CGROUP_BACKEND_NONE;
    std::string backenName;
    const char* PROC_MOUNTS_PATH = "/proc/mounts";
    std::string mountPoint;
};

} // namespace mdsd

#endif // __CGROUPBACKEND_HH__

