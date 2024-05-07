/*
 * Hardware Acceleration of Digital Pulse Shape Analysis Using FPGAs © 2024 by César González, Mariano Ruiz, Antonio Carpeño, Alejandro Piñas, Daniel Cano-Ott, Julio Plaza, Trino Martinez and David Villamarin is licensed under Creative Commons Attribution 4.0 International.
 * To view a copy of this license, visit https://creativecommons.org/licenses/by/4.0/
 */

#ifndef SP_CARD_COMMON_DEFINES_H
#define SP_CARD_COMMON_DEFINES_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_SP_CARDS 8
#define MAX_SP_CHANNELS 4
#define SP_BYTES_PER_SAMPLE 2

#define SIMPLE_SUITE
#ifdef SIMPLE_SUITE

#define MAX_ENERGY_CALCULATIONS 5 // Number of different energy (areas and amplitude) calculations allowed
#define MAX_PEAK_DETECTION_PER_SIGNAL 5

// Data structures
struct SP_Devices_Monster_Data_Header
{
  // The size of this structure is 32 bytes (and the 64-bit variables are well aligned)
  unsigned int nrecord;
  unsigned int nsamples;
  long long timestamp;
  long long recordstart;
  unsigned int position;
  unsigned short moving_average;
  char status;
  char channel;
};

struct SP_Devices_DataBlock_Information // Analysis version of the SIMPLE DAQ SP_Devices_Block_Information
{
  // The structure as it is defined now has a size of 512 bytes (and the 64-bit variables are well aligned)
  // [ID]
  char card_name[32];		// 32-bytes null terminated Card Model
  char serial_number[16];	// 16-bytes null terminated serial number
  char firmware[16];		// 16-bytes null terminated Firmware FWDAQ (standard) or FWPD (advanced for triggerless mode)
  unsigned int crate;		// Crate Number
  unsigned int slot;		// Slot Position

  // [General]
  long long i64Frequency;	// Frequency in Hz
  int clk_source;		// 0: Internal clock source, internal 10 MHz reference. 1: Internal clock source, external 10 MHz reference. 2 : External clock source
  unsigned int clk_impedance;	// 50 Ohm / 1 MOhm

  // [Acquisition]
  long long    iStartSegment;	// Starting segment
  long long    iStartSample;	// Starting sample of the segment (the only case it is not 0 is when the user asks very large segments)
  unsigned int iNumberOfIteration;// Iteration number either in multi segment or continuous
  unsigned int temperature[7];	// The temperature in Celsius times 256. There are 7 positions where it can be measured.
  unsigned int BlockSize;	// If RAW data, size of the Tag + Block Header + NFrames + NSamples + Headers + Data. If Processed data, size of the Tag + Block Header + NFrames + NSignals + Analysis Header + Processed Data
  unsigned int DataSize;	// Size of the RAW Data or 0 if Processed Data
  long long    iIniTimestamp;	// First timestamp of the block
  long long    iEndTimestamp;	// Last timestamp of the block

  // [Channel]
  float	FullVerticalScale[MAX_SP_CHANNELS];		// Actual vertical scale set in the channel
  unsigned int	iFullVerticalScale[MAX_SP_CHANNELS];	// Full vertical range in mV of channel (i)
  unsigned int	iUserVerticalScale[MAX_SP_CHANNELS];	// Advanced Full vertical range in mV of channel (i)
  int		iOffset[MAX_SP_CHANNELS];		// DC Offset in mV of channel (i)
  int		iDBS[MAX_SP_CHANNELS];			// Digital Offset (DBS) in mV of channel (i)

  // [Hardware Trigger]
  int iHw_TriggerSource;	// Tigger source. [1-4] for channels 1-4, -1 if external trigger, -2 if SYNC connector
  int iHw_TriggerCoupling;	// External trigger coupling
  int iHw_TriggerSlope;		// 1 if rising slope / 0 if falling slope
  int iHw_TriggerLevel;		// Trigger level of the hardware trigger in mV (the SYNC connector is a logic signal LVECL or simething like that -> check)

  // [Trigger Blocking]
  int iBlockingSource;		// Channel/ExtTrigger/Sync producing the trigger block
  int iBlockingMode;		// Type of blocking (Once, Window, Gate)
  unsigned int iBlockingWindow;	// For Window mode, the size of the window

  // [FWDAQ]
  unsigned int iFWDAQ_NSegments;   // Number of segments stored in the block
  unsigned int iFWDAQ_SegmentSize; // Number of samples per segment
  int          iFWDAQ_Delay;	 // Number of presamples (if positive, is the number of holdoff samples)

