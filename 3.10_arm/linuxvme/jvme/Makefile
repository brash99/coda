#
# File:
#    Makefile
#
# Description:
#    Makefile for the JLAB extensions to the VME Controller API (GEFANUC and others)
#
#
# Uncomment DEBUG line, to include some debugging info ( -g and -Wall)
DEBUG	?= 1
#QUIET	?= 1
#
ifeq ($(QUIET),1)
        Q = @
	DN = > /dev/null
else
        Q =
	DN =
endif

ARCH	?= $(shell uname -m)
OS	?= LINUX

# Check for CODA 3 environment
ifdef CODA_VME

INC_CODA	= -I${CODA_VME}/include
LIB_CODA	= -L${CODA_VME_LIB}

endif

# Some LINUXVME install defaults, if not already defined.
LINUXVME_LIB		?= ../lib
LINUXVME_INC		?= ../include

# Defs and build for VxWorks
ifeq (${ARCH}, PPC)
OS			= VXWORKS
VXWORKS_ROOT 		?= /site/vxworks/5.5/ppc/target

CC			= ${Q}ccppc
LD			= ${Q}ldppc
AR			= arppc
RANLIB			= ranlibppc
DEFS			= -mcpu=604 -DCPU=PPC604 -DVXWORKS -D_GNU_TOOL -mlongcall \
				-fno-for-scope -fno-builtin -fvolatile -DVXWORKSPPC \
				-fstrength-reduce
ifdef DEBUG
DEFS  			+= -Wall -g
endif
INCS			= -I. -I$(VXWORKS_ROOT)/h -I$(VXWORKS_ROOT)/h/rpc -I$(VXWORKS_ROOT)/h/net
CFLAGS			= $(INCS) $(DEFS)

SRC			= jvme.c
HDRS			= ${SRC:.c=.h}
OBJ			= ${SRC:.c=.o}
LIBS			= vxjvme.o vxjvme_univ.o vxjvme_univ_MV5100.o

all: echoarch $(LIBS)

jvme.o: jvme.c
	@echo " CC     $@"
	$(CC) $(CFLAGS) -c -o $@ $<

jvme_univ.o: jvme.c
	@echo " CC     $@"
	$(CC) $(CFLAGS) -c -o $@ -DVXWORKS_UNIV $<

usrTempeDma.o: usrTempeDma.c sysTempeDma.c tempe.h
	@echo " CC     $@"
	$(CC) $(CFLAGS) -c -o $@ usrTempeDma.c

universeDma.o: universeDma.c universeDma.h
	@echo " CC     $@"
	$(CC) $(CFLAGS) -DEXTENDED_VME -c -o $@ universeDma.c

universeDma_MV5100.o: universeDma.c universeDma.h
	@echo " CC     $@"
	$(CC) $(CFLAGS) -DEXTENDED_VME -DMV5100 -c -o $@ universeDma.c

vxjvme.o: jvme.o usrTempeDma.o
	@echo " LD     $<"
	$(LD) -o $@ -r jvme.o usrTempeDma.o

vxjvme_univ.o: jvme_univ.o universeDma.o
	@echo " LD     $<"
	$(LD) -o $@ -r jvme_univ.o universeDma.o

vxjvme_univ_MV5100.o: jvme_univ.o universeDma_MV5100.o
	@echo " LD     $<"
	$(LD) -o $@ -r jvme_univ.o universeDma_MV5100.o

echoarch:
	@echo "Make for $(OS)-$(ARCH)"

clean:
	@rm -vf *.o *.a
endif #OS=VXWORKS

# Defs and build for Linux
ifeq ($(OS),LINUX)

# Build of x86_64 / i686  (! armv7l)
ifneq ($(ARCH),armv7l)
CROSS_COMPILE		=
CC			= $(Q)$(CROSS_COMPILE)gcc
AR                      = $(Q)ar
RANLIB                  = ranlib
ifdef DEBUG
CFLAGS			= -Wall -g
else
CFLAGS			= -O2
endif
CFLAGS		       += -I. -I/usr/include
CFLAGS                 += -DGEFANUC -DARCH_${ARCH}=1

SRC			= jvme.c jlabTsi148.c jlabUniverseII.c jlabgef.c jlabgefDMA.c dmaPList.c

OBJS			= $(SRC:%.c=%.o)
DEPS			= $(SRC:%.c=%.d)

LIBS			= libjvme.a
SHLIBS			= libjvme.so

ifeq ($(ARCH),x86_64)
	libdir:=/usr/lib64/gef
else
	libdir:=/usr/lib/gef
endif

# End in error, if the gefvme-kmod-common RPM is not installed.
ifeq (,$(wildcard $(libdir)/libvme.a))
$(error gefvme-kmod-common RPM not installed! $(ARCH))
endif

all: echoarch $(SHLIBS) $(LIBS)

