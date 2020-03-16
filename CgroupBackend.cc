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


using namespace mdsd;
namespace fs = std::experimental::filesystem;

std::string CgroupBackend::backendControllerFileMap[CGROUP_BACKEND_TYPE_LAST-1][CGROUP_CONTROLLER_FILE_LAST];
/* this should match the enum CgroupController */
CGROUP_ENUM_DECL(CgroupBackendType);
CGROUP_ENUM_IMPL(CgroupBackendType, CGROUP_BACKEND_TYPE_LAST, "none", "cgroup2", "cgroup");

CgroupBackend::CgroupBackend(CgroupBackendType type, const std::string &placement)
    : backendType(type)
{
    backenName = CgroupBackendTypeTypeToString(backendType);
    // this->Init();
}

void CgroupBackend::Init()
{
    if (!this->Available())
        throw std::runtime_error(this->backenName + " not found, make sure your have mounted " + this->backenName + " on your system");
    
    this->DetectMounts();
}

std::string CgroupBackend::GetBackendName()
{
    return backenName;
}

CgroupBackendType CgroupBackend::GetBackendType()
{
    return backendType;
}

std::string CgroupBackend::GetControllerFileName(int controllerFileType)
{
    return CgroupBackend::backendControllerFileMap[this->backendType][controllerFileType];
}

/* We're looking for one 'cgroup' fs mount */
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
            if (strcmp(entry.mnt_type, this->backenName.c_str()) != 0)
                continue;

            if (this->backendType == CGROUP_BACKEND_TYPE_V2)
            {
                /* Systemd uses cgroup v2 for process tracking but no controller is
                * available. We should consider this configuration as cgroup v2 is
                * not available. */
                std::string contFile = std::string(entry.mnt_dir) + "/cgroup.controllers";
                contStr = FileReadAll(contFile);
            
                if (contStr == "")
                    continue;
            }

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


// virtual
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

bool CgroupBackend::HasEmptyTasks(int controller)
{
    return false;
}

// should be probably virtual pure
void CgroupBackend::SetOwner(uid_t uid, gid_t gid, int controllers)
{
    auto base = this->GetBasePath(controllers);
    
    // Change ownership of all regular files in a directory. 
    for (const auto & entry : fs::directory_iterator(base))
    {
        auto status = fs::status(entry.path());
        if (fs::is_regular_file(status) && chown(entry.path().c_str(), uid, gid) < 0) {
            throw CGroupBaseException("errno:" + std::to_string(errno) + ", cannot chown '" + entry.path().string()
                        + "' to (" + std::to_string(uid) + ", " + std::to_string(gid) + ")");
        }
    }

    // Change ownership of the cgroup directory.
    if (chown(base.c_str(), uid, gid) < 0) {
        throw CGroupBaseException("errno:" + std::to_string(errno) + ", cannot chown '" + base
                        + "' to (" + std::to_string(uid) + ", " + std::to_string(gid) + ")");
    }
}


void CgroupBackend::SetCgroupValueU64(int controller, const std::string& key, unsigned long long int value)
{
    auto strval = std::to_string(value);
    SetCgroupValueStr(controller, key, strval);
}

void CgroupBackend::SetCgroupValueI64(int controller, const std::string& key, long long int value)
{
    auto strval = std::to_string(value);
    SetCgroupValueStr(controller, key, strval);
}

void CgroupBackend::SetCgroupValueStr(int controller, const std::string& key, const std::string& value)
{
    SetCgroupValueRaw(GetPathOfController(controller, key), value);
}

void CgroupBackend::SetCgroupValueRaw(const std::string &path, const std::string& value)
{
    CGROUP_DEBUG("Set value '" << path <<"' to '" << value << "'");
    FileWriteStr(path, value);

    const char* tmp = std::strrchr(path.c_str(), '/');
    if (errno == EINVAL && tmp)
        throw CGroupBaseException("Invalid value '" + value + "' for '" + (tmp + 1) + "'");
}

unsigned long long int CgroupBackend::GetCgroupValueU64(int controller, const std::string &key)
{
    return std::stoull(GetCgroupValueStr(controller, key));
}

long long int CgroupBackend::GetCgroupValueI64(int controller, const std::string &key)
{
    return std::stoll(GetCgroupValueStr(controller, key));
}

std::string CgroupBackend::GetCgroupValueStr(int controller, const std::string &key)
{
    return GetCgroupValueRaw(GetPathOfController(controller, key));
}

std::string CgroupBackend::GetCgroupValueRaw(const std::string &path)
{
    CGROUP_DEBUG("Get value " << path);
    std::string value = FileReadAll(path);

    /* Terminated with '\n' has sometimes harmful effects to the caller */
    int size = value.size();
    if (size > 0 && value[size - 1] == '\n')
        value[size - 1] = '\0';

    return value;
}

std::string CgroupBackend::FileReadAll(const std::string &path)
{
    std::string output;
    fs::file_status status = fs::status(path);
    if (!fs::exists(status))
        throw CGroupFileNotFoundException("File '" + path + "' not found");
    // TODO: check if current user has READ permission to file in path
    // CGROUP_DEBUG("[FileReadAll] File '" << path << "' has perm=" << serialize_fileperms(status.permissions()));

    std::ifstream file(path, std::fstream::in);
    getline(file, output, '\n');
    file.close();
    return output;
}

void CgroupBackend::FileWriteStr(const std::string &path, const std::string &buffer)
{
    fs::file_status status = fs::status(path);
    // TODO: check if current user has WRITE permission to file in path
    // CGROUP_DEBUG("[FileWriteStr] File '" << path << "' has perm=" << serialize_fileperms(status.permissions()));

    if (!fs::exists(status))
        throw CGroupFileNotFoundException("File '" + path + "' not found");
    
    std::ofstream file(path, std::fstream::out);
    file << buffer;
    file.close();
}

