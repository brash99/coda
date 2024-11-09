#
# File:
#    Makefile
#
# Description:
#    Makefile for the testing remex
#
#
# $Date$
# $Rev$
#

DEBUG	?= 1
QUIET	?= 1
#
ifeq ($(QUIET),1)
        Q = @
else
        Q =
endif

ARCH	?= $(shell uname -m)
OS	?= LINUX

# Check for CODA 3 environment
ifdef CODA_LIB

INC_CMSG	?= -I${CODA}/common/include
LIB_CMSG	?= -L${CODA_LIB}

endif

# linuxvme defaults, if they're not already defined
LINUXVME_INC	?= ../../include
LINUXVME_LIB	?= ../../lib
LINUXVME_BIN	?= ../../bin

CROSS_COMPILE		=
CC			= $(CROSS_COMPILE)gcc
AR                      = ar
RANLIB                  = ranlib
INCS			= -I. -I../ ${INC_CMSG}
CFLAGS			= -L. -L../ ${LIB_CMSG} -lrt -lremex -dl -lcmsg -lcmsgRegex
ifeq ($(DEBUG),1)
	CFLAGS		+= -Wall -g
endif

SRC			= $(wildcard *.c)
DEPS			= $(SRC:.c=.d)
OBJ			= $(SRC:.c=.o)
PROGS			= $(SRC:.c=)

all: echoarch $(PROGS)

clean distclean:
	@rm -f $(PROGS) *~ $(OBJS) $(DEPS)

%: %.c
	@echo " CC     $@"
	${Q}$(CC) $(CFLAGS) $(INCS) -o $@ $<

%.d: %.c
	@echo " DEP    $@"
	@set -e; rm -f $@; \
	$(CC) -MM -shared $(INCS) $< > $@.$$$$; \
	sed 's,\($*\)\.o[ :]*,\1 $@ : ,g' < $@.$$$$ > $@; \
	rm -f $@.$$$$

-include $(DEPS)

.PHONY: all clean distclean

echoarch:
	@echo "Make for $(OS)-$(ARCH)"
