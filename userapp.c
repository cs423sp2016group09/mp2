#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include "userapp.h"

static int pid;
static FILE * statusfile;

long long unsigned fac(long long n) {
	if (n<1) return 1;

	return n*fac(n-1);
}

void REGISTER(unsigned long period, unsigned int job_process_time) {
    fprintf(statusfile, "R, %d, %lu, %u", pid, period, job_process_time);
}

void YIELD() {
    fprintf(statusfile, "Y, %d", pid);
}

void DEREGISTER() {
    fprintf(statusfile, "D, %d", pid);
}

int main(int argc, char* argv[]) {
    // get pid 
    pid = getpid();
    // write pid as a string to the proc filesystem
    statusfile = fopen("/proc/mp2/status", "r+");
    if (statusfile == NULL) {
        perror("Could not open MP2 status file.");
        exit(1);
    }
    if (argc != 3) {
        perror("usage: ./userapp period job_process_time");
        exit(1);
    }
    unsigned long period = strtoul(argv[2], NULL, 10);
    unsigned int job_process_time = strtoul(argv[3], NULL, 10);

    long long x = 0;   
    int i, j;
    for (j=0; j < 10000; j++) {
        for (i = 0; i < 10000; i++) {
            x = fac(33);		
        }
    }
    
    // read the proc fs            
    fclose(statusfile);

    return 0;
}
