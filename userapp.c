#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include "userapp.h"
#include <sys/time.h>
#include <string.h>

#define SIZE 200
#define LINE_SIZE 100

const float MIN_CPU_TIME = 167.94;

static int pid;

static FILE * writefh = NULL;
static int writefno = -1;

long long unsigned fac(long long n) {
    if (n<1) return 1;

    return n*fac(n-1);
}

void REGISTER(unsigned long period, unsigned long job_process_time) {
    char message[LINE_SIZE];
    memset(message, 0, LINE_SIZE);
    sprintf(message, "R, %d, %lu, %lu\n", pid, period, job_process_time);
    write(writefno, message, LINE_SIZE);
    // int ret = fprintf(writefh, "R, %d, %lu, %lu\n", pid, period, job_process_time);
}

void YIELD() {    
    char message[LINE_SIZE];
    memset(message, 0, LINE_SIZE);
    sprintf(message, "Y, %d\n", pid);
    write(writefno, message, LINE_SIZE);
    // int ret = fprintf(writefh, "Y, %d\n", pid);
}

void DEREGISTER() {
    char message[LINE_SIZE];
    memset(message, 0, LINE_SIZE);
    sprintf(message, "D, %d\n", pid);
    write(writefno, message, LINE_SIZE);
    // fprintf(writefh, "D, %d\n", pid);
}

void do_job(){
    int i;
    for (i = 0; i < 1000000; i++) {
        long long x = 0;
        x = fac(33);        
    }
}

int process_in_list() {
    char needle[32];
    memset(needle, 0, 32);
    sprintf(needle, "PID: %u", pid);
    printf("searching for string: %s\n", needle);
    FILE *f = fopen("/proc/mp2/status", "r");
    if (f == NULL) {
        perror("Could not open MP2 status file.");
        exit(1);
    }
    size_t sz = 0;
    char * lin = 0;

    int found = 0;
    do {
        // fprintf(stderr, "Beginning readline\n");
        ssize_t lsz = getline (&lin, &sz, f);
        // fprintf(stderr, "Got linesize %zu \n", lsz);
        // fprintf(stderr, "contents: %s \n", lin);
        if (lsz >= 0) {
            char *find = strstr(lin, needle);
            if (find != NULL) {
                found = 1;
            }
        }
        free(lin);
        lin = 0;
    } while (!feof (f));
    fclose (f);

    return found;
}

int main(int argc, char* argv[]) {
    // get pid 
    pid = getpid();
    // write pid as a string to the proc filesystem
    printf("This is my PID: %d\n", pid);

    if (argc != 3) {
        perror("usage: ./userapp period job_process_time (times in milliseconds)");
        exit(1);
    }

    unsigned long period = strtoul(argv[1], NULL, 10);
    unsigned int job_process_time = strtoul(argv[2], NULL, 10);

    if (job_process_time < MIN_CPU_TIME) {
        perror("Job process time must be at least 167.94 ms");
        exit(1);
    }

    if (job_process_time > period) {
        perror("Period must be bigger than job process time.");
        exit(1);
    }

    writefh = fopen("/proc/mp2/status", "w");
    if (writefh == NULL) {
        perror("Could not open MP2 status file.");
        exit(1);
    }
    writefno = fileno(writefh);

    REGISTER(period, job_process_time);

    int in_list = process_in_list();

    // passed registration successfully
    if (!in_list) {
        perror("Process rejected by admission control.");
        exit(1);
    } else {
        fprintf(stderr, "Accepted by admission control!\n");
    }

    YIELD();
    // printf("woken up from yield\n");
    // // struct timeval start, end;
    // // long unsigned secs_used,micros_used;

    // // gettimeofday(&start, NULL);
    
    int j;
    for (j=0; j < 5; j++) { // while exist jobs
        // printf("Hey i'm awake!\n");
        do_job();
        YIELD();
    }

    // // gettimeofday(&end, NULL);

    // // printf("start: %lu secs, %lu usecs\n",start.tv_sec,start.tv_usec);
    // // printf("end: %lu secs, %lu usecs\n",end.tv_sec,end.tv_usec);

    // // secs_used=(end.tv_sec - start.tv_sec); //avoid overflow by subtracting first
    // // micros_used= ((secs_used*1000000) + end.tv_usec) - (start.tv_usec);

    // // printf("micros_used: %lu\n",micros_used);
    // // printf("millis_used: %lu\n",micros_used / 1000);

    // // printf("avg time: %G\n",micros_used / 1000 / 100.0);

    DEREGISTER();

    return 0;
}
