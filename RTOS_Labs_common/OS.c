// *************os.c**************
// EE445M/EE380L.6 Labs 1, 2, 3, and 4 
// High-level OS functions
// Students will implement these functions as part of Lab
// Runs on LM4F120/TM4C123
// Jonathan W. Valvano 
// Jan 12, 2020, valvano@mail.utexas.edu


#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "../inc/tm4c123gh6pm.h"
#include "../inc/CortexM.h"
#include "../inc/PLL.h"
#include "../inc/LaunchPad.h"

#include "../inc/Timer4A.h"
#include "../inc/Timer5A.h"
#include "../inc/Timer0A.h"

#include "../inc/WTimer0A.h"
#include "../RTOS_Labs_common/OS.h"
#include "../RTOS_Labs_common/ST7735.h"
#include "../inc/ADCT0ATrigger.h"
#include "../RTOS_Labs_common/UART0int.h"
#include "../RTOS_Labs_common/eFile.h"

void StartOS(void);
void ContextSwitch(void);

/*code written by weilin */
#define NUMTHREADS 3 // maximum number of threads
#define STACKSIZE 100 // number of 32-bit words in stack

tcbType tcbs[NUMTHREADS];
tcbType *RunPt;
tcbType *SleepPt;
int32_t Stacks[NUMTHREADS][STACKSIZE];

// Thread Tracking
uint32_t ThreadCount = 0;
uint32_t SleepCount = 0;
uint32_t ThreadId = 0;


// Performance Measurements 
int32_t MaxJitter;             // largest time jitter between interrupts in usec
#define JITTERSIZE 64
uint32_t const JitterSize=JITTERSIZE;
uint32_t JitterHistogram[JITTERSIZE]={0,};


/*------------------------------------------------------------------------------
  Systick Interrupt Handler
  SysTick interrupt happens every 10 ms
  used for preemptive thread switch
 *------------------------------------------------------------------------------*/
//void SysTick_Handler(void) {
  
  
//} // end SysTick_Handler

unsigned long OS_LockScheduler(void){
  // lab 4 might need this for disk formating
  return 0;// replace with solution
}
void OS_UnLockScheduler(unsigned long previous){
  // lab 4 might need this for disk formating
}


void SysTick_Init(unsigned long period){
  STCTRL = 0; // disable SysTick during setup
	STCURRENT = 0; // any write to current clears it
	SYSPRI3 = (SYSPRI3&0x00FFFFFF)|0xE0000000; // priority 7
	STRELOAD = period - 1; // reload value
	STCTRL = 0x00000007; // enable, core clock and interrupt arm
}

/**
 * @details  Initialize operating system, disable interrupts until OS_Launch.
 * Initialize OS controlled I/O: serial, ADC, systick, LaunchPad I/O and timers.
 * Interrupts not yet enabled.
 * @param  none
 * @return none
 * @brief  Initialize OS
 */
void OS_Init(void){
	DisableInterrupts();
	PLL_Init(Bus80MHz);
	for(int i=0; i < NUMTHREADS; i++) //free all allocated tcb free
	{
		tcbs[i].next = NULL;
		tcbs[i].tid = -1;
		tcbs[i].index = i;
	}
}; 

// ******** OS_InitSemaphore ************
// initialize semaphore 
// input:  pointer to a semaphore
// output: none
void OS_InitSemaphore(Sema4Type *semaPt, int32_t value){
  semaPt->Value = value;
	semaPt->owner = NULL;
	semaPt->acquire_count = 0;
	semaPt->blocked_tcbs = NULL;

}; 

// ******** OS_Wait ************
// decrement semaphore 
// Lab2 spinlock
// Lab3 block if less than zero
// input:  pointer to a counting semaphore
// output: none
void OS_Wait(Sema4Type *semaPt){
  DisableInterrupts();
	while(semaPt->Value<=0){
		EnableInterrupts();
		OS_Suspend();
		DisableInterrupts();
	}
	semaPt->Value=semaPt->Value-1;
	EnableInterrupts();	
}; 

