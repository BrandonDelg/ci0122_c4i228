

// addrspace.cc 
//	Routines to manage address spaces (executing user programs).
//
//	In order to run a user program, you must:
//
//	1. link with the -N -T 0 option 
//	2. run coff2noff to convert the object file to Nachos format
//		(Nachos object code format is essentially just a simpler
//		version of the UNIX executable object code format)
//	3. load the NOFF file into the Nachos file system
//		(if you haven't implemented the file system yet, you
//		don't need to do this last step)
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "system.h"
#include "addrspace.h"
//#include "noff.h"

#ifdef VM
static AddrSpace *frameOwnerSpace[NumPhysPages];
static int frameOwnerVPN[NumPhysPages];
static int nextVictimFrame = 0;
static int lruClock = 0;
static const int SwapPages = 128;
static const int SwapSize = SwapPages * PageSize;
static BitMap *swapMap = NULL;
static int swapFileCounter = 0;

static void InitVMStructures()
{
    static bool initialized = false;

    if (!initialized) {
        for (int i = 0; i < NumPhysPages; i++) {
            frameOwnerSpace[i] = NULL;
            frameOwnerVPN[i] = -1;
        }

        swapMap = new BitMap(SwapPages);
        initialized = true;
    }
}

static void InvalidateTLBEntry(int vpn)
{
#ifdef USE_TLB
    for (int i = 0; i < TLBSize; i++) {
        if (machine->tlb[i].valid &&
            machine->tlb[i].virtualPage == vpn) {
            machine->tlb[i].valid = false;
        }
    }
#endif
}
#endif

bool AddrSpace::IsValid()
{
    return pageTable != NULL && numPages > 0;
}
//----------------------------------------------------------------------
// SwapHeader
// 	Do little endian to big endian conversion on the bytes in the 
//	object file header, in case the file was generated on a little
//	endian machine, and we're now running on a big endian machine.
//----------------------------------------------------------------------

static void  SwapHeader (NoffHeader *noffH)
{
	noffH->noffMagic = WordToHost(noffH->noffMagic);
	noffH->code.size = WordToHost(noffH->code.size);
	noffH->code.virtualAddr = WordToHost(noffH->code.virtualAddr);
	noffH->code.inFileAddr = WordToHost(noffH->code.inFileAddr);
	noffH->initData.size = WordToHost(noffH->initData.size);
	noffH->initData.virtualAddr = WordToHost(noffH->initData.virtualAddr);
	noffH->initData.inFileAddr = WordToHost(noffH->initData.inFileAddr);
	noffH->uninitData.size = WordToHost(noffH->uninitData.size);
	noffH->uninitData.virtualAddr = WordToHost(noffH->uninitData.virtualAddr);
	noffH->uninitData.inFileAddr = WordToHost(noffH->uninitData.inFileAddr);
}



