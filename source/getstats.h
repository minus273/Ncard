
#ifdef __cplusplus
extern "C" {
#endif
	
/*GETSTATS: A function to return the last games played on the Wii.  Channels are skipped.
Adapted from my Playstats utility.  By chris, 2010.  www.toxicbreakfast.com*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <math.h>
#include <ogcsys.h>
#include <gccore.h>
#include <ogc/isfs.h>
#include <time.h>

#define STATS_SUCCESS 0
#define STATS_ERR_OPENMSGBOARD -1

#define STATS_LENGTH 10

struct record {
	char title[61];
	char title_id[7];
};

typedef struct record stats[STATS_LENGTH];

int getstats(stats returned_stats, time_t since, const char* filters, void(*progress_func)(int,int));
/*
returned_stats: array to hold names of last ten (or STATS_LENGTH) games played.
	stats[0] is the most recent game played.
	If there are less than 10 games in the entire messageboard history, null strings
	will be returned for the remaining spaces.
	titles are returned in UTF-8 encoding.

since: only get games played since this time.  If 0, look through all games.

filters: string containing the name of a file listing titles to ignore.  Some titles
	i.e. standard Wii "channels" are ignored already.  This file must contain a list
	of titles, exactly as they apper on the messageboard and each on a new line.  If
	filters is a NULL pointer, no filters are loaded.

progress_func: pointer to function to show progress of parsing the messageboard file.
	This function takes two arguments, first is the message_id being processed, the
	second is the total number of messages on the board.  Note that the messages are
	not always in sequential order so, for example, a function that shows a progress
	bar should probably use a variable to hold the highest message_id returned so far.

Return value: STATS_SUCCESS on success, negative value on error.
*/

int getfriendcode(char* friendcode);
/*
friendcode: pointer to stirng to hold the retrieved friend code of the Wii.  Must be
at least 20 characters long.

Return value: STATS_SUCCESS on success, negative value on error.
*/
	
#ifdef __cplusplus
}
#endif
