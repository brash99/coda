/*  "cfg_file_display.c"  */
/*  E.J.  1/14/03,
          7/14/04 - for final cfg file format,
          9/27/04 - read name of input file  */

/*  for unix machine  */ 


#include <stdio.h>
#include <stdlib.h>
#include <strings.h>


void cfg_get(unsigned int *config);
void cfg_display(unsigned int *config);

char cfg_filename[256];
unsigned long t_offset;		

int 
main(int argc, char *argv[])
{
  unsigned int value; 
  unsigned int cfg_data[128];
	
  if (argc != 2) {
    printf("Usage: config_show <filename>\n");
    exit(1);
  }

  printf("Reading from config file %s\n",argv[1]);
  sprintf(cfg_filename,"%s\0",argv[1]);
  cfg_get(cfg_data);		/* read f1 configuration data */
  cfg_display(cfg_data);	       /* decode and display configuration data */

  exit(0);
}

void 
cfg_get(unsigned int *config)
{
  FILE *fd;
  int ii, jj;
  unsigned long value;
  
  fd = fopen(cfg_filename,"r");
  for(ii = 0; ii < 8; ii++)
    {
      for(jj = 0; jj < 16; jj++)
	{
	  fscanf(fd,"%lx",&value);
	  *(config + (16*ii + jj)) = 0xFFFF & value; 
	}    
    }
  fscanf(fd,"%ld",&value);
  t_offset = value;
  printf("\nTIME OFFSET (synchronous mode) = %ld\n", t_offset);    
  
  fclose(fd);
  return;
}

void 
cfg_display(unsigned int *config)
{
	unsigned int refcnt;
	unsigned int exponent;
	unsigned int refclkdiv;
	unsigned int hsdiv;
	unsigned int trigwin;
	unsigned int triglat;
	unsigned int cfg_offset;
	int ii,ii_chip;
	float factor;
	float tframe;
	float bin_size;
	float full_range;
	float window;
	float latency;
	float t_bin_size;
	
	for(ii_chip = 0; ii_chip <= 7; ii_chip++)
	{
	    printf("\n ---------------- Chip %d ----------------\n",ii_chip);
	    cfg_offset = 16 * ii_chip;	
	    for(ii = 0; ii < 16; ii++)
	        printf("%04X  ",*(config + cfg_offset + ii));
	    printf("\n");    
	
	    if( (*(config + cfg_offset + 1)) & 0x8000 )
	    {
	        printf("High Resolution mode\n");
	        factor = 0.5;
	    }    
	    else
	    {
	        printf("Normal Resolution mode\n");
	        factor = 1.;
	    }    
	    
	    if( (*(config + cfg_offset + 15)) & 0x4 )
	        printf("Synchronous mode\n");
	    else
	        printf("Non-synchronous mode\n");
	
	    refcnt = ( (*(config + cfg_offset + 7)) >> 6 ) & 0x1FF;
	    tframe = (float)(25 * (refcnt +2 ));
	    printf("refcnt = %d   tframe (ns) = %.1f\n",refcnt,tframe);
	
	    exponent =  ( (*(config + cfg_offset + 10)) >> 8 ) & 0x7;
	    refclkdiv = 1;
	    for(ii = 0; ii < exponent; ii++)
	        refclkdiv = 2 * refclkdiv;
	    hsdiv = (*(config + cfg_offset + 10)) & 0xFF;
	    bin_size = factor * (25./152.) * ( (float)refclkdiv )/( (float)hsdiv );
	    full_range = 65536 * bin_size;
	    printf("refclkdiv = %d   hsdiv = %d   bin_size (ns) = %.4f   full_range (ns) = %.1f\n",
		    refclkdiv,hsdiv,bin_size,full_range);
	
	    trigwin = *(config + cfg_offset + 8);
	    triglat = *(config + cfg_offset + 9); 	
	    window = ((float)trigwin) * bin_size/factor;	
	    latency = ((float)triglat) * bin_size/factor;	
	    printf("trigwin = %d   triglat = %d   window (ns) = %.1f   latency (ns) = %.1f\n",
		trigwin,triglat,window,latency);
	    t_bin_size = bin_size * 128.0;	
	    printf("trig_bin_size (ns) = %.4f\n", t_bin_size);
	}	
		
}

	            
