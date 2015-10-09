// exception.cc 
//	Entry point into the Nachos kernel from user programs.
//	There are two kinds of things that can cause control to
//	transfer back to here from user code:
//
//	syscall -- The user code explicitly requests to call a procedure
//	in the Nachos kernel.  Right now, the only function we support is
//	"Halt".
//
//	exceptions -- The user code does something that the CPU can't handle.
//	For instance, accessing memory that doesn't exist, arithmetic errors,
//	etc.  
//
//	Interrupts (which can also cause control to transfer from user
//	code into the Nachos kernel) are handled elsewhere.
//
// For now, this only handles the Halt() system call.
// Everything else core dumps.
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "system.h"
#include "syscall.h"
#include <stdio.h>
#include <iostream>

using namespace std;

int copyin(unsigned int vaddr, int len, char *buf) {
    // Copy len bytes from the current thread's virtual address vaddr.
    // Return the number of bytes so read, or -1 if an error occors.
    // Errors can generally mean a bad virtual address was passed in.
    bool result;
    int n=0;			// The number of bytes copied in
    int *paddr = new int;

    while ( n >= 0 && n < len) {
      result = machine->ReadMem( vaddr, 1, paddr );
      while(!result) // FALL 09 CHANGES
	  {
   			result = machine->ReadMem( vaddr, 1, paddr ); // FALL 09 CHANGES: TO HANDLE PAGE FAULT IN THE ReadMem SYS CALL
	  }	
      
      buf[n++] = *paddr;
     
      if ( !result ) {
	//translation failed
	return -1;
      }

      vaddr++;
    }

    delete paddr;
    return len;
}

int copyout(unsigned int vaddr, int len, char *buf) {
    // Copy len bytes to the current thread's virtual address vaddr.
    // Return the number of bytes so written, or -1 if an error
    // occors.  Errors can generally mean a bad virtual address was
    // passed in.
    bool result;
    int n=0;			// The number of bytes copied in

    while ( n >= 0 && n < len) {
      // Note that we check every byte's address
      result = machine->WriteMem( vaddr, 1, (int)(buf[n++]) );

      if ( !result ) {
	//translation failed
	return -1;
      }

      vaddr++;
    }

    return n;
}


void Create_Syscall(unsigned int vaddr, int len) {
    // Create the file with the name in the user buffer pointed to by
    // vaddr.  The file name is at most MAXFILENAME chars long.  No
    // way to return errors, though...
    char *buf = new char[len+1];	// Kernel buffer to put the name in

    if (!buf) return;

    if( copyin(vaddr,len,buf) == -1 ) {
	printf("%s","Bad pointer passed to Create\n");
	delete buf;
	return;
    }

    buf[len]='\0';

    fileSystem->Create(buf,0);
    delete[] buf;
    return;
}

int Open_Syscall(unsigned int vaddr, int len) {
    // Open the file with the name in the user buffer pointed to by
    // vaddr.  The file name is at most MAXFILENAME chars long.  If
    // the file is opened successfully, it is put in the address
    // space's file table and an id returned that can find the file
    // later.  If there are any errors, -1 is returned.
    char *buf = new char[len+1];	// Kernel buffer to put the name in
    OpenFile *f;			// The new open file
    int id;				// The openfile id

    if (!buf) {
	printf("%s","Can't allocate kernel buffer in Open\n");
	return -1;
    }

    if( copyin(vaddr,len,buf) == -1 ) {
	printf("%s","Bad pointer passed to Open\n");
	delete[] buf;
	return -1;
    }

    buf[len]='\0';

    f = fileSystem->Open(buf);
    delete[] buf;

    if ( f ) {
	if ((id = currentThread->space->fileTable.Put(f)) == -1 )
	    delete f;
	return id;
    }
    else
	return -1;
}

