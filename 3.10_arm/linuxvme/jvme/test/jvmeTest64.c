/*
 * File:
 *    jvmeTest.c
 *
 * Description:
 *    Test JLab Vme Driver
 *
 *
 */


#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "jvme.h"

int 
main(int argc, char *argv[]) {

    vmeOpenDefaultWindows();

    vmeSetDebugFlags(0xffffffff);

    vmeCloseDefaultWindows();

    exit(0);
}

