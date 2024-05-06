/*
 * Hardware Acceleration of Digital Pulse Shape Analysis Using FPGAs © 2024 by César González, Mariano Ruiz, Antonio Carpeño, Alejandro Piñas, Daniel Cano-Ott, Julio Plaza, Trino Martinez and David Villamarin is licensed under Creative Commons Attribution 4.0 International.
 * To view a copy of this license, visit https://creativecommons.org/licenses/by/4.0/
 */

#include "reader.h"


Reader::Reader() {
}

Reader::~Reader() {
}


reader_response Reader::ReadCardHeader(std::string filename,
		SP_Devices_DataBlock_Information &card_header)
{
	std::ifstream file(filename, std::ios::binary);

	if (!file.is_open()){
		std::cout<< "File not open"<<std::endl;
	}
	if (file.fail()){
		std::cout<<"File open fail"<<std::endl;
		return ERROR;
	}

	file.read((char*) &card_header,sizeof(card_header));
	return NO_ERROR;
}

reader_response Reader::ReadRecordHeader(std::string filename,
		uint32_t &nsamples, uint32_t &index,
		unsigned long int card_header_size,
		unsigned long int record_header_size,
		SP_Devices_Monster_Data_Header &record_header)
{
	std::ifstream file(filename, std::ios::binary);

	if (!file.is_open()){
		std::cout<< "File not open"<<std::endl;
	}
	if (file.fail()){
		std::cout<<"File open fail"<<std::endl;
		return ERROR;
	}

        // FIXME
	file.seekg(card_header_size + index*(record_header_size + 2*3000));

	file.read((char*) &record_header,sizeof(record_header));
	nsamples=record_header.nsamples;
	return NO_ERROR;
}

reader_response Reader::ReadWaveform(std::string filename,
			uint32_t &index,std::vector<int16_t> &signal,
			uint32_t &nsamples, unsigned long int card_header_size,
			unsigned long int record_header_size)
{
	std::ifstream file(filename, std::ios::binary);

	int16_t *waveform = new int16_t[nsamples];

	if (!file.is_open()){
		std::cout<< "File not open"<<std::endl;
		return ERROR;
	}
	if (file.fail()){
		std::cout<<"File open fail"<<std::endl;
		return ERROR;
	}

	file.seekg(card_header_size + record_header_size + index*(record_header_size + 2*nsamples));

	file.read(reinterpret_cast<char*>(waveform), nsamples*2);

	signal.resize(nsamples);
	signal.assign(waveform, waveform+nsamples);
	index++;

	delete [] waveform;
	waveform = NULL;

	if(file.eof())
	{
		std::cout<<"END FILE"<<std::endl;
		file.close();
		return END_FILE;
	}
	return NO_ERROR;
}