void
AddrSpace::LoadSegment(OpenFile *executable, int virtualAddr, int size, int inFileAddr)
{
    int remaining = size;
    int offset = 0;

    while (remaining > 0) {
        int vaddr = virtualAddr + offset;
        int vpn = vaddr / PageSize;
        int pageOffset = vaddr % PageSize;
        int ppn = pageTable[vpn].physicalPage;

        int bytes = PageSize - pageOffset;
        if (bytes > remaining) bytes = remaining;

        int physAddr = ppn * PageSize + pageOffset;

        executable->ReadAt(&(machine->mainMemory[physAddr]),
                           bytes,
                           inFileAddr + offset);

        remaining -= bytes;
        offset += bytes;
    }
}
//----------------------------------------------------------------------
// AddrSpace::AddrSpace
// 	Create an address space to run a user program.
//	Load the program from a file "executable", and set everything
//	up so that we can start executing user instructions.
//
//	Assumes that the object code file is in NOFF format.
//
//	First, set up the translation from program memory to physical 
//	memory.  For now, this is really simple (1:1), since we are
//	only uniprogramming, and we have a single unsegmented page table
//
//	"executable" is the file containing the object code to load into memory
//----------------------------------------------------------------------
AddrSpace::AddrSpace(OpenFile *executable)
{
    unsigned int i, size;

    openFilesTable = new NachosOpenFilesTable();
    openFilesTable->addThread();

    executableFile = executable;

#ifdef VM
    InitVMStructures();

    char swapName[32];
    sprintf(swapName, "SWAP.%d", swapFileCounter++);

    fileSystem->Create(swapName, SwapSize);
    swapFile = fileSystem->Open(swapName);
#endif

    executable->ReadAt((char *)&noffHeader, sizeof(noffHeader), 0);

    if ((noffHeader.noffMagic != NOFFMAGIC) &&
        (WordToHost(noffHeader.noffMagic) == NOFFMAGIC)) {
        SwapHeader(&noffHeader);
    }

    ASSERT(noffHeader.noffMagic == NOFFMAGIC);

    size = noffHeader.code.size +
           noffHeader.initData.size +
           noffHeader.uninitData.size +
           UserStackSize;

    numPages = divRoundUp(size, PageSize);
    size = numPages * PageSize;

#ifdef VM
    swapPage = new int[numPages];
    inSwap = new bool[numPages];
    lastUse = new int[numPages];
    for (unsigned int j = 0; j < numPages; j++) {
        swapPage[j] = -1;
        inSwap[j] = false;
        lastUse[j] = 0;
    }
#endif

    stackPages = divRoundUp(UserStackSize, PageSize);
    stackStartPage = numPages - stackPages;

    memoryUsers = new int;
    *memoryUsers = 1;

    DEBUG('a', "Initializing address space, num pages %d, size %d\n",
          numPages, size);

    pageTable = new TranslationEntry[numPages];

    for (i = 0; i < numPages; i++) {
        pageTable[i].virtualPage = i;
        pageTable[i].physicalPage = -1;

#ifdef VM
        pageTable[i].valid = false;
#else
        pageTable[i].physicalPage = machine->frameMap->Find();
        pageTable[i].valid = true;
#endif

        pageTable[i].use = false;
        pageTable[i].dirty = false;
        pageTable[i].readOnly = false;
    }

#ifndef VM
    for (i = 0; i < numPages; i++) {
        bzero(&(machine->mainMemory[pageTable[i].physicalPage * PageSize]),
              PageSize);
    }

    if (noffHeader.code.size > 0) {
        LoadSegment(executable,
                    noffHeader.code.virtualAddr,
                    noffHeader.code.size,
                    noffHeader.code.inFileAddr);
    }

    if (noffHeader.initData.size > 0) {
        LoadSegment(executable,
                    noffHeader.initData.virtualAddr,
                    noffHeader.initData.size,
                    noffHeader.initData.inFileAddr);
    }
#endif
}
// AddrSpace::AddrSpace(OpenFile *executable)
// {
//     NoffHeader noffH;
//     unsigned int i, size;

//     openFilesTable = new NachosOpenFilesTable();
//     openFilesTable->addThread();

//     executable->ReadAt((char *)&noffH, sizeof(noffH), 0);
//     if ((noffH.noffMagic != NOFFMAGIC) && 
// 		(WordToHost(noffH.noffMagic) == NOFFMAGIC))
//     	SwapHeader(&noffH);
//     ASSERT(noffH.noffMagic == NOFFMAGIC);

// // how big is address space?
//     size = noffH.code.size + noffH.initData.size + noffH.uninitData.size 
// 			+ UserStackSize;	// we need to increase the size
// 						// to leave room for the stack
//     numPages = divRoundUp(size, PageSize);
//     size = numPages * PageSize;

//     stackPages = divRoundUp(UserStackSize, PageSize);
//     stackStartPage = numPages - stackPages;
//     memoryUsers = new int;
//     *memoryUsers = 1;

//     ASSERT(numPages <= NumPhysPages);		// check we're not trying
// 						// to run anything too big --
// 						// at least until we have
// 						// virtual memory

//     DEBUG('a', "Initializing address space, num pages %d, size %d\n", 
// 					numPages, size);
//     // Prueba: reservar frames físicos antes de cargar el programa
//     // machine->frameMap->Mark(0);
//     // machine->frameMap->Mark(2);
//     // machine->frameMap->Mark(4);
//     // machine->frameMap->Mark(6);
//     // machine->frameMap->Mark(8);
//     // machine->frameMap->Mark(10);
// // first, set up the translation 
//     this->pageTable = new TranslationEntry[numPages];


