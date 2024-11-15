#
# File:
#    Makefile
#
# Description:
#    Makefile for the fADC250 (v2) Library using a VME Controller
#      running vxWorks or Linux
#
#
BASENAME=fadc
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

# Defs and build for VxWorks
ifeq (${ARCH}, PPC)
VXWORKS_ROOT		?= /site/vxworks/5.5/ppc/target
OS			= VXWORKS

ifdef LINUXVME_INC
VME_INCLUDE             ?= -I$(LINUXVME_INC)
endif

CC			= ccppc
LD			= ldppc
DEFS			= -mcpu=604 -DCPU=PPC604 -DVXWORKS -D_GNU_TOOL -mlongcall \
				-fno-for-scope -fno-builtin -fvolatile -DVXWORKSPPC
INCS			= -I. -I$(VXWORKS_ROOT)/h  \
				$(VME_INCLUDE)
CFLAGS			= $(INCS) $(DEFS)

else
OS			= LINUX
endif #OS=VXWORKS#

# Defs and build for Linux
ifeq ($(OS),LINUX)

# Safe defaults
LINUXVME_LIB		?= ../lib
LINUXVME_INC		?= ../include

CC			= gcc
ifeq ($(ARCH),i686)
CC			+= -m32
endif
AR                      = ar
RANLIB                  = ranlib
CFLAGS			= -L. -L${LINUXVME_LIB} ${LIB_CODA} -DLinux_vme -DLinux
INCS			= -I. -I${LINUXVME_INC} ${INC_CODA}

LIBS			= lib${BASENAME}.a lib${BASENAME}.so
endif #OS=LINUX#

ifdef DEBUG
CFLAGS			+= -Wall -g
else
CFLAGS			+= -O2
endif
SRC			= ${BASENAME}Lib.c fadc250Config.c
HDRS			= $(SRC:.c=.h)
OBJ			= ${BASENAME}Lib.o fadc250Config.o
DEPS			= $(SRC:.c=.d)

ifeq ($(OS),LINUX)
all: echoarch ${LIBS}
else
all: echoarch $(OBJ)
endif

%.o: %.c
	@echo " CC     $@"
	${Q}$(CC) $(CFLAGS) $(INCS) -c -o $@ $<

libfadc.so: ${SRC}
	@echo " CC     $@"
	${Q}$(CC) -fpic -shared $(CFLAGS) -lm $(INCS) -o $@ ${SRC}

%.a: $(OBJ)
	@echo " AR     $@"
	${Q}$(AR) ru $@ $<
	@echo " RANLIB $@"
	${Q}$(RANLIB) $@

ifeq ($(OS),LINUX)
install: $(LIBS)
	@echo " CP     $<"
	${Q}cp $(PWD)/$< $(LINUXVME_LIB)/$<
	@echo " CP     $(<:%.a=%.so)"
	${Q}cp $(PWD)/$(<:%.a=%.so) $(LINUXVME_LIB)/$(<:%.a=%.so)
	@echo " CP     ${BASENAME}Lib.h"
	${Q}cp ${PWD}/${BASENAME}Lib.h $(LINUXVME_INC)
	@echo " CP     fadc250Config.h"
	${Q}cp ${PWD}/fadc250Config.h $(LINUXVME_INC)

coda_install: $(LIBS)
	@echo " CODACP $<"
	${Q}cp $(PWD)/$< $(CODA_VME_LIB)/$<
	@echo " CODACP $(<:%.a=%.so)"
	${Q}cp $(PWD)/$(<:%.a=%.so) $(CODA_VME_LIB)/$(<:%.a=%.so)
	@echo " CODACP ${BASENAME}Lib.h"
	${Q}cp ${PWD}/${BASENAME}Lib.h $(CODA_VME)/include

%.d: %.c
	@echo " DEP    $@"
	${Q}set -e; rm -f $@; \
	$(CC) -MM -shared  -DLinux_vme -DLinux $(INCS) $< > $@.$$$$; \
	sed 's,\($*\)\.o[ :]*,\1.o $@ : ,g' < $@.$$$$ > $@; \
	rm -f $@.$$$$

-include $(DEPS)

endif

clean:
	@rm -vf ${BASENAME}Lib.{o,d} fadc250Config.{o,d} lib${BASENAME}.{a,so}

echoarch:
	@echo "Make for $(OS)-$(ARCH)"

.PHONY: clean echoarch
