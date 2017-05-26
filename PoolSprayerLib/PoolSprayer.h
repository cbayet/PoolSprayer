#pragma once

#include <Windows.h>
#include <SubAuth.h>
#include <stdio.h>
#include <exception>

#define IOCO 1
#define IOCO_SIZE 0xc0

#define DEFAULT_MAX_OBJECTS 100000

#define STATUS_INFO_LENGTH_MISMATCH 0xc0000004


#define SOME_HARDCODED_WEIRD_OFFSET 0x60

typedef enum _SYSTEM_INFORMATION_CLASS {
	SystemBasicInformation = 0,
	SystemPerformanceInformation = 2,
	SystemTimeOfDayInformation = 3,
	SystemProcessInformation = 5,
	SystemProcessorPerformanceInformation = 8,
	SystemModuleInformation = 11,
	SystemObjectInformation = 17,
	SystemInterruptInformation = 23,
	SystemExceptionInformation = 33,
	SystemRegistryQuotaInformation = 37,
	SystemLookasideInformation = 45,
	SystemExtendedHandleInformation = 64
} SYSTEM_INFORMATION_CLASS;

typedef struct _OBJECT_ATTRIBUTES {
	DWORD64           Length;
	HANDLE          RootDirectory;
	PUNICODE_STRING ObjectName;
	DWORD64           Attributes;
	PVOID           SecurityDescriptor;
	PVOID           SecurityQualityOfService;
}  OBJECT_ATTRIBUTES, *POBJECT_ATTRIBUTES;

typedef struct _SYSTEM_HANDLE_TABLE_ENTRY_INFO_EX
{
	PVOID Object;
	ULONG_PTR UniqueProcessId;
	HANDLE HandleValue;
	ULONG GrantedAccess;
	USHORT CreatorBackTraceIndex;
	USHORT ObjectTypeIndex;
	ULONG HandleAttributes;
	ULONG Reserved;
} SYSTEM_HANDLE_TABLE_ENTRY_INFO_EX, *PSYSTEM_HANDLE_TABLE_ENTRY_INFO_EX;

typedef struct _SYSTEM_EXTENDED_HANDLE_INFORMATION
{
	ULONG_PTR NumberOfHandles;
	ULONG_PTR Reserved;
	SYSTEM_HANDLE_TABLE_ENTRY_INFO_EX Handles[1];
} SYSTEM_EXTENDED_HANDLE_INFORMATION, *PSYSTEM_EXTENDED_HANDLE_INFORMATION;

typedef struct _HOLE {
	UINT		nHoleSize;
	HANDLE *	pHandlesToClose;
	PVOID		pBaseAddress;
	UINT		nBaseIndexInOriginalSpray;
} HOLE, *PHOLE;

typedef NTSTATUS(__stdcall *NtAllocateReserveObject_t) (OUT PHANDLE hObject, IN POBJECT_ATTRIBUTES ObjectAttributes, IN DWORD ObjectType);
typedef NTSTATUS(__stdcall *NtQuerySystemInformation_t)(IN SYSTEM_INFORMATION_CLASS SystemInformatioNClass, IN OUT PVOID SystemInformation, IN ULONG SystemInformationLength, OUT PULONG ReturnLength OPTIONAL);

typedef HANDLE(*Allocator_t)();

class PoolSprayer
{

protected:
	UINT		m_nSingleObjectSize;
	UINT		m_nMaxObjects;
	UINT		m_nStartIndex;
	HANDLE *	m_hObjectTable;
	UINT		m_nSomeWeirdOffset;

	
	NtQuerySystemInformation_t	m_pNtQuerySystemInformation;
	Allocator_t					m_pAllocator;

	void Init(UINT nObjectSize, UINT nMaxObjects, Allocator_t pAllocator, UINT SomeWeirdOffset);

public:
	
	PoolSprayer();
	PoolSprayer(UINT nObjectSize, UINT nMaxObject, Allocator_t pAllocator, UINT SomeWeirdOffset = SOME_HARDCODED_WEIRD_OFFSET);
	~PoolSprayer();

	void	sprayPool();
	void	freePool();
	HOLE	planHole(UINT nHoleSize);
	void	createHole(HOLE hole);

	DWORD64		getObjectAddressWithHandle(HANDLE hObjectHandle, UINT nMaxSearchTry = 50);
	HANDLE		getObjectHandleWithAddress(DWORD64 pAddress, UINT nMaxSearchTry = 50);

	UINT		getSingleObjectSize();
	UINT		getMaxObjects();
	UINT		getStartIndex();
	HANDLE *	getObjectTable();

};

class NonPagedPoolSprayer : public PoolSprayer {

public:
	NonPagedPoolSprayer();
	NonPagedPoolSprayer(UINT nObjectSize, UINT nMaxObject, Allocator_t pAllocator, UINT SomeWeirdOffset = SOME_HARDCODED_WEIRD_OFFSET);
	~NonPagedPoolSprayer();

};


class PagedPoolSprayer : public PoolSprayer {

	public:
		PagedPoolSprayer();
		PagedPoolSprayer(UINT nObjectSize, UINT nMaxObject, Allocator_t pAllocator, UINT SomeWeirdOffset = SOME_HARDCODED_WEIRD_OFFSET);
		~PagedPoolSprayer();

		HOLE	planHole(UINT nHoleSize);

};


class PoolSprayerException {

public:
	PoolSprayerException(const char * msg) : msg_(msg) {}
	~PoolSprayerException() {}

	const char * getMessage() const { return(msg_); }
private:
	const char * msg_;
};

int findHandleIndex(HANDLE handle, HANDLE * hObjectTable, UINT nMaxObjects);

void closeBunchOfHandles(HANDLE * hObjectTable, UINT startIndex, UINT nMaxIndex, int nNumberOfHandlesToClose, bool avoidCoalescing);