//     for (i = 0; i < numPages; i++) {
//         int frame = machine->frameMap->Find();
//         if (frame == -1) {
//             printf("ERROR: No hay memoria fisica para cargar el programa\n");
//             for (unsigned int j = 0; j < i; j++) {
//                 machine->frameMap->Clear(pageTable[j].physicalPage);
//             }
//             delete [] pageTable;
//             pageTable = NULL;
//             numPages = 0;
//             return;
//         }
//         pageTable[i].virtualPage = i;
//         pageTable[i].physicalPage = frame;
//         pageTable[i].valid = true;
//         pageTable[i].use = false;
//         pageTable[i].dirty = false;
//         pageTable[i].readOnly = false;
//     }
// // zero out the entire address space, to zero the unitialized data segment 
// // and the stack segment
//     for (i = 0; i < numPages; i++) {
//         bzero(&(machine->mainMemory[pageTable[i].physicalPage * PageSize]), PageSize);
//     }
// // then, copy in the code and data segments into memory
//     if (noffH.code.size > 0) {
//          LoadSegment(executable,
//                 noffH.code.virtualAddr,
//                 noffH.code.size,
//                 noffH.code.inFileAddr);
//     }
//     if (noffH.initData.size > 0) {
//         LoadSegment(executable,
//                 noffH.initData.virtualAddr,
//                 noffH.initData.size,
//                 noffH.initData.inFileAddr);
//     }

// }
AddrSpace::AddrSpace(AddrSpace *parent) {
    numPages = parent->numPages;
    stackPages = parent->stackPages;
    stackStartPage = parent->stackStartPage;

    memoryUsers = parent->memoryUsers;
    (*memoryUsers)++;

    openFilesTable = parent->openFilesTable;
    openFilesTable->addThread();

    pageTable = new TranslationEntry[numPages];

    for (unsigned int i = 0; i < numPages; i++) {
        pageTable[i] = parent->pageTable[i];
    }

    // Nueva pila física para el hijo
    for (int i = stackStartPage; i < (int)numPages; i++) {
        int frame = machine->frameMap->Find();

        ASSERT(frame != -1);

        pageTable[i].physicalPage = frame;
        pageTable[i].valid = true;
        pageTable[i].use = false;
        pageTable[i].dirty = false;
        pageTable[i].readOnly = false;

        bzero(&(machine->mainMemory[frame * PageSize]), PageSize);
    }
}
//----------------------------------------------------------------------
// AddrSpace::~AddrSpace
// 	Dealloate an address space.  Nothing for now!
//----------------------------------------------------------------------
AddrSpace::~AddrSpace()
{
    openFilesTable->delThread();

    (*memoryUsers)--;

    bool lastUser = (*memoryUsers == 0);

    for (unsigned int i = 0; i < numPages; i++) {

        bool isStackPage = ((int)i >= stackStartPage);

        if ((isStackPage || lastUser) &&
            pageTable[i].valid &&
            pageTable[i].physicalPage >= 0) {

            machine->frameMap->Clear(pageTable[i].physicalPage);
        }

#ifdef VM
        if (inSwap[i] && swapPage[i] >= 0) {
            swapMap->Clear(swapPage[i]);
        }
#endif
    }

#ifdef VM
    delete [] swapPage;
    delete [] inSwap;
    delete [] lastUse;

    if (swapFile != NULL) {
        delete swapFile;
        swapFile = NULL;
    }
#endif

    delete [] pageTable;

    if (lastUser) {
        delete openFilesTable;
        delete memoryUsers;
    }
}
// AddrSpace::~AddrSpace()
// {
//     openFilesTable->delThread();
//     delete openFilesTable;
//     for (unsigned int i = 0; i < numPages; i++) {
//         machine->frameMap->Clear(pageTable[i].physicalPage);
//     }
//     delete [] pageTable;
// }

//----------------------------------------------------------------------
// AddrSpace::InitRegisters
// 	Set the initial values for the user-level register set.
//
// 	We write these directly into the "machine" registers, so
//	that we can immediately jump to user code.  Note that these
//	will be saved/restored into the currentThread->userRegisters
//	when this thread is context switched out.
//----------------------------------------------------------------------

