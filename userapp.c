#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include "userapp.h"
#include <sys/time.h>

#define SIZE 1024

const float MIN_CPU_TIME = 167.448;

static int pid;
static FILE * statusfile;

long long unsigned fac(long long n) {
	if (n<1) return 1;

	return n*fac(n-1);
}

void REGISTER(unsigned long period, unsigned long job_process_time) {
    fprintf(statusfile, "R, %d, %lu, %lu", pid, period, job_process_time);
}

void YIELD() {
    fprintf(statusfile, "Y, %d", pid);
}

void DEREGISTER() {
    fprintf(statusfile, "D, %d", pid);
}

void do_job(){
    int i;
    for (i = 0; i < 10000; i++) {
        long long x = 0;
        x = fac(33);        
    }
}

int process_in_list() {
    double b[SIZE];
    size_t ret_code = fread(b, sizeof *b, SIZE, statusfile); // reads an array of doubles
    // figure out how to parse this string later
    return 0; 
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
        perror("usage: ./userapp period job_process_time (times in milliseconds)");
        exit(1);
    }

    unsigned long period = strtoul(argv[1], NULL, 10);
    unsigned int job_process_time = strtoul(argv[2], NULL, 10);

    if (job_process_time < MIN_CPU_TIME) {
        perror("Job process time must be at least 167.448 ms");
        exit(1);
    }

    if (job_process_time > period) {
        perror("Period must be bigger than job process time.");
        exit(1);
    }

    REGISTER(period, job_process_time);

    int in_list = process_in_list();

    // passed registration successfully
    if (!in_list) {
        perror("Process rejected by admission control.");
        exit(1);
    }

    YIELD();

    int j;
    for (j=0; j < 100; j++) { // while exist jobs
        do_job();
        YIELD();
    }

    DEREGISTER();

    // read the proc fs            
    fclose(statusfile);

    return 0;
}
