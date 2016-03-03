#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>

#include "userapp.h"

long long unsigned fac(long long n) {
	if (n<1) return 1;

	return n*fac(n-1);
}

int main(int argc, char* argv[]) {
	  // get pid 
	  int pid = getpid();

	  // write pid as a string to the proc filesystem
	  FILE * statusfile = fopen("/proc/mp1/status", "r+");

	  if (statusfile == NULL ) return 1;
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
