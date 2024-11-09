#
# Description:  Makefile for f1tdcLib.o
#   This driver is specific to VxWorks BSPs and must be compiled
#   with access to vxWorks headers.
#
# SVN: $Rev$
#
# Uncomment DEBUG line for debugging info ( -g and -Wall )
DEBUG=1
#
ARCH=Linux

#Check Operating system we are using
ifndef OSNAME
  OSNAME := $(subst -,_,$(shell uname))
endif

ifndef ARCH
  ARCH = VXWORKSPPC
endif

ifndef WIND_BASE
  WIND_BASE = /site/vxworks/5.5/ppc
endif


ifeq ($(OSNAME),SunOS)
CODALIB = $(CODA)/$(OSNAME)/lib
CODAINC = $(CODA)/common/include
CFLAGS = -O $(DEFS)
endif

ifeq ($(ARCH),Linux)

ifndef LINUXVME_LIB
	LINUXVME_LIB	= $CODA/extensions/linuxvme/libs
endif
ifndef LINUXVME_INC
	LINUXVME_INC	= $CODA/extensions/linuxvme/include
endif

CODALIB		= $(CODA)/$(OSNAME)/lib
CODAINC		= $(CODA)/common/include
CFLAGS		= $(DEFS) \
		-I. -I${LINUXVME_INC} \
		-L. -L${LINUXVME_LIB} -DJLAB
ifdef DEBUG
CFLAGS		+= -Wall -g
else
CFLAGS		+= -O
endif
AR = ar
RANLIB = ranlib
endif

ifeq ($(ARCH),VXWORKSPPC)
INCDIR=$(WIND_BASE)/target/h
CC = ccppc
LD = ldppc
DEFS = -mcpu=604 -DCPU=PPC604 -DVXWORKS -D_GNU_TOOL -DVXWORKSPPC
CFLAGS = -O $(DEFS)
INCS = -fno-for-scope -fno-builtin -fvolatile -fstrength-reduce -mlongcall -I. -I$(CODAINC) -I$(INCDIR)
endif

ifeq ($(ARCH),VXWORKS68K51)
INCDIR=/site/vxworks/5.3/68k/target/h
CC = cc68k
DEFS = -DCPU=MC68040 -DVXWORKS -DVXWORKS68K51
CFLAGS = -O $(DEFS)
INCS = -Wall -mc68020 -fvolatile -fstrength-reduce -nostdinc -I. -I$(INCDIR)
endif


ifeq ($(ARCH),Linux)
all: echoarch libf1tdc.a config_make config_show links
else
all: echoarch f1tdcLib.o config_make config_show
endif

f1tdcLib.o: f1tdcLib.c f1tdcLib.h
	$(CC) -c $(CFLAGS) $(INCS) -o $@ f1tdcLib.c

config_make: cfg_file_make.c
	cc -o $@ cfg_file_make.c
	cp $@ $@.$(OSTYPE)

config_show: cfg_file_display.c
	cc -o $@ cfg_file_display.c
	cp $@ $@.$(OSTYPE)

clean:
	rm -f f1tdcLib.o libf1tdc.{a,so} config_make config_show *~

echoarch:
	echo "Make for $(ARCH)"

ifeq ($(ARCH),Linux)
libf1tdc.a: f1tdcLib.o
	$(CC) -fpic -shared $(CFLAGS) -o libf1tdc.so f1tdcLib.c
	$(AR) ruv libf1tdc.a f1tdcLib.o
	$(RANLIB) libf1tdc.a

links: libf1tdc.a
	ln -sf $(PWD)/libf1tdc.a $(LINUXVME_LIB)/libf1tdc.a
	ln -sf $(PWD)/libf1tdc.so $(LINUXVME_LIB)/libf1tdc.so
	ln -sf $(PWD)/f1tdcLib.h $(LINUXVME_INC)/f1tdcLib.h

rol:
	make -f Makefile-rol

rolclean:
	make -f Makefile-rol clean
endif