void AddrSpace::InitRegisters()
{
    int i;

    for (i = 0; i < NumTotalRegs; i++)
	machine->WriteRegister(i, 0);

    // Initial program counter -- must be location of "Start"
    machine->WriteRegister(PCReg, 0);	

    // Need to also tell MIPS where next instruction is, because
    // of branch delay possibility
    machine->WriteRegister(NextPCReg, 4);

   // Set the stack register to the end of the address space, where we
   // allocated the stack; but subtract off a bit, to make sure we don't
   // accidentally reference off the end!
    machine->WriteRegister(StackReg, numPages * PageSize - 16);
    DEBUG('a', "Initializing stack register to %d\n", numPages * PageSize - 16);
}

//----------------------------------------------------------------------
// AddrSpace::SaveState
// 	On a context switch, save any machine state, specific
//	to this address space, that needs saving.
//
//	For now, nothing!
//----------------------------------------------------------------------

void AddrSpace::SaveState()
{
#ifdef USE_TLB
    for (int i = 0; i < TLBSize; i++) {
        if (machine->tlb[i].valid) {
            int vpn = machine->tlb[i].virtualPage;

            if (vpn >= 0 && vpn < (int)numPages) {
                pageTable[vpn].use =
                    pageTable[vpn].use || machine->tlb[i].use;

                pageTable[vpn].dirty =
                    pageTable[vpn].dirty || machine->tlb[i].dirty;

                TouchPage(vpn);
            }

            machine->tlb[i].valid = false;
        }
    }
#endif
}
//----------------------------------------------------------------------
// AddrSpace::RestoreState
// 	On a context switch, restore the machine state so that
//	this address space can run.
//
//      For now, tell the machine where to find the page table.
//----------------------------------------------------------------------

void AddrSpace::RestoreState() {
#ifdef USE_TLB
    machine->pageTable = NULL;

    for (int i = 0; i < TLBSize; i++) {
        machine->tlb[i].valid = false;
    }
#else
    machine->pageTable = pageTable;
    machine->pageTableSize = numPages;
#endif
}

bool AddrSpace::LoadPage(int vpn)
{
    if (vpn < 0 || vpn >= (int)numPages) {
        return false;
    }

    if (pageTable[vpn].valid) {
        UpdateTLB(vpn);
        TouchPage(vpn);
        return true;
    }

    stats->numPageFaults++;

    int frame = machine->frameMap->Find();

#ifdef VM
    if (frame == -1) {
        int oldestTick = 2147483647;
        int victimFrame = -1;

        for (int f = 0; f < NumPhysPages; f++) {
            AddrSpace *candidateSpace = frameOwnerSpace[f];
            int candidateVPN = frameOwnerVPN[f];

            if (candidateSpace != NULL && candidateVPN >= 0) {
                int candidateTick = candidateSpace->lastUse[candidateVPN];

                if (candidateTick < oldestTick) {
                    oldestTick = candidateTick;
                    victimFrame = f;
                }
            }
        }

        if (victimFrame == -1) {
            victimFrame = 0;
        }

        frame = victimFrame;

        AddrSpace *victimSpace = frameOwnerSpace[frame];
        int victimVPN = frameOwnerVPN[frame];

        if (victimSpace != NULL && victimVPN >= 0) {

            bool dirty = victimSpace->pageTable[victimVPN].dirty;

#ifdef USE_TLB
            for (int i = 0; i < TLBSize; i++) {
                if (machine->tlb[i].valid &&
                    machine->tlb[i].virtualPage == victimVPN) {
                    dirty = dirty || machine->tlb[i].dirty;
                    victimSpace->pageTable[victimVPN].use =
                        victimSpace->pageTable[victimVPN].use ||
                        machine->tlb[i].use;

                    machine->tlb[i].valid = false;
                }
            }
#endif

            if (dirty) {
                if (!victimSpace->inSwap[victimVPN]) {
                    int swapSlot = swapMap->Find();

                    if (swapSlot == -1) {
                        printf("ERROR: SWAP lleno.\n");
                        return false;
                    }

                    victimSpace->swapPage[victimVPN] = swapSlot;
                    victimSpace->inSwap[victimVPN] = true;
                }

                victimSpace->swapFile->WriteAt(
                    &(machine->mainMemory[frame * PageSize]),
                    PageSize,
                    victimSpace->swapPage[victimVPN] * PageSize);

                stats->numDiskWrites++;
            }

            victimSpace->pageTable[victimVPN].valid = false;
            victimSpace->pageTable[victimVPN].physicalPage = -1;
            victimSpace->pageTable[victimVPN].dirty = dirty;
        }
    }
#else
    if (frame == -1) {
        return false;
    }
#endif

    bzero(&(machine->mainMemory[frame * PageSize]), PageSize);

#ifdef VM
    if (inSwap[vpn]) {
        swapFile->ReadAt(&(machine->mainMemory[frame * PageSize]),
                         PageSize,
                         swapPage[vpn] * PageSize);

        stats->numDiskReads++;

        swapMap->Clear(swapPage[vpn]);
        swapPage[vpn] = -1;
        inSwap[vpn] = false;
    } else {
#endif

        int pageStart = vpn * PageSize;
        int pageEnd = pageStart + PageSize;

        int codeStart = noffHeader.code.virtualAddr;
        int codeEnd = codeStart + noffHeader.code.size;

        int dataStart = noffHeader.initData.virtualAddr;
        int dataEnd = dataStart + noffHeader.initData.size;

        if (pageStart < codeEnd && pageEnd > codeStart) {
            int from = pageStart > codeStart ? pageStart : codeStart;
            int to = pageEnd < codeEnd ? pageEnd : codeEnd;
            int bytes = to - from;

            executableFile->ReadAt(
                &(machine->mainMemory[frame * PageSize + (from - pageStart)]),
                bytes,
                noffHeader.code.inFileAddr + (from - codeStart));
        }

        if (pageStart < dataEnd && pageEnd > dataStart) {
            int from = pageStart > dataStart ? pageStart : dataStart;
            int to = pageEnd < dataEnd ? pageEnd : dataEnd;
            int bytes = to - from;

            executableFile->ReadAt(
                &(machine->mainMemory[frame * PageSize + (from - pageStart)]),
                bytes,
                noffHeader.initData.inFileAddr + (from - dataStart));
        }

#ifdef VM
    }
#endif

    pageTable[vpn].physicalPage = frame;
    pageTable[vpn].valid = true;
    pageTable[vpn].use = true;
    pageTable[vpn].dirty = false;

#ifdef VM
    frameOwnerSpace[frame] = this;
    frameOwnerVPN[frame] = vpn;
    TouchPage(vpn);
#endif

    UpdateTLB(vpn);

    return true;
}

