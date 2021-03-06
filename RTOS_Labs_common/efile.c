// filename ************** eFile.c *****************************
// High-level routines to implement a solid-state disk 
// Students implement these functions in Lab 4
// Jonathan W. Valvano 1/12/20
#include <stdint.h>
#include <string.h>
#include "../RTOS_Labs_common/OS.h"
#include "../RTOS_Labs_common/eDisk.h"
#include "../RTOS_Labs_common/eFile.h"
#include "../RTOS_Labs_common/UART0int.h"
#include <stdio.h>
#define EFILE_DEBUG 1
FAT_entry FAT[NUMBER_OF_FAT_ENTRY]; // This is the RAM copy of the FAT.
DT_entry DT[MAX_NUMBER_OF_FILES]; // This is the RAM copy of the directory.
unsigned char FILE_BLOCK[BLOCK_SIZE];
int FILE_BLOCK_NUM;
uint8_t MOUNTED = 0;

//---------- eFile_Init-----------------
// Activate the file system, without formating
// Input: none
// Output: 0 if successful and 1 on failure (already initialized)
int eFile_Init(void){ // initialize file system
	//eDisk_Init(0);
	DMA_SD_Init();
	for(int i=0; i<NUMBER_OF_FAT_ENTRY;i++){
		FAT[i].next_entry=0;
	}
	for(int i = 0 ; i < MAX_NUMBER_OF_FILES; i ++ ){
		DT[i].starting_block=0;
		DT[i].file_size=0;
	}
  return 0;   // replace
}

//---------- eFile_Format-----------------
// Erase all files, create blank directory, initialize free space manager
// Input: none
// Output: 0 if successful and 1 on failure (e.g., trouble writing to flash)
int eFile_Format(void){ // erase disk, add format
	unsigned char empty_block[BLOCK_SIZE];
	for(int i = 0 ; i < BLOCK_SIZE; i++){
		empty_block[i]=0;
	}
	for(int i = 0; i < NUMBER_OF_FAT_ENTRY; i++){
		DMA_SD_Write(0,empty_block,i,1);
	}
	memset(&DT, 0, DT_SIZE);
	memset(&FAT, 0, FAT_SIZE);
	
	DT[0].name[0] = '*';
	DT[0].name[1] = '\0';
	DT[0].starting_block = 5; // First free block
	
	for(int i = 5; i < NUMBER_OF_FAT_ENTRY; i++){ // Resets the FAT free space linked list
		FAT[i].next_entry = i+1;
		if(i == NUMBER_OF_FAT_ENTRY - 1){ // Last entry of free space linked list needs NULL pointer.
			FAT[i].next_entry = 0;
		}
	}
	DMA_SD_Write(0,(unsigned char *)DT, 0, 1);
	DMA_SD_Write(0,(unsigned char *)FAT, 1, 4);
	//eFile_Mount();
  return 0;
}

//---------- eFile_Mount-----------------
// Mount the file system, without formating
// Input: none
// Output: 0 if successful and 1 on failure
int eFile_Mount(void){ // initialize file system
	int status;
	status=DMA_SD_Read(0,(unsigned char*)DT,0,1);
	if(status)
	{
		#ifdef EFILE_DEBUG
		UART_OutString("\n\rDT loading fail, error code: ");
		UART_OutUDec(status);
		UART_OutString("\n\r");
		#endif
		return 1;
	}
	status=DMA_SD_Read(0,(unsigned char*)FAT,1,4);
	if(status)
	{
		#ifdef EFILE_DEBUG
		UART_OutString("\n\rFAT loading fail, error code: ");
		UART_OutUDec(status);
		UART_OutString("\n\r");
		#endif
		return 1;
	}
	MOUNTED = 1;
  return 0;
}


