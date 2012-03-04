@echo off

set BINDIR=%~dp0
cd %BINDIR%


"C:\Program Files\MinGW\bin\gcc.exe" -Wall -shared  -o midiOverIP.dll midiOverIPLib.c -lwinmm -lwsock32

"C:\Program Files\MinGW\bin\gcc.exe" -Wall -L. -o midiOverIP.exe midiOverIPExe.c -lmidiOverIP

