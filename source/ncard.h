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

#ifdef __cplusplus
extern "C" {
#endif
	
#include "http.h"

int Ncard_init();
/*Call this first to set up network connection.

Returns: 0 on success or the error code returned by net_init() on failure*/

	void encode_url(char *buffer, char *url);

char* Ncard_checkUser( char *username);
	/*
	  checks the username to see if it exits
	 */
	
	
char* Ncard_createUser( char *username, char *password);
	/*
	 creates a new username 
	 */
	
	
char* Ncard_key(char *key, char *username, char *password);
/*Gets key from username and password.  The key should ideally be stored so that
the server does not need to be queried in future.

key: pointer to an array to hold the returned key.  This should be 33 characters long.
username: pointer to string containing username.
password: pointer to string containing password.

Returns: NULL on success, error string on failiure*/


char* Ncard_submit(char ids, char *key, char *title);
/*Updates Ncard with a game

ids: if non-zero we are sending title_ids instead of titles.
key: pointer to string containing key.
title: pointer to string containg a game title or title_id to submit.

Returns: NULL on success, error string on failiure*/

char* Ncard_submit_multiple(char ids, char *key, unsigned int num_games, ...);
/*Updates Ncard with multiple games at once.

ids: if non-zero we are sending title_ids instead of titles.
key: pointer to string containing key.
num_games: number of games to submit.

This is then followed by a list of pointers to strings containing the game titles to submit.
These should be submitted in the order the games were played. The function will skip any
arguments that are null pointers or empty strings;

Returns: NULL on success, error string on failiure*/

char* Ncard_friendcode(char *key, char *code);
/*Updates Ncard with your Wii's friend code.

key: pointer to string containing key.
code: your Wii's friend code, in format nnnn-nnnn-nnnn-nnnn.

Returns: NULL on success, error string on failiure*/
#ifdef __cplusplus
}
#endif