// ******** OS_Signal ************
// increment semaphore 
// Lab2 spinlock
// Lab3 wakeup blocked thread if appropriate 
// input:  pointer to a counting semaphore
// output: none
void OS_Signal(Sema4Type *semaPt){
	long status= StartCritical();
	semaPt->Value=semaPt->Value+1;
	EndCritical(status);
}; 

// ******** OS_bWait ************
// Lab2 spinlock, set to 0
// Lab3 block if less than zero
// input:  pointer to a binary semaphore
// output: none
void OS_bWait(Sema4Type *semaPt){
	DisableInterrupts();
	if(semaPt->owner != RunPt){ // If a different thread is attempting to acquire the semaphore, wait
		while(semaPt->Value==0){
			EnableInterrupts();
			OS_Suspend();
			DisableInterrupts();
		}
		semaPt->owner = RunPt;
		semaPt->Value=0;
	} else { // Increment the hold count to keep track of how many times the same thread has acquired the same semaphore.
		semaPt->acquire_count++;
	}
	EnableInterrupts();	
		
}; 

// ******** OS_bSignal ************
// Lab2 spinlock, set to 1
// Lab3 wakeup blocked thread if appropriate 
// input:  pointer to a binary semaphore
// output: none
void OS_bSignal(Sema4Type *semaPt){
	semaPt->acquire_count--;
	if(semaPt->acquire_count == 0){ // Only release the semaphore once all chained bWaits have been paired with a bSignal.
		semaPt->Value=1;
		semaPt->owner = NULL;
	}
}; 

void SetInitialStack(int i){
	tcbs[i].sp = &Stacks[i][STACKSIZE-16]; // thread stack pointer
	Stacks[i][STACKSIZE-1] = 0x01000000; // Thumb bit
	Stacks[i][STACKSIZE-3] = 0x14141414; // R14
	Stacks[i][STACKSIZE-4] = 0x12121212; // R12
	Stacks[i][STACKSIZE-5] = 0x03030303; // R3
	Stacks[i][STACKSIZE-6] = 0x02020202; // R2
	Stacks[i][STACKSIZE-7] = 0x01010101; // R1
	Stacks[i][STACKSIZE-8] = 0x00000000; // R0
	Stacks[i][STACKSIZE-9] = 0x11111111; // R11
	Stacks[i][STACKSIZE-10] = 0x10101010; // R10
	Stacks[i][STACKSIZE-11] = 0x09090909; // R9
	Stacks[i][STACKSIZE-12] = 0x08080808; // R8
	Stacks[i][STACKSIZE-13] = 0x07070707; // R7
	Stacks[i][STACKSIZE-14] = 0x06060606; // R6
	Stacks[i][STACKSIZE-15] = 0x05050505; // R5
	Stacks[i][STACKSIZE-16] = 0x04040404; // R4
}

tcbType* tcb_alloc(){
	for(int i = 0; i < NUMTHREADS; i++){
		if(tcbs[i].tid == -1){
			tcbs[i].tid = ThreadId;
			ThreadId++;
			return &tcbs[i];
		}
	}
	return NULL;
}

//******** OS_AddThread *************** 
// add a foregound thread to the scheduler
// Inputs: pointer to a void/void foreground task
//         number of bytes allocated for its stack
//         priority, 0 is highest, 5 is the lowest
// Outputs: 1 if successful, 0 if this thread can not be added
// stack size must be divisable by 8 (aligned to double word boundary)
// In Lab 2, you can ignore both the stackSize and priority fields
// In Lab 3, you can ignore the stackSize fields
int OS_AddThread(void(*task)(void), uint32_t stackSize, uint32_t priority){
	int32_t status = StartCritical();
	
	tcbType *new_tcb = tcb_alloc(); // Allocates a new TCB for the thread
	if(!new_tcb){ // If TCB could not be allocated, thread cannot be added.
		EndCritical(status);
		return 0;
	}
	
	SetInitialStack(new_tcb->index); // Initializes TCB stack
	Stacks[new_tcb->index][STACKSIZE-2] = (int32_t)(task); // Sets PC on TCB stack
	
	if(ThreadCount==0){
		RunPt = &tcbs[new_tcb->index]; // the only existing thread will run
		tcbs[new_tcb->index].next = new_tcb;
		tcbs[new_tcb->index].prev = new_tcb;
	} else {
		RunPt->prev->next = new_tcb; // Adjusts TCB pointers to insert the array
		new_tcb->prev = RunPt->prev;
		RunPt->prev = new_tcb;
		new_tcb->next = RunPt;
	}
	ThreadCount++;
	EndCritical(status);
	return 1; // successful

};

