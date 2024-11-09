/*
 *
 * File:
 *     common_struct.h
 *
 * Description:
 *     Common structures used for testing jvme library
 *
 */


struct vmecpumem 
{
  /* 0x0000 */ volatile unsigned int data[256];
  /* 0x0400 */ volatile unsigned int id;
  /* 0x0404 */ volatile unsigned int control;
  /* 0x0408 */ volatile unsigned int status;
};
