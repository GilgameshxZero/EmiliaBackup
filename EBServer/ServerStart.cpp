#include "ServerStart.h"

namespace EBS
{
	int Start ()
	{
		WSADATA wsadata;
		struct addrinfo *addr;
		SOCKET listsock;
		std::string portname, hostaddrstr, servicenumstr;
		int error, cnamelen, delimint;
		long long updatems, sendbuf, recvbuf, fileinfobuf;
		char hostaddr[128], servicenum[128], clientaddr[128], clientport[128], mdelim;
		std::ifstream inparam;
		std::vector<SOCKET *> clientsock;
		std::vector<HANDLE> recvth, sendth;
		std::vector<ExtMessParam *> funcparam;
		std::vector<Rain::RecvParam *> recvparam;
		DWORD derror;
		struct sockaddr clientname;
		const std::string exittext = "exit";
		//unsigned long nonblocking = 1;

		std::ofstream errout;
		std::streambuf *oldcerr;

		//redirect cerr
		errout.open ("cerr.txt", std::ios::binary);
		oldcerr = std::cerr.rdbuf ();
		std::cerr.rdbuf (errout.rdbuf ());

		//read parameters from .ini file
		inparam.open ("server.ini", std::ios::binary);
		std::getline (inparam, portname);
		inparam >> delimint >> updatems >> sendbuf >> recvbuf >> fileinfobuf;
		mdelim = static_cast<char>(delimint);
		inparam.close ();
		addr = NULL;

		Rain::TrimBSR (portname);

		std::cout << "Initializing...\n";

		error = Rain::InitWinsock (wsadata);
		if (error) {
			Rain::ReportError ("InitWinsock failed!", error);
			return error;
		}
		error = Rain::GetServAddr (portname, &addr);
		if (error) {
			Rain::ReportError ("GetServAddr failed!", error);
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

		std::cout << "Server Info:\nport:\t\t" << portname << "\n";
		if (portname != servicenum)
			std::cout << "number:\t" << servicenum << "\n";

		error = Rain::CreateServLSocket (&addr, listsock);
		if (error) {
			Rain::ReportError ("CreateServLSocket failed!", error);
			return error;
		}
		error = Rain::BindServLSocket (&addr, listsock);
		if (error) {
			Rain::ReportError ("BindServLSocket failed!", error);
			return error;
		}
		error = Rain::ListenServSocket (listsock);
		if (error) {
			Rain::ReportError ("ListenServSocket failed!", error);
			return error;
		}

		std::cout << "Listening...\n";

		//continuously accept connections
		while (true)
		{
			clientsock.push_back (new SOCKET ());
			error = Rain::ServAcceptClient (*(clientsock.back ()), listsock);
			if (error) {
				Rain::ReportError ("ServAcceptClient failed!", error);
				return error;
			}

			std::cout << "Socket accepted!\n";

			//get info for client
			cnamelen = sizeof (clientname);
			error = getpeername (*(clientsock.back ()), &clientname, &cnamelen);
			if (error == SOCKET_ERROR) {
				error = WSAGetLastError ();
				Rain::ReportError ("getsockname failed!", error);
				return error;
			}
			error = getnameinfo (&clientname, sizeof (sockaddr_in), clientaddr, sizeof (clientaddr) / sizeof (char), clientport, sizeof (clientport) / sizeof (char), NI_NUMERICHOST | NI_NUMERICSERV);
			if (error) {
				Rain::ReportError ("getnameinfo failed!", WSAGetLastError ());
				return error;
			}

			std::cout << "Client address: " << clientaddr << "\nClient port: " << clientport << "\n";

			//unblock socket
			//ioctlsocket (*(clientsock.back ()), FIONBIO, &nonblocking);

			//create thread for receiving from socket, one for sending to socket, and use current thread to listen
			funcparam.push_back (new ExtMessParam ());
			funcparam.back ()->sock = clientsock.back ();
			funcparam.back ()->mdelim = mdelim;

			funcparam.back ()->exittext = exittext;
			funcparam.back ()->addr = clientaddr;

			funcparam.back ()->updatems = updatems;

			funcparam.back ()->fileinfobuf = fileinfobuf;

			recvparam.push_back (new Rain::RecvParam ());
			recvparam.back ()->sock = clientsock.back ();
			recvparam.back ()->message = &funcparam.back ()->message;
			recvparam.back ()->buflen = static_cast<int>(recvbuf);

			recvparam.back ()->funcparam = funcparam.back ();

			recvparam.back ()->ProcessMessage = EBS::ProcessMessage;
			recvparam.back ()->OnRecvEnd = EBS::OnRecvEnd;

			recvth.push_back (HANDLE ());
			sendth.push_back (HANDLE ());

			recvth.back () = CreateThread (NULL, 0, Rain::RecvThread, reinterpret_cast<LPVOID>(recvparam.back ()), 0, NULL);
			sendth.back () = CreateThread (NULL, 0, SendThread, reinterpret_cast<LPVOID>(funcparam.back ()), 0, NULL);

			if (recvth.back () == 0 || sendth.back () == 0) {
				error = GetLastError ();
				Rain::ReportError ("CreateThread failed!", error);
				return error;
			}
		}

		for (long long a = 0;a < static_cast<long long>(recvth.size ());a++)
		{
			WaitForSingleObject (recvth[a], INFINITE);
			WaitForSingleObject (sendth[a], INFINITE);

			GetExitCodeThread (recvth[a], &derror);
			error = derror;
			if (error) {
				Rain::ReportError ("RecvThread failed!", error);
				return error;
			}
			GetExitCodeThread (sendth[a], &derror);
			error = derror;
			if (error) {
				Rain::ReportError ("SendThread failed!", error);
				return error;
			}

			CloseHandle (recvth[a]);
			CloseHandle (sendth[a]);

			delete funcparam[a];
			delete recvparam[a];
		}

		//shutdown WSA
		for (long long a = 0;a < static_cast<long long>(clientsock.size ());a++)
		{
			error = Rain::ShutdownSocketSend (*(clientsock[a]));
			if (error) {
				Rain::ReportError ("ShutdownSocketSend failed!", error);
				return error;
			}
		}
		freeaddrinfo (addr);
		WSACleanup ();

		//free objects
		for (long long a = 0; a < static_cast<long long>(recvth.size ()); a++)
			delete clientsock[a];

		//restore cerr
		std::cerr.rdbuf (oldcerr);

		return 0;
	}
}