  // [FWPD]
  unsigned int iFWPD_NSegments[MAX_SP_CHANNELS];	// Number of segments stored in the block
  unsigned int iFWPD_LEW[MAX_SP_CHANNELS];		// Pre-Trigger samples
  unsigned int iFWPD_TEW[MAX_SP_CHANNELS];		// Post-Trigger Reset samples
  int iFWPD_TriggerLevel[MAX_SP_CHANNELS];		// Trigger level in mV
  unsigned int iFWPD_TriggerSlope[MAX_SP_CHANNELS];	// 1 if rising slope / 0 if falling slope

  int iFWPD_ResetLevel[MAX_SP_CHANNELS];	// Reset level mV
  int iFWPD_TriggerArmLevel[MAX_SP_CHANNELS];   // Trigger Arm level in mV
  int iFWPD_ResetArmLevel[MAX_SP_CHANNELS];     // Reset Arm level mV
  int iFWPD_SMA[MAX_SP_CHANNELS];               // Moving Average period

  int iCoincidenceSource[MAX_SP_CHANNELS];	// Bitmask channels
  int iCoincidenceWindow[MAX_SP_CHANNELS];	// Window Length after coincidence trigger where triggers are accepted
  int iCoincidenceDelay[MAX_SP_CHANNELS];	// Not implemented yet

  // Booleans (I have converted them on char to ensure a good alignment)
  char b_card;		// Boolean to mark the card as defined or not

  char clk_output;	// Clock output is enabled/disabled
  char trigger_blocking;// Trigger Blocking feature enabled/disabled
  char timestamp_sync;	// Multi-card synchronization of the timestamp performed

  char b_channel[MAX_SP_CHANNELS];	        // Boolean to indicate if channels are active
  char b_UserVerticalScale[MAX_SP_CHANNELS];	// Boolean to indicate user vertical range is active
  char b_DBS[MAX_SP_CHANNELS];			// Boolean to indicate if DBS is active

  char bFWPD_VariableLength[MAX_SP_CHANNELS];	// Boolean to indicate if record is variable length
  char bFWPD_ResetLevel[MAX_SP_CHANNELS];	// Boolean to indicate if Reset level is active
  char bFWPD_TriggerArmLevel[MAX_SP_CHANNELS];  // Boolean to indicate if Trigger Arm level is active
  char bFWPD_ResetArmLevel[MAX_SP_CHANNELS];    // Boolean to indicate if Reset Arm level is active
  char bFWPD_SMA[MAX_SP_CHANNELS];		// Boolean to indicate if Moving Average period is active
  char b_CoincidenceTrigger[MAX_SP_CHANNELS];	// Enable/Disable the trigger coincidence
};

// Analysis Parameters **************************************
struct SIMPLE_Signal_Shaping_Struct
{
  int shaping_algorithm; // Selects the integration algorithm. By default is 0 (RC). 1 for RC^4
  float rc;
  float rc_scale;
};
struct SIMPLE_Signal_CFD_Struct
{
  unsigned int fwhm;
  unsigned int delay;
  float factor;
};
struct SIMPLE_Signal_Detection_Struct
{
  SIMPLE_Signal_Shaping_Struct shaping;
  SIMPLE_Signal_CFD_Struct cfd;
  double threshold;
  int signal_from;
  int signal_to;

  bool active;
  bool use_shaping;
  bool slope;       // true - positive. false - negative
  bool cfd_method;  // true - CFD method 4 point spline. false - two points interpolation.
};
struct SIMPLE_Signal_Energy_Struct
{
  SIMPLE_Signal_Shaping_Struct shaping;
  int range_from;
  int range_to;
  int calculation_method; // 0 - Amplitude: maximum or minimum (largest absolute), Area: range integral
  bool active;
  bool use_shaping;
  bool use_timing;
};


struct SIMPLE_Channel_Analysis_Struct
{
  SIMPLE_Signal_Detection_Struct detection;
  SIMPLE_Signal_Energy_Struct energy[MAX_ENERGY_CALCULATIONS];
  int version;
  bool active;
  bool slope;
};

// Analysis Results
struct SIMPLE_Signal_Results
{
  // Info
  short pileup;
  short saturated;
  unsigned int nsignal;
  float baseline;
  float stdbaseline;

  // Detection
  double time; // Time of the signal given by CFD, Leading edge,... from 0 to npoints!

  // Energy
  double energy[MAX_ENERGY_CALCULATIONS];

};
struct SIMPLE_Record_Results
{
  // Info
  unsigned int nrecord;
  unsigned int nsignals;
  float baseline;
  float stdbaseline;
  long long timestamp;

  unsigned int position; // Position of the first signal detected in the Signal_Results vector
  short int recordstart;
  unsigned short int channel; // Crate/Slot/Channel
};
#endif

#endif // SP_CARD_COMMON_DEFINES_H
