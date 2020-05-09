// *************Interpreter.c**************
// Students implement this as part of EE445M/EE380L.12 Lab 1,2,3,4 
// High-level OS user interface
// 
// Runs on LM4F120/TM4C123
// Jonathan W. Valvano 1/18/20, valvano@mail.utexas.edu
#include <stdint.h>
#include <string.h> 
#include <stdio.h>
#include <stdlib.h>
#include "../RTOS_Labs_common/OS.h"
#include "../RTOS_Labs_common/ST7735.h"
#include "../inc/ADCT0ATrigger.h"
#include "../inc/ADCSWTrigger.h"
#include "../RTOS_Labs_common/UART0int.h"
#include "../RTOS_Labs_common/eDisk.h"
#include "../RTOS_Labs_common/eFile.h"
#include "../RTOS_Labs_common/ADC.h"
#include "../inc/tm4c123gh6pm.h"
#include "../inc/CortexM.h"

#define UART_DEBUG 1
extern uint32_t MaxJitter;
extern uint32_t MaxJitter2;
extern DT_entry DT[];
// Print jitter histogram
void Jitter(int32_t MaxJitter, uint32_t const JitterSize, uint32_t JitterHistogram[]){
	
	//ST7735_Message(1,3,"MaxJitter1: ", MaxJitter);
	//ST7735_Message(1,4,"MaxJitter2: ", MaxJitter2);
	
	UART_OutString("MaxJitter: ");
	UART_OutUDec(MaxJitter);
	UART_OutString("\n\r");
	UART_OutString("Jitter Histogram: \n\r");
	for(int i = 0; i < JitterSize; i++){
		UART_OutUDec(JitterHistogram[i]);
		UART_OutString("\n\r");
	}
	
}

// *********** Command line interpreter (shell) ************
#define MAX_COMMAND_LENGTH 80 // max screen width
#define MAX_COMMAND_NAME_LENGTH 40
#define MAX_ARGS 10
#define MAX_ARG_LENGTH 20

