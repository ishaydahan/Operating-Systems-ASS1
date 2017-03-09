#include "types.h"
#include "stat.h"
#include "user.h"

struct perf {
  int ctime;
  int ttime;
  int stime;
  int retime;
  int rutime;

};

int cpid[30];       
int avg[9];       

int
main(int argc, char *argv[])
{
	int i=0;
	int status=0;
	int pid=0;
	int wpid=0;
	int gar=0;
	struct perf mystats;

	printf(1,"\n@@SANITY CHECK IS ON... PLEASE BE PATIENT\n");

	for(i=0; i<30; i++){
    	if((pid=fork()) ==  0 && i<10){
        //priority(1000);           
    		while (running_time() < 30);
       	exit(0);  		
     	}else if(pid ==  0 && i<20){  
        priority(1000);           
     		int j;    
      		for(j=0; j<30; j++) sleep(1);
     		exit(0);  			
    	}else if(pid ==  0){   
    		//priority(1000);   
    		int b=5;
     		int j;        		
      		for(j=0; j<5; ++j){
	    		while (running_time()< b) ;
      		sleep(1);
      		b=b+5;
      		}
     		exit(0);  		
    	}else{            			
      		cpid[i] = pid;
      	}
	}

	int p=0;
    while((wpid=wait_stat(&status, &mystats)) >= 0){
		printf(1, "stat for pid #%d ", wpid);
    if (wpid<cpid[10]) printf(1, "group A:\n");
    else if(wpid<cpid[20]) printf(1, "group B:\n");
    else printf(1, "group C:\n");
		// printf(1, "creation time: %d\n", mystats.ctime);
		// printf(1, "termination time: %d\n", mystats.ttime);
		// printf(1, "sleeping time: %d\n", mystats.stime);
		printf(1, "waiting time: %d\n", mystats.retime);
		printf(1, "running time: %d\n", mystats.rutime);
		printf(1, "turnaround time: %d\n\n", mystats.ttime-mystats.ctime);
		if (wpid<cpid[10]){
			avg[0]=avg[0]+mystats.retime;
			avg[1]=avg[1]+mystats.rutime;
			avg[2]=avg[2]+mystats.ttime-mystats.ctime;			
		}else if (wpid<cpid[20]){
			avg[3]=avg[3]+mystats.retime;
			avg[4]=avg[4]+mystats.rutime;
			avg[5]=avg[5]+mystats.ttime-mystats.ctime;			
		}else{
			avg[6]=avg[6]+mystats.retime;
			avg[7]=avg[7]+mystats.rutime;
			avg[8]=avg[8]+mystats.ttime-mystats.ctime;			
		}
		p++;
    }
    printf(1,"------THE CPU USING GROUP (NON BLOCKING CALLS)------\n");
    printf(1,"AVARAGE WAITING TIME FOR GROUP A: %d\n", avg[0]/10);
    printf(1,"AVARAGE RUNNING TIME FOR GROUP A: %d\n", avg[1]/10);
    printf(1,"AVARAGE TURNAROUND TIME FOR GROUP A: %d\n", avg[2]/10);
    printf(1,"---------THE SLEEPING GROUP (BLOCKING CALLS)---------\n");
    printf(1,"AVARAGE WAITING TIME FOR GROUP B: %d\n", avg[3]/10);
    printf(1,"AVARAGE RUNNING TIME FOR GROUP B: %d\n", avg[4]/10);
    printf(1,"AVARAGE TURNAROUND TIME FOR GROUP B: %d\n", avg[5]/10);
    printf(1,"--------THE NORMAL GROUP (BOTH A AND B TYPE)--------\n");
    printf(1,"AVARAGE WAITING TIME FOR GROUP C: %d\n", avg[6]/10);
    printf(1,"AVARAGE RUNNING TIME FOR GROUP C: %d\n", avg[7]/10);
    printf(1,"AVARAGE TURNAROUND TIME FOR GROUP C: %d\n", avg[8]/10);

	printf(1,"\n");

 	exit(gar);
}