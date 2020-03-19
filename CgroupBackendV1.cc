#include "CgroupBackendV1.hh"
#include "EnumToString.hh"

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
using namespace std;
namespace fs = std::experimental::filesystem;

/* this should match the enum CgroupController */
CGROUP_ENUM_DECL(CgroupV1Controller);
CGROUP_ENUM_IMPL(CgroupV1Controller,
              CGROUP_CONTROLLER_LAST,
              "", "cpu", "cpuacct", "cpuset", "memory", "devices",
              "freezer", "blkio", "net_cls", "pids", "rdma", "perf_event", "name=systemd"
);

CGROUP_ENUM_IMPL(CgroupV1ControllerFile,
              CGROUP_CONTROLLER_FILE_LAST,
              "tasks", "cgroup.threads",
              "cpuacct.usage", "cpu.shares", "cpu.cfs_period_us", "cpu.cfs_quota_us",
              "memory.usage_in_bytes", "memory.limit_in_bytes", "memory.soft_limit_in_bytes",
              "memory.memsw.usage_in_bytes", "memory.memsw.limit_in_bytes", "memory.memsw.soft_limit_in_bytes",
);

CgroupBackendV1::CgroupBackendV1(const std::string &placement)
    : placement(placement), CgroupBackend(CGROUP_BACKEND_TYPE_V1, placement)
{
    for (size_t i = 0; i < CGROUP_CONTROLLER_FILE_LAST; i++) {
        backendControllerFileMap[GetBackendType()][i] = CgroupV1ControllerFileTypeToString(i);
    }
}

std::string CgroupBackendV1::GetBasePath(int controller)
{
    fs::path base;
    if (controller == CGROUP_CONTROLLER_NONE)
    {
        base = fs::path(CGROUP_ROOT_PATH);
        base /= GetRelativePlacement(this->placement);
    } else {
        base = fs::path(this->controllers[controller].mountPoint);
        base /= this->controllers[controller].placement;
    }
    
    return base;
}

std::string CgroupBackendV1::GetRelativeBasePath(int controller)
{
    fs::path path;
    if (controller == CGROUP_CONTROLLER_NONE)
    {
        path = GetRelativePlacement(this->placement);
    } else {
        path = this->controllers[controller].placement;
    }
    
    return path;
}


std::string CgroupBackendV1::GetControllerName(int controller)
{
    return CgroupV1ControllerTypeToString(controller);
}


void CgroupBackendV1::Init()
{
    CgroupBackend::Init();
    this->placement = GetRelativePlacement(this->placement);
    if (starts_with(this->placement, "/"))
        this->placement[0] = ' ';
    boost::trim(this->placement);
   
    for (size_t i = CGROUP_CONTROLLER_CPU; i < CGROUP_CONTROLLER_LAST; i++) {
        auto controllerName = GetControllerName(i);
        this->controllers[i].controller = static_cast<CgroupController>(i);
        auto mountPoint = this->controllers[i].mountPoint;

        // TODO: handle the case with "cpu,cpuacct"
        if (starts_with(this->placement, controllerName)) {
            this->controllers[i].placement = replace_all_copy(this->placement, controllerName, "");
            // CGROUP_DEBUG("controller=" + controllerName +" placement=" + this->controllers[i].placement + " mountPoint=" + mountPoint);
            // strip cgroup root path and controller name from placement
            this->placement = this->controllers[i].placement;
        }
    }

    // CGROUP_DEBUG("this->placement=" << this->placement );
}

int CgroupBackendV1::ResolveMountLink(const char *mntDir, const std::string& typeStr,
                            CgroupBackendV1Controller *controller)
{
    return 0;
}

int CgroupBackendV1::MountOptsMatchController(const std::string &mntOpts, const std::string& typeStr)
{
    std::vector<std::string> optList;
    split(optList, mntOpts, is_any_of(","));

    auto result = std::find(std::begin(optList), std::end(optList), typeStr);

    return result != std::end(optList);
}

int CgroupBackendV1::DetectMounts(const char *mntType, const char *mntOpts, const char *mntDir)
{
    if (strcmp(mntType, GetBackendName().c_str()) != 0)
        return 0;

    for (size_t i = CGROUP_CONTROLLER_CPU; i < CGROUP_CONTROLLER_LAST; i++)
    {
         auto typestr = GetControllerName(i);

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
   for (size_t i = CGROUP_CONTROLLER_CPU; i < CGROUP_CONTROLLER_LAST; i++)
   {
        auto typestr = GetControllerName(i);
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
            // this->controllers[i].Print();
        }
    }
    return 0;
}

int CgroupBackendV1::ValidatePlacement()
{
    return 0;
}

void CgroupBackendV1::AddTask(pid_t pid, unsigned int taskflags)
{
    for (size_t i = CGROUP_CONTROLLER_CPU; i < CGROUP_CONTROLLER_LAST; i++) {
        /* Skip over controllers not mounted */
        if (!HasController(i))
            continue;
        
        if (!this->controllers[i].PlacementExist())
            continue;

        if (!this->controllers[i].Enabled())
            continue;

        /* We must never add tasks in systemd's hierarchy
         * unless we're intentionally trying to move a
         * task into a systemd machine scope */
        if (i == CGROUP_CONTROLLER_SYSTEMD && !(taskflags & CGROUP_TASK_SYSTEMD))
            continue;

        SetCgroupValueI64(i, "tasks", pid);
    }
}

bool CgroupBackendV1::HasEmptyTasks(int controller)
{
    if (GetCgroupValueStr(controller, "tasks") == "")
        return false;

    return true;
}