void Write_Syscall(unsigned int vaddr, int len, int id) {
    // Write the buffer to the given disk file.  If ConsoleOutput is
    // the fileID, data goes to the synchronized console instead.  If
    // a Write arrives for the synchronized Console, and no such
    // console exists, create one. For disk files, the file is looked
    // up in the current address space's open file table and used as
    // the target of the write.
    
    char *buf;		// Kernel buffer for output
    OpenFile *f;	// Open file for output

    if ( id == ConsoleInput) return;
    
    if ( !(buf = new char[len]) ) {
	printf("%s","Error allocating kernel buffer for write!\n");
	return;
    } else {
        if ( copyin(vaddr,len,buf) == -1 ) {
	    printf("%s","Bad pointer passed to to write: data not written\n");
	    delete[] buf;
	    return;
	}
    }

    if ( id == ConsoleOutput) {
      for (int ii=0; ii<len; ii++) {
	printf("%c",buf[ii]);
      }

    } else {
	if ( (f = (OpenFile *) currentThread->space->fileTable.Get(id)) ) {
	    f->Write(buf, len);
	} else {
	    printf("%s","Bad OpenFileId passed to Write\n");
	    len = -1;
	}
    }

    delete[] buf;
}

int Read_Syscall(unsigned int vaddr, int len, int id) {
    // Write the buffer to the given disk file.  If ConsoleOutput is
    // the fileID, data goes to the synchronized console instead.  If
    // a Write arrives for the synchronized Console, and no such
    // console exists, create one.    We reuse len as the number of bytes
    // read, which is an unnessecary savings of space.
    char *buf;		// Kernel buffer for input
    OpenFile *f;	// Open file for output

    if ( id == ConsoleOutput) return -1;
    
    if ( !(buf = new char[len]) ) {
	printf("%s","Error allocating kernel buffer in Read\n");
	return -1;
    }

    if ( id == ConsoleInput) {
      //Reading from the keyboard
      scanf("%s", buf);

      if ( copyout(vaddr, len, buf) == -1 ) {
	printf("%s","Bad pointer passed to Read: data not copied\n");
      }
    } else {
	if ( (f = (OpenFile *) currentThread->space->fileTable.Get(id)) ) {
	    len = f->Read(buf, len);
	    if ( len > 0 ) {
	        //Read something from the file. Put into user's address space
  	        if ( copyout(vaddr, len, buf) == -1 ) {
		    printf("%s","Bad pointer passed to Read: data not copied\n");
		}
	    }
	} else {
	    printf("%s","Bad OpenFileId passed to Read\n");
	    len = -1;
	}
    }

    delete[] buf;
    return len;
}

