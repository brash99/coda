/*
 * File:
 *    triggerTest
 *
 * Description:
 *    Test new trigger table routines
 *
 *
 */


#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "jvme.h"
#include "tsLib.h"

int 
main(int argc, char *argv[]) {

    int stat;
    int slot;

    printf("\nJLAB TS trigger table test.\n");
    printf("----------------------------\n");

    tsTriggerTableDefault();

    tsDefineEventType(0,0xe0000000,3,123);
    tsPrintTriggerTable(0,3,1);
    

    exit(0);
}

