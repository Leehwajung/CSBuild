#include <windows.h>
#include <stdio.h>
#include <psapi.h>
#include <iostream>
#include <string>
#include <algorithm>
#include <set>
#include <vector>
#include <fstream>
#include "RspParser.h"
#include "GccParser.h"
//#include <thread>
//#include <mutex>	// why, why this ide don't support thread in c++11

using namespace std;

typedef NTSTATUS(NTAPI *_NtQueryInformationProcess)(
	HANDLE ProcessHandle,
	DWORD ProcessInformationClass,
	PVOID ProcessInformation,
	DWORD ProcessInformationLength,
	PDWORD ReturnLength
	);

typedef struct _PROCESS_BASIC_INFORMATION
{
	LONG ExitStatus;
	PVOID PebBaseAddress;
	ULONG_PTR AffinityMask;
	LONG BasePriority;
	ULONG_PTR UniqueProcessId;
	ULONG_PTR ParentProcessId;
} PROCESS_BASIC_INFORMATION, *PPROCESS_BASIC_INFORMATION;


typedef struct _UNICODE_STRING {
	USHORT Length;
	USHORT MaximumLength;
	PWSTR  Buffer;
} UNICODE_STRING;

typedef struct _CURDIR
{
	UNICODE_STRING    DosPath;
	HANDLE            Handle;
} CURDIR;



static set<pair<int, string> > previouses; /* pid X string */
static int NUM_TARGET_FILES = 4;
static string TARGET_FILES[] = { "CL.EXE", "LINK.EXE", "GCC", "G++" };
bool searchProcessesStop;
GccParser gccParser;
RspParser rspParser;


PVOID GetPebAddress(HANDLE ProcessHandle)
{
	_NtQueryInformationProcess NtQueryInformationProcess =
		(_NtQueryInformationProcess)GetProcAddress(
		GetModuleHandleA("ntdll.dll"), "NtQueryInformationProcess");
	PROCESS_BASIC_INFORMATION pbi;

	NtQueryInformationProcess(ProcessHandle, 0, &pbi, sizeof(pbi), NULL);

	return pbi.PebBaseAddress;
}

string ToUpperCase(const string&  str, const std::locale&  loc = std::locale::classic())
{
	string  upperCaseStr = str;
	std::transform(str.begin(), str.end(), upperCaseStr.begin(), toupper);
	return upperCaseStr;
}

bool IsInterestProcess(const string& processName) {
	for (int i = 0; i < NUM_TARGET_FILES; ++i) {
		if (processName.find(TARGET_FILES[i]) != string::npos) {
			return true;
		}
	}
	return false;
}

bool IsGCCProcess(const string& processName) {
	if (processName.find("GCC") != string::npos
		|| processName.find("G++") != string::npos) {
		return true;
	}
	return false;
}

bool IsClexeProcess(const string& processName) {
	if (processName.find("CL.EXE") != string::npos) {
		return true;
	}
	return false;
}

bool IsLinkexeProcess(const string& processName) {
	if (processName.find("LINK.EXE") != string::npos) {
		return true;
	}
	return false;
}

string GetProcessName(DWORD processID)
{
	TCHAR szProcessName[MAX_PATH] = TEXT("<unknown>");

	/* Get a handle to the process. */
	HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION |
		PROCESS_VM_READ,
		FALSE, processID);

	string processName;

	/* Get the process name. */
	if (NULL != hProcess)
	{
		HMODULE hMod;
		DWORD cbNeeded;

		if (EnumProcessModules(hProcess, &hMod, sizeof(hMod),
			&cbNeeded))
		{
			GetModuleBaseName(hProcess, hMod, szProcessName,
				sizeof(szProcessName) / sizeof(TCHAR));
		}

		// get process working directory
		PROCESS_BASIC_INFORMATION pbi;
		DWORD len;

		(hProcess, 0, &pbi, sizeof(PROCESS_BASIC_INFORMATION), &len);

	}

	/* Print the process name and identifier. */
	//printf( TEXT("%s  (PID: %u)\n"), szProcessName, processID );

	/* Release the handle to the process. */
	CloseHandle(hProcess);

	processName = szProcessName;

	return processName;
}

