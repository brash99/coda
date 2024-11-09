# Bootfile for CAEN 775 TDC crate
# with Highland V851 6 channel Digital Delay
# PowerPC version

cd "mizar:/home/abbottd/vxWorks/v775"

# Load Universe DMA Library
ld<../universeDma/universeDma.o.mv5100
#initialize (no interrupts (1))
sysVmeDmaInit(1) 
# Set for 64bit PCI transfers
sysVmeDmaSet(4,1)
# A24 VME Slave
sysVmeDmaSet(11,1)
# BLK32 (4) or MBLK(64) (5) VME transfers
sysVmeDmaSet(12,4)


# Load CAEN 775 TDC library
ld<c775Lib.o
c775Init(0x200000,0,1,0)
c775Clear(0)

# Load Highland V851 DDG library
ld<../v851/v851Lib.o
v851Init(0,0)
v851ProgExt(0)

# Load Kmax Server
ld<../kmax/kmax_server.o
sp kmax_server

# Enable Interrupts (defaults)
#c775IntConnect(0,0,0,0)
#c775IntEnable(0,4)

# Load Kmax Readout routine
ld< v775_readout.o
taskSpawn ("v775DAQ",200,0,50000,v775KmaxDAQ,0,2,0)
