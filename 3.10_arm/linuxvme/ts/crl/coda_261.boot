# Boot file for CODA ROC 2.6.1 
# PowerPC version for MV6100 with Tsi148 VME<->PCI bridge

#loginUserAdd "abbottd","yzzbdbccd"

# Add route to outside world (from 29 subnet to 120 subnet)
#mRouteAdd("129.57.120.0","129.57.29.1",0xfffffc00,0,0)
# Add Routes for Multicast Support
mRouteAdd("224.0.0.0","129.57.29.0",0xf0000000,0,0)

# Load host table entries
< /daqfs/home/abbottd/VXKERN/vxhosts.boot

# Setup environment to load coda_roc
putenv "MSQL_TCP_HOST=dafarm28"
putenv "EXPID=DAQDEVEL"
putenv "TCL_LIBRARY=/daqfs/coda/2.6.2/common/lib/tcl7.4"
putenv "ITCL_LIBRARY=/daqfs/coda/2.6.2/common/lib/itcl2.0"
putenv "DP_LIBRARY=/daqfs/coda/2.6.2/common/lib/dp"
putenv "SESSION=daqSession"

# Load Tempe DMA Library
ld< /site/coda/2.6.1/extensions/tempeDma/usrTempeDma.o
# Setup Address and data modes for transfers
#
#  usrVmeDmaConfig(addrType, dataType, sstMode);
#
#  addrType = 0 (A16)    1 (A24)    2 (A32)
#  dataType = 0 (D16)    1 (D32)    2 (BLK32) 3 (MBLK) 4 (2eVME) 5 (2eSST)
#  sstMode  = 0 (SST160) 1 (SST267) 2 (SST320)
#
usrVmeDmaConfig(2,5,1)


# Load cMsg Stuff
cd "/daqfs/coda/2.6.2/cMsg/vxworks-ppc"
ld< lib/libcmsgRegex.o
ld< lib/libcmsg.o

cd "/daqfs/home/moffit/work/ts"
ld < vx/tsLib.o

tsInit(0xa80000,0,0);
tsResetIODelay();

cd "/daqfs/home/moffit/work/td"
ld < vx/tdLib.o

# Load the ROC
cd "/daqfs/coda/2.6.2/VXWORKSPPC/bin"
ld < coda_ts_rc3.6

# Spawn the ROC
taskSpawn ("ROC",200,8,250000,coda_roc,"-s","daqSession","-objects","vxts TS")


