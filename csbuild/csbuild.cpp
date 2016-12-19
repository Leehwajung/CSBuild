#include <windows.h>
#include <stdio.h>
#include <psapi.h>
#include <iostream>
#include <string>
#include <algorithm>
#include <set>
#include <vector>

using namespace std;
 
typedef NTSTATUS (NTAPI *_NtQueryInformationProcess)(
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

bool IsInterestProcess( DWORD processID )
{
    TCHAR szProcessName[MAX_PATH] = TEXT("<unknown>");

    // Get a handle to the process.
    HANDLE hProcess = OpenProcess( PROCESS_QUERY_INFORMATION |
                                   PROCESS_VM_READ,
                                   FALSE, processID );

    // Get the process name.
    if (NULL != hProcess )
    {
        HMODULE hMod;
        DWORD cbNeeded;

        if ( EnumProcessModules( hProcess, &hMod, sizeof(hMod), 
             &cbNeeded) )
        {
            GetModuleBaseName( hProcess, hMod, szProcessName, 
                               sizeof(szProcessName)/sizeof(TCHAR) );
        }
    }

    // Print the process name and identifier.
    //printf( TEXT("%s  (PID: %u)\n"), szProcessName, processID );

    // Release the handle to the process.
    CloseHandle( hProcess );
    
	string binaryName(szProcessName);
	string name = ToUpperCase(binaryName);
    if(name.find("CL.EXE") != string::npos 
	|| name.find("LINK.EXE") != string::npos
	|| name.find("GCC.EXE") != string::npos
	|| name.find("G++.EXE") != string::npos
	){
		return true;
	}
	return false;
}

static set<pair<int, string> > previouses; // pid X string

string GetCommandLine(int pid){
	HANDLE processHandle;
	PVOID pebAddress;
	PVOID rtlUserProcParamsAddress;
	UNICODE_STRING commandLine;
	WCHAR *commandLineContents;

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
	char * rcommandLineContents = (char *)malloc(commandLine.Length * 2);

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
	string xx = rcommandLineContents;

	pair<int, string> key = make_pair(pid, xx);
	if(previouses.find(key) == previouses.end()){
		previouses.insert(key);
	}

	free(commandLineContents);
	free(rcommandLineContents);
	return xx;
}

extern vector<int> GetAllProcessId(){
	DWORD aProcesses[1024], cbNeeded, cProcesses;
	
	vector<int> v;
	if ( !EnumProcesses( aProcesses, sizeof(aProcesses), &cbNeeded ) )
	{
		return v;
	}

	cProcesses = cbNeeded / sizeof(DWORD);
	for (unsigned i = 0; i < cProcesses; i++ )
	{
		v.push_back(aProcesses[i]);
	}
	
	return v;
}

int Do(){
	int pid;
	

	DWORD aProcesses[1024], cbNeeded, cProcesses;
	unsigned int i;

	if ( !EnumProcesses( aProcesses, sizeof(aProcesses), &cbNeeded ) )
	{
		return 1;
	}


	// Calculate how many process identifiers were returned.
	cProcesses = cbNeeded / sizeof(DWORD);

	// Print the name and process identifier for each process.

	for ( i = 0; i < cProcesses; i++ )
	{
		if( aProcesses[i] != 0 && IsInterestProcess(aProcesses[i]) )
		{
			pid = aProcesses[i];
			string x = GetCommandLine(pid);
			cout << x << endl;
		}
	}

	return 0;
}

int wmain(int argc, WCHAR *argv[])
{
	while(1){
		Do();
	}
}