#include "stdafx.h"
#include "PoolSprayer.h"



NonPagedPoolSprayer::NonPagedPoolSprayer() : PoolSprayer()
{

}

NonPagedPoolSprayer::NonPagedPoolSprayer(UINT nObjectSize, UINT nMaxObject, Allocator_t pAllocator, UINT SomeWeirdOffset) : PoolSprayer(nObjectSize, nMaxObject, pAllocator, SomeWeirdOffset)
{

}

NonPagedPoolSprayer::~NonPagedPoolSprayer()
{
	if (m_hObjectTable != NULL)
		freePool();
}