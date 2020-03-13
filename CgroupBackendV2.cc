#include "CgroupBackendV2.hh"
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
#include <functional> 
#include <cctype>
#include <locale>

using namespace mdsd;
namespace fs = std::experimental::filesystem;

/* this should match the enum CgroupController */
CGROUP_ENUM_DECL(CgroupV2Controller);
CGROUP_ENUM_IMPL(CgroupV2Controller,
              CGROUP_CONTROLLER_LAST,
              "", "cpu", "cpuacct", "cpuset", "memory", "devices",
              "freezer", "io", "net_cls", "pids", "rdma", "perf_event", "name=systemd"
);

CgroupBackendV2::CgroupBackendV2(const std::string &placement)
    : placement(placement), CgroupBackend(CGROUP_BACKEND_TYPE_V2, placement)
{
}

std::string CgroupBackendV2::GetBasePath(int controller)
{
    fs::path base(this->mountPoint);
    base /= this->placement;

    return base;
}

bool CgroupBackendV2::IsCgroupCreated()
{
    fs::file_status status = fs::status(this->GetBasePath());
    return fs::exists(status);
}

void CgroupBackendV2::Init()
{
    CgroupBackend::Init();
    
    /* 
        Detect if placement started with mountPoint
        Example if placement="/sys/fs/cgroup/TEST" and mountPoint="/sys/fs/cgroup" => placement="/TEST"
        if placement="TEST" and mountPoint="/sys/fs/cgroup" => keep placement="TEST"
    */
    if (this->placement.substr(0, this->mountPoint.size()) == this->mountPoint) {
        this->placement = this->placement.substr(this->mountPoint.size());
    }

    this->ParseControllersFile();
}

int CgroupBackendV2::DetectMounts(const char *mntType, const char *mntOpts, const char *mntDir)
{
    if (strcmp(mntType, CgroupBackend::GetBackendName().c_str()) != 0)
        return 0;

    this->mountPoint = std::string(mntDir);
    // CGROUP_DEBUG("mountPoint=" << mountPoint);

    return 0;
}

int CgroupBackendV2::DetectPlacement(const std::string &path,
    const std::string &controllers,
    const std::string &selfpath)
{
    if (this->placement != "")
        return 0;

    // CGROUP_DEBUG("path=%s controllers=%s selfpath=%s\n", path.c_str(), controllers.c_str(), selfpath.c_str());

    /* controllers == "" indicates the cgroupv2 controller path */
    if (controllers != "")
        return 0;

    /*
     * selfpath == "/"                  + path == ""    -> "/"
     * selfpath == "/libvirt.service"   + path == ""    -> "/libvirt.service"
     * selfpath == "/libvirt.service"   + path == "foo" -> "/libvirt.service/foo"
     */
    this->placement = selfpath + std::string(selfpath == "/" || path == "" ? "" : "/") + path;

    return 0;
}

int CgroupBackendV2::ValidatePlacement()
{
    if (this->placement == "") {
        CGROUP_ERROR("Could not find placement for v2 controller");
        return -1;
    }

    return 0;
}

int CgroupBackendV2::AddTask(pid_t pid, unsigned int taskflags)
{
    if (taskflags & CGROUP_TASK_THREAD)
        return SetCgroupValueI64(CGROUP_CONTROLLER_NONE, "cgroup.threads", pid);
    else
        return SetCgroupValueI64(CGROUP_CONTROLLER_NONE, "cgroup.procs", pid);
}

int CgroupBackendV2::HasEmptyTasks(int controller)
{
    int ret = -1;
    std::string content;

    ret = GetCgroupValueStr(controller, "cgroup.procs", &content);

    if (ret == 0 && content == "")
        ret = 1;

    return ret;
}

int CgroupBackendV2::SetOwner(uid_t uid, gid_t gid, int controllers)
{
    auto base = this->GetBasePath();
    
    // Change ownership of all regular files in a directory. 
    for (const auto & entry : fs::directory_iterator(base))
    {
        auto status = fs::status(entry.path());
        if (fs::is_regular_file(status) && chown(entry.path().c_str(), uid, gid) < 0) {
            CGROUP_ERROR("errno:" << errno << ", cannot chown '"<< entry.path()
                        <<"' to (" << uid << ", " << gid << ")");
            return -1;
        }
    }

    // Change ownership of the cgroup directory.
    if (chown(base.c_str(), uid, gid) < 0) {
        CGROUP_ERROR("errno:" << errno << ", cannot chown '"<< base 
                <<"' to (" << uid << ", " << gid << ")");
        return -1;
    }

    return 0;
}

