#
# Makefile to build linuxvme distribution
#

ARCH ?= $(shell uname -m)
OS   ?= LINUX

LINUXVME_DIR ?= ${PWD}
LINUXVME_INC ?= ${LINUXVME_DIR}/include
LINUXVME_LIB ?= ${LINUXVME_DIR}/Linux-${ARCH}/lib
LINUXVME_BIN ?= ${LINUXVME_DIR}/Linux-${ARCH}/bin

# Export these definitions to the sub Makefiles
export LINUXVME_LIB LINUXVME_INC LINUXVME_BIN

LIB_DIRS = jvme
BIN_DIRS =

ifneq ($(ARCH),armv7l)
LIB_DIRS += ti td ts sd ctp remex\
		f1tdc fa125 fadc ssp tir tsUtil vmeDSC gtp vfTDC fadc-HallB f1tdc-v1
BIN_DIRS += jvme
endif

all: echoarch libs firmware bins

echoarch:
	@echo Make for $(ARCH)

libs: dirs
	for dir in $(LIB_DIRS); do \
		echo "Making $$dir"; \
		cd ${LINUXVME_DIR}/$$dir; \
		make install; \
		echo "done";\
	done

bins: libs
	@for dir in $(BIN_DIRS); do \
		echo "Making $$dir executables"; \
		cd ${LINUXVME_DIR}/$$dir/src; \
		make install; \
		echo "done";\
	done

dirs:
	@install -d ${LINUXVME_DIR}
	@install -d ${LINUXVME_INC}
	@install -d ${LINUXVME_LIB}
	@install -d ${LINUXVME_BIN}

clean:
	@for dir in $(LIB_DIRS); do \
		echo "Cleaning $$dir"; \
		cd ${LINUXVME_DIR}/$$dir; \
		make ${DEFS} clean; \
		echo "done";\
	done

.PHONY: libs firmware
