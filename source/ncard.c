/*NCard API interface by chris, 2010.  www.toxicbreakfast.com

    Copyright (C) 2010 chris, minus_273 (messageboardchampion.com)

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>


*/

#include "ncard.h"

char errorstring[6]="Error";
char returnstring[2048];

void encode_url(char *buffer, char *url)
{
	int count=0, bcount=0;
	while(url[count]!=0)
	{
		if((url[count]<'0'||url[count]>'z')&&(strchr("&+=/:~_-.",url[count])==NULL))
		{
			buffer[bcount++]='%';
			sprintf(&buffer[bcount],"%02x",url[count]);
			bcount+=2;
		}
		else
		{
			buffer[bcount]=url[count];
			bcount++;
		}
		count++;
	}
	buffer[bcount]=0;
}

int Ncard_init()
{
	/*int err;
    while ((err = net_init()) == -EAGAIN)
    if(err < 0){
    	return err;
	}*/
	return net_init();
}

char* Ncard_checkUser( char *username)
{
	char url[1024], buf[1024];
	struct block output;
	sprintf(url,"http://www.messageboardchampion.com/ncard/API/?cmd=checkuser&username=%s",username);
	encode_url(buf,url);
	output=downloadfile(buf);
	if(output.data[0]=='E')
	{
		//strncpy(returnstring,(char*)output.data,2048);
		//returnstring[2047]=0;
		return "Error";
	}
	else
		return NULL;
}


char* Ncard_createUser( char *username, char *password)
{
	char url[1024], buf[1024];
	struct block output;
	sprintf(url,"http://www.messageboardchampion.com/ncard/API/?cmd=createuser&username=%s&password=%s",username,password);
	encode_url(buf,url);
	output=downloadfile(buf);
	if(output.data[0]=='E')
	{
		//strncpy(returnstring,(char*)output.data,2048);
		//returnstring[2047]=0;
		return "Error";
	}
	else
		return NULL;
}


char* Ncard_key(char *key, char *username, char *password)
{
	char url[1024], buf[1024];
	struct block output;
	sprintf(url,"http://www.messageboardchampion.com/ncard/API/?cmd=login&username=%s&password=%s",username,password);
	encode_url(buf,url);
	output=downloadfile(buf);
	strncpy(key,(char*)output.data,32);
	key[32]=0;
	if(memcmp(key,errorstring,5)==0||key[0]=='(')
	{
		strncpy(returnstring,(char*)output.data,2048);
		returnstring[2047]=0;
		return returnstring;
	}
	else
		return NULL;
}

char* Ncard_submit(char ids, char *key, char *title)
{
	char url[1024], buf[1024];
	struct block output;
	sprintf(url,"http://www.messageboardchampion.com/ncard/API/?cmd=%s&key=%s&game=%s",ids?"tdbupdate":"update",key,title);
	encode_url(buf,url);
	output=downloadfile(buf);
	if(output.data[0]!='O')
	{
		strncpy(returnstring,(char*)output.data,2048);
		returnstring[2047]=0;
		return returnstring;
	}
	else
		return NULL;
}

char* Ncard_submit_multiple(char ids, char *key, unsigned int num_games, ...)
{
	char url[2048], buf[2048], *nextstring;
	struct block output;
	va_list argptr;
	int i, count;
	sprintf(url,"http://www.messageboardchampion.com/ncard/API/?cmd=%s&key=%s&count=%d",ids?"tdbupdate":"update",key,num_games);
	va_start(argptr,num_games);
	count=1;
	for(i=0;i<num_games;i++)
	{
		nextstring=va_arg(argptr,char*);
		if (nextstring!=NULL&&strlen(nextstring))
		{
			sprintf(url,"%s&game%d=%s",url,count,nextstring);
			count++;
		}
	}
	va_end(argptr);
	encode_url(buf,url);
	output=downloadfile(buf);
	if(output.data[0]!='O')
	{
		strncpy(returnstring,(char*)output.data,2048);
		returnstring[2047]=0;
		return returnstring;
	}
	else
		return NULL;
}

char* Ncard_friendcode(char *key, char *code)
{
	char url[1024], buf[1024];
	struct block output;
	sprintf(url,"http://www.messageboardchampion.com/ncard/API/?key=%s&cmd=wiicode&code=%s",key,code);
	encode_url(buf,url);
	output=downloadfile(buf);
	if(output.data[0]!='O')
	{
		strncpy(returnstring,(char*)output.data,2048);
		returnstring[2047]=0;
		return returnstring;
	}
	else
		return NULL;
}