//******** OS_Id *************** 
// returns the thread ID for the currently running thread
// Inputs: none
// Outputs: Thread ID, number greater than zero 
uint32_t OS_Id(void){
  return RunPt->tid;
};


//******** OS_AddPeriodicThread *************** 
// add a background periodic task
// typically this function receives the highest priority
// Inputs: pointer to a void/void background function
//         period given in system time units (12.5ns)
//         priority 0 is the highest, 5 is the lowest
// Outputs: 1 if successful, 0 if this thread can not be added
// You are free to select the time resolution for this function
// It is assumed that the user task will run to completion and return
// This task can not spin, block, loop, sleep, or kill
// This task can call OS_Signal  OS_bSignal   OS_AddThread
// This task does not have a Thread ID
// In lab 1, this command will be called 1 time
// In lab 2, this command will be called 0 or 1 times
// In lab 2, the priority field can be ignored
// In lab 3, this command will be called 0 1 or 2 times
// In lab 3, there will be up to four background threads, and this priority field 
//           determines the relative priority of these four threads
int OS_AddPeriodicThread(void(*task)(void), 
   uint32_t period, uint32_t priority){
  // put Lab 2 (and beyond) solution here
  
     
  return 0; // replace this line with solution
};

//******** OS_InitSW1 *************** 
// initializes PF0 with interrupts enabled
// Inputs: none
// Outputs: none
// Taken from ValvanoWare EdgeInterrupts_4C123
void OS_InitSW1(void){
	SYSCTL_RCGCGPIO_R |= 0x00000020; // (a) activate clock for port F
  GPIO_PORTF_DIR_R &= ~0x10;    // (b) make PF4 in (built-in button)
  GPIO_PORTF_AFSEL_R &= ~0x10;  //     disable alt funct on PF4
  GPIO_PORTF_DEN_R |= 0x10;     //     enable digital I/O on PF4    
  GPIO_PORTF_PCTL_R &= ~0x000F0000; // configure PF4 as GPIO
  GPIO_PORTF_AMSEL_R = 0;       //     disable analog functionality on PF
  GPIO_PORTF_PUR_R |= 0x10;     //     enable weak pull-up on PF4
  GPIO_PORTF_IS_R &= ~0x10;     // (d) PF4 is edge-sensitive
  GPIO_PORTF_IBE_R &= ~0x10;    //     PF4 is not both edges
  GPIO_PORTF_IEV_R &= ~0x10;    //     PF4 falling edge event
  GPIO_PORTF_ICR_R = 0x10;      // (e) clear flag4
  GPIO_PORTF_IM_R |= 0x10;      // (f) arm interrupt on PF4 *** No IME bit as mentioned in Book ***
  NVIC_PRI7_R = (NVIC_PRI7_R&0xFF00FFFF)|0x00A00000; // (g) priority 5 (This will likely need to change)
  NVIC_EN0_R = 0x40000000;      // (h) enable interrupt 30 in NVIC
};

/*----------------------------------------------------------------------------
  PF Interrupt Handler
 *----------------------------------------------------------------------------*/
