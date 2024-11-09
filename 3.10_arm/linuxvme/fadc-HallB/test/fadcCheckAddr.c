/*
 * File:
 *    fadcCheckAddr.c
 *
 * Description:
 *    Check the memory map of the FADC V2
 *
 *
 */


#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "jvme.h"
#define fa fADC250
#include "fadcLib.h"


int 
main(int argc, char *argv[]) {

    GEF_STATUS status;

    printf("\nJLAB fadc Lib Tests\n");
    printf("----------------------------\n");

    fADC250CheckAddresses(0);

    exit(0);
}

