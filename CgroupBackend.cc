#include "CgroupBackend.hh"
#include "Enum.hh"

#include <unistd.h>
#include <mntent.h>
#include <sys/mount.h>
#include <fstream>
#include <iostream>
#include <experimental/filesystem> // TODO: remove experimental
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

// trim from start (in place)
static inline void ltrim(std::string &s) {
    s.erase(s.begin(), std::find_if(s.begin(), s.end(),
            std::not1(std::ptr_fun<int, int>(std::isspace))));
}

// trim from end (in place)
static inline void rtrim(std::string &s) {
    s.erase(std::find_if(s.rbegin(), s.rend(),
            std::not1(std::ptr_fun<int, int>(std::isspace))).base(), s.end());
}

// trim from both ends (in place)
static inline void trim(std::string &s) {
    ltrim(s);
    rtrim(s);
}

template <class Container>
static void SplitString(const std::string& str, Container& cont, const std::string& delims = " ")
{
    std::size_t current, previous = 0;  
    current = str.find_first_of(delims);
    while (current != std::string::npos) {
        cont.push_back(str.substr(previous, current - previous));
        previous = current + 1;
        current = str.find_first_of(delims, previous);
    }
    cont.push_back(str.substr(previous, current - previous));
}

using namespace mdsd;
namespace fs = std::experimental::filesystem;

CGROUP_ENUM_DECL(CgroupV2Controller);
CGROUP_ENUM_IMPL(CgroupV2Controller,
              CGROUP_CONTROLLER_LAST,
              "cpu", "cpuacct", "cpuset", "memory", "devices",
              "freezer", "io", "net_cls", "pids", "rdma", "perf_event", "name=systemd",
);

CgroupBackend::CgroupBackend()
{
    this->Init();
}

CgroupBackend::CgroupBackend(const std::string &placement): placement(placement) 
{
    this->Init();
}

