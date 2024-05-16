/*
 * Hardware Acceleration of Digital Pulse Shape Analysis Using FPGAs © 2024 by César González, Mariano Ruiz, Antonio Carpeño, Alejandro Piñas, Daniel Cano-Ott, Julio Plaza, Trino Martinez and David Villamarin is licensed under Creative Commons Attribution 4.0 International.
 * To view a copy of this license, visit https://creativecommons.org/licenses/by/4.0/
 */

#include "SimpleDataProcess.h"

Simple_Data_Process::Simple_Data_Process() {
	config=false;
	output_file="";
	input_file="";
	p=NULL;
}

Simple_Data_Process::~Simple_Data_Process() {
	if(!p)
		delete p;
}

int Simple_Data_Process::configure(char* configfile)
{
	p=new parser(configfile,'=');

	input_file=p->GetValue("File","NONE");
	output_file=p->GetValue("Output","NONE");

	if(!input_file.compare("NONE") || !output_file.compare("NONE"))
		return PROC_BADCONFIG;

	maxnsignals=p->GetValue("Max signals per frame",10);


	for(int i=0;i<MAX_SP_CHANNELS;i++)
    {
		std::string channel="channel"+std::to_string(i)+" ";

		// General
		std::string usechannel=channel+"active";
		bool usech=p->GetValue(usechannel.data(),0)>0;
		CA[i].active=usech;
		CA[i].detection.active=usech;
		if(!usech)
			continue;

		std::string vers=channel+"version";
		CA[i].version=p->GetValue(vers.data(),0);

		std::string Slope=channel+"slope";
		const int slope = p->GetValue(Slope.data(),1)>0 ? 1 :-1; // Positiva por defecto
		CA[i].detection.slope = slope>0;
		CA[i].slope=CA[i].detection.slope;

		// Detection struct
		std::string detection=channel+"detection ";

		std::string shaping=detection+"shaping";
		std::string shaping_type=p->GetValue(shaping.data(),"NONE");
		CA[i].detection.use_shaping=SetFilter(detection,shaping_type,&CA[i].detection.shaping);

		std::string fwhm=channel+"cfd fwhm";
		CA[i].detection.cfd.fwhm=p->GetValue(fwhm.data(),300);

		std::string delay=channel+"cfd delay";
		CA[i].detection.cfd.delay=p->GetValue(delay.data(),30);

		std::string factor=channel+"cfd factor";
		CA[i].detection.cfd.factor=p->GetValue(factor.data(),0.3);

		std::string cfd_method=channel+"cfd method";
		CA[i].detection.cfd_method = (p->GetValue(cfd_method.data(),1)==1);

		std::string threshold=channel+"threshold";
		CA[i].detection.threshold=p->GetValue(threshold.data(),slope*5.0);

		std::string sig_from=channel+"signal from";
		const int s_from=p->GetValue(sig_from.data(),0);
		std::string sig_to=channel+"signal to";
		const int s_to=p->GetValue(sig_to.data(),0);

		if(s_to-s_from<=0)
		{
			fprintf(stderr,"\tWARNING: Wrong signal size for channel %i\n",i);
			CA[i].active=false;
		}
		CA[i].detection.signal_from=s_from;
		CA[i].detection.signal_to=s_to;

		////////////////////////////////////////////////

		for(int j=0;j<MAX_ENERGY_CALCULATIONS;j++)
		{
			std::string energy=channel+"energy"+std::to_string(j)+" ";

			std::string calc=energy+"method";
			std::string Ecalc=p->GetValue(calc.data(),"NONE");
			CA[i].energy[j].active=SetCalcMethod(Ecalc,&CA[i].energy[j]);

			std::string rfrom=energy+"range from";
			const int range_from=p->GetValue(rfrom.data(),0);
			std::string rto=energy+"range to";
			const int range_to=p->GetValue(rto.data(),0);

			if((range_to-range_from<=0) &&CA[i].energy[j].active)
			{
				fprintf(stderr,"\tWARNING: Wrong energy range %i, energy %i\n",i,j);
				CA[i].energy[j].active=false;
			}
			CA[i].energy[j].range_from=range_from;
			CA[i].energy[j].range_to=range_to;

			std::string Eshaping=energy+"shaping";
			std::string Eshaping_type=p->GetValue(Eshaping.data(),"NONE");
			CA[i].energy[j].use_shaping=SetFilter(energy,Eshaping_type,&CA[i].energy[j].shaping);
		}
    }

	delete p;
	config=true;
	return PROC_OK;
}

bool Simple_Data_Process::SetFilter(std::string channel, std::string filtype, SIMPLE_Signal_Shaping_Struct *shaping)
{
	if(!filtype.compare("RC"))
		shaping->shaping_algorithm=RC_SHAPING;
	else
    {
		shaping->shaping_algorithm=0;
		shaping->rc=1.0;
		shaping->rc_scale=1.0;
		return false;
    }

	std::string rc=channel+"rc";
	shaping->rc=p->GetValue(rc.data(),1.0);

	std::string rcscale=channel+"scale";
	shaping->rc_scale=p->GetValue(rcscale.data(),1.0);
	return true;
}

bool Simple_Data_Process::SetCalcMethod(std::string calctype,SIMPLE_Signal_Energy_Struct *ener)
{
	if(!calctype.compare("AMPLITUDE"))
		ener->calculation_method=MAX_AMPLITUDE;

	else if(!calctype.compare("AREA"))
		ener->calculation_method=AREA;
	else
    {
		ener->calculation_method=MAX_AMPLITUDE;
		ener->range_from=-1;
		ener->range_to=1;
		return false;
    }
	return true;
}
