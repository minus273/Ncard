/*GETSTATS: A function to return the last five games played on the Wii.  Adapted from my Playstats
utility.  By chris at toxicbreakfast.com*/

#include "getstats.h"

const char fraghead[8]={46,32,32,32,32,32,32,32},
cdbhead[8]={67,68,66,70,73,76,69,2},
formatcode[4]={0,10,37,198},//formatting code in some messages
formatcode2[4]={0,10,37,160};
unsigned int file_pos=0, fp;
char fragbuffer[180];
char readbuffer[1024] ATTRIBUTE_ALIGN(32);

struct {
	unsigned int unknown;
	char title[42];
	char ext_title[42];
	unsigned long long int start_time;
	unsigned long long int end_time;
	char title_id[6];
	char unknown2[18];
	unsigned long long int others;
} logrecord;

struct shortrecord
{
	char title[42];
	char title_id[6];
	unsigned long long int end_time;
} recent[STATS_LENGTH];

void read(void *ptr,int len,int size,int file)
{
	ISFS_Read(file,readbuffer,len*size);
	memcpy(ptr,readbuffer,len*size);
	file_pos+=len*size;
}

void seek(int file, int pos, int mode)
{
	ISFS_Seek(file,pos,mode);
	if(mode==SEEK_SET)
		file_pos=pos;
	else if(mode==SEEK_CUR)
		file_pos+=pos;
}

int wlen(short* str)
{
	short *ptr;
	ptr=str;
	while(*ptr)
	{
		ptr++;
		if((ptr-str)>1024)
			return -1;
	}
	return ptr-str;
}

void rtrim(char *string)
{
	char *pos=string+strlen(string);
	while(*--pos==' '||*pos=='\n'||*pos=='\r'||*pos=='\t');
	*(pos+1)='\0';
}

void utf16_to_utf8(char* utf8, short* utf16)
{
	int i;
	char *nptr, *split;
	short *wptr;
	nptr=utf8;
	wptr=utf16;
	split=(char*)utf16;
	for(i=0;i<wlen(utf16);i++)
	{
		if(*wptr<0x80)
		{
			split++;
			*nptr++=*split++;
		}
		else if(*wptr<0x800)
		{
			*nptr++=(0xC0|*split<<2)|*(split+1)>>6;
			split++;
			*nptr++=(*split++&0x3F)|0x80;
		}
		else
		{
			*nptr++=0xE0|*split>>4;
			*nptr++=((*split<<2&0x3F)|0x80)|*(split+1)>>6;
			split++;
			*nptr++=(*split++&0x3F)|0x80;
		}
		wptr++;
	}
	*nptr=0;
}

void unfragment(int fptr) //deals with some but not all of the fragmented stats.
{
	int recordstart,pos,i,j,loop;
	recordstart=file_pos-sizeof(logrecord);
	pos=recordstart;
	for(i=0;i<17;i++)
	{
		if(memcmp(&fragbuffer[i*8],(char*)fraghead,8)==0)
		{
			ISFS_Seek(fptr,pos+0x200,SEEK_SET);
			ISFS_Read(fptr,readbuffer,136-(i*8));
			memcpy(&fragbuffer[i*8],readbuffer,136-(i*8));
			pos+=0x200;
			i-=1;
		}
		else if(memcmp(&fragbuffer[i*8],(char*)cdbhead,8)==0)
		{
			ISFS_Seek(fptr,pos+0x548,SEEK_SET);
			pos+=0x548;
			loop=1;
			while(loop)
			{
				ISFS_Read(fptr,readbuffer,8);
				memcpy(&fragbuffer[i*8],readbuffer,8);
				if(fragbuffer[i*8]==0&&fragbuffer[i*8+2]==0&&fragbuffer[i*8+4]==0&&fragbuffer[i*8+6]==0) //is it a string fragment?
				{
					pos+=8;
				}
				else
				{
					loop=0;
					for(j=0;j<=4;j+=2) //check for formatcodes
					{
						if((memcmp(&fragbuffer[i*8+j],(char*)formatcode,4)==0)||(memcmp(&fragbuffer[i*8+j],(char*)formatcode2,4)==0))
						{
							pos+=8;
							loop=1;
							break;
						}
					}
				}
			}
			pos-=8;
		}
		else
			pos+=8;
	}
	file_pos=pos;
}

int getfriendcode(char* friendcode)
{
	char buf[17];
	unsigned int codefile;
	long long int code;
	ISFS_Initialize();
	if((codefile=ISFS_Open("/shared2/wc24/nwc24msg.cfg",ISFS_OPEN_READ)))
	{
		ISFS_Seek(codefile,8,SEEK_SET);
		read(&code,8,1,codefile);
		sprintf(buf,"%016lld",code);
		sprintf(friendcode,"%c%c%c%c-%c%c%c%c-%c%c%c%c-%c%c%c%c",buf[0],buf[1],buf[2],buf[3],buf[4],buf[5],buf[6],buf[7],buf[8],buf[9],buf[10],buf[11],buf[12],buf[13],buf[14],buf[15]);
		return STATS_SUCCESS;
	}
	else
		return -1;
}