void Interpreter(void){ 
  // the work is inside the while loop because for lab2 realmain, if you dont loop it, it will exit the function and undefined behavior happens
	while(1){
		UART_OutString("\n\r>");
		char buff[MAX_COMMAND_LENGTH+1];
		
		UART_InString(buff,MAX_COMMAND_LENGTH); // Grabs user input from the UART and places it into a buffer.

		buff[MAX_COMMAND_LENGTH]=0; // Ensures buffer is null-terminated
		int args_num=0;
		/* this whole block of code is disable because it is not reentrant or thread-safe
		char args[MAX_ARGS][MAX_ARG_LENGTH];
		char cmd[MAX_COMMAND_NAME_LENGTH];
		char * token;
		token = strtok_r(buff," ",&saveptr);
		//cmd=strtok_r(buff," ",&saveptr);
		if(token==NULL){
			UART_OutString("no command detected.\r\n");
			return;
		}

		strncpy(cmd,token,MAX_ARG_LENGTH); //the first token is always the command
		#ifdef UART_DEBUG
		UART_OutString("\r\nThe entered command is: ");
		UART_OutString(cmd);
		UART_OutString(" ;\r\n");
		#endif

		
		while(token!=NULL)
		{
			token=strtok_r(NULL," ",&saveptr);//not sure why this will not work
			//args[args_num] = strtok_r(NULL," ",&saveptr);
			if(token!=NULL){
				strncpy(args[args_num],token,MAX_ARG_LENGTH);//not sure why this will not work
				#ifdef UART_DEBUG
				UART_OutString("arg [");
				UART_OutUDec(args_num);
				UART_OutString("] is: ");			
				UART_OutString(args[args_num]);
				UART_OutString(" ; ");
				#endif
				args_num++;
			}
		}
		#ifdef UART_DEBUG
		UART_OutString("\r\n");
		#endif
		*/
		
		/////////new reentrant safe code/////
		char* args[MAX_ARGS];
		char* cmd;
		char *saveptr;
		cmd = strtok_r(buff, " ", &saveptr);
		args_num=-1;
		do { // This loop breaks up our UART input into tokens for easier parsing.
			args_num++;
			args[args_num] = strtok_r(NULL, " ", &saveptr);
		} while (args[args_num] != NULL);

		////////////////////////////////////
		
		if(strncmp(cmd,"help",strlen("help"))==0){ // If the help command is entered, display all commands and syntax.
			UART_OutString(" \r\n");
			UART_OutString("list of available commands: \r\n");
			UART_OutString("lcd_top [line] [message] [value]    display a [message] followed by a [value] in [line] of top screen\r\n");
			UART_OutString("lcd_bot [line] [message] [value]    display a [message] followed by a [value] in [line] of bottom screen\r\n");
			UART_OutString("adc_in [channel] [sample_size]      read [sample_size] samples of adc value and display them on UART\r\n");
			UART_OutString("os_mstime                           display current OS time in ms on UART\r\n");
			UART_OutString("clear_os_mstime                     reset os time\r\n");
			UART_OutString("jitter                              displays max jitter\r\n");
			UART_OutString("threadnum                           shows total number of created threads\r\n");
			UART_OutString(" \r\n");
		}
		else if(strncmp(cmd,"lcd_top",strlen("lcd_top"))==0){ // If the lcd_top command is entered, parse the data accordingly.
			int line=atoi(args[0]);
			char* message=args[1];
			int value=atoi(args[2]);
			ST7735_Message(0,line,message,value);
			UART_OutString("\n\r");

		}
		else if(strncmp(cmd,"lcd_bot",strlen("lcd_bot"))==0){ // If the lcd_bot command is entered, parse the data accordingly.
			int line=atoi(args[0]);
			char* message=args[1];
			int value=atoi(args[2]);
			ST7735_Message(1,line,message,value);
			UART_OutString("\n\r");

		}
		else if(strncmp(cmd,"adc_in",strlen("adc_in"))==0){ // If the adc_in command is entered, parse the data accordingly.
			int channel=atoi(args[0]);
			int sample_size;
			ADC_Init(channel);
			uint16_t adc_reading;
			
			if(args_num==2){ // Allows for the user to view multiple successive readings of the ADC.
				sample_size=atoi(args[1]);
				for(int i =0 ; i< sample_size; i++){
					adc_reading=ADC_In();
					UART_OutString("\n\r");
					UART_OutUDec(adc_reading);
					UART_OutString("\n\r");
				}
			} else { // Output only one reading
					adc_reading=ADC_In();
					UART_OutString("\n\r");
					UART_OutUDec(adc_reading);
					UART_OutString("\n\r");
			}
		}
		else if(strncmp(cmd,"os_mstime",strlen("os_mstime"))==0){ // If the os_mstime command is entered, output the OS_MsTime to the user.
			uint32_t mstime= OS_MsTime();
			UART_OutString("\n\r");
			UART_OutUDec(mstime);
			UART_OutString("\n\r");
		}
		else if(strncmp(cmd,"clear_os_mstime",strlen("clear_os_mstime"))==0){ // If the clear_os_mstime command is entered, restart the ms time.
			OS_ClearMsTime();
			UART_OutString("\r\nOS time is reset\r\n");
		}
		else if(strncmp(cmd,"jitter",strlen("jitter"))==0){ // If the jitter command is entered, output the max jitter to the user.
			UART_OutString("\r\nJitter: ");
			UART_OutUDec(MaxJitter);
			UART_OutString("\n\r");
			UART_OutUDec(MaxJitter2);
			UART_OutString("\n\r");		
		}
		else if(strncmp(cmd,"threadnum",strlen("threadnum"))==0){ // If the threadnum command is entered, output the total number of created threads to the user.
			UART_OutString("\r\ntotal number of threads created: ");
			UART_OutUDec(OS_TotalThreadCount());
			UART_OutString("\n\r");			

		}
		else if(strncmp(cmd,"ls",strlen("ls"))==0){ //list file
			eFile_Mount();
			char* file_names[MAX_NUMBER_OF_FILES];
			int file_num=eFile_List(file_names);
			UART_OutString("\n\r");
			for(int i =0 ; i < file_num; i++)
			{
				UART_OutString(file_names[i]);
				UART_OutString(" ");
			}
		}
		else if(strncmp(cmd,"touch",strlen("touch"))==0){	//create file
			eFile_Mount();
			eFile_Create(args[0]);
		}
		else if(strncmp(cmd,"rm",strlen("rm"))==0){	//remove file
			eFile_Mount();
			eFile_Delete(args[0]);
		}
		else if(strncmp(cmd,"cat",strlen("cat"))==0){	//display file
			char data;
			eFile_Mount();
			eFile_ROpen(args[0]);
			UART_OutString("\n\r");
			for(int i =0 ; i<eFile_Size(args[0]); i++)
			{
				eFile_ReadNext(&data);
				UART_OutChar(data);
			}
			eFile_RClose();
		}
		else if(strncmp(cmd,"echo",strlen("echo"))==0){	//used for write things into file
			char data;
			if(args_num==1)
				UART_OutString(args[0]);
			if(args_num==3 && strcmp(args[1],">>")==0)
			{
				eFile_Mount();
				eFile_WOpen(args[2]);
				//UART_OutString(args[2]);
				for(int i =0 ; i < strlen(args[0]) ; i ++)
				{
						eFile_Write(args[0][i]);
						//UART_OutChar(args[0][i]);
				}
				eFile_WClose();
			}
		}
		else if(strncmp(cmd,"DMAecho",strlen("echo"))==0){	//used for write things into file
			char data;
			if(args_num==1)
				UART_OutString(args[0]);
			if(args_num==3 && strcmp(args[1],">>")==0)
			{
				eFile_Mount();
				eFile_WOpen(args[2]);
				//UART_OutString(args[2]);
				for(int i =0 ; i < strlen(args[0]) ; i ++)
				{
						eFile_Write(args[0][i]);
						//UART_OutChar(args[0][i]);
				}
				eFile_DMAWClose();
			}
		}
		else if(strncmp(cmd,"block_write",strlen("block_write"))==0){
			uint8_t buff[512];
			memset(buff,0xFF,512 );
			eDisk_Write(0, buff, 20, 1);
		}
		else if(strncmp(cmd,"dma_write",strlen("dma_write"))==0){
			uint8_t buff[516];
			memset(buff,0xFF,516 );
			DMA_SD_Write(0, buff, 20, 1);
		}
		else if(strncmp(cmd,"block_read",strlen("block_read"))==0){
			if(args_num==0)
			{
				uint8_t buff[512];
				memset(buff,0xFF,512 );
				eDisk_Read(0, (uint8_t*)DT, 0, 1);
			}
			else{
				uint8_t buff[512];
				memset(buff,0xFF,512 );
				eDisk_Read(0, buff, atoi(args[0]), 1);
			}
		}
		else if(strncmp(cmd,"dma_read",strlen("dma_read"))==0){
			if(args_num==0)
			{
				uint8_t buff[512];
				memset(buff,0xFF,512 );
				DMA_SD_Read(0, (uint8_t*)DT, 0, 1);
			}
			else{
				uint8_t buff[512];
				memset(buff,0xFF,512 );
				DMA_SD_Read(0, buff, atoi(args[0]), 1);
			}
		}
		else if(strncmp(cmd,"format",strlen("format"))==0){	//used for write things into file
			eFile_Mount();
			eFile_Format();
		}
		else if(strncmp(cmd,"load_sd",strlen("load_sd"))==0){	//used for write things into file
			eFile_Mount();
		}
		else{ // If the user entered an invalid command, notify them.
			UART_OutString("\r\ninvalid command, please check spelling or enter 'help' to see a list of available commands\r\n");
		}
		
	}
}

