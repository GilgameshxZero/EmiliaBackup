#pragma once
#include "SendRecv.h"
#include "ServerStart.h"
#include "ServerDFF.h"
#include "Utility.h"
#include <iostream>
#include <io.h>
#include <fcntl.h>
#include <fstream>
#include <string>
#include <Windows.h>

namespace EBS
{
	struct ExtMessParam
	{
		SOCKET *sock;
		std::string message;
		std::queue<char> cbuf; //queue of chars, in sequential order of messages
		std::string fullmess;
		char mdelim;

		std::string exittext, addr;
		HWND sendwnd;

		long long updatems;

		bool recvfile;
		long long filelen, fileinfobuf, curfilesz, lastpct;
		int thisfile, totalfiles;
		std::string abspath, relpath, filebuf;
		std::ofstream outfile;
		FILETIME newfiletime;
	};

	DWORD WINAPI SendThread (LPVOID lpParameter);
	LRESULT CALLBACK SendWndProc (HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);
	int ProcessMessage (void *funcparam);
	int ProcFullMess (void *funcparam);

	//called when recv thread ends
	void OnRecvEnd (void *funcparam); //must terminate send thread
}