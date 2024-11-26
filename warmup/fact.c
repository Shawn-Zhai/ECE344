#include "common.h"
#include "string.h"
#include <ctype.h>
#include <math.h>
#include <stdlib.h>

int
main(int argc, char *argv[])
{
	//TBD();
        if(argc != 2){
            printf("Huh?\n");
            return 0;
        }
        
        if(argv[1][0] == '0'){
            printf("Huh?\n");
            return 0;
        }
        
        int isValid = 1;
        for(int i = 0; i < strlen(argv[1]); i++){
            if(isdigit(argv[1][i]) == 0){
                isValid = 0;
                break;
            }
        }
        
        if(isValid == 0){
            printf("Huh?\n");
            return 0;
        }
        
        int x = atoi(argv[1]);
        
        if(x > 12){
            printf("Overflow\n");
            return 0;
        }
        
        int ans = 1;
        while(x != 1){
            ans *= x;
            x--;
        }
        
        printf("%d\n", ans);
        
        return 0;
}
