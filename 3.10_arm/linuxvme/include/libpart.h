/*
   header file for the memory allocation system . Keep all structure
   elements (for danode) aligned on 4-byte boundaries. This insures that malloc()
   will create event buffer nodes that are 4-byte aligned.
*/

#ifndef  __INC_MEM_PART__
#define __INC_MEM_PART__

#ifndef NULL
#define NULL ((void *) 0)
#endif

#define maximum(a,b) (a<b ? b : a)

/* Comment out to get TIRPCI to compile - do we really need it  - DJA 10/08
extern char	*malloc();
*/


#include "mempart.h"

/* global data */


#endif
