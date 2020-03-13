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

/* this should match the enum CgroupController */
CGROUP_ENUM_DECL(CgroupBackendType);
CGROUP_ENUM_IMPL(CgroupBackendType, CGROUP_BACKEND_TYPE_LAST, "none", "cgroup2", "cgroup");

CgroupBackend::CgroupBackend(CgroupBackendType type, const std::string &placement)
    : backenType(type)
{
    backenName = CgroupBackendTypeTypeToString(backenType);
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
    return backenType;
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

            if (this->backenType == CGROUP_BACKEND_TYPE_V2)
            {
                /* Systemd uses cgroup v2 for process tracking but no controller is
                * available. We should consider this configuration as cgroup v2 is
                * not available. */
                std::string contFile = std::string(entry.mnt_dir) + "/cgroup.controllers";
                FileReadAll(contFile, contStr);
            
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

// virtual
int CgroupBackend::HasEmptyTasks(int controller)
{
    int ret = -1;
    std::string content;

    ret = GetCgroupValueStr(controller, "cgroup.procs", &content);

    if (ret == 0 && content == "")
        ret = 1;

    return ret;
}

// should be probably virtual pure
int CgroupBackend::SetOwner(uid_t uid, gid_t gid, int controllers)
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


int CgroupBackend::FileReadAll(const std::string &path, std::string &output)
{
    fs::file_status status = fs::status(path);
    if (!fs::exists(status))
    {
        CGROUP_ERROR("File '" << path << "' not found");
        return -1;
    }
    // TODO: check if current user has READ permission to file in path
    CGROUP_DEBUG("[FileReadAll] File '" << path << "' has perm=" << serialize_fileperms(status.permissions()));

    std::ifstream file(path, std::fstream::in);
    getline(file, output, '\n');
    file.close();
    return 0;
}

int CgroupBackend::FileWriteStr(const std::string &path, const std::string &buffer)
{
    int ret = 0;
    try
    {
        fs::file_status status = fs::status(path);
        // TODO: check if current user has WRITE permission to file in path
        CGROUP_DEBUG("[FileWriteStr] File '" << path << "' has perm=" << serialize_fileperms(status.permissions()));

        if (!fs::exists(status))
        {
            CGROUP_ERROR("File '" << path << "' not found");
            return -1;
        }
        
        std::ofstream file(path, std::fstream::out);
        file << buffer;
        file.close();
    }
    catch(const std::exception& e)
    {
        CGROUP_ERROR(e.what());
        ret = -1;
    }
    
    return ret;
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