// trim from start (in place)
inline void CgroupBackend::ltrim(std::string &s) {
    s.erase(s.begin(), std::find_if(s.begin(), s.end(),
            std::not1(std::ptr_fun<int, int>(std::isspace))));
}

// trim from end (in place)
inline void CgroupBackend::rtrim(std::string &s) {
    s.erase(std::find_if(s.rbegin(), s.rend(),
            std::not1(std::ptr_fun<int, int>(std::isspace))).base(), s.end());
}

// trim from both ends (in place)
inline void CgroupBackend::trim(std::string &s) {
    ltrim(s);
    rtrim(s);
}

void CgroupBackend::splitstring(const std::string& str,
        std::vector<std::string>& container, const std::string& delims)
{
    std::size_t current, previous = 0;  
    current = str.find_first_of(delims);
    while (current != std::string::npos) {
        container.push_back(str.substr(previous, current - previous));
        previous = current + 1;
        current = str.find_first_of(delims, previous);
    }
    container.push_back(str.substr(previous, current - previous));
}

std::string CgroupBackend::serialize_fileperms(const fs::perms &p)
{
    std::stringstream buf;
    buf << ((p & fs::perms::owner_read) != fs::perms::none ? "r" : "-")
        << ((p & fs::perms::owner_write) != fs::perms::none ? "w" : "-")
        << ((p & fs::perms::owner_exec) != fs::perms::none ? "x" : "-")
        << ((p & fs::perms::group_read) != fs::perms::none ? "r" : "-")
        << ((p & fs::perms::group_write) != fs::perms::none ? "w" : "-")
        << ((p & fs::perms::group_exec) != fs::perms::none ? "x" : "-")
        << ((p & fs::perms::others_read) != fs::perms::none ? "r" : "-")
        << ((p & fs::perms::others_write) != fs::perms::none ? "w" : "-")
        << ((p & fs::perms::others_exec) != fs::perms::none ? "x" : "-")
        << std::endl;
    return buf.str();
}


// CPU
/* The cfs_quota should be greater or equal than 1ms */
void CgroupBackend::ValidateCPUCfsQuota(long long value)
{
    if (value >= 0 &&
        (value < 1000 ||
         value > ULLONG_MAX / 1000)) {
        throw CGroupCPUException("cfs_quota '" + std::to_string(value) 
                + "' must be in range (1000, " + std::to_string(ULLONG_MAX / 1000) + ")");
    }
}

void CgroupBackend::ValidateCPUCfsPeiod(long long value)
{
    /* The cfs_period should be greater or equal than 1ms, and less or equal
     * than 1s.
     */
    if (value < 1000 || value > 1000000) {
        throw CGroupCPUException("cfs_period '" + std::to_string(value) + "' must be in range (1000, 1000000)");
    }
}

void CgroupBackend::SetCpuShares(unsigned long long shares)
{
    SetCgroupValueU64(CGROUP_CONTROLLER_CPU, GetControllerFileName(CGROUP_CONTROLLER_FILE_CPU_SHARES), shares);
}

unsigned long long CgroupBackend::GetCpuShares()
{
    return GetCgroupValueU64(CGROUP_CONTROLLER_CPU, GetControllerFileName(CGROUP_CONTROLLER_FILE_CPU_SHARES));
}


// Memory

void CgroupBackend::SetMemory(unsigned long long kb)
{
    this->SetMemoryHardLimit(kb);
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

unsigned long CgroupBackend::GetMemoryUsage()
{
    return GetCgroupValueU64(CGROUP_CONTROLLER_MEMORY, GetControllerFileName(CGROUP_CONTROLLER_FILE_MEMORY_USAGE)) >> 10;
}

void CgroupBackend::SetMemoryHardLimit(unsigned long long kb)
{
    SetMemoryLimitInKB(GetControllerFileName(CGROUP_CONTROLLER_FILE_MEMORY_HARD_LIMIT), kb);
}

unsigned long long CgroupBackend::GetMemoryHardLimit()
{
    return GetMemoryLimitInKB(GetControllerFileName(CGROUP_CONTROLLER_FILE_MEMORY_HARD_LIMIT));
}

void CgroupBackend::SetMemorySoftLimit(unsigned long long kb)
{
    SetMemoryLimitInKB(GetControllerFileName(CGROUP_CONTROLLER_FILE_MEMORY_SOFT_LIMIT), kb);
}

unsigned long long CgroupBackend::GetMemorySoftLimit()
{
    return GetMemoryLimitInKB(GetControllerFileName(CGROUP_CONTROLLER_FILE_MEMORY_SOFT_LIMIT));
}

void CgroupBackend::SetMemSwapHardLimit(unsigned long long kb)
{
    return SetMemoryLimitInKB(GetControllerFileName(CGROUP_CONTROLLER_FILE_MEMORY_SWAP_HARD_LIMIT), kb);
}

unsigned long long CgroupBackend::GetMemSwapHardLimit()
{
    return GetMemoryLimitInKB(GetControllerFileName(CGROUP_CONTROLLER_FILE_MEMORY_SWAP_HARD_LIMIT));
}

unsigned long long CgroupBackend::GetMemSwapUsage()
{
    return GetCgroupValueU64(CGROUP_CONTROLLER_MEMORY, GetControllerFileName(CGROUP_CONTROLLER_FILE_MEMORY_SWAP_USAGE)) >> 10;
}