libjvme.a: $(OBJS) $(libdir)/libvme.a Makefile
	$(AR) x $(libdir)/libvme.a
	@echo " AR     $@"
	$(AR) rv $@ $(OBJS) $(DN)
	$(AR) rv $@ gefcmn_*.o $(DN)

libjvme.so: $(SRC) $(libdir)/libvme.a Makefile libjvme.a
	@echo " CC     $@"
	$(CC) -fpic -shared $(CFLAGS) -lrt -lpthread -o $@ $(SRC) gefcmn_*.o
	@rm gefcmn_*.o

clean distclean:
	@rm -f $(OBJS) $(DEPS) $(LIBS) $(SHLIBS) *~

install: libjvme.a libjvme.so
	@echo " CP     libjvme.so"
	$(Q)cp -v $(PWD)/libjvme.so $(LINUXVME_LIB) $(DN)
	@echo " CP     libjvme.a"
	$(Q)cp -v $(PWD)/libjvme.a $(LINUXVME_LIB) $(DN)
	@echo " CP     dmaPList.h"
	$(Q)cp -v $(PWD)/dmaPList.h $(LINUXVME_INC) $(DN)
	@echo " CP     jvme.h"
	$(Q)cp -v $(PWD)/jvme.h $(LINUXVME_INC) $(DN)

coda_install: checkcoda libjvme.a libjvme.so
	@echo " CODACP libjvme.so"
	$(Q)cp -v $(PWD)/libjvme.so $(CODA_VME_LIB)/libjvme.so $(DN)
	@echo " CODACP libjvme.a"
	$(Q)cp -v $(PWD)/libjvme.a $(CODA_VME_LIB)/libjvme.a $(DN)
	@echo " CODACP dmaPList.h"
	$(Q)cp -v $(PWD)/dmaPList.h $(CODA_VME)/include $(DN)
	@echo " CODACP jvme.h"
	$(Q)cp -v $(PWD)/jvme.h $(CODA_VME)/include $(DN)

checkcoda:
ifndef CODA
	$(error CODA Environment Variable not defined!)
endif

%.o: %.c
	@echo " CC     $@"
	$(CC) $(CFLAGS) -o $@ -c $<

%.d: %.c
	@echo " DEP    $@"
	$(CC) -M $(CFLAGS) $< > $@

-include $(DEPS)

echoarch:
	@echo "Make for $(OS)-$(ARCH)"

.PHONY: all clean distclean checkcoda

else #ARCH==arm

CROSS_COMPILE		=
CC			= $(Q)$(CROSS_COMPILE)gcc
AR                      = $(Q)ar
RANLIB                  = ranlib
ifdef DEBUG
CFLAGS			= -Wall -g
else
CFLAGS			= -O2
endif
CFLAGS		       += -I. -I/usr/include
CFLAGS                 += -DARCH_${ARCH}=1

SRC			= jvme.c

OBJS			= $(SRC:%.c=%.o)
DEPS			= $(SRC:%.c=%.d)

LIBS			= libjvme.a
SHLIBS			= libjvme.so

all: echoarch $(SHLIBS) $(LIBS)

libjvme.so: $(SRC) Makefile
	@echo " CC     $@"
	$(CC) -fpic -shared $(CFLAGS) -lrt -lpthread -o $@ $(SRC)

libjvme.a: $(OBJS) Makefile
	@echo " AR     $@"
	$(AR) rv $@ $(OBJS) $(DN)

clean distclean:
	@rm -f $(OBJS) $(DEPS) $(LIBS) $(SHLIBS) *~

install: libjvme.a libjvme.so
	@echo " CP     libjvme.so"
	$(Q)cp -v $(PWD)/libjvme.so $(LINUXVME_LIB)/ $(DN)
	@echo " CP     libjvme.a"
	$(Q)cp -v $(PWD)/libjvme.a $(LINUXVME_LIB)/ $(DN)
	@echo " CP     jvme.h"
	$(Q)cp -v $(PWD)/jvme.h $(LINUXVME_INC)/ $(DN)

coda_install: checkcoda libjvme.a libjvme.so
	@echo " CODACP libjvme.so"
	$(Q)cp -v $(PWD)/libjvme.so $(CODA_VME_LIB)/libjvme.so $(DN)
	@echo " CODACP libjvme.a"
	$(Q)cp -v $(PWD)/libjvme.a $(CODA_VME_LIB)/libjvme.a $(DN)
	@echo " CODACP jvme.h"
	$(Q)cp -v $(PWD)/jvme.h $(CODA_VME)/include/ $(DN)

checkcoda:
ifndef CODA
	$(error CODA Environment Variable not defined!)
endif

%.o: %.c
	@echo " CC     $@"
	$(CC) $(CFLAGS) -o $@ -c $<

%.d: %.c
	@echo " DEP    $@"
	$(CC) -M $(CFLAGS) $< > $@

-include $(DEPS)

echoarch:
	@echo "Make for $(OS)-$(ARCH)"

.PHONY: all clean distclean checkcoda

endif #ARCH=armv71

endif #OS=Linux
