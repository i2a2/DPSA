/*
 * Hardware Acceleration of Digital Pulse Shape Analysis Using FPGAs © 2024 by César González, Mariano Ruiz, Antonio Carpeño, Alejandro Piñas, Daniel Cano-Ott, Julio Plaza, Trino Martinez and David Villamarin is licensed under Creative Commons Attribution 4.0 International.
 * To view a copy of this license, visit https://creativecommons.org/licenses/by/4.0/
 */

#ifndef READER_H_
#define READER_H_

#include <iostream>
#include <fstream>
#include <string>
#include <vector>

#include "Simple_sp_devices_defines.h"

enum reader_response
{
    NO_ERROR,
    ERROR,
    END_FILE
};

class Reader
{
public:
    Reader(std::string &filename);
    virtual ~Reader();

    reader_response ReadCardHeader(SP_Devices_DataBlock_Information &card_header);

    reader_response ReadRecordHeader(uint32_t &nsamples, uint32_t &index,
				    unsigned long int &card_header_size,
				    unsigned long int &record_header_size,
				    SP_Devices_Monster_Data_Header &record_header);

    reader_response ReadWaveform(uint32_t &index,std::vector<int16_t> &signal,
    									uint32_t &nsamples,	unsigned long int &card_header_size,
										unsigned long int &record_header_size);
private:
    std::ifstream file;
};

#endif /* READER_H_ */
