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

enum {
    CGROUP_CONTROLLER_FILE_CGROUP_PROCS = 0,
    CGROUP_CONTROLLER_FILE_CGROUP_THREADS,

    CGROUP_CONTROLLER_FILE_CPU_USAGE,
    CGROUP_CONTROLLER_FILE_CPU_SHARES,
    CGROUP_CONTROLLER_FILE_CPU_CFS_PERIOD,
    CGROUP_CONTROLLER_FILE_CPU_CFS_QUOTA,

    CGROUP_CONTROLLER_FILE_MEMORY_USAGE,
    CGROUP_CONTROLLER_FILE_MEMORY_HARD_LIMIT,
    CGROUP_CONTROLLER_FILE_MEMORY_SOFT_LIMIT,

    CGROUP_CONTROLLER_FILE_MEMORY_SWAP_USAGE,
    CGROUP_CONTROLLER_FILE_MEMORY_SWAP_HARD_LIMIT,
    CGROUP_CONTROLLER_FILE_MEMORY_SWAP_SOFT_LIMIT,

    CGROUP_CONTROLLER_FILE_LAST,
} CgroupControllerFile;

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

class CGroupBaseException : public std::runtime_error
{
public:
    CGroupBaseException(const std::string& message) : std::runtime_error(message) {}
};

class CGroupControllerNotFoundException : public CGroupBaseException
{
public:
    CGroupControllerNotFoundException(const std::string& message): CGroupBaseException(message) {}
};

class CGroupFileNotFoundException : public CGroupBaseException
{
public:
    CGroupFileNotFoundException(const std::string& message): CGroupBaseException(message) {}
};

class CGroupCPUException : public CGroupBaseException
{
public:
    CGroupCPUException(const std::string& message): CGroupBaseException(message) {}
};

class CGroupMemoryException : public CGroupBaseException
{
public:
    CGroupMemoryException(const std::string& message): CGroupBaseException(message) {}
};

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

    virtual void AddTask(pid_t pid, unsigned int taskflags = CGROUP_TASK_PROCESS) = 0;
    virtual bool HasEmptyTasks(int controller = CGROUP_CONTROLLER_NONE);

    virtual void SetOwner(uid_t uid, gid_t gid, int controllers = CGROUP_CONTROLLER_NONE);
    virtual void Remove() = 0;
    virtual void MakeGroup(unsigned int flags = CGROUP_NONE) = 0;


    int DetectControllers(int controllers, int alreadyDetected);
    virtual bool HasController(int controller = CGROUP_CONTROLLER_NONE) = 0;
    virtual std::string GetPathOfController(int controller, const std::string &key) = 0;

    void SetCgroupValueU64(int controller, const std::string &key, unsigned long long int value);
    void SetCgroupValueI64(int controller, const std::string &key, long long int value);
    void SetCgroupValueStr(int controller, const std::string &key, const std::string& value);
    void SetCgroupValueRaw(const std::string &path, const std::string& value);

    unsigned long long int GetCgroupValueU64(int controller, const std::string &key);
    long long int GetCgroupValueI64(int controller, const std::string &key);
    std::string GetCgroupValueStr(int controller, const std::string &key);
    std::string GetCgroupValueRaw(const std::string &path);

    std::string FileReadAll(const std::string &path);
    void FileWriteStr(const std::string &path, const std::string &buffer);

    virtual void ValidateCPUCfsQuota(long long value);
    void ValidateCPUCfsPeiod(long long value);
    
    virtual void SetCpuShares(unsigned long long shares);
    virtual unsigned long long GetCpuShares();

    virtual void SetCpuCfsPeriod(unsigned long long cfs_period) = 0;
    virtual void SetCpuCfsQuota(long long cfs_quota) = 0;
    virtual unsigned long long GetCpuCfsPeriod() = 0;
    virtual long long GetCpuCfsQuota() = 0;

    virtual void SetMemory(unsigned long long kb);
    virtual int GetMemoryStat(unsigned long long *cache,
                         unsigned long long *activeAnon,
                         unsigned long long *inactiveAnon,
                         unsigned long long *activeFile,
                         unsigned long long *inactiveFile,
                         unsigned long long *unevictable);
    virtual unsigned long GetMemoryUsage();
    virtual void SetMemoryHardLimit(unsigned long long kb);
    virtual unsigned long long GetMemoryHardLimit();
    virtual void SetMemorySoftLimit(unsigned long long kb);
    virtual unsigned long long GetMemorySoftLimit();
    virtual void SetMemSwapHardLimit(unsigned long long kb);
    virtual unsigned long long GetMemSwapHardLimit();
    virtual unsigned long long GetMemSwapUsage();

// helpers
    virtual std::string GetBasePath(int controller = CGROUP_CONTROLLER_NONE) = 0;
    virtual std::string GetControllerFileName(int controllerFileType);

    

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
    virtual std::string GetControllerName(int controller) = 0;

    virtual void SetMemoryLimitInKB(const std::string &keylimit, unsigned long long kb) = 0;
    virtual unsigned long long GetMemoryLimitInKB(const std::string &keylimit) = 0;

protected:
    const CgroupBackendType backendType = CGROUP_BACKEND_NONE;
    std::string backenName;
    const char* PROC_MOUNTS_PATH = "/proc/mounts";
    std::string mountPoint;

    static std::string backendControllerFileMap[CGROUP_BACKEND_TYPE_LAST-1][CGROUP_CONTROLLER_FILE_LAST];
};

} // namespace mdsd

#endif // __CGROUPBACKEND_HH__

