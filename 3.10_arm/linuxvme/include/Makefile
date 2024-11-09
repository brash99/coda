#
# Description:  Makefile for rol_include
#   This Makefile simply makes softlinks of the headers contained
#   within this directory to the LINUXVME_INC directory.
#
# SVN: $Rev: 396 $
#

LINUXVME_LIB	?= ../lib
LINUXVME_INC	?= ../include
LINUXVME_BIN	?= ../bin

HEADERS=$(wildcard *.c)
HEADERS+=$(wildcard *.h)
BINARIES=$(wildcard bin/*)

all: install

links: $(HEADERS) $(BINARIES)
	ln -sf ${HEADERS} $(LINUXVME_INC)/
	ln -sf ${BINARIES} $(LINUXVME_BIN)/

install: $(HEADERS) $(BINARIES)
	cp -v $(HEADERS) $(LINUXVME_INC)/
	cp -v $(BINARIES) $(LINUXVME_BIN)/

clean:
	rm *~