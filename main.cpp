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
#include "Cgroup.hh"

using namespace mdsd;
using namespace std;

#define PRINT_CHILD "[CHILD] "
constexpr std::size_t MB        = 1024 * 1024;
constexpr std::size_t KB        = 1024;
constexpr std::size_t page_size = 4096;

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
    int ret = 0;
    
    /* initialize libcg */
    // cgroup_set_default_logger(4); // DEBUG is 4
    // ret = cgroup_init();
    // if (ret) {
	// 	fprintf(stderr, "libcgroup initialization failed: %s\n", cgroup_strerror(ret));
	// 	return ret;
	// }

    auto cgroup = Cgroup("/test-group/nested-group");

    cgroup.backend->Remove();

    cgroup.backend->MakeGroup();

    uint abder_uid = 1000;
    uint abder_gid = 1000;
    cout <<"Setup cgroup permission for user (" << abder_uid << ", " << abder_gid << ")" << endl;
    cgroup.backend->SetOwner(abder_uid, abder_gid);

    // cgroup.backend->Remove();
    // return 0;

    unsigned long long mem_kb = 0;
    cgroup.backend->GetMemoryHardLimit(&mem_kb);
    cout << "   memory.max="<< mem_kb << " KB" << endl;

    // set Memory to hard=20MB soft=10MB
    cgroup.backend->SetMemoryHardLimit(KB * 70);
    cgroup.backend->SetMemorySoftLimit(KB * 40);

    mem_kb = 0;
    cgroup.backend->GetMemoryHardLimit(&mem_kb);
    cout << "   memory.max="<< mem_kb << " KB" << endl;

    // set CPU to hard limit of 70%
    cgroup.backend->SetCpuCfsQuota(70000);
    cgroup.backend->SetCpuCfsPeriod(100000);
    cout << "   cpu.quota="<< cgroup.backend->GetCpuCfsQuota() << endl;
    cout << "   cpu.period="<< cgroup.backend->GetCpuCfsPeriod() << endl;

    // Add current process to cgroup
    // cgroup.backend->AddTask(getpid());

    // memory_alloc(40);
    // cpu_burn();

    // pid_t mem_process_pid = create_proc_mem_alloc(50);
    // pid_t cpu_burn_pid = create_proc_cpu_burn();
    cgroup.backend->AddTask(create_proc_cpu_burn());
    cgroup.backend->AddTask(create_proc_cpu_burn());
    cgroup.backend->AddTask(create_proc_cpu_burn());
    cgroup.backend->AddTask(create_proc_mem_alloc(100));
    cgroup.backend->AddTask(create_proc_mem_alloc(100));
    
    cout << "Sleep..." << endl;
    sleep(1000);
    return ret;
}
