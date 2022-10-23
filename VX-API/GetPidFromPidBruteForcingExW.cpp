#include "Win32Helper.h"

DWORD GetPidFromPidBruteForcingExW(_In_ PWCHAR ProcessNameWithExtension)
{
	UNICODE_STRING NtfsRoot = { 0 };
	IO_STATUS_BLOCK IoBlock = { 0 };
	HANDLE hDevice = NULL;
	OBJECT_ATTRIBUTES Attributes = { 0 };
	HMODULE hModule = NULL, hShlwapi = NULL;
	PFILE_PROCESS_IDS_USING_FILE_INFORMATION ProcessIdArray = NULL;
	NTSTATUS Status = STATUS_SUCCESS;
	DWORD ProcessId = ERROR_SUCCESS, dwProcessInformationListArrayLength = 16384;

	NTCREATEFILE NtCreateFile = NULL;
	NTCLOSE NtClose = NULL;
	NTQUERYINFORMATIONFILE NtQueryInformationFile = NULL;
	NTQUERYSYSTEMINFORMATION NtQuerySystemInformation = NULL;
	PATHSTRIPPATHW PathStripPathW = NULL;

	hModule = GetModuleHandleEx2W(L"ntdll.dll");
	hShlwapi = TryLoadDllMultiMethodW((PWCHAR)L"shlwapi.dll");

	if (!hModule || !hShlwapi)
		return 0;

	NtCreateFile = (NTCREATEFILE)GetProcAddressA((DWORD64)hModule, "NtCreateFile");
	NtClose = (NTCLOSE)GetProcAddressA((DWORD64)hModule, "NtClose");
	NtQueryInformationFile = (NTQUERYINFORMATIONFILE)GetProcAddressA((DWORD64)hModule, "NtQueryInformationFile");
	NtQuerySystemInformation = (NTQUERYSYSTEMINFORMATION)GetProcAddressA((DWORD64)hModule, "NtQuerySystemInformation");
	PathStripPathW = (PATHSTRIPPATHW)GetProcAddressA((DWORD64)hShlwapi, "PathStripPathW");
	if (!NtCreateFile || !NtClose || !NtQueryInformationFile || !NtQuerySystemInformation || !PathStripPathW)
		return 0;

	RtlInitUnicodeString(&NtfsRoot, L"\\NTFS\\");

	InitializeObjectAttributes(&Attributes, &NtfsRoot, OBJ_CASE_INSENSITIVE, NULL, NULL);

	Status = NtCreateFile(&hDevice, GENERIC_READ | SYNCHRONIZE, &Attributes, &IoBlock, NULL, FILE_ATTRIBUTE_NORMAL, FILE_SHARE_READ, FILE_OPEN, 0, NULL, 0);
	if (!NT_SUCCESS(Status))
		goto EXIT_ROUTINE;

	ProcessIdArray = (PFILE_PROCESS_IDS_USING_FILE_INFORMATION)HeapAlloc(GetProcessHeapFromTeb(), HEAP_ZERO_MEMORY, dwProcessInformationListArrayLength);
	if (ProcessIdArray == NULL)
		goto EXIT_ROUTINE;

	ZeroMemory(&IoBlock, sizeof(IO_STATUS_BLOCK));

	Status = NtQueryInformationFile(hDevice, &IoBlock, ProcessIdArray, dwProcessInformationListArrayLength, FileProcessIdsUsingFileInformation);
	if (!NT_SUCCESS(Status))
		goto EXIT_ROUTINE;

	for (DWORD dwX = 0; dwX < ProcessIdArray->NumberOfProcessIdsInList; dwX++)
	{
		WCHAR ImageName[MAX_PATH * sizeof(WCHAR)] = { 0 };
		SYSTEM_PROCESS_IMAGE_NAME_INFORMATION SystemProcessInformation = { 0 };

		if (ProcessId != ERROR_SUCCESS)
			break;

#pragma warning( push )
#pragma warning( disable : 4244)
		SystemProcessInformation.ProcessId = LongToHandle(ProcessIdArray->ProcessIdList[dwX]);
#pragma warning( pop ) 
		SystemProcessInformation.ImageName.Buffer = ImageName;
		SystemProcessInformation.ImageName.Length = 0;
		SystemProcessInformation.ImageName.MaximumLength = sizeof(ImageName);

		Status = NtQuerySystemInformation(SystemProcessIdInformation, &SystemProcessInformation, sizeof(SystemProcessInformation), NULL);
		if (!NT_SUCCESS(Status))
			continue;

		if (SystemProcessInformation.ImageName.Buffer == NULL)
			continue;

		PathStripPathW(SystemProcessInformation.ImageName.Buffer);

#pragma warning( push )
#pragma warning( disable : 4244)
		if (StringCompareW(ProcessNameWithExtension, SystemProcessInformation.ImageName.Buffer) == ERROR_SUCCESS)
			ProcessId = ProcessIdArray->ProcessIdList[dwX];
#pragma warning( pop ) 
	}

EXIT_ROUTINE:

	if (ProcessIdArray)
		HeapFree(GetProcessHeapFromTeb(), HEAP_ZERO_MEMORY, ProcessIdArray);

	if (hDevice)
		NtClose(hDevice);

	if (hShlwapi)
		FreeLibrary(hShlwapi);

	return ProcessId;
}