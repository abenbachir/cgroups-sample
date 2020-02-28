#include <iostream>
#include <libcgroup.h>
#include <math.h>
#include <pthread.h>
#include <sys/prctl.h>
#include<unistd.h> 
#include<sys/types.h> 
#include<string.h> 
#include<sys/wait.h>

using namespace std;

constexpr std::size_t MB        = 1024 * 1024;
constexpr std::size_t page_size = 4096;

pid_t cpu_burn()
{   
    pid_t pid = fork();
    if (pid == 0)
    {
        prctl(PR_SET_NAME, (unsigned long)"cpu_burn", 0, 0, 0);
        cout << "[CHILD] Running cpu burn" << endl;
        double x;
        while (true) {
            x = sin(log(x+10)) + 1.1;
            x = sqrt(x*3.9 + x*x/4.2);
        }
        exit(0);
    }
    cout << "Creating process " << pid << endl;
    return pid;
}
static void escape(void* p) { asm volatile("" : : "g"(p) : "memory"); }

pid_t memory_alloc(int size_mb)
{
    pid_t pid = fork();
    if (pid == 0)
    {
        prctl(PR_SET_NAME, (unsigned long)"memory_alloc", 0, 0, 0);
        cout << "[CHILD] Running memory alloc" << endl;
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
    ret = cgroup_init();

    pid_t mem_process_pid = memory_alloc(50);
    pid_t cpu_burn_pid = cpu_burn();
    
    int status;
    cout << "Wait for process " << cpu_burn_pid << endl;
    waitpid(mem_process_pid, &status, 0); 
    if ( WIFEXITED(status) )      
        printf("Exit status of the child was %d\n", WEXITSTATUS(status));

    cout << "Wait for process " << cpu_burn_pid << endl;
    waitpid(mem_process_pid, &status, 0); 
  
    if ( WIFEXITED(status) )      
        printf("Exit status of the child was %d\n", WEXITSTATUS(status));
    return ret;
}