//---------- eFile_Create-----------------
// Create a new, empty file with one allocated block
// Input: file name is an ASCII string up to seven characters 
// Output: 0 if successful and -1 or -2 on failure (e.g., trouble writing to flash)
int eFile_Create(const char name[]){  // create new file, make it empty 
	if(!MOUNTED){
		eFile_Mount();
	}
	
	for(int i = 1 ; i < MAX_NUMBER_OF_FILES ; i++){ // Check to make sure a file with that name doesn't already exist
		if(strcmp(name, DT[i].name) == 0){
			return 0; // File already exists, so just act like the file was created successfully.
		}
	}
	
	for(int i = 1 ; i < MAX_NUMBER_OF_FILES ; i++ ){
		if(DT[i].name[0] == 0){ // There is an available empty file entry
			strcpy(DT[i].name, name);
			DT[i].file_size = 0;
			if(DT[0].starting_block != 0){//allocate a block from the free list
				DT[i].starting_block = DT[0].starting_block;
				DT[0].starting_block = FAT[DT[0].starting_block].next_entry; // have DT[0] connect to the next entry in FAT
				FAT[DT[i].starting_block].next_entry = 0; // only assign one block for newly created file; which implies you need to terminate the first block will 0 in FAT
			} else {
				return -1; // No empty blocks
			}
			DMA_SD_Write(0,(unsigned char*)DT, 0, 1);
			DMA_SD_Write(0,(unsigned char*)FAT, 1, 4);
			return 0;
		}
	}
  return -2;   // No more files can be created
}


int opened_file_index=-1;
int file_permission=-1;
int file_cursor = 0; //points to which bit of the 512 bit are being read

enum FILE_PERMISSION{
	READ,
	WRITE
};

//---------- eFile_WOpen-----------------
// Open the file, read last block into RAM 
// Input: file name is a single ASCII letter
// Output: 0 if successful and 1 on failure (e.g., trouble writing to flash)
int eFile_WOpen( const char name[]){      // open a file for writing 
	if(!MOUNTED){
		eFile_Mount();
	}
	for(int i = 0; i < MAX_FILE_NAME; i++){
		if(strcmp(name, DT[i].name) == 0){
			opened_file_index = i;
			file_permission = WRITE;
			break;
		}
	}
	int curr = DT[opened_file_index].starting_block;
	while(FAT[curr].next_entry != 0){ // Goes through linked list to find last block of file.
		curr = FAT[curr].next_entry;
	}
	
	int status=DMA_SD_Read(0,FILE_BLOCK, curr,1); // Reads last block of file into RAM buffer.
	if(status)
	{
		#ifdef EFILE_DEBUG
		UART_OutString("\n\r Wopen failed!, error code: ");
		UART_OutUDec(status);
		UART_OutString("\n\r");
		#endif
		return 1;
	}
	FILE_BLOCK_NUM = curr;
  return 0;
}

//---------- eFile_Write-----------------
// save at end of the open file
// Input: data to be saved
// Output: 0 if successful and something else on failure (e.g., trouble writing to flash)
int eFile_Write(const char data){
	if(!MOUNTED){
		eFile_Mount();
	}
	if(file_permission == READ)
		return -1;
	if(opened_file_index == -1)
		return -2;
	int location = DT[opened_file_index].file_size%BLOCK_SIZE;
	if(location == 0 && DT[opened_file_index].file_size != 0){ //we need to append a free block because the current block is full
		if(DT[0].starting_block != 0){ // We have a free block we can append
			DMA_SD_Write(0, FILE_BLOCK, FILE_BLOCK_NUM, 1);
			for(int i = 0; i < BLOCK_SIZE; i++){
				FILE_BLOCK[i] = 0;
			}
			FILE_BLOCK[0] = data;
			FAT[FILE_BLOCK_NUM].next_entry = DT[0].starting_block; // Update FAT with the new block
			FILE_BLOCK_NUM = DT[0].starting_block; // Updates the current	file block we are writing to		
			DT[0].starting_block = FAT[DT[0].starting_block].next_entry; // Updates free block linked list
		} else {
			return -3; // Cannot append to file.
		}
	} else {
		FILE_BLOCK[location] = data; // Writes data to block
	}
	DT[opened_file_index].file_size++; // Increments file size
	return 0;
}

