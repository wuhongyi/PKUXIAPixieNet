// pkudaq.cc --- 
// 
// Description: 
// Author: Hongyi Wu(吴鸿毅)
// Email: wuhongyi@qq.com 
// Created: Mon May  1 10:49:37 2017 (+0000)
// Last-Updated: Thu May  4 13:03:59 2017 (+0000)
//           By: Hongyi Wu(吴鸿毅)
//     Update #: 43
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
#include <assert.h>
#include <errno.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/file.h>
// need to compile with -lm option

#include "PixieNetDefs.h"
// #include "PixieNetCommon.h"
#include "PixieNetConfig.h"



//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

int main(int argc, char *argv[])
{
  int fd;
  void *map_addr;
  int size = 4096;
  volatile unsigned int *mapped;  

  unsigned int wone, wtwo;
  uint16_t waveform[MAX_TL];

  unsigned int evstats, R1, hit, timeL, timeH, psa0, psa1, cfd0, cfd1;
  unsigned int lsum, tsum, gsum;
  unsigned int lsumb, tsumb, gsumb;
  unsigned int chaddr;
  int rval;


  // ******************* read ini file and fill struct with values ********************
  
  PixieNetFippiConfig fippiconfig;		// struct holding the input parameters
  DigitizerRun_t PKU_DGTZ_RunManager;
  DigitizerFPGAUnit fpgaunitpar;

  const char *settings_file = "settings/pkupar.txt";
  rval = PKU_init_PixieNetFippiConfig_from_file(settings_file, &fippiconfig,&fpgaunitpar);   // second override with user settings, do allow missing
  if( rval != 0 )
    {
      printf( "Failed to parse FPGA settings from %s, rval=%d\n", settings_file, rval );
      return rval;
    }

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


  // Init FPGA
  InitFPGA(mapped,&fippiconfig,&fpgaunitpar);


  //....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......
  
  RunManagerInit(&PKU_DGTZ_RunManager);

  // Readout Loop
  PrintInterface();

  while(!PKU_DGTZ_RunManager.Quit) 
    {
      CheckKeyboard(&PKU_DGTZ_RunManager,mapped,&fippiconfig);

      if (!PKU_DGTZ_RunManager.AcqRun) 
  	{
  	  Sleep(10);
  	  continue;
  	}
      else
	{
	  // get baseline par 



	  // if data ready. read out
	  evstats = mapped[AEVSTATS];

	  if(evstats) 
	    {					  // if there are events in any channel
	      for(int ch=0; ch < NCHANNELS; ch++)
		{
		  R1 = 1 << ch;
		  if(evstats & R1)	
		    {	 // check if there is an event in the FIFO 

		      chaddr = ch*16+16;
		      hit   = mapped[chaddr+CA_HIT];
		      //    printf("channel %d, hit 0x%x\n",ch,hit);
		      if(hit & 0x20) 
			{ 
			  timeL = mapped[chaddr+CA_TSL];
			  timeH = mapped[chaddr+CA_TSH];
			  psa0  = mapped[chaddr+CA_PSAA];// Q0raw/4 | B
			  psa1  = mapped[chaddr+CA_PSAB];// M       | Q1raw/4
			  cfd0  = mapped[chaddr+CA_CFDA];
			  cfd1  = mapped[chaddr+CA_CFDB];
                 
			  //printf("channel %d, hit 0x%x, timeL %d\n",ch,hit,timeL);
			  // read raw energy sums 
			  lsum  = mapped[chaddr+CA_LSUM];// leading, larger, "S1", past rising edge
			  tsum  = mapped[chaddr+CA_TSUM];// trailing, smaller, "S0" before rising edge
			  gsum  = mapped[chaddr+CA_GSUM];// gap sum, "Sg", during rising edge; also advances FIFO and increments Nout etc

			  lsumb = mapped[chaddr+CA_LSUMB];
			  tsumb = mapped[chaddr+CA_TSUMB];
			  gsumb = mapped[chaddr+CA_GSUMB];


			  // get wave
			  mapped[AOUTBLOCK] = 3;

			  wone = mapped[AWF0+ch];  // dummy read?
			  // printf("%d ",fpgaunitpar.TL[ch]);
                          for(int k=0; k < (fpgaunitpar.TL[ch]/4); k++)
			    {
			      wone = mapped[AWF0+ch];
			      wtwo = mapped[AWF0+ch];
			      // re-order 2 sample words from 32bit FIFO

			      waveform[4*k+0] = (uint16_t)(wtwo >> 16);
			      waveform[4*k+1] = (uint16_t)(wtwo & 0xFFFF);
			      waveform[4*k+2] = (uint16_t)(wone >> 16);
			      waveform[4*k+3] = (uint16_t)(wone & 0xFFFF);

			      if(PKU_DGTZ_RunManager.PlotFlag && PKU_DGTZ_RunManager.PlotRecent && (ch == PKU_DGTZ_RunManager.DoPlotChannel))
				{
				  DoInTerminal("rm -f online.csv");
				  WriteOneOnlineWaveform(ch,fpgaunitpar.TL[ch],waveform);
				  PKU_DGTZ_RunManager.PlotRecent = false;
				}
			    }
                          mapped[AOUTBLOCK] = OB_EVREG;

			}
		      else 
			{ // event not acceptable (piled up )
			  R1 = mapped[chaddr+CA_REJECT];// read this register to advance event FIFOs without incrementing Nout etc
			}


		    }     // end event in this channel
		}        //end for ch
	    }           // end event in any channel

	}
    }


  //....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

  flock( fd, LOCK_UN );
  munmap(map_addr, size);
  close(fd);

  return 0;
}

// 
// pkudaq.cc ends here
