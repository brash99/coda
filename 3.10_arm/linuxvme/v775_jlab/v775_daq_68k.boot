# Bootfile for crate containing CAEN V775 TDCs
# 68K version

cd "mizar:/home/abbottd/vxWorks/v775"

# Load CAEN 775 TDC library, Initialize for 1 TDC and Clear it
ld<c775Lib.o
c775Init(0x100000,0,1,0)
c775Clear(0)

# Optional Enable Interrupts (defaults)
#c775IntConnect(0,0,0,0)
#c775IntEnable(0,4)

# Optional - allocate memory for DMA transfer tests
#mydata = malloc(4096)