//---------- eFile_WClose-----------------
// close the file, left disk in a state power can be removed
// Input: none
// Output: 0 if successful and 1 on failure (e.g., trouble writing to flash)
int eFile_WClose(void){ // close the file for writing
	if(!MOUNTED){
		eFile_Mount();
	}
	if(opened_file_index == -1){ 
		return -1;
	}
	int code = 0;
  if((code = DMA_SD_Write(0,(unsigned char *)DT,0,1))){
		#ifdef EFILE_DEBUG
		UART_OutString("\n\r DAM write DT in wclose fail, error code: ");
		UART_OutUDec(code);
		UART_OutString("\n\r");
		#endif
		return code;
	}
	if((code = DMA_SD_Write(0,(unsigned char *)FAT,1,4))){
		#ifdef EFILE_DEBUG
		UART_OutString("\n\r DAM write FAT in wclose fail, error code: ");
		UART_OutUDec(code);
		UART_OutString("\n\r");
		#endif
		return code;
	}
	if((code = DMA_SD_Write(0, FILE_BLOCK, FILE_BLOCK_NUM ,1))){
		#ifdef EFILE_DEBUG
		UART_OutString("\n\r DAM write block in wclose fail, error code: ");
		UART_OutUDec(code);
		UART_OutString("\n\r");
		#endif
		return code;
	}	
	
	opened_file_index = -1;
	file_permission = READ;
  return 0;
}
int eFile_DMAWClose(void){ // close the file for writing
	if(!MOUNTED){
		eFile_Mount();
	}
	if(opened_file_index == -1){ 
		return -1;
	}
	int code = 0;
  if((code = DMA_SD_Write(0,(unsigned char *)DT,0,1))){
		return code;
	}
	if((code = DMA_SD_Write(0,(unsigned char *)FAT,1,4))){
		return code;
	}
	if((code = DMA_SD_Write(0, FILE_BLOCK, FILE_BLOCK_NUM ,1))){
		#ifdef EFILE_DEBUG
		UART_OutString("\n\r DAM write block in wclose fail, error code: ");
		UART_OutUDec(code);
		UART_OutString("\n\r");
		#endif
		return code;
	}	
	
	opened_file_index = -1;
	file_permission = READ;
  return 0;
}

//---------- eFile_ROpen-----------------
// Open the file, read first block into RAM 
// Input: file name is a single ASCII letter
// Output: 0 if successful and 1 on failure (e.g., trouble read to flash)
int eFile_ROpen( const char name[]){      // open a file for reading 
	if(!MOUNTED){
		eFile_Mount();
	}
	file_cursor = 0;
	for(int i = 0; i < MAX_FILE_NAME; i++)
	{
		if(strcmp(name, DT[i].name) == 0)
		{
			opened_file_index = i;
			file_permission = READ;
			break;
		}
	}
	int curr = DT[opened_file_index].starting_block;
	int status = DMA_SD_Read(0,FILE_BLOCK, curr,1); // Reads last block of file into RAM buffer.
	if(status)
	{
		#ifdef EFILE_DEBUG
		UART_OutString("\n\r ropen failed!, error code: ");
		UART_OutUDec(status);
		UART_OutString("\n\r");
		#endif
		return 1;
	}
	FILE_BLOCK_NUM = curr;
  return 0; 
}
 
