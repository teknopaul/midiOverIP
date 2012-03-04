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

/*
	This program is a re-write of Dirk Jagdmann's multimidicast for Linux.
	It was going to be a copy, but ended up a rewrite from scratch.
	However Dirk deserves credit for the concept.
	Clearly this shoddy code is not his fault.

	N.B. this is packaged as a DLL but it may not work very well when embeded
	in other apps, there is a stab at start/stop methods but these are not really tested.
	Best to load it from ipMidi.exe and unload it with Ctrl+C.

	All Bug reports and fixes greatefully received.

	Style is an issue, Microsoft examples are in hungarian notation,
	winsock is mostly WinAPI, but it is based on berkley sockets which
	is familiar Linux style. I'm a Java dev at heart.
	What we have here is a mess, but some conventions exist.

	 - Kernnigan & Richie OTBS, no discussions, brackets obligatory for every this
	   except single line flags e.g if (flag) return 0.
	 - camelCase for exported methods.
	 - linux_style for static methods.
	 - cameCase for global vars.
	 - UPPERCASE_WITH_UNDERSCORES for constants, (MS use UPPERCASE like they don't know
	   where the caps lock key is. SOCKET is readable, but HMIDIIN <- WTF! not my choice)
	 - Stack variables linux_style
	 - Zero compiler warnings

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

#ifndef  MIDI_OVER_IP
#include "midiOverIP.h"
#endif

// Constants
const int MULTICAST_MAXSIZE = 1280; // maximum size of multicast packet
const int MAX_ERRORS = 200;

// Device/Thread IDs
// If the device id is set to -1
// this indicates that the feature is disabled, sockets and threads are not started
int midiInDeviceID  = 0;
int midiOutDeviceID = 0;
int netThreadID;
int midiThreadID;

// Handles (TODO read some where that only one of handle and id is needed for threads)
HMIDIIN  midiInHandle  = 0;
HMIDIOUT midiOutHandle = 0;
SOCKET socketInFD  = -1;
SOCKET socketOutFD = -1;
int netThreadHandle;
int midiThreadHandle;

// states
int verbose = FALSE;
int term    = FALSE;
int running = FALSE;

// forward declared static methods

// pingers, programmatically send midi signals
static int  send_note_on(int note);
static int  send_note_off(int note);

static void print_device_name(int in_or_out, int device_id);

//
static int  initialize();
static void clean_shutdown();
static int  init_threads();
static int  open_midi_out();


// main loop Thread functions, i.e. public void run() methods with ugly names
unsigned __stdcall midi_listen_loop(void *ArgList);
unsigned __stdcall net_listen_loop(void *ArgList);


// These first few methods should be made thread safe

int moipStart()
{
	running = TRUE;
	return initialize();
}

/**
 * If this is called it proably breaks two threaded mode, use for key input debugging only
 */
int moipStartMidi()
{
	return open_midi_out();
}

void moipStop()
{
	running = FALSE;
}

void moipKill()
{
	term = TRUE;
}

void moipSetInDeviceID(int device_id)
{
	//printf("Input device set to %d\n", device_id);
	midiInDeviceID = device_id;

	if ( running == TRUE ) {
		printf("Reconfigure involves restarting\n");
		moipStop();
		Sleep(1000);
		clean_shutdown();
		moipStart();
	}

}

void moipGetInDeviceID(int *device_id)
{
	*device_id = midiInDeviceID;
}

void moipSetOutDeviceID(int device_id)
{
	//printf("Output device set to %d\n", device_id);
	midiOutDeviceID = device_id;

	if ( running == TRUE ) {
		printf("Reconfigure involves restarting\n");
		moipStop();
		Sleep(1000);
		clean_shutdown();
		moipStart();
	}

}

void moipGetOutDeviceID(int *device_id)
{
	*device_id = midiOutDeviceID;
}

/**
 * get the names of the devices avaialable
 * supplied strings should be (len * MAXPNAMELEN) in length
 * Strings returned are indexed at n * MAXPNAMELEN, padded with multiple '0'
 * This is because I dont know a nicer way to return a bloody List<String> :)
 * @see print_devices() for code that parses the data returned.
 */
