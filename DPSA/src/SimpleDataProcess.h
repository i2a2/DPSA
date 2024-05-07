/*
 * Hardware Acceleration of Digital Pulse Shape Analysis Using FPGAs © 2024 by César González, Mariano Ruiz, Antonio Carpeño, Alejandro Piñas, Daniel Cano-Ott, Julio Plaza, Trino Martinez and David Villamarin is licensed under Creative Commons Attribution 4.0 International.
 * To view a copy of this license, visit https://creativecommons.org/licenses/by/4.0/
 */

#ifndef SRC_SIMPLEDATAPROCESS_H_
#define SRC_SIMPLEDATAPROCESS_H_

#include <stdio.h>
#include <string>
#include "parser.h"
#include "Simple_sp_devices_defines.h"

// Filters
#define RC_SHAPING  0
#define RC4_SHAPING 1
#define MOV_AVERAGE 2
#define TRAPEZOID   3


// Calculations
#define MAX_AMPLITUDE 0
#define AREA          1
#define RMS           2
#define PEAK2PEAK     3

enum SDP_ERR
  {
    PROC_OK,
    PROC_BADCONFIG,
    PROC_OA_BADCONFIG,
    PROC_FILENOTOPEN,
    PROC_OUTFILENOTOPEN

  };

class Simple_Data_Process {
private:
	parser *p;
	bool SetFilter(std::string channel, std::string filttype,SIMPLE_Signal_Shaping_Struct *shaping);
	bool SetCalcMethod(std::string calctype,SIMPLE_Signal_Energy_Struct *ener);
	bool config;
	int maxnsignals;
public:
	Simple_Data_Process();
	virtual ~Simple_Data_Process();
	int configure(char* configfile);
	SIMPLE_Channel_Analysis_Struct CA[4];
	std::string output_file;
	std::string input_file;
};

#endif /* SRC_SIMPLEDATAPROCESS_H_ */