void CgroupBackendV1::Remove()
{
    for (size_t i = CGROUP_CONTROLLER_CPU; i < CGROUP_CONTROLLER_LAST; i++)
    {
        auto controller = this->controllers[i];
        if (!this->HasController(i))
            continue;
        /* Don't delete the root group, if we accidentally
            ended up in it for some reason */
        if (controller.placement == "/" || controller.placement == "")
            return;

        fs::path grppath = this->GetBasePath(i);
        std::uintmax_t n = fs::remove_all(grppath);
        CGROUP_DEBUG("Deleted " << n << " files or directories here " << grppath);
    }
}

// should be probably virtual pure
void CgroupBackendV1::SetOwner(uid_t uid, gid_t gid, int controllers)
{
    for (size_t controller = CGROUP_CONTROLLER_CPU; controller < CGROUP_CONTROLLER_LAST; controller++)
    {
        if (this->controllers->Enabled())
            CgroupBackend::SetOwner(uid, gid, controller);
    }
}
void CgroupBackendV1::MakeGroup(unsigned int flags)
{
    for (size_t controller = CGROUP_CONTROLLER_CPU; controller < CGROUP_CONTROLLER_LAST; controller++)
    {
        auto cont = this->controllers[controller];
        // CGROUP_DEBUG("trying to create controller path of " << controller << " mounted=" << cont.mountPoint);
        // Skip over controllers that aren't mounted
        if (!this->HasController(controller))
            continue;

        // Controllers that are implicitly enabled if available.
        if (//controller == CGROUP_CONTROLLER_CPUACCT ||
            controller == CGROUP_CONTROLLER_DEVICES ||
            controller == CGROUP_CONTROLLER_SYSTEMD)
            continue;

        // Skip over controllers that aren't enabled
        if (!this->controllers[controller].Enabled())
            continue;

        try
        {
            auto path = fs::path(GetPathOfController(controller, ""));

            // CGROUP_DEBUG("Make group " << path << " perms:"  << static_cast<int>(fs::perms::all));

            auto fstatus = fs::status(path);

            if (!fs::exists(fstatus) && !fs::create_directory(path))
                throw CGroupFileNotFoundException(std::to_string(errno) + "Failed to create v1 cgroup " + path.string());

        } catch (CGroupBaseException ex) {
            CGROUP_ERROR("failed to enable '" << GetControllerName(controller) << "' controller, error:" << ex.what());
        }
    }
}

int CgroupBackendV1::DetectControllers(int controllers, int alreadyDetected)
{
    for (size_t i = CGROUP_CONTROLLER_CPU; i < CGROUP_CONTROLLER_LAST; i++)
    {
        bool enableController = controllers & (1 << i);

        if (enableController) {
            this->controllers[i].placement = this->placement;
        }
    }
    return 0;
}

bool CgroupBackendV1::HasController(int controller)
{
    return this->controllers[controller].mountPoint != "";
}

std::string CgroupBackendV1::GetPathOfController(int controller, const std::string &key)
{
    if (controller != CGROUP_CONTROLLER_NONE && !HasController(controller))
        throw CGroupControllerNotFoundException("Controller '" + GetControllerName(controller) + "' is not available");

    if (!this->controllers[controller].Enabled())
        throw CGroupControllerNotFoundException(GetBackendName() + " controller '" +
                GetControllerName(controller) + "' is not enabled for group");

    fs::path buildPath(this->GetBasePath(controller));
    buildPath /= key;

    return buildPath;
}


//////  CPU   //////

void CgroupBackendV1::SetCpuCfsPeriod(unsigned long long cfs_period)
{
    this->ValidateCPUCfsPeiod(cfs_period);

    SetCgroupValueU64(CGROUP_CONTROLLER_CPU, "cpu.cfs_period_us", cfs_period);
}

void CgroupBackendV1::SetCpuCfsQuota(long long cfs_quota)
{
    this->ValidateCPUCfsQuota(cfs_quota);
    SetCgroupValueU64(CGROUP_CONTROLLER_CPU, "cpu.cfs_quota_us", cfs_quota);
}

unsigned long long CgroupBackendV1::GetCpuCfsPeriod()
{
    return GetCgroupValueU64(CGROUP_CONTROLLER_CPU, "cpu.cfs_period_us");
}

long long CgroupBackendV1::GetCpuCfsQuota()
{
    return GetCgroupValueU64(CGROUP_CONTROLLER_CPU, "cpu.cfs_quota_us");
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

        mem_unlimited = rootGroup.GetCgroupValueU64(CGROUP_CONTROLLER_MEMORY, "memory.limit_in_bytes");
        memoryUnlimitedKB = mem_unlimited >> 10;
    }
    catch(const std::exception& e)
    {
        CGROUP_ERROR(e.what());
    }
}

void CgroupBackendV1::SetMemoryLimitInKB(const std::string &keylimit, unsigned long long kb)
{
    unsigned long long maxkb = CGROUP_MEMORY_PARAM_UNLIMITED;

    if (kb > maxkb) {
        throw CGroupMemoryException("Memory '" + std::to_string(kb) + "' must be less than " + std::to_string(maxkb));
    }

    if (kb == maxkb) {
        SetCgroupValueI64(CGROUP_CONTROLLER_MEMORY, keylimit, -1);
    } else {
        SetCgroupValueU64(CGROUP_CONTROLLER_MEMORY, keylimit, kb << 10);
    }
}

unsigned long long CgroupBackendV1::GetMemoryLimitInKB(const std::string &keylimit)
{
    long long unsigned int kb = GetCgroupValueU64(CGROUP_CONTROLLER_MEMORY, keylimit) >> 10;

    if (kb >= CGROUP_MEMORY_PARAM_UNLIMITED)
        kb = CGROUP_MEMORY_PARAM_UNLIMITED;

    return kb;
}