int CgroupBackendV2::Remove()
{
    /* Don't delete the root group, if we accidentally
       ended up in it for some reason */
    if (this->placement == "/" || this->placement == "")
        return 0;

    try
    {
        fs::path grppath = this->GetBasePath();
        std::uintmax_t n = fs::remove_all(grppath);
        this->controllers = 0;
        CGROUP_DEBUG("Deleted " << n << " files or directories here " << grppath);
    }
    catch(const std::exception& e)
    {
        CGROUP_ERROR(e.what());
        return -1;
    }
    
    return 0;
}


/**
 * EnableSubtreeControllerCgroupV2:
 *
 * Returns: -1 on fatal error
 *          -2 if we failed to write into cgroup.subtree_control
 *          0 on success
 */
int CgroupBackendV2::EnableSubtreeControllerCgroupV2(int controller)
{
    std::string path;
    std::string val = std::string("+") + CgroupV2ControllerTypeToString(controller);

    if (GetPathOfController(controller, "cgroup.subtree_control", &path) < 0) {
        return -1;
    }

    CGROUP_DEBUG("Enable sub controller '" << val << "' for '" << path << "', has controller: " << HasController(controller));
    if (FileWriteStr(path, val) < 0) {
        CGROUP_ERROR(errno << " Failed to enable controller '" << val << "' for '" << path << "'");
        return -2;
    }

    return 0;
}

/**
 * EnableSubtreeControllerCgroupV2:
 *
 * Returns: -1 on fatal error
 *          -2 if we failed to write into cgroup.subtree_control
 *          0 on success
 */
int CgroupBackendV2::DisableSubtreeControllerCgroupV2(int controller)
{
    std::string path;
    std::string val = std::string("-") + CgroupV2ControllerTypeToString(controller);

    if (GetPathOfController(controller, "cgroup.subtree_control", &path) < 0) {
        return -1;
    }

    if (FileWriteStr(path, val) < 0) {
        CGROUP_ERROR(errno << " Failed to disable controller '" << val << "' for '" << path << "'");
        return -2;
    }

    return 0;
}

int CgroupBackendV2::MakeGroup(unsigned int flags)
{
    fs::path path(this->GetBasePath());
    int controller;

    if (flags & CGROUP_SYSTEMD) {
        CGROUP_DEBUG("Running with systemd so we should not create cgroups ourselves.");
        return 0;
    }

    CGROUP_DEBUG("Make group " << path << " perms:"  << static_cast<int>(fs::perms::all));

    auto fstatus = fs::status(path);
    
    if (!fs::exists(fstatus) && !fs::create_directory(path)) {
        CGROUP_ERROR(errno << "Failed to create v2 cgroup " << path);
        return -1;
    }

    auto parent = CgroupBackendV2(path.parent_path());
    parent.ParseControllersFile();

    for (size_t controller = 0; controller < CGROUP_CONTROLLER_LAST; controller++)
    {
        int rc;
        // if parent does not have the controller, then skip
        if (!parent.HasController(controller) || this->HasController(controller))
            continue;

        /* Controllers that are implicitly enabled if available. */
        if (controller == CGROUP_CONTROLLER_CPUACCT || controller == CGROUP_CONTROLLER_DEVICES)
            continue;
        
        rc = parent.EnableSubtreeControllerCgroupV2(controller);
        if (rc < 0) {
            if (rc == -2) {
                CGROUP_DEBUG("failed to enable '" << CgroupV2ControllerTypeToString(controller) << "' controller, skipping" );
                this->controllers &= ~(1 << controller);
                continue;
            }

            CGROUP_ERROR("failed to enable '" << CgroupV2ControllerTypeToString(controller) << "' controller, exiting" );
            return -1;
        }
    }

    // re-update controllers
    this->ParseControllersFile();

    return 0;
}