string GetStringContentsByUnicodeString(HANDLE processHandle, UNICODE_STRING unicodeString) {
	WCHAR *wStringContents;
	char * cStringContents;
	string stringContents;

	/* allocate memory to hold the command line */
	wStringContents = (WCHAR *)malloc(unicodeString.Length);
	cStringContents = (char *)malloc(unicodeString.Length * 2);

	/* read the command line */
	if (!ReadProcessMemory(processHandle, unicodeString.Buffer,
		wStringContents, unicodeString.Length, NULL))
	{
		return "";
	}

	/* print it */
	/* the length specifier is in characters, but commandLine.Length is in bytes */
	/* a WCHAR is 2 bytes */
	sprintf(cStringContents, "%.*S", unicodeString.Length / 2, wStringContents);
	stringContents = cStringContents;

	return stringContents;
}

/* contents[0] : commandline, contents[1] : working directory */
vector<string> GetProcessContents(int pid){
	HANDLE processHandle;
	PVOID pebAddress;
	LPVOID rtlUserProcParamsAddress;

	UNICODE_STRING workingDirectory;
	UNICODE_STRING commandLine;

	vector<string> contents;

	if ((processHandle = OpenProcess(
		PROCESS_QUERY_INFORMATION | /* required for NtQueryInformationProcess */
		PROCESS_VM_READ, /* required for ReadProcessMemory */
		FALSE, pid)) == 0)
	{
		printf("Could not open process!\n");
		return vector<string>();
	}

	pebAddress = GetPebAddress(processHandle);

	/* get the address of ProcessParameters */
	if (!ReadProcessMemory(processHandle, (PCHAR)pebAddress + 0x10,
		&rtlUserProcParamsAddress, sizeof(PVOID), NULL))
	{
		printf("Could not read the address of ProcessParameters!\n");
		return vector<string>();
	}
	
	/* read the WorkingDirectory UNICODE_STRING structure */
	if (!ReadProcessMemory(processHandle, (PCHAR)rtlUserProcParamsAddress + 0x24,
		&workingDirectory, sizeof(workingDirectory), NULL))
	{
		return vector<string>();
	}
	
	/* read the CommandLine UNICODE_STRING structure */
	if (!ReadProcessMemory(processHandle, (PCHAR)rtlUserProcParamsAddress + 0x40,
		&commandLine, sizeof(commandLine), NULL))
	{
		return vector<string>();
	}

	/* allocate memory to hold the command line */

	contents.push_back( GetStringContentsByUnicodeString(processHandle, commandLine) );
	contents.push_back( GetStringContentsByUnicodeString(processHandle, workingDirectory) );

	CloseHandle(processHandle);

	return contents;
}


void SetProcessKeyTrue(const int pid, const string& processContents) {
	pair<int, string> key = make_pair(pid, processContents);
	if (previouses.find(key) == previouses.end()){
		previouses.insert(key);
	}
}

bool IsSearched(const int pid, const string& processContents) {
	pair<int, string> key = make_pair(pid, processContents);
	if (previouses.find(key) == previouses.end()) {
		return false;
	}
	return true;
}

extern vector<int> GetAllProcessId(){
	DWORD aProcesses[1024], cbNeeded, cProcesses;

	vector<int> pids;
	if (!EnumProcesses(aProcesses, sizeof(aProcesses), &cbNeeded))
	{
		return pids;
	}

	cProcesses = cbNeeded / sizeof(DWORD);
	for (unsigned i = 0; i < cProcesses; i++)
	{
		pids.push_back(aProcesses[i]);
	}

	return pids;
}

wstring ReadOneLine(FILE *File){
	wchar_t lineOfChars[512];
	wstring line;

	fgetws(lineOfChars, 512, File);
	line.append(lineOfChars);

	return line;
}

string GetRSPFileContents(const string& processContents) {
	int fileFlagIndex = processContents.find('@');
	int endFileIndex = processContents.find(".rsp");
	string fileName = processContents.substr(fileFlagIndex + 1, endFileIndex + 3 - fileFlagIndex); /* from @ to end : filename except '@' itself */
	wstring fileNameUtf8(fileName.begin(), fileName.end());
	
	FILE *file;
	wstring wLineContents;
	string lineContents;

	_wfopen_s(&file, fileNameUtf8.c_str(), L"r,ccs=UTF-16LE");
	while (!feof(file) && !ferror(file)){
		wLineContents.append(ReadOneLine(file));
	}
	fclose(file);

	lineContents = string(wLineContents.begin(), wLineContents.end());

	return lineContents;
}

