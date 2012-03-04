## midiOverIP

  by teknopaul

midiOverIP is a GPLv3 Windows clone of multimidicast by Dirk Jagdmann
for Linux and ipMIDI for Windows from http://nerds.de

It is a Windows app, it does not run on Linux, use multimidicast on 
Linux.

All three of these apps take midi packets and ping them as UDP 
packets to a fixed multicast address and porti.
They should all be compatible.

The app from nerds.de is limited to work for 60 mins unless you
pay, which is why I wrote midiOverIP.

nerds.de one is probably better if you want to spend money and
don't have a thing about free software.

The combination of multimidicast and midiOverIP enables me to have
LMMS as the master and control VST midi instruments running on a 
Windows box without needing a soundcard or any more cables.

## Setup

Setup is simple, the apps are command line based, if you dont 
have a complex midi setup just run

on Linux
`# ./multimidicast -q &`

on Windows
`C:\> midiOverIP.exe`

And you should have midi connectivity between them via the LAN.

However life is never that simple.

## Prerequisites

Windows and Linux boxes and a fast LAN, latency is not an issue on
a gigabit LAN, it might be on a busy wireless LAN,

Requirements for Linux are ALSA, I use Jack to wire up the ALSA midi.
No other sound APIs are supported by multimidicast, so get ALSA.

Requirements for Windows is a physical or virtual midi device, 
your software synth may already be a midi device, or you may need 
a virtual (or physical) midi controller.

## Virtual midi controllers

http://nerds.de do a virtual midi controller called LoopBe1 that 
I can recommend, it is free as in beer for non-commercial use, 
but not free as in speech. They also have a commercial virtual
midi device.

http://www.tobias-erichsen.de/loopMIDI.html does a midi dirver that 
is free as in beer also for commercial use, but not free as in speech.

It should not take long to write a virtual midi driver for Windows.
I don't need one right now so I have not, if you are interested get 
in touch.

## midiOverIP usage

To get help

`C:\> midiOverIP -h`

To get debug output

`C:\> midiOverIP -v`

If you have more than one midi device you have some options.

Show avaiable devices with the -d switch, here you can see
I have some physical and virtual devices installed.

`C:\> midiOverIP -d
midiOverIP by teknopaul

Input Devices
0 : LoopBe Internal MIDI
1 : bluebox
2 : Analog Experience - The Player
Output Devices
0 : Microsoft GS Wavetable Synth
1 : LoopBe Internal MIDI
2 : bluebox
3 : Analog Experience - The Player`

Device 0 is the default device for both input and output

To select a specific device for input or output use the -i and -o 
options respectivly.

`C:\> midiOverIP -i 1 -o 3`

The above command starts the appliation taking midi from the 
network and sending it to "LoopBe Internal MIDI" and from a 
pyhsical midi device "Analog Experience - The Player" sending 
back on to the network.


Contact me @ http://github.com/teknopaul