int CgroupBackendV2::ParseControllersFile()
{
    std::string controllerStr;
    std::vector<std::string> controllerList;

    if (!this->IsCgroupCreated())
        return 0;

    fs::path controllerFile(this->GetBasePath());
    controllerFile /= "cgroup.controllers";

    if (FileReadAll(controllerFile, controllerStr) < 0)
        return -1;

    CGROUP_DEBUG("Reading from controler path " << controllerFile << " => '" << controllerStr << "'");
    trim(controllerStr);
    splitstring(controllerStr, controllerList);

    if (controllerList.empty())
        return -1;

    this->controllers = CGROUP_CONTROLLER_NONE; // reset
    for (int i = 0; i < controllerList.size(); i++)
    {
        int type = CgroupV2ControllerTypeFromString(controllerList[i]);
        if (type >= 0)
            this->controllers |= 1 << type;
    }

    controllerList.clear();
    return 0;
}


int CgroupBackendV2::DetectControllers(int controllers, int alreadyDetected = 0)
{
    size_t i;

    /* In cgroup v2 there is no cpuacct controller, the cpu.stat file always
     * exists with usage stats. */
    this->controllers |= 1 << CGROUP_CONTROLLER_CPUACCT;

    if (controllers >= 0)
        this->controllers &= controllers;

    this->controllers &= ~alreadyDetected;

    for (i = 0; i < CGROUP_CONTROLLER_LAST; i++)
        CGROUP_DEBUG("Controller '"<< CgroupV2ControllerTypeToString(i) <<"' present=" << ((this->controllers & 1 << i) ? "yes" : "no"));

    return this->controllers;
}

//
bool CgroupBackendV2::HasController(int controller)
{
    return this->controllers & (1 << controller);
}

int CgroupBackendV2::GetPathOfController(int controller, const std::string &key, std::string *path)
{
    if (controller != CGROUP_CONTROLLER_NONE && !HasController(controller)) {
        CGROUP_ERROR("Controller '" << CgroupV2ControllerTypeToString(controller) << "' is not available");
        return -1;
    }

    fs::path buildPath(this->GetBasePath());
    buildPath /= key;

    *path = buildPath;

    return 0;
}


//////  CPU   //////


int CgroupBackendV2::SetCpuShares(unsigned long long shares)
{
    return SetCgroupValueU64(CGROUP_CONTROLLER_CPU, "cpu.weight", shares);
}

int CgroupBackendV2::SetCpuCfsPeriod(unsigned long long cfs_period)
{
    std::string str;

    /* The cfs_period should be greater or equal than 1ms, and less or equal
     * than 1s.
     */
    if (cfs_period < 1000 || cfs_period > 1000000) {
        CGROUP_ERROR("cfs_period '" << cfs_period << "' must be in range (1000, 1000000)");
        return -1;
    }

    if (GetCgroupValueStr(CGROUP_CONTROLLER_CPU, "cpu.max", &str) < 0) {
        return -1;
    }

    std::vector<std::string> strList;
    splitstring(str, strList, " ");

    if (strList.size() <= 1) {
        CGROUP_ERROR("Invalid 'cpu.max' data.");
        return -1;
    }

    std::string value = strList[0] + " " + std::to_string(cfs_period);

    return SetCgroupValueStr(CGROUP_CONTROLLER_CPU, "cpu.max", value);
}

int CgroupBackendV2::SetCpuCfsQuota(long long cfs_quota)
{
    /* The cfs_quota should be greater or equal than 1ms */
    if (cfs_quota >= 0 &&
        (cfs_quota < 1000 ||
         cfs_quota > ULLONG_MAX / 1000)) {
        CGROUP_ERROR("cfs_quota '" << cfs_quota << "' must be in range (1000, " << ULLONG_MAX / 1000 <<")");
        return -1;
    }

    if (cfs_quota == ULLONG_MAX / 1000) {
        return SetCgroupValueStr(CGROUP_CONTROLLER_CPU, "cpu.max", "max");
    }

    return SetCgroupValueI64(CGROUP_CONTROLLER_CPU, "cpu.max", cfs_quota);
}

unsigned long long CgroupBackendV2::GetCpuShares()
{
    unsigned long long value;
    if(GetCgroupValueU64(CGROUP_CONTROLLER_CPU, "cpu.weight", &value) < 0)
        return -1;
    return value;
}

