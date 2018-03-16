#include "ClientSRH.h"

namespace EBC
{
	DWORD WINAPI SendThread (LPVOID lpParameter)
	{
		ExtMessParam *emp = reinterpret_cast<ExtMessParam *>(lpParameter);
		int ret;

		//implement as window w/ message queue
		MSG msg;
		WNDCLASSEX wcex;
		HINSTANCE hinst = GetModuleHandle (NULL);
		std::string classname, titletext;

		//must be unique
		classname = "SendThreadWnd: " + Rain::LLToStr ((long long)emp->sock);
		titletext = classname;

		{//register and create host window
			wcex.cbSize = sizeof (WNDCLASSEX);
			wcex.style = NULL;
			wcex.lpfnWndProc = SendWndProc;
			wcex.cbClsExtra = 0;
			wcex.cbWndExtra = 0;
			wcex.hInstance = hinst;
			wcex.hIcon = NULL;
			wcex.hCursor = LoadCursor (NULL, IDC_ARROW);
			wcex.hbrBackground = NULL;
			wcex.lpszMenuName = NULL;
			wcex.lpszClassName = classname.c_str ();
			wcex.hIconSm = NULL;

			if (!RegisterClassEx (&wcex)) {
				ret = GetLastError ();
				Rain::ReportError ("Could not register " + classname + ".", ret);
				return ret;
			}

			emp->sendwnd = CreateWindowEx (WS_EX_TOPMOST, classname.c_str (), titletext.c_str (), WS_POPUP, 0, 0, 0, 0, NULL, NULL, hinst, NULL);

			if (emp->sendwnd == NULL) {
				ret = GetLastError ();
				Rain::ReportError ("Could not create " + classname + ".", ret);
				return ret;
			}

			UpdateWindow (emp->sendwnd);
			ShowWindow (emp->sendwnd, SW_HIDE);
		}

		//message loop
		while (GetMessage (&msg, NULL, 0, 0) > 0)
		{
			TranslateMessage (&msg);
			DispatchMessage (&msg);
		}

		return static_cast<int>(msg.wParam);
	}

