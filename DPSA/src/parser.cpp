/*
 * Hardware Acceleration of Digital Pulse Shape Analysis Using FPGAs © 2024 by César González, Mariano Ruiz, Antonio Carpeño, Alejandro Piñas, Daniel Cano-Ott, Julio Plaza, Trino Martinez and David Villamarin is licensed under Creative Commons Attribution 4.0 International.
 * To view a copy of this license, visit https://creativecommons.org/licenses/by/4.0/
 */

#include "parser.h"

parser::parser(const char *fname,const char separator,bool vb)
{
  filename=(char*) fname;
  sep[0]=separator;
  sep[1]=0;
  config=ReadFile();
  warn_default=vb;

}
parser::~parser()
{
  return;
}

int parser::FindCommand(const char *command)
{
  const int Nl=line.size();

  int i=0;

  while(i<Nl)
    {
      if(line[i].first.compare(command))
	i++;
      else
	break;
    }

  if(i<Nl)
    return i;
  else
    return(-PARSER_COMMAND_NOT_FOUND);


}

int parser::GetArray(const char* command, int N, int *array, const char separator[])
{
  const int i=FindCommand(command);
  if(i<0)
    {
      fprintf(stderr,"PARSER WARNING: Command \"%s\" not found. Array will be filled with o's\n",command);
      for(int j=0;j<N;j++)
	array[j]=0;
      return PARSER_COMMAND_NOT_FOUND;
    }

  char val[256];
  strcpy(val,line[i].second.data());

  char *tok=strtok(val,separator);
  if(tok==NULL)
    {
      fprintf(stderr,"PARSER WARNING: Error in value for command \"%s\". Array will be filled with 0's\n",command);
      for(int j=0;j<N;j++)
	array[j]=0;
      return PARSER_BAD_VALUE;
    }

  int j=0;
  while(j<N && tok!=NULL)
    {
      array[j]=atoi(tok);
      j++;
      tok=strtok(NULL,separator);
    }

  if(j<N)
    {
      fprintf(stderr,"PARSER WARNING: Value elements in command '%s' were too few to fill the array. Filling missing values with 0's\n",command);
      for(;j<N;j++)
	array[j]=0;
      return PARSER_ARRAY_TOO_FEW;
    }

  if(tok!=NULL)
    {
      fprintf(stderr,"PARSER WARNING: Value elements in command '%s' were more then allocated (%i) for the array\n",command,N);
      return PARSER_ARRAY_TOO_MANY;
    }

  return PARSER_OK;
}



int parser::GetArray(const char* command, int N, double *array,const char separator[])
{



  const int i=FindCommand(command);
  if(i<0)
    {
      fprintf(stderr,"PARSER WARNING: Command \"%s\" not found. Array will be filled with o's\n",command);
      for(int j=0;j<N;j++)
	array[j]=0;
    }
  char val[256];
  strcpy(val,line[i].second.data());


  char *tok=strtok(val,separator);
  if(tok==NULL)
    {
      fprintf(stderr,"PARSER WARNING: Error in value for command \"%s\". Array will be filled with 0's\n",command);
      for(int j=0;j<N;j++)
	array[j]=0;
      return PARSER_BAD_VALUE;
    }

  int j=0;
  while(j<N && tok!=NULL)
    {
      array[j]=strtod(tok,NULL);
      j++;
      tok=strtok(NULL,separator);
    }

  if(j<N)
    {
      fprintf(stderr,"PARSER WARNING: Value elements in command '%s' were too few to fill the array. Filling missing values with 0's\n",command);
      for(;j<N;j++)
	array[j]=0;
      return PARSER_ARRAY_TOO_FEW;
    }

  if(tok!=NULL)
    {
      fprintf(stderr,"PARSER WARNING: Value elements in command '%s' were more then allocated (%i) for the array\n",command,N);
      return PARSER_ARRAY_TOO_MANY;
    }

  return PARSER_OK;

}



