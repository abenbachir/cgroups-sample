#include <iostream>
#include <libcgroup.h>
#include <math.h>
#include <pthread.h>
#include <sys/prctl.h>
#include <unistd.h> 
#include <sys/types.h> 
#include <string.h> 
#include <sys/wait.h>
#include <thread>

#include "CgroupBackend.hh"
#include "CgroupBackendFactory.hh"
#include "Cgroup.hh"
#include "ConfigINI.hh"
#include "TenantConfig.hh"
#include <confini.h>

#include <boost/algorithm/string.hpp>
using namespace boost::algorithm;

using namespace mdsd;
using namespace std;

#define PRINT_CHILD "[CHILD] "
constexpr std::size_t MB        = 1024 * 1024;
constexpr std::size_t KB        = 1024;
constexpr std::size_t page_size = 4096;
#define LOG(log) std::cout << log << std::endl

void cpu_burn()
{
    double x;
    while (true) {
        x = sin(log(x+10)) + 1.1;
        x = sqrt(x*3.9 + x*x/4.2);
    }
}

static void escape(void* p) { asm volatile("" : : "g"(p) : "memory"); }
void memory_alloc(int size_mb)
{
    for (size_t i = 1; i <= size_mb; i+=1)
    {
        char* buf;
        {
            buf = (char*)calloc(MB, sizeof(char));
            for (size_t i = 0; i < MB; i += page_size)
                buf[i] = 0;
            buf[MB - 1] = 0;
            escape(&buf);
        }
        std::cout << "Allocated "<< i << "MB\n";
        sleep(1);
    }
}

void thread_wait_for_process(std::string thread_name, int pid)
{
    int status;
    cout << "Wait for process " << thread_name << ":" << pid << endl;
    waitpid(pid, &status, 0); 
    if ( WIFEXITED(status) )   
        printf("Exit status of the child was %d\n", WEXITSTATUS(status));
}

pid_t create_proc_cpu_burn()
{
    string name = "main:cpu_burn";
    pid_t pid = fork();
    if (pid == 0)
    {
        prctl(PR_SET_NAME, (unsigned long)name.c_str(), 0, 0, 0);
        cout << PRINT_CHILD"Running cpu burn" << endl;
        cpu_burn();
        exit(0);
    }

    std::thread t(&thread_wait_for_process, name, pid);
    t.detach();
    return pid;
}

pid_t create_proc_mem_alloc(int size_mb)
{
    string name = "main:memory_alloc";
    pid_t pid = fork();
    if (pid == 0)
    {
        prctl(PR_SET_NAME, (unsigned long)name.c_str(), 0, 0, 0);
        cout << PRINT_CHILD"Running memory alloc" << endl;
        memory_alloc(size_mb);
        sleep(60);
        exit(0);
    }
    std::thread t(&thread_wait_for_process, name, pid);
    t.detach();
    return pid;
}


