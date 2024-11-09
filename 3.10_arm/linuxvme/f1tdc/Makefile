#
# File:
#    Makefile
#
# Description:
#    Makefile for f1TDC v2/v3 driver.
#
#
BASENAME=f1tdc
#
# Uncomment DEBUG line, to include some debugging info ( -g and -Wall)
DEBUG	?= 1
QUIET	?= 1
#
ifeq ($(QUIET),1)
        Q = @
else
        Q =
endif

ARCH	?= $(shell uname -m)

# Check for CODA 3 environment
ifdef CODA_VME

INC_CODA	= -I${CODA_VME}/include
LIB_CODA	= -L${CODA_VME_LIB}

endif

# Defs and build for PPC using VxWorks
ifeq (${ARCH}, PPC)
OS			= VXWORKS
VXWORKS_ROOT		?= /site/vxworks/5.5/ppc/target

ifdef LINUXVME_INC
VME_INCLUDE             ?= -I$(LINUXVME_INC)
endif

CC			= ccppc
LD			= ldppc
DEFS			= -mcpu=604 -DCPU=PPC604 -DVXWORKS -D_GNU_TOOL -mlongcall \
				-fno-for-scope -fno-builtin -fvolatile -DVXWORKSPPC
INCS			= -I. -I$(VXWORKS_ROOT)/h  \
				$(VME_INCLUDE) ${INC_CODA}
CFLAGS			= $(INCS) $(DEFS)

else
OS			= LINUX
endif #OS=VXWORKS#

# Defs and build for Linux
ifeq ($(OS),LINUX)
LINUXVME_LIB		?= ../lib
LINUXVME_INC		?= ../include

CC			= gcc
ifeq ($(ARCH),i686)
CC			+= -m32
endif
AR                      = ar
RANLIB                  = ranlib
CFLAGS			= -L. -L${LINUXVME_LIB} ${LIB_CODA}
INCS			= -I. -I${LINUXVME_INC} ${INC_CODA}

LIBS			= lib${BASENAME}.a lib${BASENAME}.so
endif #OS=LINUX#

ifdef DEBUG
CFLAGS			+= -Wall -g
else
CFLAGS			+= -O2
endif
SRC			= ${BASENAME}Lib.c
HDRS			= $(SRC:.c=.h)
OBJ			= ${BASENAME}Lib.o
DEPS			= $(SRC:.c=.d) config_make.d config_show.d

ifeq ($(OS),LINUX)
all: echoarch ${LIBS} config_make config_show
else
all: echoarch $(OBJ)
endif

%.o: %.c
	@echo " CC     $@"
	${Q}$(CC) $(CFLAGS) $(INCS) -c -o $@ $(SRC)

%.so: $(SRC)
	@echo " CC     $@"
	${Q}$(CC) -fpic -shared $(CFLAGS) $(INCS) -o $(@:%.a=%.so) $(SRC)

%.a: $(OBJ)
	@echo " AR     $@"
	${Q}$(AR) ru $@ $<
	@echo " RANLIB $@"
	${Q}$(RANLIB) $@

ifeq ($(OS),LINUX)
config_make: cfg_file_make.c
	@echo " CC     $@"
	${Q}$(CC) -o $@ $<

config_show: cfg_file_display.c
	@echo " CC     $@"
	${Q}$(CC) -o $@ $<

install: $(LIBS)
	@echo " CP     $<"
	${Q}cp $(PWD)/$< $(LINUXVME_LIB)/$<
	@echo " CP     $(<:%.a=%.so)"
	${Q}cp $(PWD)/$(<:%.a=%.so) $(LINUXVME_LIB)/$(<:%.a=%.so)
	@echo " CP     ${BASENAME}Lib.h"
	${Q}cp ${PWD}/${BASENAME}Lib.h $(LINUXVME_INC)

coda_install: $(LIBS)
	@echo " CODACP $<"
	${Q}cp $(PWD)/$< $(CODA_VME_LIB)/$<
	@echo " CODACP $(<:%.a=%.so)"
	${Q}cp $(PWD)/$(<:%.a=%.so) $(CODA_VME_LIB)/$(<:%.a=%.so)
	@echo " CODACP ${BASENAME}Lib.h"
	${Q}cp ${PWD}/${BASENAME}Lib.h $(CODA_VME)/include

%.d: %.c
	@echo " DEP    $@"
	@set -e; rm -f $@; \
	$(CC) -MM -shared $(INCS) $< > $@.$$$$; \
	sed 's,\($*\)\.o[ :]*,\1.o $@ : ,g' < $@.$$$$ > $@; \
	rm -f $@.$$$$

-include $(DEPS)

endif

clean:
	@rm -vf ${BASENAME}Lib.{o,d} lib${BASENAME}.{a,so} config_show config_make

echoarch:
	@echo "Make for $(OS)-$(ARCH)"

.PHONY: clean echoarch
