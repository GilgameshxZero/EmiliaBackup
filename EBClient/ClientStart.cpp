#include "ClientStart.h"

namespace EBC
{
	int Start ()
	{
		WSADATA wsadata;
		struct addrinfo *addr;
		SOCKET sock;
		std::string portname, hostname, hostaddrstr, servicenumstr, tmp;
		int error, delimint, hnamecut;
		long long retryms, cdir, sendbuf, recvbuf, fileinfobuf;
		char hostaddr[128], servicenum[128], mdelim;
		std::ifstream inparam;
		HANDLE recvth, sendth;
		Rain::RecvParam recvparam;
		ExtMessParam funcparam;
		DWORD derror;
		const std::string exittext = "exit";
		std::vector<std::string> backupdir;

		std::ofstream errout;
		std::streambuf *oldcerr;

		//redirect cerr
		errout.open ("cerr.txt", std::ios::binary);
		oldcerr = std::cerr.rdbuf ();
		std::cerr.rdbuf (errout.rdbuf ());

		//read parameters from .ini file
		inparam.open ("client.ini"); //file in UTF-8, no signature
		std::getline (inparam, hostname);
		std::getline (inparam, portname);
		inparam >> delimint >> retryms >> sendbuf >> recvbuf >> fileinfobuf;
		mdelim = static_cast<char>(delimint);
		inparam >> cdir;
		backupdir.resize (cdir);
		std::getline (inparam, tmp); //ignore newline
		for (long long a = 0;a < cdir;a++)
		{
			std::getline (inparam, backupdir[a]);
			Rain::TrimBSR (backupdir[a]);

			if (backupdir[a].back () != '\\')
				backupdir[a].push_back ('\\');
		}
		inparam.close ();
		addr = NULL;

		Rain::TrimBSR (hostname);
		Rain::TrimBSR (portname);

		//if the .ini file has a signature (Unicode or something else), there are odd characters before hostname
		for (hnamecut = 0; hnamecut < static_cast<int>(hostname.length ()); hnamecut++)
			if (isalnum (static_cast<unsigned char>(hostname[hnamecut])))
				break;
		hostname = hostname.substr (hnamecut, hostname.length () - hnamecut);

		std::cout << "Initializing...\n";

		error = Rain::InitWinsock (wsadata);
		if (error) {
			Rain::ReportError ("InitWinsock failed!", error);
			return error;
		}
		error = Rain::GetClientAddr (hostname, portname, &addr);
		if (error) {
			Rain::ReportError ("GetClientAddr failed!", error);
			return error;
		}

		//get IP of host and port
		error = getnameinfo (addr->ai_addr, sizeof (sockaddr_in), hostaddr, sizeof (hostaddr) / sizeof (char), servicenum, sizeof (servicenum) / sizeof (char), NI_NUMERICHOST | NI_NUMERICSERV);
		if (error) {
			Rain::ReportError ("getnameinfo failed!", WSAGetLastError ());
			return error;
		}
		hostaddrstr = hostaddr;
		servicenumstr = servicenum;

		std::cout << "Server Info:\nhost name:\t" << hostname << "\n";
		if (hostname != hostaddrstr)
			std::cout << "address:\t" << hostaddr << "\n";
		std::cout << "port:\t\t" << portname << "\n";
		if (portname != servicenum)
			std::cout << "number:\t" << servicenum << "\n";

		//retry until succeed, or if server disconnects
		while (true)
		{
			error = Rain::CreateClientSocket (&addr, sock);
			if (error) {
				Rain::ReportError ("CreateClientSocket failed!", error);
				return error;
			}

			while (true)
			{
				error = Rain::ConnToServ (&addr, sock);
				if (error)
				{
					Rain::ReportError ("ConnToServ failed!", error);
					std::cout << "Failed to connect to server!\n" << "Retrying...\n";
					Sleep (static_cast<DWORD>(retryms));
				}
				else
					break;
			}

			std::cout << "Connected to server!\n";

			//create one thread for receiving from socket, one for sending, and idle current one until we need to re-establish connection (todo)
			funcparam.sock = &sock;

			funcparam.exittext = exittext;
			funcparam.backupdir = &backupdir;
			funcparam.mdelim = mdelim;
			funcparam.fileinfobuf = fileinfobuf;

			recvparam.sock = &sock;
			recvparam.message = &funcparam.message;
			recvparam.buflen = static_cast<int>(recvbuf);

			recvparam.funcparam = &funcparam;
			recvparam.ProcessMessage = EBC::ProcessMessage;
			recvparam.OnRecvEnd = EBC::OnRecvEnd;

			recvth = CreateThread (NULL, 0, Rain::RecvThread, reinterpret_cast<LPVOID>(&recvparam), 0, NULL);
			sendth = CreateThread (NULL, 0, SendThread, reinterpret_cast<LPVOID>(&funcparam), 0, NULL);

			if (recvth == 0 || sendth == 0) {
				error = GetLastError ();
				Rain::ReportError ("CreateThread failed!", error);
				return error;
			}

			WaitForSingleObject (recvth, INFINITE);
			WaitForSingleObject (sendth, INFINITE);

			//reset relevant parameters, for reconnect
			while (!funcparam.cbuf.empty ())
				funcparam.cbuf.pop ();
			funcparam.fullmess.clear ();
			funcparam.files.clear ();
			funcparam.fileslong.clear ();
			funcparam.lastmod.clear ();
		}

		GetExitCodeThread (recvth, &derror);
		error = derror;
		if (error) {
			Rain::ReportError ("RecvThread failed!", error);
			return error;
		}
		GetExitCodeThread (sendth, &derror);
		error = derror;
		if (error) {
			Rain::ReportError ("SendThread failed!", error);
			return error;
		}

		CloseHandle (recvth);
		CloseHandle (sendth);

		//shutdown WSA
		error = Rain::ShutdownSocketSend (sock);
		if (error) {
			Rain::ReportError ("ShutdownSocketSend failed!", error);
			return error;
		}
		freeaddrinfo (addr);
		WSACleanup ();

		//restore cerr
		std::cerr.rdbuf (oldcerr);

		return 0;
	}
}