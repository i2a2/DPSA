/*
 * Hardware Acceleration of Digital Pulse Shape Analysis Using FPGAs © 2024 by César González, Mariano Ruiz, Antonio Carpeño, Alejandro Piñas, Daniel Cano-Ott, Julio Plaza, Trino Martinez and David Villamarin is licensed under Creative Commons Attribution 4.0 International.
 * To view a copy of this license, visit https://creativecommons.org/licenses/by/4.0/
 */

#ifndef SRC_PARSER_H_
#define SRC_PARSER_H_

#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <string.h>
#include <vector>
#include <fstream>
#include <unistd.h>

enum PARSER_ERROR {
	PARSER_OK,
    PARSER_NOFILE,
    PARSER_COMMAND_NOT_FOUND,
    PARSER_BAD_VALUE,
    PARSER_ARRAY_TOO_FEW,
    PARSER_ARRAY_TOO_MANY
};

class parser {
private:
	std::string filename;
	int filesize;
	std::vector <std::pair<std::string,std::string> > line;
	char sep[2];
	bool warn_default;
	int FindCommand(const char *command);
public:
	parser(const char *fname,const char separator=':', bool vb=false);
	virtual ~parser();
	int config;

	std::string GetValue(const char *command, const char *defvalue);
	int GetValue(const char *command, int defvalue);
	double GetValue(const char *command, double defvalue);
	float GetValue(const char* command,float defvalue);
	int GetArray(const char* command, int N, int *array, const char separator[]=",");
	int GetArray(const char* command, int N, double *array,const char separator[]=",");
	int GetArray(const char* command, int N, float *array, const char separator[]=",");
	int ReadFile();
	void Info();
};

int remove_spaces(char *str);

#endif /* SRC_PARSER_H_ */