void Close_Syscall(int fd) {
    // Close the file associated with id fd.  No error reporting.
    OpenFile *f = (OpenFile *) currentThread->space->fileTable.Remove(fd);

    if ( f ) {
      delete f;
    } else {
      printf("%s","Tried to close an unopen file\n");
    }
}
void Yield_Syscall() {
  currentThread->Yield();
}
void CreateLock(){
  lockTLock->Acquire(); //acquire table lock

  //make sure there is available lock in lockT
  // make sure the lock being created is allowed to be created....who creates locks? does the user program ask the OS to create a lock?

  Lock *lock = new Lock();//NEED TO PASS IN A NAME TO THIS..HOW TO CREATE A NAME..idk. ha

  lockTLock->Release();


}
void AcquireLock(){
  lockTlock->Acquire();
  //DO STUFF

  /*
  TO DO
  make sure the lock is avail..does the synch.cc acquire method already do this/is it redundant to do it again?
  */

  lock->Acquire(); //HOW TO KNOW WHICH LOCK ACQUIRING? look up in lock table?? position in table?


  lockTlock->Release();
}
void ReleaseLock(){
  lockTlock->Acquire();
  
  //make sure the program thread that is trying to release the lock, has the right to do so..is this same as lock owner?

  lock->Release();

  lockTlock->Release();
}
void DestroyLock(){
  lockTlock->Acquire();

  //get index of lock to be destroyed, from lock table.
  // make sure the lock being destroyed is allowed to be destroyed....who creates and destroys locks?
  //remove from lock table...free memory

  lockTlock->Release();
}
void CreateCV(){
  cvTLock->Acquire();

  /*
  TO DO 
  make sure there is available cv in cvT and all aren't being used.
  */

  Condition *condition= new Condition(); //NEED TO PASS IN A NAME TO THIS..HOW TO CREATE A NAME..idk. ha
  
  cvTLock->Release();
}
void Wait(){
  cvTLock->Acquire();
  

  cvTLock->Release();
}
void Signal(){
  cvTLock->Acquire();
  //DO STUFF
  cvTLock->Release();
}
void Broadcast(){
  cvTLock->Acquire();
  //DO STUFF
  cvTLock->Release();
}
void DestroyCV(){
  cvTLock->Acquire();
  //DO STUFF
  cvTLock->Release();
}
void Halt(){
  interrupt->Halt();
}
void Exit_Syscall(){
/*********
3 EXIT CASES:
1. A thread called exit in a process but it is not the last executing thread, 
so we still need the code and data. only can get rid of 8 stack pages for that 
thread since those aren’t shared data..keep track of where the 8 pages are 
for every thread that is created. advantage: wherever 8 stack pages are for 
particular thread, they are continuous..i.e. starts at virtual page 10, ends 
at virtual page 17. can keep track of only the 1st # or last #, preferably 
last…register stays pointing at the last. go by sets of 8.

the 1st thread created in a process is created in Exec. 
Everything else is done in fork. add something process 
table to keep track of where stacks are, if this stack 
completes then just get rid of the stack.

nachos -x ___ ___ ___ ___ -x arg is handled by startProcess 
in progest.cc in userprog….first process and address space gets 
created. whatever code in Exec to populate process table must also 
go into progest.cc otherwise you’ll be off by 1 in the number of 
processes..identical code. virtual page #, physical page number,
 valid bit. page table indexed by virtual page. #. read physical 
 page number, memory bit map with a for loop 8 times.

BELOW IS CODE FROM CLASS.

*****/
memoryBitMap->Clear(8 physical page numbers); //give up page of memory
memoryBitMap->Find(allocate page #)
valid=false //no longer in physical memory
must be reclaimed for something else to use.

/*
2. easiest. last executing thread in last process, 
empty. no need to reclaim anything. nobody left. 
stop nachos. turn off OS equivalent nobody left 
to give any memory or locks or cvs to.

interrupt->halt();

3. most work.. last executing thread in a process- not last process- addrSpace* for the process, search entire table,, if addrSpace pointers match, clear it out. set null, isDeleted, ro something. ready queue is not empty because there are other processes running. not last process. kill the process. reclaim all memory that hasn’t already been reclaimed (code, data, 8 page stacks, any other stacks), in page table entry, piece of data called valid bit, that virtual page is in memory somewhere, data for that entry in the page table can be trusted, when virtual page isn’t in memory that valid bit is set to false.
-locks/cvs- match the addrSpace * w/ Process table
****************/
}
void Fork(void (*func)){

}
void Exec_Syscall(){

}

void Join_Syscall(){

}


void ExceptionHandler(ExceptionType which) {
    int type = machine->ReadRegister(2); // Which syscall?
    int rv=0; 	// the return value from a syscall

    if ( which == SyscallException ) {
	switch (type) {
	    default:
		DEBUG('a', "Unknown syscall - shutting down.\n");
	    case SC_Halt:
		DEBUG('a', "Shutdown, initiated by user program.\n");
		interrupt->Halt();
		break;
	    case SC_Create:
		DEBUG('a', "Create syscall.\n");
		Create_Syscall(machine->ReadRegister(4), machine->ReadRegister(5));
		break;
	    case SC_Open:
		DEBUG('a', "Open syscall.\n");
		rv = Open_Syscall(machine->ReadRegister(4), machine->ReadRegister(5));
		break;
	    case SC_Write:
		DEBUG('a', "Write syscall.\n");
		Write_Syscall(machine->ReadRegister(4),
			      machine->ReadRegister(5),
			      machine->ReadRegister(6));
		break;
	    case SC_Read:
		DEBUG('a', "Read syscall.\n");
		rv = Read_Syscall(machine->ReadRegister(4),
			      machine->ReadRegister(5),
			      machine->ReadRegister(6));
		break;
	    case SC_Close:
		DEBUG('a', "Close syscall.\n");
		Close_Syscall(machine->ReadRegister(4));
		break;
	}

	// Put in the return value and increment the PC
	machine->WriteRegister(2,rv);
	machine->WriteRegister(PrevPCReg,machine->ReadRegister(PCReg));
	machine->WriteRegister(PCReg,machine->ReadRegister(NextPCReg));
	machine->WriteRegister(NextPCReg,machine->ReadRegister(PCReg)+4);
	return;
    } else {
      cout<<"Unexpected user mode exception - which:"<<which<<"  type:"<< type<<endl;
      interrupt->Halt();
    }
}