void moipGetDeviceIDs(char *inids, char *outids, int len)
{
	int in,out;

	memset(inids, 0, sizeof(char) * len * MAXPNAMELEN);
	memset(outids, 0, sizeof(char) * len * MAXPNAMELEN);

	if (verbose) printf("ipMidi number of input devices found:%d\n", midiInGetNumDevs());
	if (verbose) printf("ipMidi number of output devices found:%d\n", midiOutGetNumDevs());

	MIDIINCAPS lpMidiInCaps;
	int inDevCount =  midiInGetNumDevs();

	for ( in = 0 ; in < inDevCount && in < len ; in++ ) {

		MMRESULT ret = midiInGetDevCaps(in, &lpMidiInCaps, sizeof(MIDIINCAPS));

		if ( ret == MMSYSERR_NOERROR ) {

			if (verbose) printf("Input Device %d %s\n", in, lpMidiInCaps.szPname);
			strncpy(inids + (in * MAXPNAMELEN), lpMidiInCaps.szPname, MAXPNAMELEN);

		} else {

			printf("Error getting dev details %d\n", ret);

		}

	}

	MIDIOUTCAPS lpMidiOutCaps;
	int outDevCount = midiOutGetNumDevs();

	for ( out = 0 ; out < outDevCount && out < len ; out++ ) {

		MMRESULT ret = midiOutGetDevCaps(out, &lpMidiOutCaps, sizeof(MIDIOUTCAPS));

		if ( ret == MMSYSERR_NOERROR ) {

			if (verbose) printf("Output Device %d %s\n", out, lpMidiOutCaps.szPname);
			strncpy(outids + (out*MAXPNAMELEN), lpMidiOutCaps.szPname, MAXPNAMELEN);

		} else {

			printf("Error getting dev details %d\n", ret);

		}
	}

}

void moipSetVerbose(int verbose_in)
{
	verbose = verbose_in == 1 ? TRUE : FALSE;
}

/**
 * Send C note on/note off
 */
void moipMidiPing()
{
	moipMidiPingNote(0);
}

/**
 * Send a note on hang for 500ms and send a note off,
 * i.e. only usefull for debugging.
 */
void moipMidiPingNote(int note)
{
	if (verbose) fprintf(stderr, "Sending note %c\n", note + 'a');

	send_note_on(note);
	Sleep(500);
	send_note_off(note);

}

/**
 * Open the midi port for output
 */
static int open_midi_out()
{
	if (midiOutDeviceID == -1) return 0;

	int flag;

	if (verbose) print_device_name(0, midiOutDeviceID);

	flag = midiOutOpen(&midiOutHandle, midiOutDeviceID, 0, 0, CALLBACK_NULL);

	if ( flag != MMSYSERR_NOERROR ) {

	  printf("Error opening MIDI Output.\n");
	  return -1;

	}

	return 0;
}


/**
 * Close midi handles.
 * Nothing drastic seems to happen if this is not run.
 */
static int shutdown_midi()
{

	if (midiOutDeviceID != -1) {

		midiOutReset(midiOutHandle);
		midiOutClose(midiOutHandle);

	}

	if (midiInDeviceID != -1) {

		midiInReset(midiInHandle);
		midiInClose(midiInHandle);

	}

	return 0;
}


/**
 * For debug, sends midi note on messages
 */
static int send_note_on(int note)
{
	if (midiOutDeviceID == -1) return 0;

	union { unsigned long word; unsigned char data[4]; } midi_message;
	midi_message.data[0] = 0x90;
	midi_message.data[1] = 60 + note;
	midi_message.data[2] = 100;
	midi_message.data[3] = 0;

	int flag = midiOutShortMsg(midiOutHandle, midi_message.word);

	if (flag != MMSYSERR_NOERROR) {

		printf("Warning: MIDI Output is not open.\n");

	}

	return flag;
}

/**
 * For debug, sends midi note off messages,
 * if you don't send note off the note stays on for ever.
 */
static int send_note_off(int note)
{
	if (midiOutDeviceID == -1) return 0;

	union { unsigned long word; unsigned char data[4]; } midi_message;
	midi_message.data[0] = 0x80;
	midi_message.data[1] = 60 + note;
	midi_message.data[2] = 100;
	midi_message.data[3] = 0;

	int flag = midiOutShortMsg(midiOutHandle, midi_message.word);

	if ( flag != MMSYSERR_NOERROR ) {

		printf("Warning: MIDI Output is not open.\n");

	}

	return flag;
}

