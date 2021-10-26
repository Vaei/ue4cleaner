// Copyright (c) 2019 Jared Taylor / Vaei. All Rights Reserved.
// UE4Cleaner.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include "pch.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <windows.h>
#include <filesystem>

using namespace std;
namespace fs = std::filesystem;

bool StringStartsWith(const string& inString, const string& startsWith)
{
	return (inString.compare(0, startsWith.length(), startsWith) == 0);
}

bool StringEndsWith(const string& inString, const string& endsWith)
{
	return (inString.compare(inString.length() - endsWith.length(), endsWith.length(), endsWith) == 0);
}

void StringRemove(string& inString, const string& delimiter)
{
	inString = inString.erase(0, inString.find(delimiter) + delimiter.length());
}

string StringReplaceAll(std::string str, const std::string& from, const std::string& to)
{
	size_t start_pos = 0;
	while ((start_pos = str.find(from, start_pos)) != std::string::npos)
	{
		str.replace(start_pos, from.length(), to);
		start_pos += to.length(); // Handles case where 'to' is a substring of 'from'
	}
	return str;
}

wstring s2ws(const string& str)
{
	if (str.empty()) return std::wstring();
	int size_needed = MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), NULL, 0);
	std::wstring wstrTo(size_needed, 0);
	MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), &wstrTo[0], size_needed);
	return wstrTo;
}

string ws2s(const std::wstring& wstr)
{
	if (wstr.empty()) return std::string();
	int size_needed = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), NULL, 0, NULL, NULL);
	std::string strTo(size_needed, 0);
	WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), &strTo[0], size_needed, NULL, NULL);
	return strTo;
}

int FatalError(const string& errorString, const int errorId)
{
	const char *errorChar = errorString.c_str();
	cout << "[Fatal] " << errorChar << ". Exiting with error code: " << to_string(errorId) << endl;
	return errorId;
}

wstring ReadRegValue(HKEY root, wstring key, wstring name)
{
	HKEY hKey;
	if (RegOpenKeyEx(root, key.c_str(), 0, KEY_READ, &hKey) != ERROR_SUCCESS)
		throw "Could not open registry key";

	DWORD type;
	DWORD cbData;
	if (RegQueryValueEx(hKey, name.c_str(), NULL, &type, NULL, &cbData) != ERROR_SUCCESS)
	{
		RegCloseKey(hKey);
		throw "Could not read registry value";
	}

	if (type != REG_SZ)
	{
		RegCloseKey(hKey);
		throw "Incorrect registry value type";
	}

	wstring value(cbData / sizeof(wchar_t), L'\0');
	if (RegQueryValueEx(hKey, name.c_str(), NULL, NULL, reinterpret_cast<LPBYTE>(&value[0]), &cbData) != ERROR_SUCCESS)
	{
		RegCloseKey(hKey);
		throw "Could not read registry value";
	}

	RegCloseKey(hKey);

	size_t firstNull = value.find_first_of(L'\0');
	if (firstNull != string::npos)
		value.resize(firstNull);

	return value;
}

/**
 * return: number of files or directories deleted
 */
uintmax_t delete_directory(const string& path)
{
	fs::path dir = path;
	return fs::remove_all(path);
}

int main(int argc, char* argv[])
{
	TCHAR pwd[MAX_PATH];
	GetCurrentDirectory(MAX_PATH, pwd);

	static const wstring regPath = s2ws("Software\\Epic Games\\Unreal Engine\\Builds");  // Path to registry entry used by UE4 when generating project files

	fs::path path = pwd;
	bool cleanonly = false;
	bool plugins = false;

	// Check if plugins flag is set or if we want to skip generation
	if (argc > 1)  // First is application name
	{
		for (int i = 1; i < argc; i++)
		{
			if (argv[i] == "/plugins")
			{
				plugins = true;
			}
			else if (argv[i] == "/clean")
			{
				cleanonly = true;
			}
		}
	}

	// Validate args
	if (path.string().length() == 0)
	{
		return FatalError("Invalid path", 997);
	}

	// Check the directory exists
	if (fs::exists(path))
	{
		// Delete the binaries, intermediate, sln in the project root
		{
			fs::path path_binaries = path.string() + "\\binaries";
			fs::path path_intermediate = path.string() + "\\intermediate";
			if (fs::exists(path_binaries))
			{
				fs::remove_all(path_binaries);
			}
			if (fs::exists(path_intermediate))
			{
				fs::remove_all(path_intermediate);
			}

			for (const auto& entry : fs::directory_iterator(path))
			{
				if (StringEndsWith(entry.path().string(), ".sln"))
				{
					fs::remove_all(entry.path());
					break;
				}
			}
		}

		// Delete the binaries and intermediate for each plugin (if set)
		if (plugins)
		{
			fs::path path_plugins = path.string() + "\\plugins";
			if (fs::exists(path_plugins))
			{
				for (const auto& entry : fs::directory_iterator(path_plugins))
				{
					if(fs::is_directory(entry.path()))
					{
						fs::path path_binaries =  entry.path().string() + "\\binaries";
						fs::path path_intermediate = entry.path().string() + "\\intermediate";
						if (fs::exists(path_binaries))
						{
							fs::remove_all(path_binaries);
						}
						if (fs::exists(path_intermediate))
						{
							fs::remove_all(path_intermediate);
						}
					}
				}
			}
		}
	}
	else
	{
		return FatalError("path does not exist", 995);
	}

	// Generate project files...
	if (!cleanonly)
	{
		for (const auto& entry : fs::directory_iterator(path))
		{
			// Locate the .uproject
			if (StringEndsWith(entry.path().string(), ".uproject"))
			{
				system("setlocal");
				wstring regVal2 = ReadRegValue(HKEY_CLASSES_ROOT, s2ws("Unreal.ProjectFile\\shell\\rungenproj"), s2ws("Icon"));

				string sysCmd = "\"" + ws2s(regVal2);
				sysCmd += " /projectfiles";
				sysCmd += " \"" + entry.path().string() + "\"";
				sysCmd += "\"";

				system(sysCmd.c_str());
			}
		}
	}
}