int main(int argc, char *argv[])
{
    ConfigINI ini;
    IniParsedDataMap data;

    ini.Parse("./config.ini", data);

    std::vector<TenantConfig> tenantsConfig;
    unsigned int DefaultSoftQuota = 0;
    std::vector<std::string> allowedTenantsList;
    for (IniParsedDataMap::iterator it = data.begin(); it != data.end(); ++it)
    {
        auto sectionName = it->first;
        auto sectionKeys = it->second;
        LOG("it->first=" << it->first);
        // Root section
        if (it->first == std::string(ROOT_SECTION_NAME) && sectionKeys.find("AllowedTenants") != sectionKeys.end())
        {
            split(allowedTenantsList, sectionKeys["AllowedTenants"], boost::is_any_of(","));
        }
        
        // Global TENANTS section
        if (it->first == "TENANTS" && sectionKeys.find("SoftQuotaCushion") != sectionKeys.end())
        {
            DefaultSoftQuota = std::stoul(sectionKeys["SoftQuotaCushion"]);
        }

        // SubSection tenant config
        if (starts_with(it->first, "TENANTS."))
        {
            TenantConfig tenant(erase_first_copy(it->first, "TENANTS."), DefaultSoftQuota);
            tenant.ApplyConfig(it->second);
            tenantsConfig.push_back(tenant);
        }
    }
    
    CgroupBackendFactory cgroupFactory = CgroupBackendFactory();

    // load control groups from current process
    auto mdsdmgr = cgroupFactory.GetCgroup("/");
    mdsdmgr->backend->DetectPlacement(20741);

    LOG("mdsdmgr->GetCPUInPercent()=" << mdsdmgr->GetCPUInPercent() << "%");
    LOG("mdsdmgr->GetMemoryInMB()=" << mdsdmgr->GetMemoryInMB() << "MB");
    
    uint abder_uid = 1000;
    uint abder_gid = 1000;
    // create root cgroup for tenants
    unsigned int enablingControllers = 1 << CGROUP_CONTROLLER_CPU;
    enablingControllers |= 1 << CGROUP_CONTROLLER_MEMORY;
    std::string rootpath = fs::path(mdsdmgr->backend->GetRelativeBasePath(CGROUP_CONTROLLER_MEMORY)).append("/TENANTS");
    LOG("rootpath=" << rootpath << "");
    auto rootCgroup = cgroupFactory.GetCgroup(rootpath);
    rootCgroup->backend->DetectControllers(enablingControllers);
    rootCgroup->backend->Remove();    
    rootCgroup->backend->MakeGroup();
    rootCgroup->SetOwner(abder_uid, abder_gid);
    // remove 10 MB to keep for current process
    rootCgroup->SetMemoryLimitInMB(mdsdmgr->GetMemoryInMB()-10);
   
    auto maxTenantsMemoryLimitMB = rootCgroup->GetMemoryInMB();
    unsigned int totalTenantsMemoryLimitFromConfigInMB = 0;

    std::vector<std::shared_ptr<Cgroup>> tenantsCgroup;
    for (size_t i = 0; i < tenantsConfig.size(); i++)
    {
        TenantConfig& tenant = tenantsConfig[i];
        tenant.Print();
        fs::path tenantpath(rootpath);
        tenantpath /= tenant.name;
        auto tenantCgroup = cgroupFactory.GetCgroup(tenantpath);
        tenantCgroup->backend->DetectControllers(enablingControllers);
        tenantCgroup->backend->Remove();
        tenantCgroup->backend->MakeGroup(enablingControllers);
        tenantCgroup->SetOwner(abder_uid, abder_gid);
        // set CPU
        tenantCgroup->SetCPULimitInPercentage(tenant.cpu, tenant.softquota);
        
        // set Memory
        float memory = tenant.memory;
        if (tenant.memoryUnit == CONFINIT_UNIT_KILOBYTE) {
            memory = CGROUP_MEM_KB_TO_MB(memory);
        } else if (tenant.memoryUnit == CONFINIT_UNIT_PERCENTAGE) {
            memory = maxTenantsMemoryLimitMB * double(memory)/100;
        }

        tenantCgroup->SetMemoryLimitInMB(memory, tenant.softquota);

        totalTenantsMemoryLimitFromConfigInMB += memory;

        // add tasks to cgroup
        tenantCgroup->backend->AddTask(create_proc_cpu_burn());
        tenantCgroup->backend->AddTask(create_proc_mem_alloc(100));

        tenantsCgroup.push_back(tenantCgroup);
    }

    LOG("maxTenantsMemoryLimitMB=" << maxTenantsMemoryLimitMB);
    LOG("totalTenantsMemoryLimitFromConfigInMB=" << totalTenantsMemoryLimitFromConfigInMB);
    // Re-adjust memory limits
    if (totalTenantsMemoryLimitFromConfigInMB > maxTenantsMemoryLimitMB)
    {
        for (size_t i = 0; i < tenantsCgroup.size(); i++)
        {
        }
    }

    
    // auto cgroup = cgroupFactory.GetCgroup("/test-group");
    // cgroup->backend->Remove();
    // cgroup->backend->MakeGroup();
    // uint abder_uid = 1000;
    // uint abder_gid = 1000;
    // cout <<"Setup cgroup permission for user (" << abder_uid << ", " << abder_gid << ")" << endl;
    // cgroup->SetOwner(abder_uid, abder_gid);
    // cgroup->SetCPULimitInPercentage(30);
    // cgroup->SetMemoryLimitInMB(20, 20);

    // cgroup->backend->AddTask(create_proc_cpu_burn());
    // cgroup->backend->AddTask(create_proc_cpu_burn());
    // cgroup->backend->AddTask(create_proc_cpu_burn());
    // cgroup->backend->AddTask(create_proc_mem_alloc(100));
    // cgroup->backend->AddTask(create_proc_mem_alloc(100));
    
    cout << "Sleep..." << endl;
    sleep(1000);
    return 0;
}
