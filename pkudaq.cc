// pkudaq.cc --- 
// 
// Description: 
// Author: Hongyi Wu(吴鸿毅)
// Email: wuhongyi@qq.com 
// Created: Mon May  1 10:49:37 2017 (+0000)
// Last-Updated: Tue May  2 14:02:18 2017 (+0000)
//           By: Hongyi Wu(吴鸿毅)
//     Update #: 10
// URL: http://wuhongyi.cn 

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <math.h>
#include <time.h>
#include <signal.h>
#include <errno.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/file.h>
// need to compile with -lm option

#include "PixieNetDefs.h"
#include "PixieNetCommon.h"
#include "PixieNetConfig.h"

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

int main(int argc, char *argv[])
{
  int fd;
  void *map_addr;
  int size = 4096;
  volatile unsigned int *mapped;  

  unsigned int SyncT;
  int rval;

  // ******************* read ini file and fill struct with values ********************
  
  PixieNetFippiConfig fippiconfig;		// struct holding the input parameters
  DigitizerRun_t PKU_DGTZ_RunManager;


  const char *settings_file = "settings/pkupar.txt";
  rval = PKU_init_PixieNetFippiConfig_from_file(settings_file, &fippiconfig);   // second override with user settings, do allow missing
  if( rval != 0 )
    {
      printf( "Failed to parse FPGA settings from %s, rval=%d\n", settings_file, rval );
      return rval;
    }

  // assign to local variables, including any rounding/discretization
  SyncT = fippiconfig.SYNC_AT_START;




  // *************** PS/PL IO initialization *********************
  // open the device for PD register I/O
  fd = open("/dev/uio0", O_RDWR);
  if (fd < 0) {
    perror("Failed to open devfile");
    return -2;
  }

  //Lock the PL address space so multiple programs cant step on eachother.
  if( flock( fd, LOCK_EX | LOCK_NB ) )
  {
    printf( "Failed to get file lock on /dev/uio0\n" );
    return -3;
  }

  map_addr = mmap( NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

  if (map_addr == MAP_FAILED) {
    perror("Failed to mmap");
    return -4;
  }

  mapped = (unsigned int *) map_addr;


  // ********************** Run Start **********************

  if(SyncT) mapped[ARTC_CLR] = 1;              // write to reset time counter
  mapped[AOUTBLOCK] = 2;





  mapped[ADSP_CLR] = 1;             // write to reset DAQ buffers
  mapped[ACOUNTER_CLR] = 1;         // write to reset RS counters
  mapped[ACSRIN] = 1;               // set RunEnable bit to start run
  mapped[AOUTBLOCK] = OB_EVREG;     // read from event registers







  // ********************** Run Stop **********************

  // clear RunEnable bit to stop run
  mapped[ACSRIN] = 0;               
  // todo: there may be events left in the buffers. need to stop, then keep reading until nothing left
                     


  // mapped[AOUTBLOCK] = OB_RSREG;
  // read_print_runstats(0, 0, mapped);// print (small) set of RS to file, visible to web
  // mapped[AOUTBLOCK] = OB_IOREG;


  //....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......
  
  RunManagerInit(&PKU_DGTZ_RunManager);

  // Readout Loop
  PrintInterface();

  while(!PKU_DGTZ_RunManager.Quit) 
    {
      CheckKeyboard(&PKU_DGTZ_RunManager);

      if (!PKU_DGTZ_RunManager.AcqRun) 
  	{
  	  Sleep(10);
  	  continue;
  	}






    }




  flock( fd, LOCK_UN );
  munmap(map_addr, size);
  close(fd);

  return 0;
}

// 
// pkudaq.cc ends here
