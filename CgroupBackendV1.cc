#include "CgroupBackendV1.hh"
#include "Enum.hh"

#include <unistd.h>
#include <mntent.h>
#include <sys/mount.h>
#include <fstream>
#include <iostream>
#include <experimental/filesystem> // TODO: remove 'experimental'
#include <cstring>

#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
// for file open/read
#include <fcntl.h>
#include <sys/file.h>

#include <algorithm>
#include <iterator>
#include <functional>
#include <cctype>
#include <locale>

using namespace mdsd;
namespace fs = std::experimental::filesystem;

/* this should match the enum CgroupController */
CGROUP_ENUM_DECL(CgroupV1Controller);
CGROUP_ENUM_IMPL(CgroupV1Controller,
              CGROUP_CONTROLLER_LAST,
              "", "cpu", "cpuacct", "cpuset", "memory", "devices",
              "freezer", "blkio", "net_cls", "pids", "rdma", "perf_event", "name=systemd"
);

CgroupBackendV1::CgroupBackendV1(const std::string &placement)
    : placement(placement), CgroupBackend(CGROUP_BACKEND_TYPE_V1, placement)
{
}

std::string CgroupBackendV1::GetBasePath(int controller)
{
    fs::path base(this->controllers[controller].mountPoint);
    base /= this->controllers[controller].placement;

    return base;
}


void CgroupBackendV1::Init()
{
    CgroupBackend::Init();
    CGROUP_DEBUG("this->placement="+this->placement);
    for (size_t i = CGROUP_CONTROLLER_CPU; i < CGROUP_CONTROLLER_LAST; i++) {
        auto controllerName = CgroupV1ControllerTypeToString(i);
        auto mountPoint = this->controllers[i].mountPoint;

        this->controllers[i].placement = this->placement;
        if (this->placement.substr(0, mountPoint.size()) == mountPoint) {
            this->controllers[i].placement = this->placement.substr(mountPoint.size());
        }

        CGROUP_DEBUG("controller=" + controllerName +" placement=" + this->controllers[i].placement + " mountPoint=" + mountPoint);
    }
}

int CgroupBackendV1::ResolveMountLink(const char *mntDir, const std::string& typeStr,
                            CgroupBackendV1Controller *controller)
{
    return 0;
}

int CgroupBackendV1::MountOptsMatchController(const std::string &mntOpts, const std::string& typeStr)
{
    std::vector<std::string> optList;
    splitstring(mntOpts, optList, ",");

    auto result = std::find(std::begin(optList), std::end(optList), typeStr);

    return result != std::end(optList);
}

int CgroupBackendV1::DetectMounts(const char *mntType, const char *mntOpts, const char *mntDir)
{
    if (strcmp(mntType, GetBackendName().c_str()) != 0)
        return 0;

    for (size_t i = CGROUP_CONTROLLER_CPU; i < CGROUP_CONTROLLER_LAST; i++)
    {
         auto typestr = CgroupV1ControllerTypeToString(i);

        if (MountOptsMatchController(std::string(mntOpts), typestr)) {
            /* Note that the lines in /proc/mounts have the same
             * order than the mount operations, and that there may
             * be duplicates due to bind mounts. This means
             * that the same mount point may be processed more than
             * once. We need to save the results of the last one,
             * and we need to be careful to release the memory used
             * by previous processing. */
            auto controller = &this->controllers[i];

            controller->linkPoint = "";
            controller->mountPoint = std::string(mntDir);
            CGROUP_DEBUG("mountPoint=" << controller->mountPoint);

            /* If it is a co-mount it has a filename like "cpu,cpuacct"
             * and we must identify the symlink path */
            if (ResolveMountLink(mntDir, typestr, controller) < 0)
                return -1;
        }
    }

    // this->mountPoint = std::string(mntDir);
    // CGROUP_DEBUG("mountPoint=" << mountPoint);

    return 0;
}

int CgroupBackendV1::DetectPlacement(const std::string &path,
    const std::string &controllers,
    const std::string &selfpath)
{
    // CGROUP_DEBUG("path=%s controllers=%s selfpath=%s\n", path.c_str(), controllers.c_str(), selfpath.c_str());
   for (size_t i = CGROUP_CONTROLLER_CPU; i < CGROUP_CONTROLLER_LAST; i++) {
        auto typestr = CgroupV1ControllerTypeToString(i);

        if (MountOptsMatchController(controllers, typestr) &&
            !this->controllers[i].mountPoint.empty() &&
            this->controllers[i].placement.empty()) {
            /*
             * selfpath == "/" + path == "" -> "/"
             * selfpath == "/libvirt.service" + path == "" -> "/libvirt.service"
             * selfpath == "/libvirt.service" + path == "foo" -> "/libvirt.service/foo"
             */
            if (i == CGROUP_CONTROLLER_SYSTEMD) {
                this->controllers[i].placement = selfpath;
            } else {
                fs::path buildPath = selfpath;
                buildPath /= path;
                this->controllers[i].placement = buildPath;
            }
        }
    }
    return 0;
}

