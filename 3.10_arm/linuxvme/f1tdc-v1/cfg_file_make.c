/*  "cfg_file_make.c"  */
/*  E.J.   10/4/04 - modify default cfg values and write output file  */

/*  for unix machine  */ 

#include <stdio.h>
#include <stdlib.h>
#include <strings.h>

void cfg_display(unsigned int *config, unsigned int *t_offset);
void cfg_modify(unsigned int *config, unsigned int *t_offset);
void cfg_write(unsigned int *config, unsigned int *t_offset);
void cfg_get(char * filename, unsigned int *config);


char cfg_filename[256];

int 
main(int argc, char *argv[])
{
  int key_value; 
  int tdc_mode;
  int ii,jj; 
  unsigned int t_offset; 
  unsigned int cfg_data[128];
  char cfg_file[256] = "";
  char filename[100];
  /* Default Configurations */
  unsigned int cfg_hs[16] = {0x0180, 0x8000, 0x407F, 0x407F, 0x407F, 0x407F, 0x003F, 0xA400, 
			     0x31D2, 0x66F6, 0x1FBC, 0x0000, 0x0000, 0x0000, 0x0000, 0x000C};
  unsigned int cfg_h[16]  = {0x0180, 0x8000, 0x407F, 0x407F, 0x407F, 0x407F, 0x003F, 0xA400, 
			     0x31D2, 0x66F6, 0x1FBC, 0x0000, 0x0000, 0x0000, 0x0000, 0x0008};
  unsigned int cfg_ns[16] = {0x0180, 0x0000, 0x4040, 0x4040, 0x4040, 0x4040, 0x003F, 0xC880, 
			     0x63A4, 0xCDEC, 0x1FBC, 0x0000, 0x0000, 0x0000, 0x0000, 0x000C};
  unsigned int cfg_n[16]  = {0x0180, 0x0000, 0x4040, 0x4040, 0x4040, 0x4040, 0x003F, 0xC880, 
			     0x63A4, 0xCDEC, 0x1FBC, 0x0000, 0x0000, 0x0000, 0x0000, 0x0008};

  if (argc > 2) {
    printf("Too Many arguments. Usage: config_make [filename]\n");
    exit(1);
  }
  
  if(argc == 2) { /* read default data from file */

    printf("\nReading default data from config file %s\n\n",argv[1]);
    sprintf(filename,"%s\0",argv[1]);
    cfg_get(filename,cfg_data);
  
  }else{
	
    printf("\nEnter TDC mode (1 = high resolution, synchronous)\n");
    printf("               (2 = high resolution, non-synchronous (start requiredd))\n");
    printf("               (3 = normal resolution, synchronous)\n");
    printf("               (4 = normal resolution, non-synchronous (start required))\n");
    scanf("%ld",&key_value);
    if( (key_value) < 1 || (key_value > 4) )
      exit(1) ;
    tdc_mode = key_value;     
    switch( key_value )					/* create data for 8 chips */
      {
      case 1:
	for(ii = 0; ii < 8; ii++)
	  {
	    for(jj = 0; jj < 16; jj++)
	      cfg_data[16*ii + jj] = cfg_hs[jj]; 
	  }
	break;    
      case 2:
	for(ii = 0; ii < 8; ii++)
	  {
	    for(jj = 0; jj < 16; jj++)
	      cfg_data[16*ii + jj] = cfg_h[jj]; 
	  }    
	break;    
      case 3:
	for(ii = 0; ii < 8; ii++)
	  {
	    for(jj = 0; jj < 16; jj++)
	      cfg_data[16*ii + jj] = cfg_ns[jj]; 
	  }    
	break;    
      case 4:
	for(ii = 0; ii < 8; ii++)
	  {
	    for(jj = 0; jj < 16; jj++)
	      cfg_data[16*ii + jj] = cfg_n[jj]; 
	  }    
      }

  }

  /* Output file */
  printf("Enter output config file name :\n");
  scanf("%s",cfg_file);
  sprintf(cfg_filename,"%s\0",cfg_file);
  printf("config data output to file: %s\n",cfg_filename);
    

  cfg_modify(cfg_data, &t_offset);	/* modify configuration data */
  cfg_display(cfg_data, &t_offset);	/* decode and display new configuration data */
  cfg_write(cfg_data, &t_offset);	/* write new configuration data to a file */
	
  exit(0);

}

void 
cfg_get(char *filename, unsigned int *config)
{
  FILE *fd;
  int ii, jj;
  unsigned int value, t_offset;
  
  fd = fopen(filename,"r");
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
  
  fclose(fd);
  return;
}


void 
cfg_display(unsigned int *config, unsigned int *t_offset)
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
	        printf("%X  ",*(config + cfg_offset + ii));
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
	    printf("Trigger Offset (rollover) = %ld\n", *t_offset);
	    t_bin_size = bin_size * 128.0;	
	    printf("Trigger Bin Size (ns)     = %.4f\n", t_bin_size);
	}	
		
}

