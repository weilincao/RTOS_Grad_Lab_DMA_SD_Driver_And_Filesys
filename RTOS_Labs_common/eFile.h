/**
 * @file      eFile.h
 * @brief     high-level file system
 * @details   This file system sits on top of eDisk.
 * @version   V1.0
 * @author    Valvano
 * @copyright Copyright 2020 by Jonathan W. Valvano, valvano@mail.utexas.edu,
 * @warning   AS-IS
 * @note      For more information see  http://users.ece.utexas.edu/~valvano/
 * @date      Jan 12, 2020
 ******************************************************************************/
#include <stdint.h>

#define BLOCK_SIZE 					512
#define MAX_FILE_NAME 			26
#define FILESYS_SIZE 				1024*1024
#define NUMBER_OF_FAT_ENTRY FILESYS_SIZE/BLOCK_SIZE
#define FAT_ENTRY_SIZE 			2
#define FAT_SIZE 						NUMBER_OF_FAT_ENTRY*FAT_ENTRY_SIZE
#define DT_ENTRY_SIZE 			32
#define DT_SIZE							512
#define MAX_NUMBER_OF_FILES	512/32

typedef struct File_Allocation_Table_Entry { //each fat_entry takes 2 byte;
	uint16_t next_entry;
}FAT_entry;

typedef struct Directory_Table_Entry {//each directory table entry takes 32 byte
	char name[MAX_FILE_NAME];
	uint16_t starting_block;
	uint32_t file_size;
}DT_entry;



/**
 * @details This function must be called first, before calling any of the other eFile functions
 * @param  none
 * @return 0 if successful and 1 on failure (already initialized)
 * @brief  Activate the file system, without formating
 */
int eFile_Init(void); // initialize file system

/**
 * @details Erase all files, create blank directory, initialize free space manager
 * @param  none
 * @return 0 if successful and 1 on failure (e.g., trouble writing to flash)
 * @brief  Format the disk
 */
int eFile_Format(void); // erase disk, add format

/**
 * @details Mount disk and load file system metadata information
 * @param  none
 * @return 0 if successful and 1 on failure (e.g., already mounted)
 * @brief  Mount the disk
 */
int eFile_Mount(void); // mount disk and file system

/**
 * @details Create a new, empty file with one allocated block
 * @param  name file name is an ASCII string up to seven characters
 * @return 0 if successful and 1 on failure (e.g., already exists)
 * @brief  Create a new file
 */
int eFile_Create(const char name[]);  // create new file, make it empty 

/**
 * @details Open the file for writing, read into RAM last block
 * @param  name file name is an ASCII string up to seven characters
 * @return 0 if successful and 1 on failure (e.g., trouble reading from flash)
 * @brief  Open an existing file for writing
 */
int eFile_WOpen(const char name[]);      // open a file for writing 

/**
 * @details Save one byte at end of the open file
 * @param  data byte to be saved on the disk
 * @return 0 if successful and 1 on failure (e.g., trouble writing to flash)
 * @brief  Format the disk
 */
int eFile_Write(const char data);  

/**
 * @details Close the file, leave disk in a state power can be removed.
 * This function will flush all RAM buffers to the disk.
 * @param  none
 * @return 0 if successful and 1 on failure (e.g., trouble writing to flash)
 * @brief  Close the file that was being written
 */
int eFile_WClose(void); // close the file for writing

/**
 * @details Open the file for reading, read first block into RAM
 * @param  name file name is an ASCII string up to seven characters
 * @return 0 if successful and 1 on failure (e.g., trouble reading from flash)
 * @brief  Open an existing file for reading
 */
int eFile_ROpen(const char name[]);      // open a file for reading 
   
/**
 * @details Read one byte from disk into RAM
 * @param  pt call by reference pointer to place to save data
 * @return 0 if successful and 1 on failure (e.g., trouble reading from flash)
 * @brief  Retreive data from open file
 */
int eFile_ReadNext(char *pt);       // get next byte 
                              
/**
 * @details Close the file, leave disk in a state power can be removed.
 * @param  none
 * @return 0 if successful and 1 on failure (e.g., wasn't open)
 * @brief  Close the file that was being read
 */
int eFile_RClose(void); // close the file for writing

/**
 * @details Delete the file with this name, recover blocks so they can be used by another file
 * @param  name file name is an ASCII string up to seven characters
 * @return 0 if successful and 1 on failure (e.g., file doesn't exist)
 * @brief  delete this file
 */
int eFile_Delete(const char name[]);  // remove this file 

/**
 * @details Open a (sub)directory, read into RAM
 * @param directory name is an ASCII string up to seven characters
 * if subdirectories are supported (optional, empty sring for root directory)
 * @return 0 if successful and 1 on failure (e.g., trouble reading from flash)
 */
int eFile_DOpen(const char name[]);
	
/**
 * @details Retreive directory entry from open directory
 * @param pointers to return file name and size by reference
 * @return 0 if successful and 1 on failure (e.g., end of directory)
 */
int eFile_DirNext(char *name[], unsigned long *size);

/**
 * @details Close the directory
 * @param none
 * @return 0 if successful and 1 on failure (e.g., wasn't open)
 */
int eFile_DClose(void);

/**
 * @details Unmount and deactivate the file system.
 * @param  none
 * @return 0 if successful and 1 on failure (e.g., trouble writing to flash)
 * @brief  Close the disk
 */
int eFile_Close(void); 
int eFile_List(char* name[]);
int eFile_Size(const char name[]);
