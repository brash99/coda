/* Module: f1FirmwareToos.c
 *
 * Description: f1TDC Firmware Tools Library
 *              Firmware specific functions.
 *
 * Author:
 *        Bryan Moffit and Ed Jastrzembski
 *        JLab Data Acquisition Group
 *
 * Revision History:
 *      Revision 1.0   2013/08/20  moffit
 *         - Initial version for f1TDC v2 and v3
 *
 */

#include <stdlib.h>
#include "jvme.h"
unsigned char reverse(unsigned char b);

/* #define DEBUGFW */
#define MAX_FW_DATA 0x800000
unsigned char fw_data[MAX_FW_DATA];
int fw_size=0;

int
f1FirmwareReadFile(char *filename)
{
  FILE *fwFile = NULL;
  int c=0, idata=0, rval=OK;

  printf(" Opening firmware file: %s\n",filename);
  fwFile = fopen(filename,"r");
  if(fwFile==NULL)
    {
      perror("fopen");
      printf("%s: ERROR opening file (%s) for reading\n",
	     __FUNCTION__,filename);
      return ERROR;
    }

  while( (c = getc(fwFile)) != EOF )
    {
      fw_data[idata] = reverse(c);
      idata++;
      if(idata>=MAX_FW_DATA)
	{
	  printf("%s: ERROR: Firmware size greater than expected: %d (0x%x)\n",
		 __FUNCTION__,idata, idata);
	  rval=ERROR;
	  break;
	}
    }
  fclose(fwFile);

  fw_size = idata++;

#ifdef DEBUGFW
  printf("%s: Firmware size: %d (0x%x)\n",__FUNCTION__,fw_size,fw_size);
#endif

  return rval;
}

int
f1FirmwareEraseEPROM(int id)
{
  unsigned int csr=0, busy=0;
  int iprint=0;
  if(id==0) id=f1ID[0];

  if((id<=0) || (id>21) || (f1p[id] == NULL)) 
    {
      printf("%s: ERROR : TDC in slot %d is not initialized \n",
	     __FUNCTION__,id);
      return(ERROR);
    }

  F1LOCK;
#ifdef DEBUGFW
  printf("%s: BULK ERASE\n",__FUNCTION__);
#endif

  vmeWrite32(&f1p[id]->config_csr, 0xC0000000);	// set up for bulk erase
  csr = vmeRead32(&f1p[id]->config_csr);
#ifdef DEBUGFW
  printf("\n--- CSR = %X\n", csr);
#endif
      
  vmeWrite32(&f1p[id]->config_data, 0);		// write triggers erase
  csr = vmeRead32(&f1p[id]->config_csr);
#ifdef DEBUGFW
  printf("\n--- CSR = %X\n", csr);
#endif
      
  printf("     Erasing EPROM\n");
  printf("%2d: ",id);
  fflush(stdout);
  do {
    if((iprint%100)==0)
      {
	printf(".");
	fflush(stdout);
      }
    taskDelay(1);
    csr = vmeRead32(&f1p[id]->config_csr);      // test for busy
    busy = (csr & 0x100) >> 8;
    iprint++;
  } while(busy);

  printf(" Done!\n");

  csr = vmeRead32(&f1p[id]->config_csr);
#ifdef DEBUGFW
  printf("\n--- CSR = %X\n", csr);
#endif
  vmeWrite32(&f1p[id]->config_csr, 0);		// set up for read

  F1UNLOCK;
  return OK;
}

int
f1FirmwareGEraseEPROM()
{
  unsigned int csr=0, busy=0;
  int iprint=0;
  int id, if1;

  F1LOCK;
#ifdef DEBUGFW
  printf("%s: GLOBAL BULK ERASE\n",__FUNCTION__);
#endif

  printf("     Erasing EPROM\n");
  for(if1=0; if1<nf1tdc; if1++)
    {
      id = f1Slot(if1);
      vmeWrite32(&f1p[id]->config_csr, 0xC0000000);	// set up for bulk erase
      csr = vmeRead32(&f1p[id]->config_csr);
#ifdef DEBUGFW
      printf("\n--- CSR = %X\n", csr);
#endif
      
      vmeWrite32(&f1p[id]->config_data, 0);		// write triggers erase
      csr = vmeRead32(&f1p[id]->config_csr);
#ifdef DEBUGFW
      printf("\n--- CSR = %X\n", csr);
#endif
    }

  for(if1=0; if1<nf1tdc; if1++)
    {
      id = f1Slot(if1);
      printf("%2d: ",id);
      fflush(stdout);
      do {
	if((iprint%100)==0)
	  {
	    printf(".");
	    fflush(stdout);
	  }
	taskDelay(1);
	csr = vmeRead32(&f1p[id]->config_csr);      // test for busy
	busy = (csr & 0x100) >> 8;
	iprint++;
      } while(busy);

      printf(" Done!\n");

      csr = vmeRead32(&f1p[id]->config_csr);
#ifdef DEBUGFW
      printf("\n--- CSR = %X\n", csr);
#endif
      vmeWrite32(&f1p[id]->config_csr, 0);		// set up for read
    }

  F1UNLOCK;
  return OK;
}