void 
cfg_modify(unsigned int *config, unsigned int *t_offset)
{
	unsigned int refcnt;
	unsigned int exponent;
	unsigned int refclkdiv;
	unsigned int hsdiv;
	unsigned int trigwin;
	unsigned int triglat;
	unsigned int cfg_offset;
	int ii,ii_chip;
	int sync_mode;
	float factor;
	float tframe;
	float bin_size;
	float full_range;
	float window;
	float window_max;
	float latency;
	float t_bin_size;
	float bin_size_value;
	float window_value;
	float latency_value;
	
	    printf("\n----------------- Default data ----------------\n");
	    cfg_offset = 0;	
	    for(ii = 0; ii < 16; ii++)
	        printf("%X  ",*(config + cfg_offset + ii));
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
	    {
	        printf("Synchronous mode\n");
	        sync_mode = 1;
	    }
	    else
	    {
	        printf("Non-synchronous mode\n");
	        sync_mode = 0;
	    }
	
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
	    printf("------------------------------------------------\n");
	
	printf("\n\nEnter bin size (ns) ('0' to keep existing value) :\n");
	scanf("%f",&bin_size_value);
	if( bin_size_value > 0. )
	{
	    hsdiv = (25./152.) * factor * ((float)refclkdiv)/bin_size_value  +  0.5;    /* force round up */
	    bin_size = factor * (25./152.) * ( (float)refclkdiv )/( (float)hsdiv );	/* actual bin size */
	    full_range = 65536. * bin_size;
	    refcnt = full_range/25. - 3.0;
	    *(config + 7) = (0x8000) | ( (refcnt & 0x1FF) << 6 );
	    *(config + 10) = (0x1F00) | (hsdiv & 0xFF);
	    tframe = (float)(25 * (refcnt + 2 ));
	}	    
	printf("refclkdiv = %d   hsdiv = %d   bin_size (ns) = %.4f   full_range (ns) = %.1f   tframe (ns) = %.1f\n",
		    refclkdiv,hsdiv,bin_size,full_range,tframe);
		    

	printf("\n");
	if( sync_mode )
	    printf("Note: In SYNCHRONOUS mode: choose latency < %.1f  (0.90 * tframe)\n",0.9*tframe);
	else
	    printf("Note: In NON-SYNCHRONOUS mode: choose latency < (Trigger - Start - 75)  (max = %.1f  (Full range - 100))\n",
	    			full_range - 100.);	    	    
	printf("Enter trigger latency (ns) ('0' to keep existing value) :\n");
	scanf("%f",&latency_value);
	if( latency_value > 0. )
	{
	    latency = latency_value;
	    triglat = latency * factor/bin_size + 0.5;	/* force round up */
	    *(config + 9) = triglat & 0xFFFF;
	}    
	
	window_max = 0.40 * tframe;
	printf("\n");
	if( sync_mode )
	{
	    if( latency < window_max ) 
	        printf("Note: In SYNCHRONOUS mode, choose window < %.1f (latency)\n", latency);
	    else 
	        printf("Note: In SYNCHRONOUS mode, choose window < %.1f (0.40 * tframe)\n", window_max);
	}   	    
	else
	    printf("Note: In NON-SYNCHRONOUS mode: choose window < (Trigger - %.1f)  (max = %.1f  (Full range - 125)))\n",
	    			latency - 25.0, full_range - 125.);	    	    
	printf("Enter trigger window (ns) ('0' to keep existing value) :\n");
	scanf("%f",&window_value);
	if( window_value > 0. )
	{
	    window = window_value;
	    trigwin = window * factor/bin_size + 0.5;	/* force round up */
	    *(config + 8) = trigwin & 0xFFFF;
	}    
	printf("trigwin = %d   triglat = %d   window (ns) = %.1f   latency (ns) = %.1f\n",
		trigwin,triglat,window,latency);

/*	printf("\ncfg[7] = %X   cfg[8] = %X   cfg[9] = %X   cfg[10] = %X\n",
			*(config + 7),*(config + 8),*(config + 9),*(config + 10));	*/
			
	for(ii_chip = 1; ii_chip <= 7; ii_chip++)	/* copy reg 7,8,9,10 of chip 0 to chip 1-7 */
	{
	    cfg_offset = 16 * ii_chip;	
	    *(config + cfg_offset + 7) = *(config + 7);
	    *(config + cfg_offset + 8) = *(config + 8);
	    *(config + cfg_offset + 9) = *(config + 9);
	    *(config + cfg_offset + 10) = *(config + 10);
	}
	
	*t_offset = tframe/bin_size;		/* effective # bins in dynamic range for sync mode */
			
}

void 
cfg_write(unsigned int *config, unsigned int *t_offset)
{
  FILE *fd;
  int ii,  jj;
	
  fd = fopen(cfg_filename,"w");	
	
  for(ii = 0;ii < 8;ii++)			/* write cfg data to file */
    {
      for(jj = 0;jj < 16;jj++)
	fprintf(fd,"%04X ", *(config + (16*ii + jj)) );
      fprintf(fd,"\n");	
    }
  fprintf(fd,"%d\n",*t_offset);
  
  fclose(fd);
	
  return;
}	
	
	            
