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

CGROUP_ENUM_IMPL(CgroupV2ControllerFile,
              CGROUP_CONTROLLER_FILE_LAST,
              "cgroup.procs", "cgroup.threads",
              "cpu.stat", "cpu.weight", "cpu.max", "cpu.max",
              "memory.current", "memory.max", "memory.high",
              "memory.swap.current", "memory.swap.max", "memory.swap.high",
);

CgroupBackendV2::CgroupBackendV2(const std::string &placement)
    : placement(placement), CgroupBackend(CGROUP_BACKEND_TYPE_V2, placement)
{
    for (size_t i = 0; i < CGROUP_CONTROLLER_FILE_LAST; i++) {
        backendControllerFileMap[GetBackendType()][i] = CgroupV2ControllerFileTypeToString(i);        
    }
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

std::string CgroupBackendV2::GetControllerName(int controller)
{
    return CgroupV2ControllerTypeToString(controller);
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

void CgroupBackendV2::AddTask(pid_t pid, unsigned int taskflags)
{
    if (taskflags & CGROUP_TASK_THREAD)
        SetCgroupValueI64(CGROUP_CONTROLLER_NONE, "cgroup.threads", pid);
    else
        SetCgroupValueI64(CGROUP_CONTROLLER_NONE, "cgroup.procs", pid);
}

bool CgroupBackendV2::HasEmptyTasks(int controller)
{
    if (GetCgroupValueStr(controller, "cgroup.procs") == "")
        return false;

    return true;
}

void CgroupBackendV2::Remove()
{
    /* Don't delete the root group, if we accidentally
       ended up in it for some reason */
    if (this->placement == "/" || this->placement == "")
        return;

    fs::path grppath = this->GetBasePath();
    std::uintmax_t n = fs::remove_all(grppath);
    this->controllers = 0;
    CGROUP_DEBUG("Deleted " << n << " files or directories here " << grppath);
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
    std::string val = std::string("+") + GetControllerName(controller);
    std::string path = GetPathOfController(controller, "cgroup.subtree_control");
    FileWriteStr(path, val);

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
    std::string val = std::string("-") + GetControllerName(controller);
    std::string path = GetPathOfController(controller, "cgroup.subtree_control");
    FileWriteStr(path, val);

    return 0;
}

void CgroupBackendV2::MakeGroup(unsigned int flags)
{
    fs::path path(this->GetBasePath());
    int controller;

    if (flags & CGROUP_SYSTEMD)
        throw CGroupBaseException("Running with systemd so we should not create cgroups ourselves.");

    CGROUP_DEBUG("Make group " << path << " perms:"  << static_cast<int>(fs::perms::all));

    auto fstatus = fs::status(path);
    
    if (!fs::exists(fstatus) && !fs::create_directory(path))
        throw CGroupFileNotFoundException(std::to_string(errno) + "Failed to create v2 cgroup " + path.string());

    auto parent = CgroupBackendV2(path.parent_path());
    parent.ParseControllersFile();

    for (size_t controller = 0; controller < CGROUP_CONTROLLER_LAST; controller++)
    {
        // if parent does not have the controller, then skip
        if (!parent.HasController(controller) || this->HasController(controller))
            continue;

        /* Controllers that are implicitly enabled if available. */
        if (controller == CGROUP_CONTROLLER_CPUACCT || controller == CGROUP_CONTROLLER_DEVICES)
            continue;
        
        try
        {
            parent.EnableSubtreeControllerCgroupV2(controller);
        } catch (CGroupBaseException ex) {
            this->controllers &= ~(1 << controller);
            CGROUP_ERROR("failed to enable '" << GetControllerName(controller) << "' controller, error:" << ex.what());
        }
    }

    // re-update controllers
    this->ParseControllersFile();
}


int CgroupBackendV2::ParseControllersFile()
{
    std::string controllerStr;
    std::vector<std::string> controllerList;

    if (!this->IsCgroupCreated())
        return 0;

    fs::path controllerFile(this->GetBasePath());
    controllerFile /= "cgroup.controllers";

    controllerStr = FileReadAll(controllerFile);

    CGROUP_DEBUG("Parsing controlers from path " << controllerFile << " => '" << controllerStr << "'");
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
        CGROUP_DEBUG("Controller '"<< GetControllerName(i) <<"' present=" << ((this->controllers & 1 << i) ? "yes" : "no"));

    return this->controllers;
}

//
bool CgroupBackendV2::HasController(int controller)
{
    return this->controllers & (1 << controller);
}

std::string CgroupBackendV2::GetPathOfController(int controller, const std::string &key)
{
    if (controller != CGROUP_CONTROLLER_NONE && !HasController(controller))
        throw CGroupControllerNotFoundException("Controller '" + GetControllerName(controller) + "' is not available");

    fs::path buildPath(this->GetBasePath());
    buildPath /= key;

    return buildPath;
}


//////  CPU   //////

void CgroupBackendV2::SetCpuCfsPeriod(unsigned long long cfs_period)
{
    this->ValidateCPUCfsPeiod(cfs_period);

    std::string str = GetCgroupValueStr(CGROUP_CONTROLLER_CPU, "cpu.max");

    std::vector<std::string> strList;
    splitstring(str, strList, " ");
    if (strList.size() <= 1)
        throw CGroupCPUException("Invalid 'cpu.max' data.");

    std::string value = strList[0] + " " + std::to_string(cfs_period);

    SetCgroupValueStr(CGROUP_CONTROLLER_CPU, "cpu.max", value);
}

void CgroupBackendV2::SetCpuCfsQuota(long long cfs_quota)
{
    this->ValidateCPUCfsQuota(cfs_quota);

    if (cfs_quota == ULLONG_MAX / 1000) {
        SetCgroupValueStr(CGROUP_CONTROLLER_CPU, "cpu.max", "max");
    }

    SetCgroupValueI64(CGROUP_CONTROLLER_CPU, "cpu.max", cfs_quota);
}

unsigned long long CgroupBackendV2::GetCpuCfsPeriod()
{
    std::string str = GetCgroupValueStr(CGROUP_CONTROLLER_CPU, "cpu.max");

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
    std::string str = GetCgroupValueStr(CGROUP_CONTROLLER_CPU, "cpu.max");
    
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

void CgroupBackendV2::SetMemoryLimitInKB(const std::string &keylimit, unsigned long long kb)
{
    unsigned long long maxkb = CGROUP_MEMORY_PARAM_UNLIMITED;

    if (kb > maxkb) {
        throw CGroupMemoryException("Memory '" + std::to_string(kb) + "' must be less than " + std::to_string(maxkb));
    }

    if (kb == maxkb) {
        SetCgroupValueStr(CGROUP_CONTROLLER_MEMORY, keylimit, "max");
    } else {
        SetCgroupValueU64(CGROUP_CONTROLLER_MEMORY, keylimit, kb << 10);
    }
}

unsigned long long CgroupBackendV2::GetMemoryLimitInKB(const std::string &keylimit)
{
    unsigned long long value;
    std::string strval = GetCgroupValueStr(CGROUP_CONTROLLER_MEMORY, keylimit);

    if (strval == "max")
        return CGROUP_MEMORY_PARAM_UNLIMITED;

    value = std::stoull(strval) >> 10;
    if (value >= CGROUP_MEMORY_PARAM_UNLIMITED)
        value = CGROUP_MEMORY_PARAM_UNLIMITED;

    return value;
}

