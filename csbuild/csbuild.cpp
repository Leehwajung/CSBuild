#include <windows.h>
#include <stdio.h>
#include <psapi.h>
#include <iostream>
#include <string>
#include <algorithm>
#include <set>
#include <vector>
#include <fstream>
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

typedef struct _UNICODE_STRING
{
	USHORT Length;
	USHORT MaximumLength;
	PWSTR Buffer;
} UNICODE_STRING, *PUNICODE_STRING;

typedef struct _PROCESS_BASIC_INFORMATION
{
	LONG ExitStatus;
	PVOID PebBaseAddress;
	ULONG_PTR AffinityMask;
	LONG BasePriority;
	ULONG_PTR UniqueProcessId;
	ULONG_PTR ParentProcessId;
} PROCESS_BASIC_INFORMATION, *PPROCESS_BASIC_INFORMATION;


static set<pair<int, string> > previouses; /* pid X string */
static int NUM_TARGET_FILES = 4;
static string TARGET_FILES[] = { "CL.EXE", "LINK.EXE", "GCC", "G++" };
bool searchProcessesStop;


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

bool IsVisualStudioProcess(const string& processName) {
	if (processName.find("CL.EXE") != string::npos
		|| processName.find("LINK.EXE") != string::npos) {
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
	}

	/* Print the process name and identifier. */
	//printf( TEXT("%s  (PID: %u)\n"), szProcessName, processID );

	/* Release the handle to the process. */
	CloseHandle(hProcess);

	processName = szProcessName;

	return processName;
}

string GetCommandLine(int pid){
	HANDLE processHandle;
	PVOID pebAddress;
	PVOID rtlUserProcParamsAddress;
	UNICODE_STRING commandLine;
	WCHAR *commandLineContents;
	char * rcommandLineContents;
	string strCommandLineContents;

	if ((processHandle = OpenProcess(
		PROCESS_QUERY_INFORMATION | /* required for NtQueryInformationProcess */
		PROCESS_VM_READ, /* required for ReadProcessMemory */
		FALSE, pid)) == 0)
	{
		printf("Could not open process!\n");
		return "";
	}

	pebAddress = GetPebAddress(processHandle);

	/* get the address of ProcessParameters */
	if (!ReadProcessMemory(processHandle, (PCHAR)pebAddress + 0x10,
		&rtlUserProcParamsAddress, sizeof(PVOID), NULL))
	{
		printf("Could not read the address of ProcessParameters!\n");
		return "";
	}

	/* read the CommandLine UNICODE_STRING structure */
	if (!ReadProcessMemory(processHandle, (PCHAR)rtlUserProcParamsAddress + 0x40,
		&commandLine, sizeof(commandLine), NULL))
	{
		return "";
	}

	/* allocate memory to hold the command line */
	commandLineContents = (WCHAR *)malloc(commandLine.Length);
	rcommandLineContents = (char *)malloc(commandLine.Length * 2);

	/* read the command line */
	if (!ReadProcessMemory(processHandle, commandLine.Buffer,
		commandLineContents, commandLine.Length, NULL))
	{
		return "";
	}

	/* print it */
	/* the length specifier is in characters, but commandLine.Length is in bytes */
	/* a WCHAR is 2 bytes */
	sprintf(rcommandLineContents, "%.*S", commandLine.Length / 2, commandLineContents);
	CloseHandle(processHandle);

	strCommandLineContents = rcommandLineContents;

	free(commandLineContents);
	free(rcommandLineContents);

	return strCommandLineContents;
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
	string fileName = processContents.substr(fileFlagIndex + 1); /* from @ to end : filename except '@' itself */
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
	string processContents;
	string rspFileContents;

	while (!searchProcessesStop) {
		processIDs = GetAllProcessId();

		/* Print the name and process identifier for each process. */
		for ( i = 0; i < processIDs.size(); ++i)
		{
			processName = ToUpperCase(GetProcessName(processIDs[i]));

			if ( IsInterestProcess(processName) ) {
				processContents = GetCommandLine(processIDs[i]);
				if ( !IsSearched(processIDs[i], processContents) ) {
					
					if ( IsGCCProcess(processName) ) {
						cout << processIDs[i] << " " << processContents << endl;

						/* parsing codes here; should be function call(or class member function call) */
					}

					if ( IsVisualStudioProcess(processName) ) {
						rspFileContents = GetRSPFileContents(processContents);
						cout << processIDs[i] << " " << rspFileContents << endl;

						/* parsing codes here; should be function call(or class member function call) */

						ofstream fout("output.out", ios::app);
						fout << rspFileContents << endl; // don't work
					}
					SetProcessKeyTrue(processIDs[i], processContents);
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
	while (true) {
		SearchProcessThread(NULL);
	}

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