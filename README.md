# PoolSprayer


**PoolSprayer** is a basic library to spray the **Windows Kernel Pool**. 
The code currently handles the spraying of **NonPagedPool** (NonPagedPoolNx  for windows >=8 ) and **PagedPool**, but might support other types of pool.

The method used to spray the NonPagedPool are explained in details in this paper:
http://trackwatch.com/windows-kernel-pool-spraying/

The method used to spray the PagedPool is a bit different since there is by default severals pools used as PagedPools, making the previous methods obsolete.
For the PagedPool, when searching for a correct gap, we use the address to retrieves the handles instead of using the handles to retrieve the addresses. This method could be applied to NonPagedPool but the current method is faster, so I kept it for the NonPagedPool.

# How to use 

You can find some code snippets in ``example/``.
Also, there is one default spraying for each pool type.

Quick notes on the objects:
The first thing to do before spraying is to chose an object. You have to chose the object following those criterias:
- The PoolType where the object is allocated
- The size of the chunk allocated when creating the object

For example, the ``IoCompleReservedObject``  is allocated in the **NonPagedPool** (NonPagedPoolNx for windows >=8) and the size of its chunk is ``0xC0``. 
The ``RegistryKey`` is allocated in the **PagedPool** and the size of its chunk is ``0x100``.

You can easyly retrieve these informations by allocating an object and debugging your kernel.
The size of the object is really important because it's directly related to the size of the holes you're going to create.
With an object of size ``0x100``, you can create holes of ``0x100``, ``0x200``, ``0x300``, ``0x400``...

Once you chose your object, you can just allocate a sprayer and start having fun !

## Quick note on the weird offset

If you read the source code you're going to see multiple variables / macros about a weird offset.

This offset is the offset between the object's address leaked by the function NtQuerySystemInformation() and the address of the beginning of the actual chunk containing the object.
I don't know why but this offset is changing between objects, so I call it weird.
For example, for most object, this offset is ALWAYS ``0x60``, but for the Registry Key object (Allocated by RegOpenKeyEx()) this offset is ``0x70``.

So when spraying, I suggest you to let this offset at ``0x60`` by default, but if the spraying doesn't work (the function PlanHole() isn't finding any valid hole), you should check the offset for the object you chose.

## Thanks

Those researchs and library were done during my internship at Armature Technologies.
