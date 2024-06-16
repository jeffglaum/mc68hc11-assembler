//
//  common.h
//  MC68HC11 Assembler
//
//  Created by Jeff Glaum on 9/22/11.
//  Copyright 2011 __MyCompanyName__. All rights reserved.
//


typedef unsigned char  UINT8;
typedef unsigned short UINT16;
typedef unsigned long  UINT32;

#define START_SYMBOL_NAME       "START"

#define ASM_FILE_EXTENSION      "asm"
#define S19_FILE_EXTENSION      "s19"
#define SYM_FILE_EXTENSION      "sym"
#define LST_FILE_EXTENSION      "lst"

#define MAX_LINE_LENGTH         256
#define MAX_SYMBOL_NAME_LENGTH  16
#define MAX_SYMBOL_COUNT        2000

#define MAX_S19_CHARPAIRS       32
#define MAX_S19_CHARS           (MAX_S19_CHARPAIRS * 2)
#define NUM_SREC_TYPE_CHARS		2			// Number of S-record type characters.		
#define NUM_CHARPAIR_CHARS		2			// Number of code/data character.
#define NUM_LOADADDRESS_CHARS	4			// Number of load address characters.
#define NUM_CHECKSUM_CHARS		2			// Number of checksum characters.

// S-record type enumeration
enum SREC_TYPES
{
	INVALID_SREC,			// Invalid record.
	S0,						// Header record.
	S1,						// Code/data record.
	S2,						// N/A to DSP56000 programming.
	S3,						// Code/data record and the 4-byte reside address.
	S4,						// N/A to DSP56000 programming.
	S5,						//		"
	S6,						//		"
	S7,						// Termination record for a block of S3 records.
	S8,						// N/A to DSP56000 programming.
	S9						// Termination record for a block of S1 records.
};

typedef enum _symboltype_
{
    SYMBOL_TYPE_NUMBER_8BIT,    // 8-bit symbol
    SYMBOL_TYPE_NUMBER_16BIT,   // 16-bit symbol
    SYMBOL_TYPE_STRING          // String symbol
} SYMBOLTYPE;

typedef union  _symbolvalue_
{
    UINT8  nsymbolValue8;
    UINT16 nsymbolValue16;
    char   symbolValueStr[MAX_LINE_LENGTH];            
} SYMBOLVALUE;

typedef struct _symbol_
{
    char       symbolName[MAX_SYMBOL_NAME_LENGTH];
    SYMBOLTYPE symbolType;
    SYMBOLVALUE u;
} SYMBOL;


typedef enum _addrmode_
{
    IMM,        // Immediate
    INH,        // Inherent (instruction data)
    DIR,        // Direct (same as extended but only first 256 bytes)
    EXT,        // Extended
    REL,        // Relative
    INDX,       // Indirect-X
    INDY,       // Indirect-Y
    INVALID = -1
} ADDRMODE;

typedef struct _sourcefile_
{
    char *pFile;        // Pointer to file contents
    int  fileSize;      // File size
    char *piterOffset;  // Current file read pointer
    int  byteOffset;    // Current file read byte offset
    bool fEOF;          // EOF flag
    int  lineNumber;    // Current file line number
} SOURCEFILE;
