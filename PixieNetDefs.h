/*----------------------------------------------------------------------
 * Copyright (c) 2017 XIA LLC
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, 
 * with or without modification, are permitted provided 
 * that the following conditions are met:
 *
 *   * Redistributions of source code must retain the above 
 *     copyright notice, this list of conditions and the 
 *     following disclaimer.
 *   * Redistributions in binary form must reproduce the 
 *     above copyright notice, this list of conditions and the 
 *     following disclaimer in the documentation and/or other 
 *     materials provided with the distribution.
 *   * Neither the name of XIA LLC
 *     nor the names of its contributors may be used to endorse 
 *     or promote products derived from this software without 
 *     specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND 
 * CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, 
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF 
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. 
 * IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE 
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR 
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, 
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, 
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON 
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR 
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF 
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF 
 * SUCH DAMAGE.
 *----------------------------------------------------------------------*/

// system constants
#define PS_CODE_VERSION 0x0108 
#define PN_BOARD_VERSION_12_150_A 0xA990     
#define ADC_CLK_MHZ 250
#define SYSTEM_CLOCK_MHZ 125
#define FILTER_CLOCK_MHZ 125
#define NCHANNELS 4
#define V_OFFSET_MAX			1.25			// Pixie voltage offset maximum
#define MAX_MCA_BINS       32768
#define WEB_MCA_BINS       4096
#define MCA2D_BINS       100 // in each dimension
#define WEB_LOGEBIN        3
#define DACWAIT 20  // usleep cycles to wait for DAC programming
#define DACSETTLE 80000  // usleep cycles to wait for DAC stable output after filter
#define NTRACE_SAMPLES 8192
#define TWOTO32   4294967296
#define ICRSCALE 15        // factor between current iCR read and ICR in cps

// Limits for settings
#define MIN_CW 5             // Coinc Window limits
#define MAX_CW 511
#define MIN_FR 1             // FR limits
#define MAX_FR 6
#define MIN_SL 2             // energy filter limits
#define MIN_SG 3
#define MAX_SLSG 126
#define MIN_FL 2             // trigger filter limits
#define MIN_FG 3
#define MAX_FLFG 63
#define MAX_TH 1023
#define GAIN_HIGH 5          // gain limits
#define GAIN_LOW 2
#define MAX_TL 4092           // max length of captured waveform and pre-trigger delay
#define TWEAK_UD 28           // adjustment to pre-trigger delay for internal pipelining
#define MAX_BFACT 16
#define MAX_PSATH 2044
#define MAX_GW 255
#define MAX_GD 255
#define MAX_CD 255
#define MAX_QDCL  62          // length of QDC sum samples
#define MAX_QDCLD 254         // length plus delay of QDC sum, in samples
#define MAX_BLAVG 10
#define MAX_BADBL 20



// system reg addr defines
// block 0     Use these to specify parameters that control the data acquisition,Can be read back to verify I/O 
#define ACSRIN        0x000   // =0x0000 all off  Run Control Register bits 0 RunEnable (set to start DAQ run)  9 nLive (set to 1 to pause DAQ run)
#define ACOINCPATTERN 0x001   // bit0-15 COINCIDENCE_PATTERN; bit16 coincidence mode;bit17 no trace out mode;bit18 only 1 record per CW
#define AI2CREG       0x002   // Control the SDA and SCL lines 0 SDA;1 SCL;2 SDA ENA (SDA output enable)
#define AOUTBLOCK     0x003   // =OB_IOREG  read from IO block      Specifies address range for reads  If 0: 0x000-0x04F;If 1: 0x100-0x14F;If 2: 0x200-0x29F;If 3: 0x300-0x303
#define AHVDAC        0x004   // (int)floor((fippiconfig.HV_DAC/5.0)*65535);	map 0..5V range to 0..64K
#define ASERIALIO     0x005   // Offboard serial IO
#define AAUXCTRL      0x006   // bit0 pulser enabled脉冲发生器   bit1 LED red on/off(NYI)
#define AADCCTRL      0x007   // Controls certain aspects of ADC operation  0 swap channel 0/1 data streams;1 swap channel 2/3 data streams
#define ADSP_CLR      0x008   // Writing to this register issues a dspclr pulse(processing init)
#define ACOUNTER_CLR  0x009   // Writing to this register issues a pulse to clear runstats counters
#define ARTC_CLR      0x00A   // Writing to this register issues a pulse to clear RTC time counter
#define ABVAL         0x00B
#define CA_DAC        0x004

