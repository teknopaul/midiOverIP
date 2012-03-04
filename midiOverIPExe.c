/*
    Copyright (C) 2012  Paul Hinds

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.

*/

#include <conio.h>
#include <string.h>
#include <stdio.h>
#include <winsock2.h>
#include <Ws2tcpip.h>
#include <mswsock.h>

#include <stdlib.h>
#include <signal.h>

#include <windows.h>   /* required before including mmsystem.h */
#include <mmsystem.h>  /* multimedia functions (such as MIDI) for Windows */

#include <unistd.h>

#ifndef  IP_MIDI
#include "midiOverIP.h"
#endif

int verbose = FALSE;

static void help();
static void print_devices();
static int key_loop();

/**
 *  main entry point
 *  open midi ports connect to the IP ports and start the main loop.
 *  TODO configurable ports, if, and switch for midi debug mode.
 *  @return 0 is ok, positive number is an error
 */
int main(int argc, char **argv)
{

	int returnCode = 0;

  	printf("midiOverIP by teknopaul\n\n");

	// parse command line
	int do_key_loop = 0;
	int c;

	while ( (c = getopt(argc, argv, "hdvkio")) != EOF ) {

		//printf("c=%c optarg %s, optind %d\n", c, optarg, optind);
		switch ( c ) {
			default:
			case 'h': {
				help();
			}
			case 'd' : {
				print_devices();
			}
			case 'v': {
				moipSetVerbose(1);
				verbose = TRUE;
				break;
			}
			case 'k': {
				do_key_loop = 1;
				break;
			}
			case 'i' : {
				// bug? in getopt  "-p 1" does not have a valid optarg
				if (optind < argc) optarg = argv[optind];
				if (strcmp(optarg, "off") == 0) optarg = "-1";
				moipSetInDeviceID(atoi(optarg));
				break;
			}
			case 'o' : {
				// bug? in getopt  "-p 1" does not have a valid optarg
				if (optind < argc) optarg = argv[optind];
				if (strcmp(optarg, "off") == 0) optarg = "-1";
				moipSetOutDeviceID(atoi(optarg));
				break;
			}
		}

	}

	if ( do_key_loop ) return key_loop() * -1;

	signal(SIGTERM, moipKill);
	signal(SIGBREAK, moipMidiPing);

	if ( (returnCode = moipStart()) < 0 ) {

		return returnCode = returnCode * -1;

	}

	return returnCode;
}

/**
 * @return no return, print and exit(1)
 */
static void help()
{
  fprintf(stderr, "options: -h - display this text\n");
  fprintf(stderr, "         -i [n] - midi in port, set the MIDI port, default 0\n");
  fprintf(stderr, "         -o [n] - midi out port, set the MIDI port, default 0\n");
  fprintf(stderr, "         -v - verbose, show MIDI and network events\n");
  fprintf(stderr, "         -d - dump, print device IDs\n");
  fprintf(stderr, "         -k - keyboard, don't connect to network accept keyboard input for testing midi\n");

  exit(1);

}

/**
 * @return no return, print and exit(0)
 */
static void print_devices()
{
	char inids [10 * MAXPNAMELEN];
	char outids[10 * MAXPNAMELEN];

	moipGetDeviceIDs(inids, outids, 10);

	printf("Input Devices\n");

	int c, line;
	for ( line = 0 ; line < 10 ; line++ ) {

		char * start = inids + (line * MAXPNAMELEN);

		if ( *start != 0 ) {

			printf("%d : ", line);

		}
		else continue;

		for ( c = 0 ; c < MAXPNAMELEN; c++ ) {

			if (*start == 0) {

				printf("\n");
				break;

			} else {

				printf("%c", *start++);

			}
		}
	}

	printf("Output Devices\n");

	for ( line = 0 ; line < 10 ; line++ ) {

		char * start = outids + (line * MAXPNAMELEN);

		if ( *start != 0 ) {

			printf("%d : ", line);

		}
		else continue;

		for ( c = 0 ; c < MAXPNAMELEN; c++ ) {

			if ( *start == 0 ) {

				printf("\n");
				break;

			} else {

				printf("%c", *start++);

			}
		}
	}
	exit(0);
}

/**
 * @return 0 is ok, negative number is an error
 */
static int key_loop()
{
	if ( moipStartMidi() < 0 ) return -1;

	int ckey;

	printf("a-g play a note any other key to exit\n");

	while (1) {

		ckey = getch();

		if ( ckey >= 'a' && ckey <= 'g' ) {

			if (verbose) printf("note:%d\n", ckey);
			moipMidiPingNote(ckey - 'a');

		} else {

			return 0;

		}
	}
}