int parser::GetArray(const char* command, int N, float *array, const char separator[])
{
  const int i=FindCommand(command);
  if(i<0)
    {
      fprintf(stderr,"PARSER WARNING: Command \"%s\" not found. Array will be filled with o's\n",command);
      for(int j=0;j<N;j++)
	array[j]=0;
    }
  char val[256];
  strcpy(val,line[i].second.data());


  char *tok=strtok(val,separator);
  if(tok==NULL)
    {
      fprintf(stderr,"PARSER WARNING: Error in value for command \"%s\". Array will be filled with 0's\n",command);
      for(int j=0;j<N;j++)
	array[j]=0;
      return PARSER_BAD_VALUE;
    }

  int j=0;
  while(j<N && tok!=NULL)
    {
      array[j]=strtof(tok,NULL);
      j++;
      tok=strtok(NULL,separator);
    }

  if(j<N)
    {
      fprintf(stderr,"PARSER WARNING: Value elements in command '%s' were too few to fill the array. Filling missing values with 0's\n",command);
      for(;j<N;j++)
	array[j]=0;
      return PARSER_ARRAY_TOO_FEW;
    }

  if(tok!=NULL)
    {
      fprintf(stderr,"PARSER WARNING: Value elements in command '%s' were more then allocated (%i) for the array\n",command,N);
      return PARSER_ARRAY_TOO_MANY;
    }

  return PARSER_OK;

}
std::string parser::GetValue(const char *command, const char *defvalue)
{
  const int i=FindCommand(command);

  if(i<0)
    {
      if(warn_default)
	fprintf(stderr,"PARSER WARNING: Command \"%s\" not found. Using default value \"%s\"\n",command, defvalue);
      return defvalue;
    }

  return line[i].second;
}

int parser::GetValue(const char *command, int defvalue)
{
  const int i=FindCommand(command);

  if(i<0)
    {
      if(warn_default)
	fprintf(stderr,"PARSER WARNING: Command \"%s\" not found. Using default value %i\n",command, defvalue);
      return defvalue;
    }

  return(atoi(line[i].second.data()));
}

double parser::GetValue(const char *command, double defvalue)
{
  const int i=FindCommand(command);
  if(i<0)
    {
      if(warn_default)
	fprintf(stderr,"PARSER WARNING: Command \"%s\" not found. Using default value %g\n",command, defvalue);
	    return defvalue;
    }

  return (strtod(line[i].second.data(),NULL));
}
float parser::GetValue(const char* command,float defvalue)
{
  const int i=FindCommand(command);
  if(i<0)
    {
      if(warn_default)
	fprintf(stderr,"PARSER WARNING: Command \"%s\" not found. Using default value %f\n",command, defvalue);
      return defvalue;
    }

  return (strtof(line[i].second.data(),NULL));

}

int parser::ReadFile()
{

  if(access(filename.data(),F_OK)!=0)
    {
      fprintf(stderr,"PARSER ERROR: could not find file %s\n",filename.data());
      return PARSER_NOFILE;
    }


  std::ifstream f;

  f.open(filename.data());
  if(f.is_open() && f.good())
    {
      int Nline=0;
      char newline[256];
      while(f.good() && !f.eof())
	{

	  f.getline(newline,256);
	  Nline++;

	  if(strlen(newline)<2)
	    continue;

	  if(!remove_spaces(newline))
	    continue;

	  if(newline[0]=='#' || newline[0]==0)
	    continue;

	  char *command=strtok(newline,sep);

	  if(!command)
	    continue;

	  remove_spaces(command);

	  const int Nl=line.size();
	  bool flag=false;
	  if(Nl>0)
	    {
	      for(int i=0;i<Nl;i++)
		{
		  if(line[i].first.compare(command)==0)
		    {
		      fprintf(stderr,"Warning: repeated command \"%s\" in line %i\n\tIgnoring line\n",command,Nline);
		      flag=true;
		    }
		}
	    }
	  if(flag)
	    continue;




	  char *value=strtok(NULL,sep);
	  if(!value)
	    continue;



	  remove_spaces(value);

	  line.push_back({command, value});

	}
    }
  else
    {
      fprintf(stderr,"PARSER ERROR: could not open file %s\n",filename.data());
      return PARSER_NOFILE;
    }

  return PARSER_OK;
}

void parser::Info()
{
  const int NLines=line.size();
  fprintf(stdout,"\n- Configuration File: %s\n--------------\n",filename.data());
  for(int i=0;i<NLines;i++)
    fprintf(stdout,"%s : %s\n",line[i].first.data(), line[i].second.data());

  return;


}

int remove_spaces(char *str)
{
  int len=strlen(str);

  int i=len-1;
  while(str[i]==' ' && i>0)
    i--;
  str[i+1]=0;

  if(i<=0) return 0;

  len=strlen(str);

  i=0;
  while(str[i]==' ' && i<len)
    i++;

  if (i==len)
    return 0;

  for(int j=0;j<len-i;j++)
    str[j]=str[j+i];
  str[len-i]=0;

  return 1;

}
