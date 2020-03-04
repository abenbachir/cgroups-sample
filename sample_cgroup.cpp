#include <iostream>
#include <libcgroup.h>
#include <math.h>
#include <pthread.h>
#include <sys/prctl.h>
#include <unistd.h> 
#include <sys/types.h> 
#include <string.h> 
#include <sys/wait.h>

#include "CgroupBackend.hh"

using namespace mdsd;
using namespace std;

#define PRINT_CHILD "[CHILD] "
constexpr std::size_t MB        = 1024 * 1024;
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
    for (size_t i = 1; i <= size_mb; i+=5)
    {
        char* buf;
        {
            buf = (char*)calloc(MB, sizeof(char)); // alloc 1 MB
            for (size_t i = 0; i < MB; i += page_size)
                buf[i] = 0;
            buf[MB - 1] = 0;
            escape(&buf);
        }
        std::cout << "Allocated "<< i << "MB\n";
        sleep(1);
    }
}

pid_t create_proc_cpu_burn()
{   
    pid_t pid = fork();
    if (pid == 0)
    {
        prctl(PR_SET_NAME, (unsigned long)"cpu_burn", 0, 0, 0);
        cout << PRINT_CHILD"Running cpu burn" << endl;
        cpu_burn();
        exit(0);
    }
    cout << "Creating process " << pid << endl;
    return pid;
}

pid_t create_proc_mem_alloc(int size_mb)
{
    pid_t pid = fork();
    if (pid == 0)
    {
        prctl(PR_SET_NAME, (unsigned long)"memory_alloc", 0, 0, 0);
        cout << PRINT_CHILD"Running memory alloc" << endl;
        memory_alloc(size_mb);
        sleep(60);
        exit(0);
    }
    cout << "Creating process " << pid << endl;
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

    auto cgroupBackend = CgroupBackend("/sys/fs/cgroup/test");
    cgroupBackend.Available();

    unsigned long long int cpu_shares = 0;
    cgroupBackend.GetCpuShares(&cpu_shares);

    cout << " cpu.shares="<< cpu_shares << endl;

    cpu_burn();

    // pid_t mem_process_pid = create_proc_mem_alloc(50);
    // pid_t cpu_burn_pid = create_proc_cpu_burn();
    
    // int status;
    // cout << "Wait for process " << cpu_burn_pid << endl;
    // waitpid(mem_process_pid, &status, 0); 
    // if ( WIFEXITED(status) )      
    //     printf("Exit status of the child was %d\n", WEXITSTATUS(status));

    // cout << "Wait for process " << cpu_burn_pid << endl;
    // waitpid(mem_process_pid, &status, 0); 
  
    // if ( WIFEXITED(status) )      
    //     printf("Exit status of the child was %d\n", WEXITSTATUS(status));
    return ret;
}