void (*SW1task)(void) = NULL;
tcbType *SW1tcb = NULL;
void (*SW2task)(void) = NULL;
tcbType *SW2tcb = NULL;
void GPIOPortF_Handler(void){
	
}
//******** OS_AddSW1Task *************** 
// add a background task to run whenever the SW1 (PF4) button is pushed
// Inputs: pointer to a void/void background function
//         priority 0 is the highest, 5 is the lowest
// Outputs: 1 if successful, 0 if this thread can not be added
// It is assumed that the user task will run to completion and return
// This task can not spin, block, loop, sleep, or kill
// This task can call OS_Signal  OS_bSignal   OS_AddThread
// This task does not have a Thread ID
// In labs 2 and 3, this command will be called 0 or 1 times
// In lab 2, the priority field can be ignored
// In lab 3, there will be up to four background threads, and this priority field 
//           determines the relative priority of these four threads
int OS_AddSW1Task(void(*task)(void), uint32_t priority){ 
	int32_t status = StartCritical(); // IDK what we are supposed to do for this function, we will need to rewrite it...
	
	tcbType *new_tcb = tcb_alloc(); // Allocates a new TCB for the thread
	if(!new_tcb){ // If TCB could not be allocated, thread cannot be added.
		EndCritical(status);
		return 0;
	}
	
	SetInitialStack(new_tcb->index); // Initializes TCB stack
	Stacks[new_tcb->index][STACKSIZE-2] = (int32_t)(task); // Sets PC on TCB stack
	
	SW1task = task;
	
	EndCritical(status);
	return 1;
};

//******** OS_AddSW2Task *************** 
// add a background task to run whenever the SW2 (PF0) button is pushed
// Inputs: pointer to a void/void background function
//         priority 0 is highest, 5 is lowest
// Outputs: 1 if successful, 0 if this thread can not be added
// It is assumed user task will run to completion and return
// This task can not spin block loop sleep or kill
// This task can call issue OS_Signal, it can call OS_AddThread
// This task does not have a Thread ID
// In lab 2, this function can be ignored
// In lab 3, this command will be called will be called 0 or 1 times
// In lab 3, there will be up to four background threads, and this priority field 
//           determines the relative priority of these four threads
int OS_AddSW2Task(void(*task)(void), uint32_t priority){
  // put Lab 2 (and beyond) solution here
    
  return 0; // replace this line with solution
};

// ******** OS_Sleep ************
// place this thread into a dormant state
// input:  number of msec to sleep
// output: none
// You are free to select the time resolution for this function
// OS_Sleep(0) implements cooperative multitasking
void OS_Sleep(uint32_t sleepTime){
	if(sleepTime == 0){ 
		OS_Suspend();
	}
	long sr = StartCritical();
	RunPt->sleep_count = sleepTime;
	
	if(SleepCount == 0){ // The first element in the sleep list will point to itself.
		SleepPt = RunPt;
		SleepPt->snext = SleepPt;
		SleepPt->sprev = SleepPt;
	} else { // Inserts thread into the tail of the sleep list
		SleepPt->sprev->snext = RunPt;
		RunPt->snext = SleepPt;
		RunPt->sprev = SleepPt->sprev;
		SleepPt->sprev = RunPt;
	}
	
	SleepCount++;
	while(SleepCount == ThreadCount){} // Spin if all threads are sleeping to prevent system crash.
		
	EndCritical(sr);
	OS_Suspend();
};  

// ******** OS_Sleep_Decrement ************
// decrement all sleep counters in the sleep queue
// input:  none
// output: none
void OS_Sleep_Decrement(void){
	tcbType *sleep_curr = SleepPt;
	
	do{
		sleep_curr->sleep_count--; // Decrement current TCB sleep counter
		if(sleep_curr->sleep_count == 0){ // If the sleep counter becomes zero, remove from sleep list and add to run list
			sleep_curr->sprev->snext = sleep_curr->snext; // The previous element's next pt now skips over the thread we are removing.
			sleep_curr->snext->sprev = sleep_curr->sprev; // The next element's prev pt now skips over the thread we are removing.
			sleep_curr->snext = NULL;
			sleep_curr->sprev = NULL;
			SleepCount--;
		}
		sleep_curr = sleep_curr->snext;
	} while(sleep_curr != SleepPt);
	
}

