#include "ServerSRH.h"

namespace EBS
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

			SetWindowLongPtr (emp->sendwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(emp));
			UpdateWindow (emp->sendwnd);
			ShowWindow (emp->sendwnd, SW_HIDE);

			//server specific
			SetTimer (emp->sendwnd, 1, 1, NULL);
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
		ExtMessParam *emp = reinterpret_cast<ExtMessParam *>(GetWindowLongPtr (hwnd, GWLP_USERDATA));

		switch (msg)
		{
			case WM_CLOSE:
			{
				PostQuitMessage (0);
				return 0;
			}

			case WM_TIMER:
			{
				//request client for update
				if (!emp->recvfile) //if not receiving file
				{
					std::string updreq = "update\f";
					Rain::SendText (*emp->sock, updreq.c_str (), updreq.length ());

					std::cout << "Requesting " << emp->addr << " for update...\n";
				}

				SetTimer (hwnd, wparam, static_cast<UINT>(emp->updatems), NULL);
			}

			default:
				break;
		}

		return DefWindowProc (hwnd, msg, wparam, lparam);
	}

	int ProcessMessage (void *funcparam)
	{
		ExtMessParam *emp = reinterpret_cast<ExtMessParam *>(funcparam);

		if (emp->recvfile) //cbuf must be empty, from last time this function was called
		{
			long long nextfilesz = emp->curfilesz + static_cast<long long>(emp->message.length ());
			if (nextfilesz <= emp->filelen)
			{
				emp->fullmess = emp->message;
				int ret = ProcFullMess (funcparam);
				if (ret)
					return ret;
				emp->fullmess.clear ();

				return 0;
			}
			else //>, so split message
			{
				long long split = emp->filelen - emp->curfilesz;

				emp->fullmess = emp->message.substr (0, split);
				emp->message = emp->message.substr (split, emp->message.length () - split);

				int ret = ProcFullMess (funcparam);
				if (ret)
					return ret;
				emp->fullmess.clear ();
			}
		}

		//extract message into cbuf, keep track of last deliminating character
		long long lastdelim = -1;
		for (long long a = 0; a < static_cast<long long>(emp->message.length ()); a++)
		{
			emp->cbuf.push (emp->message[a]);
			if (emp->mdelim == emp->message[a])
				lastdelim = emp->cbuf.size ();
		}

		//if we can extract any full message from cbuf, do that and process it
		for (long long a = 0; a < lastdelim; a++)
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

			if (emp->recvfile) //if processing has changed recvfile status
			{
				while (!emp->cbuf.empty () && static_cast<long long>(emp->fullmess.length ()) < emp->filelen)
				{
					emp->fullmess.push_back (emp->cbuf.front ());
					emp->cbuf.pop ();
					a++;
				}

				//send what we have to ProcFullMess - if that is all, then we continue parsing cbuf; if there is more to the file, then cbuf must be empty, and we exit the loop, and wait for next message
				int ret = ProcFullMess (funcparam);
				if (ret)
					return ret;
				emp->fullmess.clear ();
			}
		}

		return 0;
	}

	int ProcFullMess (void *funcparam)
	{
		ExtMessParam *emp = reinterpret_cast<ExtMessParam *>(funcparam);

		if (emp->recvfile) //receiving parts of a file, block stream until all are here
		{
			emp->filebuf += emp->fullmess;
			emp->curfilesz += emp->fullmess.size ();

			if (static_cast<long long>(emp->filebuf.size ()) > emp->fileinfobuf || emp->curfilesz == emp->filelen) //output file
			{
				emp->outfile << emp->filebuf;
				emp->filebuf.clear ();

				long long curpct;
				
				if (emp->filelen == 0)
					curpct = 100;
				else
					curpct = emp->curfilesz * 100 / emp->filelen;

				if (curpct != emp->lastpct)
				{
					emp->lastpct = curpct;
					std::cout << "\rFile " << emp->thisfile << " of " << emp->totalfiles << " from " << emp->addr << ": " << emp->curfilesz << " of " << emp->filelen << " bytes (" << curpct << "%)";
				}
			}

			if (emp->curfilesz == emp->filelen)
			{
				emp->recvfile = false;
				emp->outfile.close ();

				//set new filetime
				wchar_t unicode[MAX_PATH];
				MultiByteToWideChar (CP_UTF8, 0, emp->abspath.c_str (), -1, unicode, MAX_PATH);
				HANDLE hfile = CreateFileW (unicode, GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
				SetFileTime (hfile, NULL, NULL, &(emp->newfiletime));
				CloseHandle (hfile);

				std::cout << "\n";
			}
		}
		else
		{
			std::string query;
			std::stringstream fmss (emp->fullmess, std::ios_base::binary | std::ios_base::in | std::ios_base::out);

			fmss >> query;

			if (query == emp->exittext)
			{
				Rain::SendText (*emp->sock, emp->exittext.c_str (), emp->exittext.length ());
				return 1;
			}
			else if (query == "listdir")
			{
				long long cdirs, cfiles;
				fmss >> cdirs >> cfiles;

				std::vector<std::string> files (cfiles), dirs (cdirs);
				std::vector<FILETIME> lastmod (cfiles);
				std::string dirpath = Rain::GetExePath () + "\\" + emp->addr + "\\", tmp;

				std::getline (fmss, tmp);
				for (long long a = 0; a < cdirs; a++)
					std::getline (fmss, dirs[a]);
				for (long long a = 0; a < cfiles; a++)
				{
					std::getline (fmss, files[a]);
					fmss >> lastmod[a].dwLowDateTime >> lastmod[a].dwHighDateTime;
					std::getline (fmss, tmp);
				}

				//make a copy of the files list, sort it, and compare it with a sorted list of files on server. delete files on server which are not on the files list
				std::vector<std::string> filescopy (files), serverfiles;
				std::vector<long long> needdelete;
				long long ptr1, ptr2;

				//make sure dirpath exists
				CreateDirToFile (dirpath);

				Rain::GetRelFilePathRec (dirpath, serverfiles, "*");
				std::sort (filescopy.begin (), filescopy.end ());
				std::sort (serverfiles.begin (), serverfiles.end ());
				ptr1 = ptr2 = 0;
				for (; ptr1 < static_cast<long long>(filescopy.size ()) && ptr2 < static_cast<long long>(serverfiles.size ());)
				{
					if (serverfiles[ptr2] < filescopy[ptr1])
						needdelete.push_back (ptr2++);
					else if (serverfiles[ptr2] == filescopy[ptr1])
						ptr1++, ptr2++;
					else
						ptr1++;
				}

				//any files after and at pt2 are also gone
				for (;ptr2 < static_cast<long long>(serverfiles.size ());ptr2++)
					needdelete.push_back (ptr2);

				wchar_t unicode[MAX_PATH];
				std::cout << "Deleting " << needdelete.size () << " files from " << emp->addr << " backup...\n";
				for (long long a = 0;a < static_cast<long long>(needdelete.size ());a++)
				{
					MultiByteToWideChar (CP_UTF8, 0, serverfiles[needdelete[a]].c_str (), -1, unicode, MAX_PATH);
					//std::cout << "Deleting " << unicode << " from " << emp->addr << "...\n";
					std::cout << "Deleting " << serverfiles[needdelete[a]] << " from " << emp->addr << "...\n";
					MultiByteToWideChar (CP_UTF8, 0, (dirpath + serverfiles[needdelete[a]]).c_str (), -1, unicode, MAX_PATH);
					DeleteFileW (unicode);
				}

				//match directory list as well, and delete extra dirs, which should now be empty
				std::vector<std::string> serverdirs;
				std::vector<long long> needcreate;
				Rain::GetRelDirPathRec (dirpath, serverdirs, "*");
				std::sort (dirs.begin (), dirs.end ());
				std::sort (serverdirs.begin (), serverdirs.end ());

				needdelete.clear ();
				ptr1 = ptr2 = 0;
				for (; ptr1 < static_cast<long long>(dirs.size ()) && ptr2 < static_cast<long long>(serverdirs.size ());)
				{
					if (serverdirs[ptr2] < dirs[ptr1])
						needdelete.push_back (ptr2++);
					else if (serverdirs[ptr2] == dirs[ptr1])
						ptr1++, ptr2++;
					else
						needcreate.push_back (ptr1++);
				}

				for (; ptr2 < static_cast<long long>(serverdirs.size ()); ptr2++)
					needdelete.push_back (ptr2);
				for (; ptr1 < static_cast<long long>(dirs.size ()); ptr1++)
					needcreate.push_back (ptr1);

				std::cout << "Creating " << needcreate.size () << " directories for " << emp->addr << " backup...\n";
				for (long long a = 0; a < static_cast<long long>(needcreate.size ()); a++)
				{
					MultiByteToWideChar (CP_UTF8, 0, dirs[needcreate[a]].c_str (), -1, unicode, MAX_PATH);
					//std::cout << "Creating directory " << unicode << " for " << emp->addr << "...\n";
					std::cout << "Creating directory " << dirs[needcreate[a]] << " for " << emp->addr << "...\n";
					CreateDirToFile ((dirpath + dirs[needcreate[a]]).c_str ());
				}

				std::cout << "Deleting " << needdelete.size () << " directories from " << emp->addr << " backup...\n";
				for (long long a = 0; a < static_cast<long long>(needdelete.size ()); a++)
				{
					MultiByteToWideChar (CP_UTF8, 0, serverdirs[needdelete[a]].c_str (), -1, unicode, MAX_PATH);
					//std::cout << "Deleting directory " << unicode << " from " << emp->addr << "...\n";
					std::cout << "Deleting directory " << serverdirs[needdelete[a]] << " from " << emp->addr << "...\n";
					Rain::RecursiveRmDir (dirpath + serverdirs[needdelete[a]]);
					MultiByteToWideChar (CP_UTF8, 0, (dirpath + serverdirs[needdelete[a]]).c_str (), -1, unicode, MAX_PATH);
					RemoveDirectoryW (unicode);
				}

				//go to each files and match FILETIME. if not match, add it to array to eventually send back to client, to request that file
				std::vector<FILETIME> curlastmod;
				std::vector<std::string> fileslong;
				for (long long a = 0; a < static_cast<long long>(files.size ()); a++)
					fileslong.push_back (dirpath + files[a]);
				Rain::GetLastModTime (fileslong, curlastmod);

				std::vector<long long> needupdate;
				for (long long a = 0; a < static_cast<long long>(lastmod.size ()); a++)
					if (!SameFileTime (lastmod[a], curlastmod[a]))
						needupdate.push_back (a);

				//send list of requests to client
				std::string outtext;
				outtext = "updatefiles " + Rain::LLToStr (static_cast<long long>(needupdate.size ()));
				for (long long a = 0; a < static_cast<long long>(needupdate.size ()); a++)
					outtext += " " + Rain::LLToStr (needupdate[a]);
				outtext += emp->mdelim;

				Rain::SendText (*emp->sock, outtext.c_str (), outtext.length ());

				std::cout << emp->addr << ": " << needupdate.size () << " file(s) need updating.\n";
			}
			else if (query == "file")
			{
				//server has sent us a file, that we requested to be updated. input that file and store it where it goes
				fmss >> emp->thisfile >> emp->totalfiles >> emp->filelen >> emp->newfiletime.dwLowDateTime >> emp->newfiletime.dwHighDateTime;

				std::string tmp;
				std::getline (fmss, tmp);
				std::getline (fmss, emp->relpath);
				emp->thisfile++;

				//set the mode of this socket to receiving file, and don't switch until we've got all our bytes
				emp->recvfile = true;
				emp->abspath = Rain::GetExePath () + "\\" + emp->addr + "\\" + emp->relpath;
				emp->lastpct = -1;
				CreateDirToFile (emp->abspath);
				wchar_t unicode[MAX_PATH];
				MultiByteToWideChar (CP_UTF8, 0, emp->abspath.c_str (), -1, unicode, MAX_PATH);
				emp->outfile.open (unicode, std::ios_base::binary);
				emp->curfilesz = 0;

				MultiByteToWideChar (CP_UTF8, 0, emp->relpath.c_str (), -1, unicode, MAX_PATH);
				//std::cout << "Receiving: " << unicode << "\n";
				std::cout << "Receiving: " << emp->relpath << "\n";
			}
			else
				std::cout << "Unexpected message.\n";
		}

		return 0;
	}

	void OnRecvEnd (void *funcparam)
	{
		ExtMessParam *emp = reinterpret_cast<ExtMessParam *>(funcparam);

		SendMessage (emp->sendwnd, WM_CLOSE, 0, 0);
		std::cout << "Message received to disconnect from client " << emp->addr << ". Terminating send/recv threads...\n";
	}
}