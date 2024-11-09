#
# Description:  Makefile for v775Lib.o
#   This driver is specific to VxWorks BSPs and must be compiled
#   with access to vxWorks headers.
#
# SVN: $Rev: 401 $
#
#ARCH=Linux

#Check Operating system we are using
ifndef OSNAME
  OSNAME := $(subst -,_,$(shell uname))
endif

ifndef ARCH
  ARCH = VXWORKSPPC
endif

ifeq ($(OSNAME),SunOS)
LIBDIR = $(CODA)/$(ARCH)/lib
endif

ifeq ($(ARCH),VXWORKSPPC)
INCDIR=/site/vxworks/5.5/ppc/target/h
CC = ccppc
CFLAGS = -O $(DEFS)
LD = ldppc
DEFS = -mcpu=604 -DCPU=PPC604 -DVXWORKS -D_GNU_TOOL -DVXWORKSPPC
INCS = -Wall -fno-for-scope -fno-builtin -fvolatile -fstrength-reduce -mlongcall -I. -I$(INCDIR)

endif

ifeq ($(ARCH),VXWORKS68K51)
INCDIR=/site/vxworks/5.3/68k/target/h
CC = cc68k
CFLAGS = -O $(DEFS)
DEFS = -DCPU=MC68040 -DVXWORKS -DVXWORKS68K51
INCS = -Wall -mc68020 -fvolatile -fstrength-reduce -nostdinc -I. -I$(INCDIR)
endif

ifeq ($(ARCH),Linux)

ifndef LINUXVME_LIB
	LINUXVME_LIB	= $CODA/extensions/linuxvme/libs
endif
ifndef LINUXVME_INC
	LINUXVME_INC	= $CODA/extensions/linuxvme/include
endif
CC = gcc
AR = ar
RANLIB = ranlib
DEFS = -DJLAB
INCS = -I. -I${LINUXVME_INC}
CFLAGS = -O ${DEFS} -O2  -L. -L${LINUXVME_LIB}
ifdef DEBUG
CFLAGS += -Wall -g
endif

endif

ifeq ($(ARCH),Linux)
all: echoarch libc775.a
else
all: echoarch c775Lib.o v775_readout.o
endif


c775Lib.o: caen775Lib.c c775Lib.h
	$(CC) -c $(CFLAGS) $(INCS) -o $@ caen775Lib.c

libc775.a: c775Lib.o
	$(CC) -fpic -shared $(CFLAGS) $(INCS) -o libc775.so caen775Lib.c
	$(AR) ruv libc775.a c775Lib.o
	$(RANLIB) libc775.a

v775_readout.o: v775_readout.c c775Lib.h
	$(CC) -c $(CFLAGS) $(INCS) -o $@ v775_readout.c

links: libc775.a
	ln -sf $(PWD)/libc775.a $(LINUXVME_LIB)/libc775.a
	ln -sf $(PWD)/libc775.so $(LINUXVME_LIB)/libc775.so
	ln -sf $(PWD)/c775Lib.h $(LINUXVME_INC)/c775Lib.h

clean:
	rm -f *.o *.so *.a

echoarch:
	echo "Make for $(ARCH)"

rol:
	make -f Makefile-rol

rolclean:
	make -f Makefile-rol clean