unsigned long long CgroupBackendV2::GetCpuCfsPeriod()
{
    std::string str;

    if (GetCgroupValueStr(CGROUP_CONTROLLER_CPU, "cpu.max", &str) < 0)
        return -1;

    std::vector<std::string> strList;
    splitstring(str, strList, " ");

    if (strList.size() <= 1) {
        CGROUP_ERROR("Invalid 'cpu.max' data.");
        return -1;
    }

    return std::stoull(strList[1]);;
}

long long CgroupBackendV2::GetCpuCfsQuota()
{
    std::string str;

    if (GetCgroupValueStr(CGROUP_CONTROLLER_CPU, "cpu.max", &str) < 0)
        return -1;
    
    if (strncmp(str.c_str(), "max", 3) == 0) {
        return ULLONG_MAX / 1000;
    }

    std::vector<std::string> strList;
    splitstring(str, strList, " ");

    if (strList.size() <= 1) {
        CGROUP_ERROR("Invalid 'cpu.max' data.");
        return -1;
    }

    return stoll(strList[0]);
}


////// Memory //////

int CgroupBackendV2::SetMemoryLimit(const std::string &keylimit, unsigned long long kb)
{
    unsigned long long maxkb = CGROUP_MEMORY_PARAM_UNLIMITED;

    if (kb > maxkb) {
        CGROUP_ERROR("Memory '" << kb << "' must be less than " << maxkb);
        return -1;
    }

    if (kb == maxkb) {
        return SetCgroupValueStr(CGROUP_CONTROLLER_MEMORY, keylimit, "max");
    } else {
        return SetCgroupValueU64(CGROUP_CONTROLLER_MEMORY, keylimit, kb << 10);
    }
}

int CgroupBackendV2::GetMemoryLimit(const std::string &keylimit, unsigned long long *kb)
{
    unsigned long long value;
    std::string strval;

    if (GetCgroupValueStr(CGROUP_CONTROLLER_MEMORY, keylimit, &strval) < 0)
        return -1;

    if (strval == "max") {
        *kb = CGROUP_MEMORY_PARAM_UNLIMITED;
        return 0;
    }

    value = std::stoull(strval);

    *kb = value >> 10;
    if (*kb >= CGROUP_MEMORY_PARAM_UNLIMITED)
        *kb = CGROUP_MEMORY_PARAM_UNLIMITED;

    return 0;
}

int CgroupBackendV2::SetMemory(unsigned long long kb)
{
    return SetMemoryLimit("memory.max", kb);
}
int CgroupBackendV2::GetMemoryStat(unsigned long long *cache,
                        unsigned long long *activeAnon,
                        unsigned long long *inactiveAnon,
                        unsigned long long *activeFile,
                        unsigned long long *inactiveFile,
                        unsigned long long *unevictable)
{
    return 0;
}

int CgroupBackendV2::GetMemoryUsage(unsigned long *kb)
{
    unsigned long long usage_in_bytes;
    int ret = GetCgroupValueU64(CGROUP_CONTROLLER_MEMORY,
                                   "memory.current", &usage_in_bytes);
    if (ret == 0)
        *kb = (unsigned long) usage_in_bytes >> 10;
    return ret;
}
int CgroupBackendV2::SetMemoryHardLimit(unsigned long long kb)
{
    return SetMemoryLimit("memory.max", kb);
}
int CgroupBackendV2::GetMemoryHardLimit(unsigned long long *kb)
{
    return GetMemoryLimit("memory.max", kb);
}

int CgroupBackendV2::SetMemorySoftLimit(unsigned long long kb)
{
    return SetMemoryLimit("memory.high", kb);
}

int CgroupBackendV2::GetMemorySoftLimit(unsigned long long *kb)
{
    return GetMemoryLimit("memory.high", kb);
}

int CgroupBackendV2::SetMemSwapHardLimit(unsigned long long kb)
{
    return SetMemoryLimit("memory.swap.max", kb);
}
int CgroupBackendV2::GetMemSwapHardLimit(unsigned long long *kb)
{
    return GetMemoryLimit("memory.swap.max", kb);
}
int CgroupBackendV2::GetMemSwapUsage(unsigned long long *kb)
{
    unsigned long long usage_in_bytes;
    int ret = GetCgroupValueU64(CGROUP_CONTROLLER_MEMORY,
                                   "memory.swap.current", &usage_in_bytes);
    if (ret == 0)
        *kb = (unsigned long) usage_in_bytes >> 10;
    return ret;
}