//---------- eFile_ReadNext-----------------
// retreive data from open file
// Input: none
// Output: return by reference data
//         0 if successful and 1 on failure (e.g., end of file)
int eFile_ReadNext( char *pt){       // get next byte 
	if(!MOUNTED){
		eFile_Mount();
	}
  //if(file_permission == WRITE)
	//	return -1;
	if(opened_file_index == -1)
		return -2;
	if(file_cursor > DT[opened_file_index].file_size){ // Reached end of file
		return -3;
	}
	*pt = FILE_BLOCK[file_cursor%BLOCK_SIZE];
	file_cursor++;
	if(file_cursor % BLOCK_SIZE == 0 ){ // Check to see if we need to load in the next block
		FILE_BLOCK_NUM = FAT[FILE_BLOCK_NUM].next_entry;
		DMA_SD_Read(0, FILE_BLOCK, FILE_BLOCK_NUM, 1); // Loads next block into RAM
	}
  return 0;
}
    
//---------- eFile_RClose-----------------
// close the reading file
// Input: none
// Output: 0 if successful and 1 on failure (e.g., wasn't open)
int eFile_RClose(void){ // close the file for writing
	if(!MOUNTED){
		eFile_Mount();
	}
  if(opened_file_index == -1)
		return -1;
	opened_file_index = -1;
	file_permission = READ;
  return 0;
}


//---------- eFile_Delete-----------------
// delete this file
// Input: file name is a single ASCII letter
// Output: 0 if successful and 1 on failure (e.g., trouble writing to flash)
int eFile_Delete( const char name[]){  // remove this file 
	if(!MOUNTED){
		eFile_Mount();
	}
	int curr = DT[0].starting_block;
	while(FAT[curr].next_entry != 0){
		curr = FAT[curr].next_entry;
	}
	for(int i = 0 ; i < MAX_NUMBER_OF_FILES ; i++){
		if(strcmp(name, DT[i].name) == 0){
			DT[i].name[0] = 0;
			DT[i].file_size = 0;
			FAT[curr].next_entry = DT[i].starting_block; // Appends file's block list to free block list
			DT[i].starting_block = 0;
			int code = 0;
			code = DMA_SD_Write(0, (unsigned char *)DT, 0, 1) || DMA_SD_Write(0, (unsigned char *)FAT, 1, 4); // Write FAT and directory back to disk.
			return code;
		}
	}
	
  return -1;
}                             


//---------- eFile_DOpen-----------------
// Open a (sub)directory, read into RAM
// Input: directory name is an ASCII string up to seven characters
//        (empty/NULL for root directory)
// Output: 0 if successful and 1 on failure (e.g., trouble reading from flash)
int eFile_DOpen( const char name[]){ // open directory
   
  return 1;   // replace
}
  
//---------- eFile_DirNext-----------------
// Retreive directory entry from open directory
// Input: none
// Output: return file name and size by reference
//         0 if successful and 1 on failure (e.g., end of directory)
int eFile_DirNext( char *name[], unsigned long *size){  // get next entry 
   
  return 1;   // replace
}

//---------- eFile_DClose-----------------
// Close the directory
// Input: none
// Output: 0 if successful and 1 on failure (e.g., wasn't open)
int eFile_DClose(void){ // close the directory
   
  return 1;   // replace
}


//---------- eFile_Close-----------------
// Deactivate the file system
// Input: none
// Output: 0 if successful and 1 on failure (not currently open)
int eFile_Close(void){ 
	int code = 0;
	code = DMA_SD_Write(0, (unsigned char *)DT, 0, 1) || DMA_SD_Write(0, (unsigned char *)FAT, 1, 4); // Write FAT and directory back to disk.
	MOUNTED = 0;
	return code;
}

DT_entry* find_DT_entry(const char name[]){
	for(int i = 0 ; i < MAX_NUMBER_OF_FILES ; i++){
		if(strcmp(name, DT[i].name) == 0){
			return &DT[i];
		}
	}
	return (DT_entry*)0; // Error
}
int eFile_List(char* name[]){
	int number_of_files=0;	
	for(int i = 1 ; i < MAX_NUMBER_OF_FILES ; i++){
		if(DT[i].name[0] != 0){
			name[number_of_files] = DT[i].name;
			number_of_files++;
		}
	}
	return number_of_files;
}


int eFile_Size(const char name[]){
	return find_DT_entry(name)->file_size;
}