/**
 * @param in_or_out 1 = in, 0 = out
 */
static void print_device_name(int in_or_out, int device_id)
{
	if ( in_or_out == 1 ) {

		MIDIINCAPS lpMidiInCaps;
		MMRESULT ret = midiInGetDevCaps(device_id, &lpMidiInCaps, sizeof(MIDIINCAPS));

		if ( ret == MMSYSERR_NOERROR ) {

			printf("Input Device %d %s\n", device_id, lpMidiInCaps.szPname);

		}

	} else { // out

		MIDIOUTCAPS lpMidiOutCaps;
		MMRESULT ret = midiOutGetDevCaps(device_id, &lpMidiOutCaps, sizeof(MIDIOUTCAPS));

		if ( ret == MMSYSERR_NOERROR ) {

			printf("Output Device %d %s\n", device_id, lpMidiOutCaps.szPname);

		}

	}
}


// this is copy paste from MSDN.
// hardcoded source IP will not work
// lefthere since it tooke me ages to work out that M$ return different
// insignificat values from most methods and the error is
// availalbe via WSAGetLastError()
/*
static int loosing_patience()
{

	int ret = 0;
	WSADATA wsadata;
	if (WSAStartup(MAKEWORD(2, 0), &wsadata) == SOCKET_ERROR) {
		printf("Error creating socket.\n");
		return 1;
	}

	SOCKET s;
	SOCKADDR_IN localif;
	struct ip_mreq mreq;
	s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	localif.sin_family = AF_INET;
	localif.sin_port = htons(30309);
	localif.sin_addr.s_addr = htonl(INADDR_ANY);
	//First Bind to WildCard Address.
	ret = bind(s, (SOCKADDR *)&localif, sizeof(localif));
	if(ret != 0)
	{
		printf("Bind failed %d\n",WSAGetLastError());
		return 0;
	}
	//Add to the desired multicast group on the desired interface by calling SetSockOpt.
	mreq.imr_interface.s_addr = inet_addr("192.168.1.25");
	mreq.imr_multiaddr.s_addr = inet_addr("225.0.0.37");
	ret = setsockopt(s, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char *)&mreq, sizeof(mreq));
	if(ret != 0);
	{
		printf("Multicast IP_ADD_MEMBERSHIP failed %d\n", WSAGetLastError());
		return 0;
	}
	return 0;
}
*/

static int init_winsock()
{
	WSADATA wsadata;

	if (WSAStartup(MAKEWORD(2, 0), &wsadata) == SOCKET_ERROR) {

		printf("Error initialising winsock.\n");
		return -1;

	}

	return 0;
}

/**
 * Bind 0.0.0.0 to multicast 225.0.0.37:21928
 * Sets the global socketInFD
 * TODO options for multi-homed machines
 */
static int open_in_socket()
{
	if (midiOutDeviceID == -1) return 0;

    socketInFD = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);

    if ( socketInFD < 0 ) {

    	printf("Cannot create socket%d \n", WSAGetLastError());
    	return -2;

    } else {

		if (verbose) printf("socketInFD=%d \n", socketInFD);

	}

    struct sockaddr_in stSockAddr;
    memset(&stSockAddr, 0, sizeof(struct sockaddr_in));
    stSockAddr.sin_family = AF_INET;
    stSockAddr.sin_port = htons(21928);
    stSockAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    //stSockAddr.sin_addr.s_addr = inet_addr("127.0.0.1");

    if ( bind(socketInFD, (LPSOCKADDR)&stSockAddr, sizeof(struct sockaddr_in)) != 0 ) {

		printf("Connect failed %d\n", WSAGetLastError());
		closesocket(socketInFD);
		return -4;

    }

	struct ip_mreq mreq;
	memset(&mreq, 0, sizeof(struct ip_mreq));
	mreq.imr_multiaddr.s_addr = inet_addr("225.0.0.37");
	mreq.imr_interface.s_addr = inet_addr("0.0.0.0");

    if ( setsockopt(socketInFD, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char*)&mreq, sizeof(mreq)) != 0 ) {

		fprintf(stderr, "IP_ADD_MEMBERSHIP failed. %d\n", WSAGetLastError());
		return -5;

	}

	// try to free up the socket so other prcesses can bind to the same socket.
	int one = 1;
	if ( setsockopt(socketInFD, SOL_SOCKET, SO_REUSEADDR, (char *)&one, sizeof(one)) != 0 ) {

		fprintf(stderr, "Cannot share socket. %d\n", WSAGetLastError());

	}

	return 0;
}

