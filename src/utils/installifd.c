/******************************************************************
 
        MUSCLE SmartCard Development ( http://www.linuxnet.com )
            Title  : installifd.c
            Package: pcsc lite
            Author : David Corcoran
            Date   : 5/16/00
            License: Copyright (C) 2000 David Corcoran
                     <corcoran@linuxnet.com>
            Purpose: This is a reader installer for pcsc-lite.
 
********************************************************************/ 

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

int main(int argc, char **argv) {

  struct stat statbuf;
  char lpcReader[50];
  char lpcLibrary[95];
  char *lpcPortID;
  int iPort;
  int iStat;
  FILE *fd;

  printf("Please enter a friendly name for your reader (50 char max)\n");
  printf("-----> ");

  gets( lpcReader );

  /* Possible memory corruption */
  if ( strlen( lpcReader ) + 1 > 50 ) {
    printf("Reader name too long: exiting\n");
    return 1;
  }

 retrylib:

  printf("Please enter the full path of the readers driver (75 char max)\n");
  printf("Example: /usr/local/pcsc/drivers/librdr_generic.so\n");
  printf("-----> ");

  gets( lpcLibrary );

  /* Possible memory corruption */
  if ( strlen( lpcLibrary ) + 1 > 95 ) {
    printf("Library name too long: exiting\n");
    return 1;
  } 

  iStat = stat(lpcLibrary, &statbuf);

  if ( iStat != 0 ) {
    /* Library does not exist */
    printf("Library path does not exist.\n\n");
    goto retrylib;
  }

  printf("Please select the I/O port from the list below:\n");
  printf("------------------------------------------------\n");
  printf("1) COM1 (Serial Port 1)\n");
  printf("2) COM2 (Serial Port 2)\n");
  printf("3) COM3 (Serial Port 3)\n");
  printf("4) COM4 (Serial Port 4)\n");
  printf("5) LPT1 (Parallel Port 1)\n");
  printf("6) USR1 (Sym Link Defined)\n");
  printf("------------------------------------------------\n");

 retryport:

  printf("\n");
  printf("Enter number (1-6): ");
  
  scanf("%d", &iPort);

  if ( iPort < 1 || iPort > 6 ) {
    printf("Invalid input please choose (1-5)\n");
    goto retryport;
  }


  switch( iPort ) {

    case 1:
      lpcPortID = strdup("0x0103F8");
    break;
    case 2:
      lpcPortID = strdup("0x0102F8");
    break;
    case 3:
      lpcPortID = strdup("0x0103E8");
    break;
    case 4:
      lpcPortID = strdup("0x0102E8");
    break;
    case 5:
      lpcPortID = strdup("0x020378");
    break;
    case 6:
      lpcPortID = strdup("0x000001");
    break;

  }


  printf("\n\n");
  printf("Now creating new /etc/reader.conf: \n");

  fd = fopen("/etc/reader.conf", "w" );

  if ( fd == 0 ) {
    printf("Cannot open file /etc/reader.conf (are you root ?)\n");
    free(lpcPortID);
    return 1;
  }


  fprintf(fd, "%s", "# Configuration file for pcsc-lite\n");
  fprintf(fd, "%s", "# David Corcoran <corcoran@linuxnet.com\n");

  fprintf(fd, "%s", "\n\n");

  fprintf(fd, "FRIENDLYNAME     \"%s\"\n", lpcReader);
  fprintf(fd, "DEVICENAME       GEN_SMART_RDR\n");
  fprintf(fd, "LIBPATH          %s\n", lpcLibrary);
  fprintf(fd, "CHANNELID        %s\n", lpcPortID);

  fprintf(fd, "%s", "\n\n");

  fprintf(fd, "%s", "# End of file\n");


  free(lpcPortID);
  return 0;
} 