// ******** OS_Kill ************
// kill the currently running thread, release its TCB and stack
// input:  none
// output: none
void OS_Kill(void){
  long sr = StartCritical();
		
	RunPt->tid = -1; // Sets TCB tid to -1 to indicate TCB is available
	
	RunPt->prev->next = RunPt->next; // The previous element's next pt now skips over the thread we are removing.
	RunPt->next->prev = RunPt->prev; // The next element's prev pt now skips over the thread we are removing.
	
	ThreadCount--;
	
	while(!ThreadCount){} // Spin if no active foreground threads to prevent system crash.
	
  EndCritical(sr);   // end of atomic section 
	OS_Suspend();      // TODO: Consider what happens if OS_Suspend is called when the last thread is killed.
  for(;;){};        // can not return    
}; 

// ******** OS_Suspend ************
// suspend execution of currently running thread
// scheduler will choose another thread to execute
// Can be used to implement cooperative multitasking 
// Same function as OS_Sleep(0)
// input:  none
// output: none
void OS_Suspend(void){
	ContextSwitch();
};

// FIFO code based on code from pg. 214 of the textbook
#define OSFIFOSIZE 32
uint32_t *OS_Put;
uint32_t *OS_Get;

static uint32_t OSFIFO[OSFIFOSIZE];

Sema4Type FIFOSize;
Sema4Type FIFOmutex;
uint32_t DataLost;
  
// ******** OS_Fifo_Init ************
// Initialize the Fifo to be empty
// Inputs: size
// Outputs: none 
// In Lab 2, you can ignore the size field
// In Lab 3, you should implement the user-defined fifo size
// In Lab 3, you can put whatever restrictions you want on size
//    e.g., 4 to 64 elements
//    e.g., must be a power of 2,4,8,16,32,64,128
void OS_Fifo_Init(uint32_t size){
	OS_Put = OS_Get = &OSFIFO[0];
	DataLost = 0;
	OS_InitSemaphore(&FIFOSize, 0);
	OS_InitSemaphore(&FIFOmutex, 1);  
};

// ******** OS_Fifo_Put ************
// Enter one data sample into the Fifo
// Called from the background, so no waiting 
// Inputs:  data
// Outputs: true if data is properly saved,
//          false if data not saved, because it was full
// Since this is called by interrupt handlers 
//  this function can not disable or enable interrupts
int OS_Fifo_Put(uint32_t data){
  if(FIFOSize.Value == OSFIFOSIZE){ // If FIFO full, data is lost.
		DataLost++;
		return -1;
	}
	*(OS_Put) = data;
	OS_Put++;
	if(OS_Put == &OSFIFO[OSFIFOSIZE]){ // Necessary to make the FIFO circular.
		OS_Put = &OSFIFO[0];
	}
	OS_Signal(&FIFOSize);
	return 0;
};  

// ******** OS_Fifo_Get ************
// Remove one data sample from the Fifo
// Called in foreground, will spin/block if empty
// Inputs:  none
// Outputs: data 
uint32_t OS_Fifo_Get(void){
  OS_Wait(&FIFOSize);
	OS_Wait(&FIFOmutex);
	uint32_t data = *OS_Get;
	OS_Get++;
	if(OS_Get == &OSFIFO[OSFIFOSIZE]){ // Necessary to make the FIFO circular.
		OS_Get = &OSFIFO[0];
	}
	OS_Signal(&FIFOmutex);
  return data;
};

// ******** OS_Fifo_Size ************
// Check the status of the Fifo
// Inputs: none
// Outputs: returns the number of elements in the Fifo
//          greater than zero if a call to OS_Fifo_Get will return right away
//          zero or less than zero if the Fifo is empty 
//          zero or less than zero if a call to OS_Fifo_Get will spin or block
int32_t OS_Fifo_Size(void){
	return FIFOSize.Value;
};

// Mailbox code based off of pg. 207 in the textbook
uint32_t OS_Mail;
Sema4Type OS_Send;
Sema4Type OS_Ack;