DWORD WINAPI SearchProcessThread(LPVOID lpParam) {
	vector<int> processIDs;
	unsigned int i;

	string processName;
	vector<string> processContents; /* contents[0] : commandline, contents[1] : working directory */
	string rspFileContents;

	while (!searchProcessesStop) {
		processIDs = GetAllProcessId();

		/* Print the name and process identifier for each process. */
		for ( i = 0; i < processIDs.size(); ++i)
		{
			processName = ToUpperCase(GetProcessName(processIDs[i]));

			if ( IsInterestProcess(processName) ) {
				/* contents[0] : commandline, contents[1] : working directory */
				processContents = GetProcessContents(processIDs[i]);

				if ( !IsSearched(processIDs[i], processContents[0]) ) {
					if ( IsGCCProcess(processName) ) {
					//	cout << processIDs[i] << " " << processContents[1] << endl;
					//	cout << processContents[0] << endl;

						/* gcc parsing part start */
						gccParser.setBuildLocation(processContents[1]);
						gccParser.addCompileInfo(processContents[0]);
						/* gcc parsing part end */
					}

					if ( IsClexeProcess(processName) ) {
						//cout << processIDs[i] << " " << processContents[1] << endl;	//working directory
						//cout << processContents[0] << endl;	//rsp file address : @...

						rspFileContents = GetRSPFileContents(processContents[0]);
						//cout << processIDs[i] << " " << rspFileContents << endl;	//rsp file content

						/* parsing codes here; should be function call(or class member function call) */
						rspParser.parseClexe(processContents[1], processContents[0], rspFileContents);

						//ofstream fout("output.out", ios::app);
						//fout << rspFileContents << endl; // don't work
					}

					if ( IsLinkexeProcess(processName) ) {
						//cout << processIDs[i] << " " << processContents[1] << endl;	//working directory
						//cout << processContents[0] << endl;	//rsp file address : @...

						rspFileContents = GetRSPFileContents(processContents[0]);
						//cout << processIDs[i] << " " << rspFileContents << endl;	//rsp file content

						/* parsing codes here; should be function call(or class member function call) */
						rspParser.parseLinkexe(processContents[1], rspFileContents);

						//ofstream fout("output.out", ios::app);
						//fout << rspFileContents << endl; // don't work
					}
					SetProcessKeyTrue(processIDs[i], processContents[0]);
				}
			}
		}
	}
	return 0;
}

void StartCompileProcess(string& processName, PROCESS_INFORMATION& pi) {
	STARTUPINFO si = { 0, };
	si.cb = sizeof(si);
	CreateProcess(
		NULL,
		&processName[0],	//argv[1...]
		NULL, NULL, TRUE, 0, NULL, NULL,
		&si, &pi
		);
}

int wmain(int argc, WCHAR *argv[])
{
	searchProcessesStop = false;
	SearchProcessThread(NULL);

	//rspParser.printResult();

	//if (argc < 2) {
	//	cout << "usage : " << endl;
	//	return -1;
	//}

	//searchProcessesStop = false;

	//// make thread
	////thread toThread(&SearchProcessThread);
	////toThread.detach();
	//DWORD searchThreadID = 0;
	//HANDLE searchThreadHandle = CreateThread(NULL, 0, SearchProcessThread, NULL, 0, &searchThreadID);
	//if (searchThreadHandle == NULL) {
	//	cout << "CreateThread() failed, error : " << GetLastError() << endl;
	//}
	//if (CloseHandle(searchThreadHandle) == 0) {
	//	cout << "Handle to thread close error : " << GetLastError() << endl;
	//}

	//// argv[1] parse
	//wstring ws(argv[1]);
	//string tmp(ws.begin(), ws.end());
	//string processName = ToUpperCase(tmp);

	//if (IsInterestProcessByName(processName)) {
	//	for (int i = 2; i < argc; i++) {
	//		wstring ws(argv[i]);
	//		string test(ws.begin(), ws.end());
	//		processName.append(" \"");
	//		processName.append(test);
	//		processName.append("\"");
	//	}
	//	PROCESS_INFORMATION pi;

	//	StartCompileProcess(processName, pi);
	//	WaitForSingleObject(pi.hProcess, INFINITE);
	//	searchProcessesStop = true;
	//}
	////make process argv[1]
			
}