/**
 * Open a UDP socket 225.0.0.37:21928 for writing to the multicast address.
 */
static int open_out_socket()
{

	if (midiInDeviceID == -1) return 0;

	// setup socketOutFD

	socketOutFD = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);

	if ( socketOutFD == SOCKET_ERROR ) {

		printf("Connect to out socket failed. %d\n", WSAGetLastError());
		return -1;

	} else {

		if (verbose) printf("socketOutFD=%d \n", socketOutFD);

	}

	// turn off loopback
	if ( setsockopt(socketOutFD, IPPROTO_IP, IP_MULTICAST_LOOP, "0", 1) != 0 ) {

	  printf("Disconnect loopback failed IP_MULTICAST_LOOP. %d\n", WSAGetLastError());
	  return -2;

	}

    return 0;
}

/**
 * Clean shutdown of socket,
 * nothing bad seems to happen if this does not run.
 */
static int shutdown_sockets()
{
	if ( midiOutDeviceID != -1 ) {

	    shutdown(socketInFD, SD_BOTH);
	    closesocket(socketInFD);

	}

	if ( midiInDeviceID != -1 ) {

		shutdown(socketOutFD, SD_BOTH);
		closesocket(socketOutFD);

	}

    return 0;
}

static void clean_shutdown()
{

	shutdown_sockets();
	shutdown_midi();

}

static int initialize()
{

	if ( init_winsock() < 0 ) {
		return -1;
	}

	if ( open_midi_out() < 0 ) {
		return -2;
	}

	if ( open_in_socket() < 0 ) {
		return -3;
	}


	if ( open_out_socket() < 0 ) {
		return -4;
	}

	init_threads();
	//net_listen_loop();

	return 0;

}


/**
 * Create two threads
 * One for reading from the network and writing to midi out.
 * One other for reading from midi IN and writing to the network.
 *
 * Midi IN is started here since it can not be done till there is a thread to do it with.
 */
static int init_threads()
{

	HANDLE thread_handles[2];
	int threadCount = 0;

	if ( midiOutDeviceID != -1 ) {

		netThreadHandle = _beginthreadex(NULL, 0, net_listen_loop, NULL, 0, &netThreadID );
		thread_handles[threadCount++] = (HANDLE*)netThreadHandle;

	}

	if ( midiInDeviceID != -1 ) {

		midiThreadHandle = _beginthreadex(NULL, 0, midi_listen_loop, NULL, 0, &midiThreadID );
		thread_handles[threadCount++] = (HANDLE*)midiThreadHandle;

		if (verbose) print_device_name(1, midiInDeviceID);
		// Tell mmwin about the midi thread
		int ret = midiInOpen(&midiInHandle, midiInDeviceID, midiThreadID, 0, CALLBACK_THREAD);

		if ( ret != 0 ) {

			printf("Error opening midi in %d", ret);

		} else {

			midiInStart(midiInHandle);

		}

	}

	WaitForMultipleObjects(threadCount, thread_handles, TRUE, INFINITE);

	if ( midiOutDeviceID != -1 ) {

		CloseHandle(&netThreadHandle);

	}
	if ( midiInDeviceID != -1 ) {

		CloseHandle(&midiThreadHandle);

	}

	return 0;
}



