# Bootfile for CAEN 792 ADC crate
# PowerPC version

cd "alcor:/mizar/home/abbottd/vxWorks/v792"

# Load Universe DMA Library
ld<../universeDma/universeDma.o
#initialize (no interrupts)
sysVmeDmaInit(1) 
# Set for 64bit PCI transfers
sysVmeDmaSet(4,1)
# A24 VME Slave
sysVmeDmaSet(11,1)
# BLK32 (4) or MBLK(64) (5) VME transfers
sysVmeDmaSet(12,4)


# Load CAEN 792 QDC library
ld<c792Lib.o
c792Init(0x00da0000,0,1,0)
c792Clear(0)
c792Sparse(0,0,0)

# Setup some local debug variables
mydata = malloc(8192)
bzero mydata,8192

# Enable Interrupts (defaults)
#c792IntConnect(0,0,0,0)
#c792IntEnable(0,4)

# Load ROC
#ld<rocMon
#rocMsgInit(0,0)

