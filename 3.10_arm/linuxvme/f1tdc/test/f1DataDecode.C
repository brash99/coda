//////////////////////////////////////////////////////////////////////
//
// f1DataDecode.C
//
//  Bryan Moffit - September 2016
//
//   Root macro for decoding f1TDC data.
//
//   Usage:
//     - from root prompt:
//       .x f1DataDecode.C(f1TDC_version, dataword)
//       .x f1DataDecode.C(f1TDC_version, filename)
//
//     - from Shell Prompt (suitable for redirection into file):
//       echo '.x f1DataDecode.C(f1TDC_version, dataword)' | root
//       echo '.x f1DataDecode.C(f1TDC_version, filename)' | root
//
//      Where 
//         f1TDC_version := 2 or 3
//         dataword      := 32bit data word
//         filename      := name of file containing list 
//                            of 32bit data words
//
//
int useSpacers=0;

int
f1DataDecode(int rev, TString filename)
{
  ifstream myfile;
  myfile.open(filename.Data());
  
  UInt_t thisword=0;

  useSpacers = 1;

  while(1)
    {
      myfile >> hex >> thisword;
      if(!myfile.good())
	break;
      f1DataDecode(rev, thisword);
    }
  
  useSpacers = 0;
  
  return 0;
}

void 
f1DataDecode(int rev, unsigned int data)
{
  static unsigned int type_last = 15;	/* initialize to type FILLER WORD */
	
  unsigned int data_type, slot_id_hd, slot_id_tr, blk_evts, blk_num, blk_words;
  unsigned int new_type, evt_num, time_1, time_2;
  int rev=0;
  int mode=0, factor=0;

  factor = 2 - mode;
	
  if( data & 0x80000000 )		/* data type defining word */
    {
      new_type = 1;
      data_type = (data & 0x78000000) >> 27;
    }
  else
    {
      new_type = 0;
      data_type = type_last;
    }
        
  switch( data_type )
    {
    case 0:		/* BLOCK HEADER */
      slot_id_hd = (data & 0x7C00000) >> 22;
      blk_num = (data & 0x3FF00) >> 8;
      blk_evts = (data & 0xFF);
      if(useSpacers)
	printf("--------------------------------------------------------------------------------\n");
      printf("      %08X - BLOCK HEADER  - slot = %u   blk_evts = %u   n_blk = %u\n",
	     data, slot_id_hd, blk_evts, blk_num);
      break;

    case 1:		/* BLOCK TRAILER */
      slot_id_tr = (data & 0x7C00000) >> 22;
      blk_words = (data & 0x3FFFFF);
      printf("      %08X - BLOCK TRAILER - slot = %u   n_words = %u\n",
	     data, slot_id_tr, blk_words);
      break;

    case 2:		/* EVENT HEADER */
      evt_num = (data & 0x3FFFFF);
      if(useSpacers)
	printf("\n");
      printf("      %08X - EVENT HEADER - evt_num = %u\n", data, evt_num);
      break;

    case 3:		/* TRIGGER TIME */
      if( new_type )
	{
	  time_1 = (data & 0xFFFFFF);
	  printf("      %08X - TRIGGER TIME 1 - time = %u\n", data, time_1);
	  type_last = 3;
	}    
      else
	{
	  if( type_last == 3 )
	    {
	      time_2 = (data & 0xFFFF);
	      printf("      %08X - TRIGGER TIME 2 - time = %u\n", data, time_2);
	    }    
	  else
	    printf("      %08X - TRIGGER TIME - (ERROR)\n", data);	                
	}    
      break;

    case 7:		/* EVENT DATA */
      printf("TDC = %08X   ED   ERR=%X  chip=%u  chan=%u  t = %u (%u ps)\n", 
	     data, 
	     ((data >> 24) & 0x7), // ERR
	     ((data >> 19) & 0x7), // chip
	     ((data >> 16) & 0x7), // chan
	     (data & 0xFFFF), // t
	     ( (data & 0xFFFF) * 56 * factor ));
      break;

    case 8:		/* CHIP HEADER */
      /* need 2 prints - maximum # of variables is 7 in VxWorks printf (?) */
      printf("TDC = %08X --CH-- (%X,%u)  chip=%u  chan=%u  trig = %u  t = %3u ", 
	     data, 
	     ((data >> 24) & 0x7), 
	     ((data >> 6) & 0x1), 
	     ((data >> 3) & 0x7), // chip
	     (data & 0x7),  //chan
	     ((data >> 16) & 0x3F),  // trig
	     ((data >> 7) & 0x1FF)); // t
      printf("(%u ps) (P=%u)\n", 
	     ( ( (data >> 7) & 0x1FF) * 56 * factor * 128 ),
	     ((data >> 6) & 0x1));
      break;

    case 13:		/* EVENT TRAILER */
      /* need 2 prints - maximum # of variables is 7 in VxWorks printf (?) */
      printf("TDC = %08X --ET-- (%08X,%u)  chip=%u  chan=%u  trig = %u  t = %3u ", 
	     data, 
	     ((data >> 24) & 0x7), 
	     ((data >> 6) & 0x1), 
	     ((data >> 3) & 0x7), 
	     (data & 0x7), 
	     ((data >> 16) & 0x3F), 
	     ((data >> 7) & 0x1FF));
      printf("(%u ps) (P=%u)\n", 
	     ( ( (data >> 7) & 0x1FF) * 56 * factor * 128 ),
	     ((data >> 6) & 0x1));
      break;

    case 14:		/* DATA NOT VALID (no data available) */
      printf("      %08X - DATA NOT VALID = %u\n", data, data_type);
      break;
    case 15:		/* FILLER WORD */
      printf("      %08X - FILLER WORD = %u\n", data, data_type);
      break;
       	        
    case 4:		/* UNDEFINED TYPE */
    case 5:		/* UNDEFINED TYPE */
    case 6:		/* UNDEFINED TYPE */
    case 9:		/* UNDEFINED TYPE */
    case 10:		/* UNDEFINED TYPE */
    case 11:		/* UNDEFINED TYPE */
    case 12:		/* UNDEFINED TYPE */
    default:
      printf("      %08X - UNDEFINED TYPE = %u\n", data, data_type);
      break;
    }
	        
}        