int CgroupBackendV1::ValidatePlacement()
{
    return 0;
}

int CgroupBackendV1::AddTask(pid_t pid, unsigned int taskflags)
{
    for (size_t i = CGROUP_CONTROLLER_CPU; i < CGROUP_CONTROLLER_LAST; i++) {
        /* Skip over controllers not mounted */
        if (this->controllers[i].mountPoint.empty())
            continue;
        
        if (!this->controllers[i].PlacementExist())
            continue;
      
        /* We must never add tasks in systemd's hierarchy
         * unless we're intentionally trying to move a
         * task into a systemd machine scope */
        if (i == CGROUP_CONTROLLER_SYSTEMD && !(taskflags & CGROUP_TASK_SYSTEMD))
            continue;

        if (SetCgroupValueI64(i, "tasks", pid) < 0)
            return -1;
    }
}

int CgroupBackendV1::HasEmptyTasks(int controller)
{
    int ret = -1;
    std::string content;

    ret = GetCgroupValueStr(controller, "tasks", &content);

    if (ret == 0 && content == "")
        ret = 1;

    return ret;
}

int CgroupBackendV1::SetOwner(uid_t uid, gid_t gid, int controllers)
{
    return 0;
}

int CgroupBackendV1::Remove()
{ 
    return 0;
}

int CgroupBackendV1::MakeGroup(unsigned int flags)
{
    return 0;
}

int CgroupBackendV1::DetectControllers(int controllers, int alreadyDetected = 0)
{
    return 0;
}


bool CgroupBackendV1::HasController(int controller)
{
    return this->controllers[controller].mountPoint != "";
}

int CgroupBackendV1::GetPathOfController(int controller, const std::string &key, std::string *path)
{
    if (controller != CGROUP_CONTROLLER_NONE && !HasController(controller)) {
        CGROUP_ERROR("Controller '" << CgroupV1ControllerTypeToString(controller) << "' is not available");
        return -1;
    }

    if (this->controllers[controller].placement  == "") {
        CGROUP_ERROR(GetBackendName() + " controller '" << CgroupV1ControllerTypeToString(controller) << "' is not enabled for group");
        return -1;
    }

    fs::path buildPath(this->GetBasePath(controller));
    buildPath /= key;

    *path = buildPath;

    return 0;
}


//////  CPU   //////


int CgroupBackendV1::SetCpuShares(unsigned long long shares)
{
    return SetCgroupValueU64(CGROUP_CONTROLLER_CPU, "cpu.shares", shares);
}

int CgroupBackendV1::SetCpuCfsPeriod(unsigned long long cfs_period)
{
    std::string str;

    /* The cfs_period should be greater or equal than 1ms, and less or equal
     * than 1s.
     */
    if (cfs_period < 1000 || cfs_period > 1000000) {
        CGROUP_ERROR("cfs_period '" << cfs_period << "' must be in range (1000, 1000000)");
        return -1;
    }

    return SetCgroupValueU64(CGROUP_CONTROLLER_CPU, "cpu.cfs_period_us", cfs_period);
}

int CgroupBackendV1::SetCpuCfsQuota(long long cfs_quota)
{
    /* The cfs_quota should be greater or equal than 1ms */
    if (cfs_quota >= 0 &&
        (cfs_quota < 1000 ||
         cfs_quota > ULLONG_MAX / 1000)) {
        CGROUP_ERROR("cfs_quota '" << cfs_quota << "' must be in range (1000, " << ULLONG_MAX / 1000 <<")");
        return -1;
    }

   return SetCgroupValueU64(CGROUP_CONTROLLER_CPU, "cpu.cfs_quota_us", cfs_quota);
}

unsigned long long CgroupBackendV1::GetCpuShares()
{
    unsigned long long value;
    if(GetCgroupValueU64(CGROUP_CONTROLLER_CPU, "cpu.shares", &value) < 0)
        return -1;
    return value;
}

unsigned long long CgroupBackendV1::GetCpuCfsPeriod()
{
    unsigned long long value;
    if(GetCgroupValueU64(CGROUP_CONTROLLER_CPU, "cpu.cfs_period_us", &value) < 0)
        return -1;

    return value;
}

