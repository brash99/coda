#
# File:
#    Makefile
#
# Description:
#    Makefile for the JLAB extensions to the GEFANUC Controller API
#
#
# $Date$
# $Rev$
#

ifndef LINUXVME_LIB
	LINUXVME_LIB	= $CODA/extensions/linuxvme/libs
endif
ifndef LINUXVME_INC
	LINUXVME_INC	= $CODA/extensions/linuxvme/include
endif



CROSS_COMPILE		=
CC			= $(CROSS_COMPILE)gcc
AR                      = ar
RANLIB                  = ranlib
CFLAGS			= -Wall -g  \
			-I. -I${LINUXVME_INC} \
			-L. -L${LINUXVME_LIB} -DJLAB
LINKLIBS		= -lrt -ljvme
PROGS			= jvmeTest shmTest

all: $(PROGS)

jvmeTest.o: jvmeTest.c
	$(CC) -c $(CFLAGS) $(INCS) -o $@ $<

dmaReadTest.o: dmaReadTest.c
	$(CC) -c $(CFLAGS) $(INCS) -o $@ $<

dmaPListTest.o: dmaPListTest.c
	$(CC) -c $(CFLAGS) $(INCS) -o $@ $<

readSpeed.o: readSpeed.c
	$(CC) -c $(CFLAGS) $(INCS) -o $@ $<

clean distclean:
	@rm -f $(PROGS) *~ *.o

%: %.c
	$(CC) $(CFLAGS) -o $@ $(@:%=%.c) $(LINKLIBS)

.PHONY: all clean distclean
