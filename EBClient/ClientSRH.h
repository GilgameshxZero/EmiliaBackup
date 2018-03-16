#pragma once
#include "SendRecv.h"
#include "ClientDFF.h"
#include "ClientStart.h"
#include "Utility.h"
#include <iostream>
#include <string>
#include <Windows.h>

namespace EBC
{
	struct ExtMessParam
	{
		SOCKET *sock;
		std::string message;
		std::queue<char> cbuf; //queue of chars, in sequential order of messages
		std::string fullmess;
		std::vector<std::string> files, fileslong;
		std::vector<FILETIME> lastmod;
		std::vector<std::string> *backupdir;
		char mdelim;
		long long fileinfobuf;

		std::string exittext;
		HWND sendwnd;
	};

	DWORD WINAPI SendThread (LPVOID lpParameter);
	LRESULT CALLBACK SendWndProc (HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);
	int ProcessMessage (void *funcparam);
	int ProcFullMess (void *funcparam);

	//called when recv thread ends
	void OnRecvEnd (void *funcparam); //must terminate send thread
}