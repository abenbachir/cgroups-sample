#include "CgroupBackend.hh"
#include "Enum.hh"

#include <unistd.h>
#include <mntent.h>
#include <sys/mount.h>
#include <fstream>
#include <iostream>
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

using namespace mdsd;

VIR_ENUM_DECL(virCgroupV2Controller);
VIR_ENUM_IMPL(virCgroupV2Controller,
              VIR_CGROUP_CONTROLLER_LAST,
              "cpu", "cpuacct", "cpuset", "memory", "devices",
              "freezer", "io", "net_cls", "perf_event", "name=systemd",
);

std::string CgroupBackend::FileReadAll(const std::string &path)
{
    std::ifstream file(path, std::fstream::in);

    std::string output;
    getline(file, output, '\0');
    file.close();
    return output;
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
        std::cerr << e.what() << '\n';
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

    if (!(mounts = fopen("/proc/mounts", "r")))
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
            auto contStr = FileReadAll(contFile);

            std::cout << "contFile=" << contFile << std::endl;
            std::cout << "contStr=" << contStr << std::endl;
        
            if (contStr == "")
                continue;

            ret = true;
            break;
        }

    }
    catch(const std::exception& e)
    {
        std::cerr << e.what() << std::endl;
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

    return 0;
}

int CgroupBackend::DetectPlacement(const std::string &path,
    const std::string &controllers,
    const std::string &selfpath)
{
    if (this->placement != "")
        return 0;

    VIR_DEBUG("path=%s controllers=%s selfpath=%s", path.c_str(), controllers.c_str(), selfpath.c_str());

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

int CgroupBackend::SetCpuShares(unsigned long long shares)
{
    return SetCgroupValueU64(VIR_CGROUP_CONTROLLER_CPU, "cpu.weight", shares);
}

int CgroupBackend::GetCpuShares(unsigned long long *shares)
{
    return GetCgroupValueU64(VIR_CGROUP_CONTROLLER_CPU, "cpu.weight", shares);
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
    

    VIR_DEBUG("Set value '%s' to '%s'", path, value);
    if (FileWriteStr(path, value) < 0)
    {
        const char* tmp = std::strrchr(path.c_str(), '/');
        if (errno == EINVAL && tmp)
        {
            std::cerr <<"Invalid value '" << value << "' for '"<< tmp + 1 << "'";
            return -1;
        }
        std::cerr << "Unable to write to '" << path << "'";
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

    VIR_DEBUG("Get value %s", path.c_str());
    
    *value = FileReadAll(path);

    /* Terminated with '\n' has sometimes harmful effects to the caller */
    int size = value->size();
    if (size > 0 && (*value)[size - 1] == '\n')
        (*value)[size - 1] = '\0';

    return 0;
}

//
bool CgroupBackend::HasController(int controller)
{
    return this->controllers & (1 << controller);
}

int CgroupBackend::GetPathOfController(int controller, const std::string &key, std::string *path)
{
    if (!HasController(controller)) {
        VIR_DEBUG("v2 controller '%s' is not available",
                       virCgroupV2ControllerTypeToString(controller));
        return -1;
    }

    *path = this->mountPoint + this->placement + "/" + key;

    return 0;
}