

#define MIDI_OVER_IP 1

/**
 * Starts the main loop
 */
int moipStart();
/**
 * Just starts midi, only Ping methos will work if you call this
 */
int moipStartMidi();
/**
 * Stop the main loop
 */
void moipStop();
/**
 * Stop everything close connections and die,
 * lib can not be used after calling this.
 */
void moipKill();
/**
 *
 */
void moipSetInDeviceID(int device_id);
void moipSetOutDeviceID(int device_id);

void moipGetInDeviceID(int *device_id);
void moipGetOutDeviceID(int *device_id);
void moipGetDeviceIDs(char *inids, char *outids, int len);

void moipSetVerbose(int verbose);
void moipMidiPing();
void moipMidiPingNote(int note);

