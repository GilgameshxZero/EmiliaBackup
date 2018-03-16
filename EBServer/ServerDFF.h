/*Stands for Directory & File Functions*/

#pragma once
#include <string>
#include <vector>
#include "Utility.h"

namespace EBS
{
	bool SameFileTime (FILETIME x, FILETIME y);
	void CreateDirToFile (std::string abspath); //makes sure directories along the path exist
}