// block 1    Use these during the run to get info of the current status,Read only
#define ACSROUT       0x100   // Run Status info bits    0 RunEnable (set to start DAQ run);2 SDA readback;4 zdtfull;9 nLive;10 PSA enabled;11 VetoIn;13 ACTIVE (=RunEnable);15..31 debug
#define AEVSTATS      0x101   // EVSTATS (Event status information)     0 DataReadyA (if 1, there is data in channel’s ZDT buffer);1 DataReadyB;2 DataReadyC;3 DataReadyD
#define ABRDINFO      0x102
#define APPSTIME      0x103   // Current PPStime (local time latched with external trigger)
#define AEVHIT        0x104
#define AEVTSL        0x105   // Event time stamp M, L (mode 0x402, 503)
#define AEVTSH        0x106   // Event time stamp X, H (mode 0x402, 503)
#define AEVPPS        0x107   // Event PPS time (mode 0x402, 503)


// block 2   Used for run statistics and other output values. Sometimes two 16bit words per address Unit “ticks” means 2ns clock ticks,Read only
#define ARS0_MOD      0x200
#define AREALTIME     0x201

// channel reg addr defines
// block 1    Use these during the run to read event data,Read only
// channel independent lower bits of event registers
#define CA_HIT			0x100   // Hitpattern
#define CA_TSL			0x101   // Event or local time stamp M, L(local TS in mode 0x402, 0x503 is lower 24 bits * 256)
#define CA_TSH		   0x102   // Event time stamp X, H
#define CA_PSAA		0x103   // PSA value
#define CA_PSAB		0x104   // PSA value or gate pulse counter
#define CA_CFDA		0x105   // CFD values
#define CA_CFDB		0x106   // CFD values
#define CA_LSUM		0x107   // Lsum
#define CA_TSUM		0x108   // Tsum
#define CA_GSUM		0x109   // Gsum, read advances event buffers and increments NOUT
#define CA_REJECT		0x10A   // Read advances event buffers without incrementing NOUT
#define CA_LSUMB		0x10B   // Lsum for BL avg
#define CA_TSUMB		0x10C   // Tsum for BL avg
#define CA_GSUMB		0x10D   // Gsum for BL avg
// ADC registers
#define AADC0        0x11F
#define AADC1        0x12F
#define AADC2        0x13F
#define AADC3        0x14F
//block 2
#define ARS0_CH0     0x220
#define ARS0_CH1     0x240
#define ARS0_CH2     0x260
#define ARS0_CH3     0x280
// block 3
#define AWF0         0x300   // Waveform FIFO channel 0
#define AWF1         0x301   // Waveform FIFO channel 1
#define AWF2         0x302   // Waveform FIFO channel 2
#define AWF3         0x303   // Waveform FIFO channel 3

// outblocks
#define OB_IOREG     0x0			// I/O
#define OB_EVREG     0x1			// Event data
#define OB_RSREG     0x2			// run statistics
#define OB_WFREG     0x3			// channel waveforms



// program control constants
#define LINESZ                1024  // max number of characters in ini file line
#define I2CWAIT               4     // us between I2C clock toggles
#define SDA                   1     // bit definitions for I2C I/O
#define SCL                   2     // bit definitions for I2C I/O
#define SDAENA                4     // bit definitions for I2C I/O
#define N_PL_IN_PAR           16    // number of input parameters for system and each channel
#define N_PL_RS_PAR           32    // number of runstats parameters for system and each channel
#define N_USED_RS_PAR         20    // not all RS parapmeters are used, can save some readout and printout cycles
#define MAX_PAR_NAME_LENGTH   65    // Maximum length of parameter names
#define BLREADPERIOD          20
#define MIN_POLL_TIME         100
#define BLOCKSIZE_400         32    // waveform block size (# 16bit words) in run type 0x400
#define FILE_HEAD_LENGTH_400  32    // file header size (# 16bit words) in run type 0x400
#define CHAN_HEAD_LENGTH_400  32    // event/channel header size (# 16bit words) in run type 0x400
#define WATERMARK     0x12345678    // for LM QC routine
#define EORMARK       0x01000002    // End Of Run

// channel hit pattern & info in LM data
#define HIT_ACCEPT            5     //  result of local coincdence test & pileup & veto & rangebad
#define HIT_COINCTEST         16    //  result of local coincdence test
#define HIT_PILEUP            18    //  result of local pileup test
#define HIT_LOCALHIT          20    //  set if this channel has a hit
#define HIT_OOR               22    //  set if this channel had the out of range flag set