	LRESULT CALLBACK SendWndProc (HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
	{
		switch (msg)
		{
			case WM_CLOSE:
			{
				PostQuitMessage (0);
				return 0;
			}

			default:
				break;
		}

		return DefWindowProc (hwnd, msg, wparam, lparam);
	}

	//deal with message (emp->message)
	int ProcessMessage (void *funcparam)
	{
		ExtMessParam *emp = reinterpret_cast<ExtMessParam *>(funcparam);

		//extract message into cbuf, keep track of last deliminating character
		long long lastdelim = -1;
		for (long long a = 0;a < static_cast<long long>(emp->message.length ());a++)
		{
			emp->cbuf.push (emp->message[a]);
			if (emp->mdelim == emp->message[a])
				lastdelim = static_cast<long long>(emp->cbuf.size ());
		}

		//if we can extract any full message from cbuf, do that and process it
		if (lastdelim == -1)
			return 0;

		for (int a = 0;a < lastdelim;a++)
		{
			int ret;
			char next = emp->cbuf.front ();
			emp->cbuf.pop ();

			if (next == emp->mdelim)
			{
				ret = ProcFullMess (funcparam);
				if (ret)
					return ret;
				emp->fullmess.clear ();
			}
			else
				emp->fullmess.push_back (next);
		}

		return 0;
	}

	int ProcFullMess (void *funcparam)
	{
		//process message stored in emp->fullmess
		ExtMessParam *emp = reinterpret_cast<ExtMessParam *>(funcparam);
		std::string query;
		std::stringstream fmss (emp->fullmess, std::ios_base::binary | std::ios_base::in | std::ios_base::out);

		fmss >> query;

		if (query == emp->exittext)
		{
			Rain::SendText (*emp->sock, emp->exittext.c_str (), emp->exittext.length ());
			return 1;
		}
		else if (query == "update")
		{
			std::cout << "Updating server...\n";

			//give server a list of directories, relative paths
			std::vector<std::string> dirs;
			for (long long a = 0; a < static_cast<long long>(emp->backupdir->size ()); a++)
			{
				std::vector<std::string> dirstmp;
				std::string shortdirname = Rain::GetShortName ((*(emp->backupdir))[a]);
				dirs.push_back (shortdirname);
				Rain::GetRelDirPathRec ((*(emp->backupdir))[a], dirstmp, "*");

				for (long long b = 0; b < static_cast<long long>(dirstmp.size ()); b++)
					dirs.push_back (shortdirname + dirstmp[b]);
			}

			//give server a list of files, relative paths
			std::vector<std::string> filestmp;

			emp->lastmod.clear ();
			emp->files.clear ();
			emp->fileslong.clear ();
			for (long long a = 0; a < static_cast<long long>(emp->backupdir->size ()); a++)
			{
				std::vector<std::string> filestmp;
				Rain::GetRelFilePathRec ((*(emp->backupdir))[a], filestmp, "*");

				std::vector<std::string> fileslongtmp;
				std::string shortdirname = Rain::GetShortName ((*(emp->backupdir))[a]);
				for (long long b = 0; b < static_cast<long long>(filestmp.size ()); b++)
				{
					emp->files.push_back (shortdirname + filestmp[b]);
					fileslongtmp.push_back ((*(emp->backupdir))[a] + filestmp[b]);
				}

				//also get lastmodified time
				std::vector<FILETIME> lastmodtmp;
				Rain::GetLastModTime (fileslongtmp, lastmodtmp);
				for (long long b = 0; b < static_cast<long long>(fileslongtmp.size ()); b++)
				{
					emp->fileslong.push_back (fileslongtmp[b]);
					emp->lastmod.push_back (lastmodtmp[b]);
				}
			}

			//output
			std::string outtext;

			outtext = "listdir " + Rain::LLToStr (static_cast<long long>(dirs.size ())) + " " + Rain::LLToStr (static_cast<long long>(emp->files.size ())) + "\n";
			for (long long a = 0; a < static_cast<long long>(dirs.size ()); a++)
				outtext += dirs[a] + "\n";
			for (long long a = 0; a < static_cast<long long>(emp->files.size ()); a++)
			{
				outtext += emp->files[a] + "\n";
				outtext += Rain::LLToStr (static_cast<long long>(emp->lastmod[a].dwLowDateTime)) + " " + Rain::LLToStr (static_cast<long long>(emp->lastmod[a].dwHighDateTime)) + "\n";
			}
			outtext += emp->mdelim;

			Rain::SendText (*emp->sock, outtext.c_str (), outtext.length ());
		}
		else if (query == "updatefiles")
		{
			//server has requested us send these files
			long long cfiles;
			fmss >> cfiles;

			std::cout << "Server requested " << cfiles << " updates.\n";

			std::vector<long long> update (cfiles);
			for (long long a = 0; a < cfiles; a++)
				fmss >> update[a];

			//for each file, send "file [this file id] [total files] [file length in bytes] [filetime.low] [filetime.high]\nRelative file path\n\f" and then the file
			for (long long a = 0; a < static_cast<long long>(update.size ()); a++)
			{
				std::string outtext;
				outtext = "file " + Rain::LLToStr (a) + " " + Rain::LLToStr (static_cast<long long>(update.size ())) + " ";

				std::ifstream filein;
				long long filelen;
				wchar_t unicode[MAX_PATH];
				MultiByteToWideChar (CP_UTF8, 0, emp->fileslong[update[a]].c_str (), -1, unicode, MAX_PATH);
				filein.open (unicode, std::ios_base::binary);
				filein.seekg (0, filein.end);
				filelen = static_cast<long long>(filein.tellg ());
				filein.seekg (0, filein.beg);

				//std::cout << "Sending file: " << unicode << "\n";
				std::cout << "Sending file: " << emp->files[update[a]] << "\n";
				std::cout << "File " << a + 1 << " of " << update.size () << ": 0 of " << filelen << " bytes (0%) done";

				//send header first
				outtext += Rain::LLToStr (filelen) + " " + Rain::LLToStr (static_cast<long long>(emp->lastmod[update[a]].dwLowDateTime)) + " " + Rain::LLToStr (static_cast<long long>(emp->lastmod[update[a]].dwHighDateTime)) + "\n" + emp->files[update[a]] + "\n" + emp->mdelim;
				Rain::SendText (*emp->sock, outtext.c_str (), outtext.length ());
				outtext.clear ();

				//send actual file in buffers (emp->fileinfobuf size)
				char *fibuf;
				int ret;
				fibuf = new char[emp->fileinfobuf];
				for (long long b = 0, lastpct = 0, curpct;b < filelen;b += emp->fileinfobuf)
				{
					if (b + emp->fileinfobuf > filelen)
					{
						filein.read (fibuf, filelen - b);
						ret = Rain::SendText (*emp->sock, fibuf, filelen - b);
					}
					else
					{
						filein.read (fibuf, emp->fileinfobuf);
						ret = Rain::SendText (*emp->sock, fibuf, emp->fileinfobuf);
					}

					if (ret) //sending has failed - skip all files, and close this thread
						return 1;

					if (filelen == 0)
						curpct = 100;
					else
						curpct = static_cast<long long>(b) * 100 / filelen;

					if (curpct != lastpct)
					{
						lastpct = curpct;
						std::cout << "\rFile " << a + 1 << " of " << update.size () << ": " << b << " of " << filelen << " bytes (" << curpct << "%) done";
					}
				}
				delete[] fibuf;
				std::cout << "\rFile " << a + 1 << " of " << update.size () << ": " << filelen << " of " << filelen << " bytes (100%) done\n";
			}

			std::cout << "Update done.\n";
		}
		else
			std::cout << "Unexpected message.\n";

		return 0;
	}

	void OnRecvEnd (void *funcparam)
	{
		ExtMessParam *emp = reinterpret_cast<ExtMessParam *>(funcparam);

		SendMessage (emp->sendwnd, WM_CLOSE, 0, 0);
	}
}