// ******** OS_MailBox_Init ************
// Initialize communication channel
// Inputs:  none
// Outputs: none
void OS_MailBox_Init(void){
  OS_Mail = -1;
	OS_InitSemaphore(&OS_Send, 0);
	OS_InitSemaphore(&OS_Ack, 0);
};

// ******** OS_MailBox_Send ************
// enter mail into the MailBox
// Inputs:  data to be sent
// Outputs: none
// This function will be called from a foreground thread
// It will spin/block if the MailBox contains data not yet received 
void OS_MailBox_Send(uint32_t data){
  OS_Mail = data;
	OS_Signal(&OS_Send);
	OS_Wait(&OS_Ack);
};

// ******** OS_MailBox_Recv ************
// remove mail from the MailBox
// Inputs:  none
// Outputs: data received
// This function will be called from a foreground thread
// It will spin/block if the MailBox is empty 
uint32_t OS_MailBox_Recv(void){
	OS_Wait(&OS_Send);
	uint32_t data = OS_Mail;
	OS_Signal(&OS_Ack);
	return data;
};

// ******** OS_Time ************
// return the system time 
// Inputs:  none
// Outputs: time in 12.5ns units, 0 to 4294967295
// The time resolution should be less than or equal to 1us, and the precision 32 bits
// It is ok to change the resolution and precision of this function as long as 
//   this function and OS_TimeDifference have the same resolution and precision 
uint32_t OS_Time(void){
  // put Lab 2 (and beyond) solution here

  return 0; // replace this line with solution
};

// ******** OS_TimeDifference ************
// Calculates difference between two times
// Inputs:  two times measured with OS_Time
// Outputs: time difference in 12.5ns units 
// The time resolution should be less than or equal to 1us, and the precision at least 12 bits
// It is ok to change the resolution and precision of this function as long as 
//   this function and OS_Time have the same resolution and precision 
uint32_t OS_TimeDifference(uint32_t start, uint32_t stop){
  // put Lab 2 (and beyond) solution here

  return 0; // replace this line with solution
};


// ******** OS_ClearMsTime ************
// sets the system time to zero (solve for Lab 1), and start a periodic interrupt
// Inputs:  none
// Outputs: none
// You are free to change how this works
uint32_t MsTime;

void TimeIncr(void){
  MsTime++;
	OS_Sleep_Decrement();
  return;
}

void OS_ClearMsTime(void){
  Timer5A_Init(&TimeIncr,80000,0); // 1 ms ,highest priority
  MsTime=0;
};

// ******** OS_MsTime ************
// reads the current time in msec (solve for Lab 1)
// Inputs:  none
// Outputs: time in ms units
// You are free to select the time resolution for this function
// For Labs 2 and beyond, it is ok to make the resolution to match the first call to OS_AddPeriodicThread
uint32_t OS_MsTime(void){
	return MsTime;
};


//******** OS_Launch *************** 
// start the scheduler, enable interrupts
// Inputs: number of 12.5ns clock cycles for each time slice
//         you may select the units of this parameter
// Outputs: none (does not return)
// In Lab 2, you can ignore the theTimeSlice field
// In Lab 3, you should implement the user-defined TimeSlice field
// It is ok to limit the range of theTimeSlice to match the 24-bit SysTick
void OS_Launch(uint32_t theTimeSlice){
	LaunchPad_Init();
  SysTick_Init(theTimeSlice);
	EnableInterrupts();
	StartOS(); // start on the first task    
};

//******** I/O Redirection *************** 
// redirect terminal I/O to UART

int fputc (int ch, FILE *f) { 
  UART_OutChar(ch);
  return ch; 
}

int fgetc (FILE *f){
  char ch = UART_InChar();  // receive from keyboard
  UART_OutChar(ch);         // echo
  return ch;
}
int OS_RedirectToFile(char *name){
  
  return 1;
}
int OS_RedirectToUART(void){
  
  return 1;
}

int OS_RedirectToST7735(void){
  
  return 1;
}

int OS_EndRedirectToFile(void){
  
  return 1;
}
