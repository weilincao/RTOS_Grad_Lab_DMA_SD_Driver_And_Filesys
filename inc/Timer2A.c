// Timer2A.c
// Runs on LM4F120/TM4C123
// Use TIMER2 in 32-bit periodic mode to request interrupts at a periodic rate
// Daniel Valvano
// May 5, 2020

/* This example accompanies the book
   "Embedded Systems: Real Time Interfacing to Arm Cortex M Microcontrollers",
   ISBN: 978-1463590154, Jonathan Valvano, copyright (c) 2020
  Program 7.5, example 7.6

 Copyright 2020 by Jonathan W. Valvano, valvano@mail.utexas.edu
    You may use, edit, run or distribute this file
    as long as the above copyright notice remains
 THIS SOFTWARE IS PROVIDED "AS IS".  NO WARRANTIES, WHETHER EXPRESS, IMPLIED
 OR STATUTORY, INCLUDING, BUT NOT LIMITED TO, IMPLIED WARRANTIES OF
 MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE APPLY TO THIS SOFTWARE.
 VALVANO SHALL NOT, IN ANY CIRCUMSTANCES, BE LIABLE FOR SPECIAL, INCIDENTAL,
 OR CONSEQUENTIAL DAMAGES, FOR ANY REASON WHATSOEVER.
 For more information about my classes, my research, and my books, see
 http://users.ece.utexas.edu/~valvano/
 */
#include <stdint.h>
#include "../inc/tm4c123gh6pm.h"
#include "../RTOS_labs_common/OS.h"

void (*PeriodicTask2)(void);   // user function

// ***************** Timer2A_Init ****************
// Activate Timer2 interrupts to run user task periodically
// Inputs:  task is a pointer to a user function
//          period in units (1/clockfreq)
//          priority 0 (highest) to 7 (lowest)
// Outputs: none
void Timer2A_Init(void(*task)(void), uint32_t period, uint32_t priority){
  SYSCTL_RCGCTIMER_R |= 0x04;   // 0) activate timer2
  PeriodicTask2 = task;         // user function
  TIMER2_CTL_R = 0x00000000;    // 1) disable timer2A during setup
  TIMER2_CFG_R = 0x00000000;    // 2) configure for 32-bit mode
  TIMER2_TAMR_R = 0x00000002;   // 3) configure for periodic mode, default down-count settings
  TIMER2_TAILR_R = period-1;    // 4) reload value
  TIMER2_TAPR_R = 0;            // 5) bus clock resolution
  TIMER2_ICR_R = 0x00000001;    // 6) clear timer2A timeout flag
  TIMER2_IMR_R = 0x00000001;    // 7) arm timeout interrupt
  NVIC_PRI5_R = (NVIC_PRI5_R&0x00FFFFFF)|(priority<<29); // priority  
// interrupts enabled in the main program after all devices initialized
// vector number 39, interrupt number 23
  NVIC_EN0_R = 1<<23;           // 9) enable IRQ 23 in NVIC
  TIMER2_CTL_R = 0x00000001;    // 10) enable timer2A
}

extern int32_t MaxJitter2;
extern uint32_t const JitterSize2;
extern uint32_t JitterHistogram2[];

void Timer2A_Handler(void){
  TIMER2_ICR_R = TIMER_ICR_TATOCINT;// acknowledge TIMER2A timeout
	
	uint32_t current_time	=OS_Time();			//current time in unit of clock ticks;
	static uint32_t last_time=0;
	uint32_t diff;
	if(last_time!=0){
		diff=current_time-last_time;
	}
	uint32_t period=TIMER2_TAILR_R+1;
	uint32_t jitter;
	
	if(diff>period){
		jitter = (diff-period+4)/8;  // in 0.1 usec
	}else{
		jitter = (period-diff+4)/8;  // in 0.1 usec
	}
	if(jitter > MaxJitter2){
		MaxJitter2 = jitter; // in usec
	}       // jitter should be 0
	if(jitter >= JitterSize2){
		jitter = JitterSize2-1;
	}
  JitterHistogram2[jitter]++;
	last_time=current_time;			
	
  (*PeriodicTask2)();               // execute user task
}

void Timer2A_Stop(void){
  NVIC_DIS0_R = 1<<23;        // 9) disable interrupt 23 in NVIC
  TIMER2_CTL_R = 0x00000000;  // 10) disable timer2A
}