void CgroupBackend::Init()
{
    if (!this->Available())
        throw std::runtime_error("CgroupV2 not found, make sure your have mounted Cgroup on your system");
    
    this->DetectMounts();

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

int CgroupBackend::FileReadAll(const std::string &path, std::string &output)
{
    std::ifstream file(path, std::fstream::in);
    if (!file.good())
    {
        CGROUP_ERROR("This path '" << path << "' was not found");
        return -1;
    }
    getline(file, output, '\n');
    file.close();
    return 0;
}

int CgroupBackend::FileWriteStr(const std::string &path, const std::string &buffer)
{
    int ret = 0;
    try
    {
        std::ofstream out(path, std::fstream::out);
        out << buffer;
        out.close();
    }
    catch(const std::exception& e)
    {
        CGROUP_ERROR(e.what());
        ret = -1;
    }
    
    return ret;
}


/* We're looking for one 'cgroup2' fs mount which has some
 * controllers enabled. */
bool CgroupBackend::Available()
{
    bool ret = false;
    FILE *mounts = NULL;
    struct mntent entry;
    char buf[CGROUP_MAX_VAL];
    std::string contStr;

    if (!(mounts = fopen(PROC_MOUNTS_PATH, "r")))
        return false;
    try
    {    
        while (getmntent_r(mounts, &entry, buf, sizeof(buf)) != NULL)
        {
            if (strcmp(entry.mnt_type, "cgroup2") != 0)
                continue;

            /* Systemd uses cgroup v2 for process tracking but no controller is
            * available. We should consider this configuration as cgroup v2 is
            * not available. */
            std::string contFile = std::string(entry.mnt_dir) + "/cgroup.controllers";
            FileReadAll(contFile, contStr);
        
            if (contStr == "")
                continue;

            ret = true;
            break;
        }

    }
    catch(const std::exception& e)
    {
        CGROUP_ERROR(e.what());
    }

    if (mounts)
        fclose(mounts);
    return ret;
}

int CgroupBackend::DetectMounts(const char *mntType, const char *mntOpts, const char *mntDir)
{
    if (strcmp(mntType, "cgroup2") != 0)
        return 0;

    this->mountPoint = std::string(mntDir);

    CGROUP_DEBUG("mountPoint=" << mountPoint);

    return 0;
}

int CgroupBackend::DetectMounts()
{
    FILE *mounts = NULL;
    struct mntent entry;
    char buf[CGROUP_MAX_VAL];
    int ret = -1;
    size_t i;

    mounts = fopen(PROC_MOUNTS_PATH, "r");
    if (mounts == NULL) {
        CGROUP_DEBUG("errno=" << errno <<" Unable to open " << PROC_MOUNTS_PATH);
        return -1;
    }

    try
    {
        while (getmntent_r(mounts, &entry, buf, sizeof(buf)) != NULL)
        {
            if(this->DetectMounts(entry.mnt_type, entry.mnt_opts, entry.mnt_dir) < 0)
                break;
        }

        ret = 0;
    }
    catch(const std::exception& e)
    {
        CGROUP_ERROR(e.what());
    }

    if (mounts)
        fclose(mounts);
    return ret;
}

int CgroupBackend::DetectPlacement(const std::string &path,
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

int CgroupBackend::ValidatePlacement()
{
    if (this->placement == "") {
        CGROUP_ERROR("Could not find placement for v2 controller");
        return -1;
    }

    return 0;
}

int CgroupBackend::SetCpuShares(unsigned long long shares)
{
    return SetCgroupValueU64(CGROUP_CONTROLLER_CPU, "cpu.weight", shares);
}

int CgroupBackend::GetCpuShares(unsigned long long *shares)
{
    return GetCgroupValueU64(CGROUP_CONTROLLER_CPU, "cpu.weight", shares);
}

int CgroupBackend::SetCgroupValueU64(int controller, const std::string& key, unsigned long long int value)
{
    auto strval = std::to_string(value);
    return SetCgroupValueStr(controller, key, strval);
}

int CgroupBackend::SetCgroupValueI64(int controller, const std::string& key, long long int value)
{
    auto strval = std::to_string(value);
    return SetCgroupValueStr(controller, key, strval);
}

int CgroupBackend::SetCgroupValueStr(int controller, const std::string& key, const std::string& value)
{
    std::string keypath = "";

    if (GetPathOfController(controller, key, &keypath) < 0)
        return -1;

    return SetCgroupValueRaw(keypath, value);
}

int CgroupBackend::SetCgroupValueRaw(const std::string &path, const std::string& value)
{
    CGROUP_DEBUG("Set value '" << path <<"' to '" << value << "'");
    if (FileWriteStr(path, value) < 0)
    {
        const char* tmp = std::strrchr(path.c_str(), '/');
        if (errno == EINVAL && tmp)
        {
            CGROUP_ERROR("Invalid value '" << value << "' for '"<< tmp + 1 << "'");
            return -1;
        }
        CGROUP_ERROR("Unable to write to '" << path << "'");
        return -1;
    }

    return 0;
}


int CgroupBackend::GetCgroupValueU64(int controller, const std::string &key, unsigned long long int *value)
{
    std::string strval = "";
    if (GetCgroupValueStr(controller, key, &strval) < 0)
        return -1;

    *value = std::stoull(strval);
    return 0;
}

int CgroupBackend::GetCgroupValueI64(int controller, const std::string &key, long long int *value)
{
    std::string strval = "";
    if (GetCgroupValueStr(controller, key, &strval) < 0)
        return -1;

    *value = std::stoll(strval);
    return 0;
}
int CgroupBackend::GetCgroupValueStr(int controller, const std::string &key, std::string* value)
{
    std::string keypath = "";

    if (GetPathOfController(controller, key, &keypath) < 0)
        return -1;

    return GetCgroupValueRaw(keypath, value);
}
int CgroupBackend::GetCgroupValueRaw(const std::string &path, std::string* value)
{
    int rc;

    CGROUP_DEBUG("Get value " << path);
    
    FileReadAll(path, *value);

    /* Terminated with '\n' has sometimes harmful effects to the caller */
    int size = value->size();
    if (size > 0 && (*value)[size - 1] == '\n')
        (*value)[size - 1] = '\0';

    return 0;
}


int CgroupBackend::ParseControllersFile()
{
    int rc;
    std::string controllerStr;
    std::vector<std::string> controllerList;
    char **tmp;

    fs::path controllerFile = this->mountPoint;
    controllerFile /= this->placement;
    controllerFile /= "cgroup.controllers";

    if (FileReadAll(controllerFile, controllerStr) < 0)
        return -1;

    CGROUP_DEBUG("Reading from controler path " << controllerFile << " => '" << controllerStr << "'");
    trim(controllerStr);
    SplitString(controllerStr, controllerList);

    if (controllerList.empty())
        return -1;

    for (int i = 0; i < controllerList.size(); i++)
    {
        int type = CgroupV2ControllerTypeFromString(controllerList[i].c_str());
        if (type >= 0)
            this->controllers |= 1 << type;
    }

    controllerList.clear();
    return 0;
}


int CgroupBackend::DetectControllers(int controllers, int alreadyDetected = 0)
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
bool CgroupBackend::HasController(int controller)
{
    return this->controllers & (1 << controller);
}

int CgroupBackend::GetPathOfController(int controller, const std::string &key, std::string *path)
{
    if (!HasController(controller)) {
        CGROUP_ERROR("v2 controller '" << CgroupV2ControllerTypeToString(controller) << "' is not available");
        return -1;
    }

    fs::path buildPath = this->mountPoint;
    buildPath /= this->placement;
    buildPath /= key;

    *path = buildPath;
    CGROUP_DEBUG("GetPathOfController:() path=" << *path);

    return 0;
}

////// Memory //////
int CgroupBackend::SetMemory(unsigned long long kb)
{
    unsigned long long maxkb = CGROUP_MEMORY_PARAM_UNLIMITED;

    if (kb > maxkb) {
        CGROUP_ERROR("Memory '" << kb <<"' must be less than " << maxkb);
        return -1;
    }

    if (kb == maxkb) {
        return SetCgroupValueStr(CGROUP_CONTROLLER_MEMORY, "memory.max", "max");
    } else {
        return SetCgroupValueU64(CGROUP_CONTROLLER_MEMORY, "memory.max", kb << 10);
    }
}
int CgroupBackend::GetMemoryStat(unsigned long long *cache,
                        unsigned long long *activeAnon,
                        unsigned long long *inactiveAnon,
                        unsigned long long *activeFile,
                        unsigned long long *inactiveFile,
                        unsigned long long *unevictable)
{
    return 0;
}
int CgroupBackend::GetMemoryUsage(unsigned long *kb)
{
    unsigned long long usage_in_bytes;
    int ret = GetCgroupValueU64(CGROUP_CONTROLLER_MEMORY,
                                   "memory.current", &usage_in_bytes);
    if (ret == 0)
        *kb = (unsigned long) usage_in_bytes >> 10;
    return ret;
}
int CgroupBackend::SetMemoryHardLimit(unsigned long long kb)
{
    return 0;
}
int CgroupBackend::GetMemoryHardLimit(unsigned long long *kb)
{
    return 0;
}
int CgroupBackend::SetMemorySoftLimit(unsigned long long kb)
{
    return 0;
}
int CgroupBackend::GetMemorySoftLimit(unsigned long long *kb)
{
    return 0;
}
int CgroupBackend::SetMemSwapHardLimit(unsigned long long kb)
{
    return 0;
}
int CgroupBackend::GetMemSwapHardLimit(unsigned long long *kb)
{
    return 0;
}
int CgroupBackend::GetMemSwapUsage(unsigned long long *kb)
{
    return 0;
}