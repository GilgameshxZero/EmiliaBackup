#include "ServerDFF.h"

namespace EBS
{	
	bool SameFileTime (FILETIME x, FILETIME y)
	{
		//compares filetimes to some epsilon
		const static int EPS = 0; //must be identical

		if (x.dwHighDateTime != y.dwHighDateTime)
			return false;
		if (abs ((int)(x.dwLowDateTime - y.dwLowDateTime)) > EPS)
			return false;
		return true;
	}

	//takes UTF8, but uses unicode
	void CreateDirToFile (std::string abspath)
	{
		wchar_t unicode[MAX_PATH];
		for (long long a = static_cast<long long>(abspath.find ('\\', 0));a != static_cast<long long>(std::string::npos); a = static_cast<long long>(abspath.find ('\\', a + 1)))
		{
			MultiByteToWideChar (CP_UTF8, 0, abspath.substr (0, a).c_str (), -1, unicode, MAX_PATH);
			CreateDirectoryW (unicode, NULL);
		}
	}
}