int getstats(stats returned_stats, time_t since, const char* filters, void(*progress_func)(int,int))
{
	char buf[80];
	int i,j,k;
	unsigned int total_messages, message_id, play_entries, message_end, logrecord_header, where;
	time_t tm;
	unsigned long long int now, then=11505369600000000LLU, tm_since;//then=01/01/2006
	FILE *filtfile;
	char* filtstrings[1024];
	int filtcount=0, filtmatched;
	tm=time(NULL);
	now=(((long long int)tm-946684800)*60750000)+3645000000LLU;
	if(since)
		tm_since=(((long long int)since-946684800)*60750000);
	else tm_since=0LL;
	for(i=0;i<STATS_LENGTH-1;i++);
	{
		recent[i].title[0]='\0';
		recent[i].title_id[0]='\0';
		recent[i].end_time=0;
	}
	recent[STATS_LENGTH-1].title[0]='\0';
	recent[STATS_LENGTH-1].title_id[0]='\0';
	recent[STATS_LENGTH-1].end_time=0;
	memset(filtstrings,0,1024*sizeof(char*));
	if((filtfile=fopen(filters,"r")))
	{
		while(!feof(filtfile))
		{
			fgets(buf,22,filtfile);
			if(strlen(buf)) rtrim(buf);
			if(strlen(buf)&&buf[0]!='#')
			{
				filtstrings[filtcount]=malloc(strlen(buf)+1);
				strcpy(filtstrings[filtcount],buf);
				filtcount++;
			}
		}
		fclose(filtfile);
	}
	ISFS_Initialize();
	if((fp=ISFS_Open("/title/00000001/00000002/data/cdb.vff",ISFS_OPEN_READ)))
	{
		seek(fp,0x29020,SEEK_SET);
		read(&total_messages,4,1,fp);
		seek(fp,0x1004,SEEK_CUR);
		while(1)
		{
			seek(fp,0xC,SEEK_CUR);
			read(buf,1,12,fp);
			buf[11]=0;
			seek(fp,0x50,SEEK_CUR);
			read(&message_id,4,1,fp);
			progress_func(message_id,total_messages);
			if(strcmp(buf,"playtimelog")==0)
			{
				read(&play_entries,4,1,fp);
				if(play_entries>10)
					play_entries=10;
				seek(fp,0x4B4,SEEK_CUR);
				read(&message_end,4,1,fp);
				message_end-=0x130;
				seek(fp,message_end,SEEK_CUR);
				read(&logrecord_header,4,1,fp);
				if(logrecord_header==1)
					seek(fp,0x04,SEEK_CUR);
				else
					seek(fp,0x0C,SEEK_CUR);
				for(i=0;i<play_entries;i++)
				{
					read(&fragbuffer,sizeof(logrecord),1,fp);
					unfragment(fp);
					memcpy(&logrecord,&fragbuffer,sizeof(logrecord));
					if(logrecord.end_time>now||logrecord.end_time<then)
						break; //skip to next record.
					if(logrecord.end_time!=0&&logrecord.end_time<tm_since)//sanity check
						continue;
					if(logrecord.title_id[0]=='H'||(logrecord.title_id[0]=='J'&&logrecord.title_id[1]=='O'&&logrecord.title_id[2]=='D'&&logrecord.title_id[3]=='I'))//skip channels.
						continue;
					if(logrecord.title_id[0]=='S'&&logrecord.title_id[1]=='N'&&logrecord.title_id[2]=='T'&&logrecord.title_id[3]=='E')//skip netflix.
						continue;
					if(filtstrings[0]!=NULL)
					{
						filtmatched=0;
						for(j=0;j<filtcount&&filtmatched==0;j++)
						{
							utf16_to_utf8(buf,(short*)logrecord.title);
							if(strcmp(buf,filtstrings[j])==0)
								filtmatched=1;
						}
						if(filtmatched)
							continue;
					}
					if(logrecord.end_time>recent[0].end_time)
					{
						for(j=0;j<STATS_LENGTH;j++)
						{
							if(memcmp(logrecord.title_id,recent[j].title_id,6)==0)
							{
								if(j==0)
									recent[0].end_time=logrecord.end_time;
								else
								{
									for(k=j;k>0;k--)
									{
										memcpy(recent[k].title_id,recent[k-1].title_id,6);
										memcpy(recent[k].title,recent[k-1].title,42);
										recent[k].end_time=recent[k-1].end_time;
									}
									memcpy(recent[0].title_id,logrecord.title_id,6);
									memcpy(recent[0].title,logrecord.title,42);
									recent[0].end_time=logrecord.end_time;
								}
								break;
							}
							if(j==STATS_LENGTH-1)
							{
								for(k=STATS_LENGTH-1;k>0;k--)
								{
									memcpy(recent[k].title_id,recent[k-1].title_id,6);
									memcpy(recent[k].title,recent[k-1].title,42);
									recent[k].end_time=recent[k-1].end_time;
								}
								memcpy(recent[0].title,logrecord.title,42);
								memcpy(recent[0].title_id,logrecord.title_id,6);
								recent[0].end_time=logrecord.end_time;
							}
						}
					}
				}
			}
			if(message_id==total_messages)
				break;
			where=(((int)((file_pos-0x21)/0x100))+1)*0x100+0x20;
			while(1)
			{
				seek(fp,where,SEEK_SET);
				read(&buf,8,1,fp);
				buf[7]=0;
				if(strcmp(buf,"CDBFILE")==0)
					break;
				else
					where+=0x100;
			}
		}
		for(i=0;i<STATS_LENGTH;i++)
		{
			utf16_to_utf8(returned_stats[i].title,(short *)recent[i].title);
			memcpy(returned_stats[i].title_id,recent[i].title_id,6);
			returned_stats[i].title_id[6]='\0';
		}
	}
	else
	{
		ISFS_Deinitialize();
		return STATS_ERR_OPENMSGBOARD;
	}
	ISFS_Close(fp);
	ISFS_Deinitialize();
	for(i=0;i<filtcount;i++)
	{
		if(filtstrings[i]!=NULL)
			free(filtstrings[i]);
		else break;
	}
	return STATS_SUCCESS;
}
