#include "types.h"
#include "stat.h"
#include "user.h"

int cpid[5];       

void sigCatcher2(int signum) {  
  printf(1, "^^this is sigCatcher2 ^^. PID %d caught NOT DEAFULT signal %d (GOOD)\n", getpid(), signum);
}

void sigCatcher(int signum) {  
  printf(1, "PID %d caught NOT DEAFULT signal %d (GOOD)\n", getpid(), signum);
}

int
main(int argc, char *argv[])
{
	int mypid =getpid();
	int pid;
	int i;
  	int *status=0;	
  	int wpid;

	printf(1,"\n@@PART 1 - SENDING DEAFULT SIGNALS TO MYSELF\n");
	printf(1,"sending signal #0 to myself. expecting 0\n");
	if (sigsend(mypid, 0)==0) printf(1,"***PASSED***\n\n");
	else goto bad;	
	printf(1,"sending signal #31 to myself. expecting 0\n");
	if (sigsend(mypid, 31)==0) printf(1,"***PASSED***\n\n");
	else goto bad;	
	printf(1,"sending signal #32 (illigal) to myself. expecting -1\n");
	if ((int)sigsend(mypid, 32)==-1) printf(1,"***PASSED***\n\n");
	else goto bad;
	printf(1,"sending signal #1 to unknown pid 65 (illigal). expecting -1\n");
	if ((int)sigsend(65, 1)==-1) printf(1,"***PASSED***\n\n");
	else goto bad;
	printf(1,"\n");
	sleep(100);
	printf(1,"@@PART 2 - SENDING NOT DEAFULT SIGNALS TO MYSELF\n");
	printf(1,"registering new handler for sig #11994. expecting -1\n");
	if ((int)signal(11994, sigCatcher)==-1) printf(1,"***PASSED***\n\n");
	else goto bad;	
	printf(1,"registering new handler for sig #11. expecting 0xfffffffe (add prev func = kernal add = deafult handler)\n");
	if ((int)signal(11, sigCatcher)==0xfffffffe) printf(1,"***PASSED***\n\n");
	else goto bad;	
	printf(1,"sending signal #11 to myself. expecting 0\n");
	if (sigsend(mypid, 11)==0) printf(1,"***PASSED***\n\n");
	else goto bad;	
	printf(1,"\n");
	sleep(100);
	printf(1,"@@PART 3a - SENDING 31 (without 11) DIFFFERENT DEAFULT SIGNALS TO CHILD\n");
	for(i=0; i<1; i++){
    	if((pid=fork()) ==  0){      
      		printf(1, "PID %d ready - GOOD\n", getpid());
      		sleep(400);
     		exit(0);  			
    	}else            			
      		cpid[i] = pid;
			sleep(10);      		
	} 
	for(i=0; i<32; i++){
		if (i==11) i++;
		printf(1,"sending signals to child. expecting 0\n");
		if (sigsend(cpid[0], i)==0) printf(1,"***PASSED***\n");
		else goto bad;
	} 	
    while((wpid=wait(status)) >= 0)
      printf(1, "killed zombie! - GOOD\n");
	sleep(100);	
	printf(1,"\n");
	printf(1,"@@PART 3b - SENDING 32 DIFFFERENT NON-DEAFULT SIGNALS TO CHILD\n");
	for(i=0; i<1; i++){
    	if((pid=fork()) ==  0){  
    		printf(1, "PID %d ready - GOOD\n", getpid());    
			for(i=0; i<32; i++){
				if (i==11) i++;
				printf(1,"registering new handler for sig #%d. expecting 0xfffffffe (add prev func = kernal add = deafult handler)\n", i);
				if ((int)signal(i, sigCatcher)==0xfffffffe) printf(1,"***PASSED***\n\n");
				else goto bad;	
			}
      		sleep(700);
     		exit(0);  			
    	}else            			
      		cpid[i] = pid;
			sleep(300);      		
	} 
	for(i=0; i<32; i++){
		printf(1,"sending signals to child. expecting 0\n");
		if (sigsend(cpid[0], i)==0) printf(1,"***PASSED***\n");
		else goto bad;
	} 	
    while((wpid=wait(status)) >= 0)
      printf(1, "killed zombie! - GOOD\n");
	sleep(100);	
	printf(1,"\n");
	printf(1,"@@PART 3c - SENDING 32 DIFFFERENT MIXED SIGNALS TO CHILD\n");
	for(i=0; i<1; i++){
    	if((pid=fork()) ==  0){  
    		printf(1, "PID %d ready - GOOD\n", getpid());    
			for(i=0; i<16; i++){
				if (i==5) i++;				
				printf(1,"registering new handler for sig #%d. expecting 0xfffffffe (add prev func = kernal add = deafult handler)\n", 2*i +1);
				if ((int)signal(2*i +1 , sigCatcher)==0xfffffffe) printf(1,"***PASSED***\n\n");
				else goto bad;	
			}
      		sleep(700);
     		exit(0);  			
    	}else            			
      		cpid[i] = pid;
			sleep(300);      		
	} 
	for(i=0; i<32; i++){
		printf(1,"sending signals to child. expecting 0\n");
		if (sigsend(cpid[0], i)==0) printf(1,"***PASSED***\n");
		else goto bad;
	} 	
    while((wpid=wait(status)) >= 0)
      printf(1, "killed zombie! - GOOD\n");
	sleep(100);	
	printf(1,"\n");
	printf(1,"@@PART 4 - SENDING DEAFULT SIGNALS TO 5 CHILDREN\n");
	for(i=0; i<5; i++){
    	if((pid=fork()) ==  0){      
      		printf(1, "PID %d ready - GOOD\n", getpid());
			sleep(1000);
			printf(1,"sending signal back to padre. expecting 0\n");
			if (sigsend(mypid, 0)==0) printf(1,"***PASSED***\n\n");
			else goto bad;		
     		exit(0);  			
    	}else            			
      		cpid[i] = pid;
      		sleep(100);		
	} 
	for(i=0; i<5; i++){
		printf(1,"sending signal to child. expecting 0\n");
		if (sigsend(cpid[i], 0)==0) printf(1,"***PASSED***\n\n");
		else goto bad;
	} 	
    while((wpid=wait(status)) >= 0)
      printf(1, "killed zombie! - GOOD\n");
	printf(1,"\n");
	sleep(100);
	printf(1,"@@PART 5 - SENDING NOT DEAFULT SIGNALS TO 5 CHILDREN && SAME SIGNALS - DIFFFERENT HANDLERS FOR EACH PROCCESS\n");
	printf(1,"registering new handler for sig #11 (to father). expecting add sigCatcher\n");
	int test = (int)signal(11, sigCatcher2);
	if (test!=0 && test!=-1 && test!=0xfffffffe) printf(1,"***PASSED***\n\n");
	else goto bad;	

	for(i=0; i<5; i++){
    	if((pid=fork()) ==  0){      		
      		printf(1, "PID %d ready - GOOD\n", getpid());
			sleep(1000);
			printf(1,"sending signal #11 back to padre. expecting 0\n");
			if (sigsend(mypid, 11)==0) printf(1,"***PASSED***\n\n");
			else goto bad;			
     		exit(0);  			
    	}else            
      		cpid[i] = pid;
      		sleep(100);	 
	} 
	for(i=0; i<5; i++){
		printf(1,"sending signal to child. expecting 0\n");
		if (sigsend(cpid[i], 11)==0) printf(1,"***PASSED***\n\n");
		else goto bad;
	} 	
    while((wpid=wait(status)) >= 0)
      printf(1, "killed zombie! - GOOD\n");
	printf(1,"\n");
	goto good;

	bad:
	      printf(1, "@@@@@@@@@@@@@@  YOU FAILD! @@@@@@@@@@@@@@\n");
	      goto exit;
	good:
	      printf(1, "@@@@@@@@@@@@@@  ALL CLEAR! @@@@@@@@@@@@@@\n");
	      goto exit;

	exit:
 	exit(0);
}