void AddrSpace::UpdateTLB(int vpn)
{
#ifdef USE_TLB
    if (vpn < 0 || vpn >= (int)numPages)
        return;

    // Si ya está en TLB, solo refrescar LRU y copiar bits
    for (int i = 0; i < TLBSize; i++) {
        if (machine->tlb[i].valid &&
            machine->tlb[i].virtualPage == vpn) {

            machine->tlb[i].use = true;
            TouchPage(vpn);
            return;
        }
    }

    int victim = -1;

    // Primero buscar entrada libre
    for (int i = 0; i < TLBSize; i++) {
        if (!machine->tlb[i].valid) {
            victim = i;
            break;
        }
    }

    // Si no hay libre, escoger la menos recientemente usada
    if (victim == -1) {
        int oldestUse = 2147483647;

        for (int i = 0; i < TLBSize; i++) {
            int oldVpn = machine->tlb[i].virtualPage;

            if (oldVpn >= 0 && oldVpn < (int)numPages) {
                int lu = lastUse[oldVpn];

                if (lu < oldestUse) {
                    oldestUse = lu;
                    victim = i;
                }
            }
        }
    }

    // Guardar bits use/dirty de la víctima antes de sacarla
    if (machine->tlb[victim].valid) {
        int oldVpn = machine->tlb[victim].virtualPage;

        if (oldVpn >= 0 && oldVpn < (int)numPages) {
            pageTable[oldVpn].use =
                pageTable[oldVpn].use || machine->tlb[victim].use;

            pageTable[oldVpn].dirty =
                pageTable[oldVpn].dirty || machine->tlb[victim].dirty;
        }
    }

    machine->tlb[victim] = pageTable[vpn];
    machine->tlb[victim].valid = true;

    TouchPage(vpn);
#endif
}

void AddrSpace::TouchPage(int vpn)
{
#ifdef VM
    if (vpn >= 0 && vpn < (int)numPages) {
        lastUse[vpn] = ++lruClock;
    }
#endif
}