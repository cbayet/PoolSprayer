#include <Windows.h>
#include <SubAuth.h>
#include <stdio.h>
#include <exception>
#include "..\PoolSprayerLib\PoolSprayer.h"



HANDLE AllocatorNonPagedPool()
{
	HANDLE file = INVALID_HANDLE_VALUE;

	file = CreateFile(L"C:\\Users\\Public\\random", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (file == INVALID_HANDLE_VALUE)
	{
		throw PoolSprayerException("Failed to open file !\n");
	}
	return file;
}

int main(void)
{
	NonPagedPoolSprayer *pNonPagedPoolSprayer;
	HOLE			FirstHole;
	HOLE			SecondHole;

	try
	{
		pNonPagedPoolSprayer = new NonPagedPoolSprayer(0x190, DEFAULT_MAX_OBJECTS, &AllocatorNonPagedPool);

		// Allocate massively the object
		printf("[+]Spraying Pool...\n");
		pNonPagedPoolSprayer->sprayPool();

		// searching for a valid hole. The size of the equal will be equal to 3 * 0x190 = 0x4B0
		printf("[+]Planning first hole...\n");
		FirstHole = pNonPagedPoolSprayer->planHole(3);

		// size: 4 * 0x190 = 0x640 !
		printf("[+]Planning second hole...\n");
		SecondHole = pNonPagedPoolSprayer->planHole(4);

		printf("[+]First hole is precisely at address 0x%p\n", FirstHole.pBaseAddress);
		printf("[+]Second hole is precisely at address 0x%p\n", SecondHole.pBaseAddress);

		printf("[+]Creating Holes...\n");

		// The holes are created now.. Use them fast before something else does !
		pNonPagedPoolSprayer->createHole(FirstHole);
		pNonPagedPoolSprayer->createHole(SecondHole);

		printf("[+]Holes created ! You can check if you want...\n Press any key to continue...\n");
		getc(stdin);

		printf("[+]Freeing chunks...\n");
		pNonPagedPoolSprayer->freePool();
	}
	catch (PoolSprayerException &e)
	{
		printf("[-]Error occured ! %s\n", e.getMessage());
		exit(1);
	}
}