int
f1FirmwareDownloadConfigData(int id, int print_header)
{
  int iaddr=0, busy=0;
  unsigned int data_word=0, value=0;
  if(id==0) id=f1ID[0];

  if((id<=0) || (id>21) || (f1p[id] == NULL)) 
    {
      printf("%s: ERROR : TDC in slot %d is not initialized \n",
	     __FUNCTION__,id);
      return(ERROR);
    }

  if(fw_size==0)
    {
      printf("%s: ERROR: FW File not yet loaded\n",
	     __FUNCTION__);
      return ERROR;
    }

#ifdef DEBUGFW
  printf("%s: WRITE CFG DATA\n",__FUNCTION__);
#endif

  F1LOCK;
  vmeWrite32(&f1p[id]->config_csr, 0x80000000);	// set up for byte writes
      
  if(print_header)
    printf("     Writing to EPROM\n");

  printf("%2d: ",id);
  fflush(stdout);
  for(iaddr=0; iaddr<fw_size; iaddr++)
    {
      data_word = (iaddr << 8) | fw_data[iaddr];		  
      vmeWrite32(&f1p[id]->config_data, data_word);
	  
      do {
	value = vmeRead32(&f1p[id]->config_csr);	// test for busy
	busy = (value & 0x100) >> 8;
      } while(busy);

      if((iaddr%100000)==0)
	{
	  printf(".");
	  fflush(stdout);
	}
    }
  printf(" Done!\n");
      
  vmeWrite32(&f1p[id]->config_csr, 0);			// default state is read
  F1UNLOCK;

  return OK;
}

int
f1FirmwareGDownloadConfigData()
{
  int id, if1;
  int rval=0;

  printf("     Writing to EPROM\n");
  for(if1=0; if1<nf1tdc; if1++)
    {
      id = f1Slot(if1);
      rval |= f1FirmwareDownloadConfigData(id,0);
    }

  return rval;
}

int
f1FirmwareVerifyDownload(int id, int print_header)
{
  unsigned int data_word=0;
  int idata=0, busy=0, error=0;

  if(id==0) id=f1ID[0];

  if((id<=0) || (id>21) || (f1p[id] == NULL)) 
    {
      printf("%s: ERROR : TDC in slot %d is not initialized \n",
	     __FUNCTION__,id);
      return(ERROR);
    }

  if(fw_size==0)
    {
      printf("%s: ERROR: FW File not yet loaded\n",
	     __FUNCTION__);
      return ERROR;
    }

  F1LOCK;
  vmeWrite32(&f1p[id]->config_csr, 0); // Set up for read

  if(print_header)
    printf("     Verifying Data\n");
  printf("%2d: ",id);
  fflush(stdout);
  for(idata =0; idata<fw_size; idata++)
    {
      data_word = (idata<<8);
      vmeWrite32(&f1p[id]->config_data, data_word);

      do {
	busy = (vmeRead32(&f1p[id]->config_csr) & 0x100)>>8;
      } while (busy);
      
      data_word = vmeRead32(&f1p[id]->config_csr) & 0xFF;

      if(data_word != fw_data[idata])
	{
	  printf(" Data ERROR: addr = 0x%06x   data: 0x%02x  !=  file: 0x%02x\n",
		 idata, data_word, fw_data[idata]);
	  error=1;
	}
      if((idata%100000)==0)
	{
	  printf(".");
	  fflush(stdout);
	}
    }
  F1UNLOCK;
  printf(" Done!\n");
  
  if(error)
    return ERROR;
#ifdef DEBUGFW
  else
    printf("%s: Download Verified !\n",__FUNCTION__);
#endif

  return OK;
}

int
f1FirmwareGVerifyDownload()
{
  int id, if1;
  int rval=0;

  printf("     Verifying Data\n");
  for(if1=0; if1<nf1tdc; if1++)
    {
      id = f1Slot(if1);
      rval |= f1FirmwareVerifyDownload(id,0);
    }

  return rval;
}



unsigned char 
reverse(unsigned char b) 
{
  b = (b & 0xF0) >> 4 | (b & 0x0F) << 4;
  b = (b & 0xCC) >> 2 | (b & 0x33) << 2;
  b = (b & 0xAA) >> 1 | (b & 0x55) << 1;
  return b;
}