unsigned __stdcall net_listen_loop(void *ArgList)
{

	union { unsigned long word; unsigned char data[4]; } message;
	char buf_[MULTICAST_MAXSIZE];
	char *buf = buf_;
	int max_errors = MAX_ERRORS;

	struct timeval net_timout;
	net_timout.tv_sec  = 10; //  loop every 10 seconds but we dont wait for ever it seems
	net_timout.tv_usec = 0;

	fd_set read_fds;

	while (1) {

		if (running == FALSE) Sleep(500);

		if (term == TRUE) {
			clean_shutdown();
			return 0;
		}

		// Process but dont wait for ever using select
		FD_ZERO(&read_fds);
		FD_SET(socketInFD, &read_fds);

		int ret = select(0, &read_fds, NULL , NULL, &net_timout);

		if ( ret < 0 ) {

			printf("ooops in select %d\n",  WSAGetLastError());

		}
		else if ( ret == 0 ) {

			continue;  // nothing to do

		}
		else if ( ret > 0 ) { // read_fds it has data

			if ( FD_ISSET(socketInFD, &read_fds) ) {

				int ret = recv(socketInFD, buf, MULTICAST_MAXSIZE, 0 );

				if ( ret == 0 ) {

					printf("Socket closed: %d\n", ret);
					return 1;

				}
				else if ( ret < 0 ) {

					// error
					if (verbose) printf("ooops %d\n", WSAGetLastError());
					max_errors--;
					Sleep(1000);
					if ( max_errors == 0 ) {
						return 1;
					}

					continue;

				}
				else if( ret > 0 ) {

					// ret = packet size, lets assume it is midi
					if (verbose) printf("Midi received:%x:%x:%x\n", buf[0], buf[1], buf[2]);

					if ( ret == 3 || ret == 4 ) {

						message.data[0] = buf[0];
						message.data[1] = buf[1];
						message.data[2] = buf[2];

						midiOutShortMsg(midiOutHandle, message.word);

					}
					else {
						// what goes here? support for sysex messages
					}
				}
			}

		}  // or no network data

	}

}

unsigned __stdcall midi_listen_loop(void *ArgList)
{

    struct sockaddr_in addressout;
    memset(&addressout, 0, sizeof(struct sockaddr_in));
    addressout.sin_family = AF_INET;
    addressout.sin_addr.s_addr = inet_addr("225.0.0.37");
    addressout.sin_port = htons(21928);

	MSG thread_msg;

	while (1) {

		if (term == TRUE) return 0;

		if (running == FALSE) Sleep(500);

		memset(&thread_msg, 0, sizeof(MSG));

		int ret = GetMessage(&thread_msg, NULL, 0, INFINITE);

		if ( ret <= 0 ) {

			if (verbose) printf("got a midi error: %d\n", ret);
			continue;

		}
		else if ( ret == 0 ) {

			if (verbose) printf("got a quit message: %d\n", ret);
			continue; // what to do here?

		}
		else if ( ret >= 0 ) {

			//if(verbose)printf("got a midi message: %d\n", ret);

			long unsigned int lMidiMessage = 0;

			switch ( thread_msg.message ) { //.message is really the type of the message
				//handle = thread_msg.wParam;
				case MM_MIM_OPEN : {
					if (verbose) printf("MM_MIM_OPEN\n");
					break;
				}
				case MM_MIM_CLOSE : {
					if (verbose) printf("MM_MIM_CLOSE\n");
					break;
				}
				case MM_MIM_DATA : {
					if (verbose) printf("MM_MIM_DATA\n");
					lMidiMessage = thread_msg.lParam;
					break;
				}
				case MM_MIM_LONGDATA : {
					if (verbose) printf("MM_MIM_LONGDATA\n");
					//MIDIHDR buffer = thread_msg.lParam;
					break;
				}
				case MM_MIM_ERROR : {
					if (verbose) printf("MM_MIM_ERROR\n");
					lMidiMessage = thread_msg.lParam;
					break;
				}
				case MM_MIM_LONGERROR : {
					if (verbose) printf("MM_MIM_LONGERROR\n");
					//MIDIHDR buffer = thread_msg.lParam;
					break;
				}
				case MM_MIM_MOREDATA : {
					if (verbose) printf("MM_MIM_MOREDATA\n");
					lMidiMessage = thread_msg.lParam;
					break;
				}
				default: {
					// this should not happen per docs
					// but docs are shitty anyway and numbers
					// returned do not even match anything in the headers
					if (verbose) printf("unknown %d \n", thread_msg.message);
					break;
				}
			}

			if ( lMidiMessage != 0 ) {

				if (verbose) printf("Sending message\n");

				ret = sendto(socketOutFD, (const char *)&lMidiMessage, 4, 0, (LPSOCKADDR)&addressout, sizeof(struct sockaddr));

				if ( ret == SOCKET_ERROR ) {

					printf("Socket error: %d\n", WSAGetLastError());

				}

			}
		}
	}

	return 0;

}


