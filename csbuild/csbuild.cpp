#include <windows.h>
#include <stdio.h>
#include <psapi.h>
#include <iostream>
#include <string>
#include <algorithm>
#include <set>
#include <vector>
#include <fstream>
#include <sstream>
#include <codecvt>
#include <locale>
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
static int NUM_TARGET_FILES = 5;
static string TARGET_FILES[] = { "CL.EXE", "LINK.EXE", "GCC", "G++", "MAKE" };
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

bool IsInterestProcessByName(const string& processName) {
	for (int i = 0; i < NUM_TARGET_FILES; ++i) {
		if (processName.find(TARGET_FILES[i]) != string::npos) {
			return true;
		}
	}
	return false;
}

bool IsInterestProcess(DWORD processID)
{
	TCHAR szProcessName[MAX_PATH] = TEXT("<unknown>");

	/* Get a handle to the process. */
	HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION |
		PROCESS_VM_READ,
		FALSE, processID);

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

	string binaryName(szProcessName);
	string processName = ToUpperCase(binaryName);

	return IsInterestProcessByName(processName);

	/*
	if(name.find("CL.EXE") != string::npos
	|| name.find("LINK.EXE") != string::npos
	|| name.find("GCC.EXE") != string::npos
	|| name.find("G++.EXE") != string::npos
	){
	return true;
	}*/
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

	vector<int> v;
	if (!EnumProcesses(aProcesses, sizeof(aProcesses), &cbNeeded))
	{
		return v;
	}

	cProcesses = cbNeeded / sizeof(DWORD);
	for (unsigned i = 0; i < cProcesses; i++)
	{
		v.push_back(aProcesses[i]);
	}

	return v;
}

string ToUtf8(wstring str)
{
	std::string ret;
	int len = WideCharToMultiByte(CP_UTF8, 0, str.c_str(), str.length(), NULL, 0, NULL, NULL);
	if (len > 0)
	{
		ret.resize(len);
		WideCharToMultiByte(CP_UTF8, 0, str.c_str(), str.length(), &ret[0], len, NULL, NULL);
	}
	return ret;
}

wstring ReadOneLine(FILE *File, wstring Line){

	wchar_t LineOfChars[512];
	fgetws(LineOfChars, 512, File);

	Line.clear();
	Line.append(LineOfChars);

	return Line;
}

DWORD WINAPI SearchProcessThread(LPVOID lpParam) {
	while (!searchProcessesStop) {
		int pid;

		DWORD aProcesses[1024], cbNeeded, cProcesses;
		unsigned int i;
		string processContents;

		if (!EnumProcesses(aProcesses, sizeof(aProcesses), &cbNeeded))
		{
			continue;
		}

		/* Calculate how many process identifiers were returned. */
		cProcesses = cbNeeded / sizeof(DWORD);

		/* Print the name and process identifier for each process. */
		for (i = 0; i < cProcesses; i++)
		{
			if (aProcesses[i] != 0 && IsInterestProcess(aProcesses[i]))
			{
				pid = aProcesses[i];
				processContents = GetCommandLine(pid);
				if (!IsSearched(pid, processContents)) {
					
					cout << i << " " << processContents << endl;
				/*	int index = processContents.find('@');
					string fileName = processContents.substr(index + 1);

					wstring fileNameUtf8(fileName.begin(), fileName.end());

					FILE *file;
					wstring line;

					ofstream fout("output.out", ios::app);

					_wfopen_s(&file, fileNameUtf8.c_str(), L"r,ccs=UTF-16LE");

					while (!feof(file) && !ferror(file)){
						line = ReadOneLine(file, line);
						wcout << line;
						string linew(line.begin(), line.end());

						fout << linew;
					}

					fclose(file);*/

					//ifstream in(fileName);
					//in.imbue(locale(locale::empty(), new codecvt_utf8<wchar_t>));

					//stringstream ss;
					//ss << in.rdbuf();
					//
					//string utf8FileName = ss.str();
			/*		wstring fileNameUtf8(fileName.begin(), fileName.end());

					wifstream fin(fileNameUtf8);
					wstring buff;
					wofstream fout("output.out", ios::app);

					while ( getline(fin, buff) ) {

						wcout << buff;
						fout << buff;
					}
					fout << endl;
					wcout << endl;
				*/	
					SetProcessKeyTrue(pid, processContents);
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
//	std::locale::global(std::locale("ko_KR.UTF-8"));

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