long long CgroupBackendV1::GetCpuCfsQuota()
{
    unsigned long long value;
    if(GetCgroupValueU64(CGROUP_CONTROLLER_CPU, "cpu.cfs_quota_us", &value) < 0)
        return -1;

    return value;
}


////// Memory //////
/*
 * Retrieve the "memory.limit_in_bytes" value from the memory controller
 * root dir. This value cannot be modified by userspace and therefore
 * is the maximum limit value supported by cgroups on the local system.
 * Returns this value scaled to KB or falls back to the original
 * VIR_DOMAIN_MEMORY_PARAM_UNLIMITED. Either way, remember the return
 * value to avoid unnecessary cgroup filesystem access.
 */
void CgroupBackendV1::MemoryInit()
{
    memoryUnlimitedKB = CGROUP_MEMORY_PARAM_UNLIMITED;
    try
    {
        CgroupBackendV1 rootGroup("/");
        unsigned long long int mem_unlimited = 0ULL;

        if (!HasController(CGROUP_CONTROLLER_MEMORY))
            return;

        rootGroup.GetCgroupValueU64(CGROUP_CONTROLLER_MEMORY, "memory.limit_in_bytes", &mem_unlimited);
        memoryUnlimitedKB = mem_unlimited >> 10;
    }
    catch(const std::exception& e)
    {
        CGROUP_ERROR(e.what());
    }
}

int CgroupBackendV1::SetMemoryLimit(const std::string &keylimit, unsigned long long kb)
{
    unsigned long long maxkb = CGROUP_MEMORY_PARAM_UNLIMITED;

    if (kb > maxkb) {
        CGROUP_ERROR("Memory '" << kb << "' must be less than " << maxkb);
        return -1;
    }

    if (kb == maxkb) {
        return SetCgroupValueI64(CGROUP_CONTROLLER_MEMORY, keylimit, -1);
    } else {
        return SetCgroupValueU64(CGROUP_CONTROLLER_MEMORY, keylimit, kb << 10);
    }
}

int CgroupBackendV1::GetMemoryLimit(const std::string &keylimit, unsigned long long *kb)
{
    long long unsigned int limit_in_bytes;

    if (GetCgroupValueU64(CGROUP_CONTROLLER_MEMORY, keylimit, &limit_in_bytes) < 0)
        return -1;

    *kb = limit_in_bytes >> 10;
    if (*kb >= memoryUnlimitedKB)
        *kb = CGROUP_MEMORY_PARAM_UNLIMITED;

    return 0;
}

int CgroupBackendV1::SetMemory(unsigned long long kb)
{
    return SetMemoryLimit("memory.max", kb);
}
int CgroupBackendV1::GetMemoryStat(unsigned long long *cache,
                        unsigned long long *activeAnon,
                        unsigned long long *inactiveAnon,
                        unsigned long long *activeFile,
                        unsigned long long *inactiveFile,
                        unsigned long long *unevictable)
{
    return 0;
}

int CgroupBackendV1::GetMemoryUsage(unsigned long *kb)
{
    unsigned long long usage_in_bytes;
    int ret = GetCgroupValueU64(CGROUP_CONTROLLER_MEMORY,
                                   "memory.usage_in_bytes", &usage_in_bytes);
    if (ret == 0)
        *kb = (unsigned long) usage_in_bytes >> 10;
    return ret;
}
int CgroupBackendV1::SetMemoryHardLimit(unsigned long long kb)
{
    return SetMemoryLimit("memory.limit_in_bytes", kb);
}
int CgroupBackendV1::GetMemoryHardLimit(unsigned long long *kb)
{
    return GetMemoryLimit("memory.limit_in_bytes", kb);
}

int CgroupBackendV1::SetMemorySoftLimit(unsigned long long kb)
{
    return SetMemoryLimit("memory.soft_limit_in_bytes", kb);
}

int CgroupBackendV1::GetMemorySoftLimit(unsigned long long *kb)
{
    return GetMemoryLimit("memory.soft_limit_in_bytes", kb);
}

int CgroupBackendV1::SetMemSwapHardLimit(unsigned long long kb)
{
    return SetMemoryLimit("memory.memsw.limit_in_bytes", kb);
}
int CgroupBackendV1::GetMemSwapHardLimit(unsigned long long *kb)
{
    return GetMemoryLimit("memory.memsw.limit_in_bytes", kb);
}
int CgroupBackendV1::GetMemSwapUsage(unsigned long long *kb)
{
    unsigned long long usage_in_bytes;
    int ret = GetCgroupValueU64(CGROUP_CONTROLLER_MEMORY,
                                   "memory.memsw.usage_in_bytes", &usage_in_bytes);
    if (ret == 0)
        *kb = (unsigned long) usage_in_bytes >> 10;
    return ret;
}