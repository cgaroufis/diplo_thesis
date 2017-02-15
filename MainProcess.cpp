//Main Process - shell
//Handles creation/opening of the other processes
//(namely Kinect data analyzer, audio player and [to do later on] gesture-based GUI)

#include "stdafx.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <Windows.h>
#include <iostream>
#include <string>

#define BUFSIZE (512) //again, lost data is lost data. :)
#define PIPENAME "\\\\.\\pipe\\mode"

int _tmain(int argc, _TCHAR* argv[])
{
	int mode;

	//user input(running mode).
	printf("Choose play mode: 0 for position tracking, 1 for hand tracking\n");
	scanf_s("%d",&mode);
	char c = mode + 48; //get ascii code.

	//create the other processes.
	
	STARTUPINFO info = { sizeof(info) };
	STARTUPINFO info2 = { sizeof(info2) };
	PROCESS_INFORMATION procinfo;
	PROCESS_INFORMATION procinfo2;

	//create pipe handle for passing mode.
	HANDLE hPipe = CreateNamedPipeA(PIPENAME, PIPE_ACCESS_DUPLEX, PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
		PIPE_UNLIMITED_INSTANCES, BUFSIZE, BUFSIZE, 0, NULL);

	CreateProcess(TEXT("../../KinectApp2/Debug/KinectApp2.exe"), NULL, NULL, NULL, TRUE, 0, NULL, NULL, &info, &procinfo);

	while(ConnectNamedPipe(hPipe, NULL)==0);
	WriteFile(hPipe, &c, 1, NULL, NULL);

	CreateProcess(TEXT("../../SineCreate/Debug/SineCreate.exe"), NULL, NULL, NULL, TRUE, 0, NULL, NULL, &info2, &procinfo2);

	//lock permanently (would like to: until a termination signal (SIGINT) has been received)
	//or hang until termination or sth

	while (1 > 0)
	{
		Sleep(1000);
	}

	return 0;

}

