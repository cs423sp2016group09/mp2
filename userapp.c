#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include "userapp.h"

long long unsigned fac(long long n) {
	if (n<1) return 1;

	return n*fac(n-1);
}

void REGISTER() {
    
}

void DEREGISTER() {
    
}

void YIELD() {

}
static int pid;
static FILE * statusfile;
int main(int argc, char* argv[]) {
    // get pid 
    pid = getpid();
    // write pid as a string to the proc filesystem
    statusfile = fopen("/proc/mp2/status", "r+");
    if (statusfile == NULL) exit(1);
    fprintf(statusfile, "%d", pid);            	  
    // read the proc fs            
    fclose(statusfile);
    long long x = 0;
                
    int i, j;
    for (j=0; j < 10000; j++) {
        for (i = 0; i < 10000; i++) {
            x = fac(33);		
        }
    }
                	
    return 0;
}
