#include "types.h"
#include "stat.h"
#include "user.h"


int
main(int argc, char **argv)
{

   	if(schedp((char)*argv[1]-48)==0 ){
   		printf(1, "changed policy to: %s\n", argv[1]);
   	}else{
   		printf(1, "error\n");
   	}
 	exit(0);
}