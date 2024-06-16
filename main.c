//
//  main.c
//  MC68HC11 Assembler
//
//  Created by Jeff Glaum on 9/16/11.
//  Copyright 2011 __MyCompanyName__. All rights reserved.
//

#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "common.h"
#include "utility.h"
#include "opcodes.h"


UINT16 g_startAddress;
UINT16 symbolCount;
SYMBOL symbols[MAX_SYMBOL_COUNT];


// NOTES:
// * Start Address: If the symbols "START" is defined, this is used as the program's start address else the first ORG block is used.
//

int pushSymbol(char *pszName, SYMBOLTYPE Type, void *pValue)
{
    if (symbolCount > MAX_SYMBOL_COUNT)
    {
        printf("ERROR: Maximum symbol count exceeded (%d)\r\n", MAX_SYMBOL_COUNT);
        return -1;
    }

    strncpy(symbols[symbolCount].symbolName, pszName, MAX_SYMBOL_NAME_LENGTH);
    symbols[symbolCount].symbolType = Type;
    
    switch(Type)
    {
        case SYMBOL_TYPE_NUMBER_8BIT:
            symbols[symbolCount].u.nsymbolValue8 = ((*(UINT8 *)pValue) & 0xFF);
            break;
        case SYMBOL_TYPE_NUMBER_16BIT:
            symbols[symbolCount].u.nsymbolValue16 = ((*(UINT16 *)pValue) & 0xFFFF);
            break;
        case SYMBOL_TYPE_STRING:
            strncpy(symbols[symbolCount].u.symbolValueStr, pValue, MAX_LINE_LENGTH);
            break;
        default:
            break;
    }
    
    ++symbolCount;
    
    return 0;
}


bool findSymbol(char *pszName, SYMBOLTYPE *pType, SYMBOLVALUE **pValue)
{
    UINT16 nCount;
    
    for (nCount=0 ; nCount <= symbolCount ; nCount++)
    {
        if (strcasecmp(symbols[nCount].symbolName, pszName) == 0)
        {
            *pType  = symbols[nCount].symbolType;
            *pValue = &symbols[nCount].u;
            break;
        }
    }
    
    if (nCount <= symbolCount)
        return true;
        
    return false;
}


INSTRUCTION *lookUpMneumonic(char *pszMneumonic)
{
    INSTRUCTION *pTemp = &instructions[0];
    
    while (pTemp->mnemonic[0] != '\0')
    {
        if (strcasecmp(pTemp->mnemonic, pszMneumonic) == 0)
        {
            return pTemp;
        }
        ++pTemp;
    }
    return NULL;
}


INSTRUCTION *lookUpMatchingAddrMode(INSTRUCTION *pInst, ADDRMODE addrMode)
{
    INSTRUCTION *pStart = pInst;
    
    do
    {
        if (pStart->addrMode == addrMode)
            return pStart;
        
        ++pStart;
    }
    while (strcasecmp(pStart->mnemonic, pInst->mnemonic) == 0);

    return NULL;
}


int computeAddrMode(UINT16 nCurrentAddr, INSTRUCTION *pInst, char *pszParamString, ADDRMODE *paddrMode, UINT16 *pnParamValue)
{
    int  iRet = 0;
    char szValue[MAX_SYMBOL_NAME_LENGTH];
    ADDRMODE addrMode;
    
    // No parameter string == Inherent mode
    //
    if (pszParamString == NULL || *pszParamString == '\0')
    {
        *pnParamValue = 0;
        *paddrMode    = INH;
        return 0;
    }
    
    *paddrMode = INVALID;
    if (*pszParamString == '#')
    {
        *paddrMode = IMM;
        strncpy(szValue, (pszParamString + 1), MAX_SYMBOL_NAME_LENGTH);
    } 
    else if (isIndirectParams(pszParamString, &szValue[0], &addrMode))               
    {
        *paddrMode = addrMode;
    }
    else
    {
        strncpy(szValue, pszParamString, MAX_SYMBOL_NAME_LENGTH);
    }
    
    // Trim any trailing whitespace
    //
    trimTrailingWhitespace(szValue);
    
    switch(*paddrMode)
    {
        case IMM:
            // Immediate
            // Value can be number ($, %, @, [0-9]) or a known symbol
            if (isValidNumber(szValue))
            {
                convertToNumber(szValue, pnParamValue);
            }
            else if (isValidSymbolName(szValue))
            {
                SYMBOLTYPE   tempSymbolType;
                SYMBOLVALUE *tempSymbolValue;
                
                // Look-Up the symbol - if we can't find it, it's an error
                if (!findSymbol(szValue, &tempSymbolType, &tempSymbolValue))
                {
                    // TODO - special return type - not a failure (yet) but instead, we couldn't find the symbol in the symbol table.
                    iRet = -2;
                }
                else
                {
                    *pnParamValue = tempSymbolValue->nsymbolValue16;
                }
            }
            else
            {
                printf("ERROR: Invalid immediate address mode (invalid symbol name '%s')\r\n", szValue);
                iRet = -1;
            }
            break;
        case INDX:
        case INDY:
            // Indirect-X or Y
            // Value can be number ($, %, @, [0-9]) or a known symbol
            if (isValidNumber(szValue))
            {
                convertToNumber(szValue, pnParamValue);
                if (*pnParamValue > 255)
                {
                    printf("ERROR: Invalid indirect address mode (value can't be larger than 256 bytes)\r\n");
                    iRet = -1;
                }
            }
            else if (isValidSymbolName(szValue))
            {
                SYMBOLTYPE   tempSymbolType;
                SYMBOLVALUE *tempSymbolValue;
                
                // Look-Up the symbol - if we can't find it, it's an error
                if (!findSymbol(szValue, &tempSymbolType, &tempSymbolValue))
                {
                    printf("ERROR: Invalid indirect address mode (unknown symbol name '%s' - forward declaration?)\r\n", szValue);
                    iRet = -1;
                }
                else if (tempSymbolType != SYMBOL_TYPE_NUMBER_8BIT)
                {
                    printf("ERROR: Invalid indirect address mode (symbol '%s' value can't be larger than 256 bytes)\r\n", szValue);
                    iRet = -1;
                }
                *pnParamValue = tempSymbolValue->nsymbolValue8;
            }
            else
            {
                printf("ERROR: Invalid indirect address mode (invalid symbol name '%s')\r\n", szValue);
                iRet = -1;
            }
            break;
        case REL:
        case DIR:
        case EXT:
        case INVALID:
        default:
            // TODO - REL - only certain commands can handle it.
            //
            // Value can be number ($, %, @, [0-9]) or a known symbol
            if (isValidNumber(szValue))
            {
                int nTemp;
                convertToNumber(szValue, pnParamValue);
                if (*pnParamValue <= 255)
                {
                    *paddrMode = DIR;
                }
                // TODO - fix "+2 math" below - used because relative address is from the *end* of the branch instruction.
                else if (pInst->addrMode == REL &&
                         ((nTemp = ((int)*pnParamValue - (int)(nCurrentAddr + 2))  <= 127) || 
                         (nTemp >= -128)))
                {
                    // Change to relative address mode, parameter value becomes the negative relative offset.
                    *paddrMode    = REL;
                    *pnParamValue = (UINT8)nTemp;  // TODO
                }
                else
                {
                    // Last choice is extended mode since it requires an additional byte and cycle to complete
                    *paddrMode = EXT;
                }
            }
            else if (isValidSymbolName(szValue))
            {
                SYMBOLTYPE   tempSymbolType;
                SYMBOLVALUE *tempSymbolValue;
                    
                // Look-Up the symbol - if we can't find it, it's an error - also note that it has to be a 16-bit address
                if (!findSymbol(szValue, &tempSymbolType, &tempSymbolValue) || tempSymbolType != SYMBOL_TYPE_NUMBER_16BIT)
                {
                    // TODO - special return type - not a failure (yet) but instead, we couldn't find the symbol in the symbol table.
                    iRet = -2;
                }
                else
                {
                    int nTemp;
                    
                    if (tempSymbolValue->nsymbolValue16 <= 255)
                    {
                        *paddrMode = DIR;
                        *pnParamValue = (UINT8)(tempSymbolValue->nsymbolValue16 & 0xFF);
                    }
                    // TODO - fix code that determines if REL can be supported
                    // TODO - fix "+2 math" below - used because relative address is from the *end* of the branch instruction.
                    else if (pInst->addrMode == REL &&
                             (((nTemp = ((int)tempSymbolValue->nsymbolValue16 - (int)(nCurrentAddr + 2)))  <= 127) || 
                              (nTemp >= -128)))
                    {
                        // Change to relative address mode, parameter value becomes the negative relative offset.
                        *paddrMode    = REL;
                        *pnParamValue = (UINT8)nTemp;  // TODO
                    }
                    else
                    {
                        // Last choice is extended mode since it requires an additional byte and cycle to complete
                        *paddrMode    = EXT;
                        *pnParamValue = (UINT16)tempSymbolValue->nsymbolValue16;
                    }
                }

            }
            else
            {
                printf("ERROR: Invalid address mode (invalid symbol name '%s')\r\n", szValue);
                iRet = -1;
            }
            break;
    }
    
    return iRet;
}

char convertToChar(UINT8 nNumber)
{
    if (nNumber >= 0 && nNumber <= 9)
        return (char)('0' + nNumber);
    
    if (nNumber >= 0xA && nNumber <= 0xF)
        return (char)('A' + nNumber - 0xA);
    
    return '0';
}

int writeSRecordLine(int fpSRecord, UINT16 nAddr, char *pDataChars, UINT16 nNumDataChars, UINT16 nChecksum)
{
    char szTemp[NUM_SREC_TYPE_CHARS + NUM_CHARPAIR_CHARS + NUM_LOADADDRESS_CHARS + 1];
    UINT16 nCharPairs = ((NUM_LOADADDRESS_CHARS + nNumDataChars + NUM_CHECKSUM_CHARS) >> 1);
    UINT16 nSRecChecksum = nChecksum;
    
    sprintf(szTemp, "S1%02X%04X", nCharPairs, nAddr);
    write(fpSRecord, szTemp, strlen(szTemp));
    write(fpSRecord, pDataChars, nNumDataChars);
    nSRecChecksum += (UINT8)((nAddr & 0xFF00) >> 8);
    nSRecChecksum += (UINT8)(nAddr & 0xFF);
    nSRecChecksum += nCharPairs;
    nSRecChecksum = (0xffff - (nSRecChecksum & 0xff));
    sprintf(szTemp, "%02X\r\n", (UINT8)nSRecChecksum);
    write(fpSRecord, szTemp, strlen(szTemp));
    
    return 0;
}

// SRecord Line: S1LLNNNNddddddddCC  (LL == number of char pairs to follow, NNNN == address, dddddd == data char pairs, CC == checksum)
int writeToSRecord(int fpSRecord, UINT16 nAddr, UINT8 *pBytes, int NumBytes)
{
    static int    SRecLineChars  = 0;
    static UINT16 nSRecCurrAddr  = 0;
    static UINT16 nSRecStartAddr = 0;
    static UINT16 nSRecChecksum  = 0;
    static char   szSRecLine[MAX_S19_CHARS];

    if ((nSRecCurrAddr != nAddr) || (0 == nAddr && NULL == pBytes && 0 == NumBytes))
    {
        // If there are characters in the write buffer, flush it here.
        if (SRecLineChars)
        {
            writeSRecordLine(fpSRecord, nSRecStartAddr, szSRecLine, SRecLineChars, nSRecChecksum);
            
            SRecLineChars = 0;
            nSRecChecksum = 0;
        }
        nSRecCurrAddr  = nAddr;
        nSRecStartAddr = nAddr;
    }

    while(NumBytes--)
    {
        szSRecLine[SRecLineChars++] = convertToChar((*pBytes & 0xF0) >> 4);
        szSRecLine[SRecLineChars++] = convertToChar(*pBytes & 0xF);
        nSRecChecksum += *pBytes;
        nSRecCurrAddr++;
        pBytes++;
                
        if (SRecLineChars >= MAX_S19_CHARS)
        {
            writeSRecordLine(fpSRecord, nSRecStartAddr, szSRecLine, SRecLineChars, nSRecChecksum);
            
            SRecLineChars  = 0;
            nSRecChecksum  = 0;
            nSRecStartAddr = nSRecCurrAddr;
        }
    }
       
    return 0;
}


int assembleSource(SOURCEFILE *pSourceFile, int fpSRecord, int fpListing)
{
    char line[MAX_LINE_LENGTH];
    char saveLine[MAX_LINE_LENGTH];

    char symbolName[MAX_SYMBOL_NAME_LENGTH];
    char *pszToken;
    INSTRUCTION *pInst;
    UINT16 nParam = 0;
    char mneumonic[MAX_MNEUMONIC_LENGTH + 1];
    ADDRMODE addrMode;
    int  nLocalLineNum = pSourceFile->lineNumber;
    UINT16 nAddr = 0;
    char szTempString[MAX_LINE_LENGTH];
    
    // Read each source file line until we encounter EOF or an error.
    //
    while(getNextFileLine(pSourceFile, line, MAX_LINE_LENGTH) == 0)
    {   
        // Make a copy of the line since we're going to modify it during parsing...
        //
        strncpy(saveLine, line, MAX_LINE_LENGTH);
        
        // Output the current source line to a listing file.
        //
        if (fpListing)
        {
            sprintf(szTempString, "%04d ", pSourceFile->lineNumber);
            write(fpListing, szTempString, strlen(szTempString));
        }
        
        // Increment the local line number (used because the source file line number is used recursively).
        //
        ++nLocalLineNum;
        
        // Skip comments or blank lines.
        //
        if (isCommentLine(line) || isBlankLine(line))
        {
            if (fpListing)
            {
                sprintf(szTempString, "%s\r\n", saveLine);
                write(fpListing, szTempString, strlen(szTempString));
            }
            continue;
        }
        
        // If the line contains a symbol definition, read the value or compute the address it
        // refers to and push the information into the symbol table for later use.
        //
        if (isSymbolLine(line))
        {
            // Get the first token - this should be the symbol name (may end with a ':' character).
            //
            if (NULL != (pszToken = strtok (line, ": \t\r\n")))
            {
                strncpy(symbolName, pszToken, MAX_SYMBOL_NAME_LENGTH);
                if (NULL != (pszToken = strtok (NULL, " \t\r\n")))
                {                
                    // *** EQU ***
                    if (strcasecmp(pszToken, "EQU") == 0)
                    {
                        if (fpListing)
                        {
                            sprintf(szTempString, "%s\r\n", saveLine);
                            write(fpListing, szTempString, strlen(szTempString));
                        }
                        continue;
                    }
                }
                else
                {
                    continue;
                }
            }
        }
        else
        {
            if (NULL == (pszToken = strtok (line, " \t\r\n")))
            {
                continue;
            }
            
        }
        
        // If there is no more data to process on this line, continue to the next.
        //
        if (isCommentLine(pszToken) || isBlankLine(pszToken))
        {
            continue;
        }
        
        // At this point we should have a valid instruction, start processing known instructions.
        //
        
        // *** ORG ***
        if (strcasecmp(pszToken, "ORG") == 0)
        {
            if (NULL == (pszToken = strtok (NULL, " \t\r\n")) || convertToNumber(pszToken, &nAddr))
            {
                printf("ERROR: Invalid ORG instruction\r\n");
                return -1;
            }
            
            // If we haven't already found the start address ("START" symbol), use the first ORG section found...
            if (!g_startAddress)
            {
                g_startAddress = nAddr;
            }
            
            if (fpListing)
            {
                sprintf(szTempString, "%s\r\n", saveLine);
                write(fpListing, szTempString, strlen(szTempString));
            }
            continue;
        }
        
        // *** RMB ***
        if (strcasecmp(pszToken, "RMB") == 0)
        {
            if (fpListing)
            {
                sprintf(szTempString, "%s\r\n", saveLine);
                write(fpListing, szTempString, strlen(szTempString));
            }
            continue;
        }
        
        // *** FCB ***
        if (strcasecmp(pszToken, "FCB") == 0)
        {
            if (NULL != (pszToken = strtok (NULL, " \t\r\n")))
            {
                UINT16 nValue = 0;
                if (convertToNumber(pszToken, &nValue))
                {
                    SYMBOLVALUE *symbolValue;
                    SYMBOLTYPE  symbolType;
                    if (!findSymbol(pszToken, &symbolType, &symbolValue) || symbolType != SYMBOL_TYPE_NUMBER_8BIT)
                    {
                        printf("ERROR: FCB symbol \'%s\' type doesn't match expected (type=%d)\r\n", pszToken, (int)symbolType);
                        return -1;
                    }
                    nValue = symbolValue->nsymbolValue8;
                }
                if (nValue > 255)
                {
                    printf("ERROR: FCB symbol value is larger than allowed (value=0x%04x)\r\n", (int)nValue);
                    return -1;
                }
                
                writeToSRecord(fpSRecord, nAddr, (UINT8 *)&nValue, 1);

                if (fpListing)
                {
                    sprintf(szTempString, "%04x %02x", nAddr, (nValue & 0xff));
                    write(fpListing, szTempString, strlen(szTempString));
                    sprintf(szTempString, "%s\r\n", saveLine);
                    write(fpListing, szTempString, strlen(szTempString));
                }
            }
                    
            nAddr += 1; // One byte
            continue;
        }
        
        // *** FDB ***
        if (strcasecmp(pszToken, "FDB") == 0)
        {
            if (NULL != (pszToken = strtok (NULL, " \t\r\n")))
            {
                UINT16 nValue = 0;
                UINT8 nTemp;
                if (convertToNumber(pszToken, &nValue))
                {
                    SYMBOLVALUE *symbolValue;
                    SYMBOLTYPE  symbolType;
                    if (!findSymbol(pszToken, &symbolType, &symbolValue) || symbolType != SYMBOL_TYPE_NUMBER_16BIT)
                    {
                        printf("ERROR: FCB symbol \'%s\' type doesn't match expected (type=%d)\r\n", pszToken, (int)symbolType);
                        return -1;
                    }
                    nValue = symbolValue->nsymbolValue16;
                }
                
                nTemp = ((nValue & 0xff00) >> 8);
                writeToSRecord(fpSRecord, nAddr, &nTemp, 1);
                nTemp = (nValue & 0xff);
                writeToSRecord(fpSRecord, (nAddr + 1), &nTemp, 1);
                
                if (fpListing)
                {
                    sprintf(szTempString, "%04x %04x", nAddr, nValue);
                    write(fpListing, szTempString, strlen(szTempString));
                    sprintf(szTempString, "%s\r\n", saveLine);
                    write(fpListing, szTempString, strlen(szTempString));
                }
            }
            
            nAddr += 2; // Two bytes
            continue;
        }
        
        // *** FCC ***
        if (strcasecmp(pszToken, "FCC") == 0)
        {
            if (NULL == (pszToken = strtok (NULL, "\"\r\n")))
            {
                printf("ERROR: Invalid FCC instruction\r\n");
                return -1;
            }
            else
            {
                writeToSRecord(fpSRecord, nAddr, (UINT8 *)pszToken, (int)strlen(pszToken));
                
                if (fpListing)
                {
                    sprintf(szTempString, "%04x ", nAddr);
                    write(fpListing, szTempString, (int)strlen(szTempString));
        
                    for (char *pTemp = pszToken ; *pTemp != '\0' ; pTemp++)
                    {
                        sprintf(szTempString, "%02x ", *pTemp);
                        write(fpListing, szTempString, (int)strlen(szTempString));
                    }
                    sprintf(szTempString, "%s\r\n", saveLine);
                    write(fpListing, szTempString, (int)strlen(szTempString));
                }
                
            }
            
            // TODO - how to handle leading spaces?
            
            nAddr += strlen(pszToken);
            continue;
        }
        
        if (fpListing)
        {
            sprintf(szTempString, "%s\r\n", saveLine);
            write(fpListing, szTempString, strlen(szTempString));
        }

        // For all other commands, look for the instruction mneumonic in the command list.
        //
        if (NULL == (pInst = lookUpMneumonic(pszToken)))
        {
            printf("ERROR: Invalid mneumonic \'%s\' on line %d\r\n", pszToken, nLocalLineNum);
            return -1;
        }
        strncpy(mneumonic, pszToken, MAX_MNEUMONIC_LENGTH);
        
        // Now, try to find an exact instruction match based on addressing mode.  If this command takes no parameters, we can continue to the next.
        //
        // TODO - need a better way to determine that this command only supports inherent addressing.
        if (NULL == (pszToken = strtok (NULL, " \t\r\n")) || isCommentLine(pszToken) || isBlankLine(pszToken))
        {
            if (pInst->preByte)
            {
                writeToSRecord(fpSRecord, nAddr,   &pInst->preByte, 1);
                writeToSRecord(fpSRecord, nAddr+1, &pInst->opCode, 1);
            }
            else
                writeToSRecord(fpSRecord, nAddr, &pInst->opCode, 1);

            if (fpListing)
            {
                sprintf(szTempString, "%04x ", nAddr);
                write(fpListing, szTempString, strlen(szTempString));
                if (pInst->preByte)
                {
                    sprintf(szTempString, "%02x %02x %s\r\n", pInst->preByte, pInst->opCode, line);
                    write(fpListing, szTempString, strlen(szTempString));
                }
                else
                {
                    sprintf(szTempString, "%02x %s\r\n", pInst->opCode, line);
                    write(fpListing, szTempString, strlen(szTempString));
                }
            }

            nAddr += pInst->numBytes;
            continue;
        }
        
        // Compute the addressing mode from the insruction parameters.  At this point all the symbols will be in the symbol table so if we can't
        // find the addressing mode now, it's an error.
        //
        if (computeAddrMode(nAddr, pInst, pszToken, &addrMode, &nParam))
        {
            printf("ERROR: Invalid address mode on line %d\r\n", nLocalLineNum);
            return -1;            
        }
        
        // Now that we know the instruction addressing mode, look up the exact match in the instruction table.
        //
        if (NULL == (pInst = lookUpMatchingAddrMode(pInst, addrMode)))
        {
            printf("ERROR: Instruction \'%s\' doesn\'t offer addressing mode %d\r\n", mneumonic, (int)addrMode);
            return -1;
        }
        
        // TODO - clean-up
        if (fpSRecord)
        {
            UINT16 nLocalAddr = nAddr;
            UINT8  nTemp;

            if (pInst->preByte)
            {
                writeToSRecord(fpSRecord, nLocalAddr++, &pInst->preByte, 1);
            }
            
            writeToSRecord(fpSRecord, nLocalAddr++, &pInst->opCode, 1);
            
            if (nParam > 255)
            {
                nTemp = ((nParam & 0xff00)>>8);
                writeToSRecord(fpSRecord, nLocalAddr++, &nTemp, 1);
                nTemp = (nParam & 0xff);
                writeToSRecord(fpSRecord, nLocalAddr++, &nTemp, 1);
            }
            else
            {
                // TODO - clean up.
                if (pInst->numBytes == 4 || (pInst->preByte == 0 && pInst->numBytes == 3))
                {
                    // Case where value can fit in one byte but instruction is expecting two bytes.
                    nTemp = 0;
                    writeToSRecord(fpSRecord, nLocalAddr++, &nTemp, 1);                
                }
                nTemp = (nParam & 0xff);
                writeToSRecord(fpSRecord, nLocalAddr++, &nTemp, 1);                
            }
        }
      
        // Dump source line's corresponding byte code.
        //
        // TODO - clean up.
        if (fpListing)
        {
            sprintf(szTempString, "%04x ", nAddr);
            write(fpListing, szTempString, strlen(szTempString));
            if (pInst->preByte)
            {
                sprintf(szTempString, "%02x ", pInst->preByte);
                write(fpListing, szTempString, strlen(szTempString));
            }

            sprintf(szTempString, "%02x ", pInst->opCode);
            write(fpListing, szTempString, strlen(szTempString));
                
            if (nParam > 255)
            {
                sprintf(szTempString, "%02x %02x %s\r\n", ((nParam & 0xff00)>>8), (nParam & 0xff), line);
                write(fpListing, szTempString, strlen(szTempString));
            }
            else
            {
                // TODO - clean up.
                if (pInst->numBytes == 4 || (pInst->preByte == 0 && pInst->numBytes == 3))
                {
                    sprintf(szTempString, "00 ");
                    write(fpListing, szTempString, strlen(szTempString));
                }
                sprintf(szTempString, "%02x %s\r\n",(nParam & 0xff), line);
                write(fpListing, szTempString, strlen(szTempString));
            }
        }
        
        // Increment the address counter by the number of bytes required for the instruction.
        //
        nAddr += pInst->numBytes;
    }
    
    // Special SRecord write - flushes remaining contents to file.
    //
    writeToSRecord(fpSRecord, 0, NULL, 0);
    
    // Add a final S9 SRecord line.
    //
    // TODO - compute real checksum
    
    UINT16 nSRecChecksum = (UINT8)((g_startAddress & 0xFF00) >> 8);
    nSRecChecksum += (UINT8)(g_startAddress & 0xFF);
    nSRecChecksum += 03;
    nSRecChecksum = (0xffff - (nSRecChecksum & 0xff));
    
    sprintf(szTempString, "S903%04X%02X\r\n", g_startAddress, (UINT8)nSRecChecksum);
    write(fpSRecord, szTempString, strlen(szTempString));
    
    return 0;
}


// this function is called recursively.
int buildSymbolTable(SOURCEFILE *pSourceFile, UINT16 nAddr)
{
    int  nRetVal = 0;
    char line[MAX_LINE_LENGTH];
    char symbolName[MAX_SYMBOL_NAME_LENGTH];
    char *pszToken;
    INSTRUCTION *pInst;
    UINT16 nParam = 0;
    char mneumonic[MAX_MNEUMONIC_LENGTH + 1];
    ADDRMODE addrMode;
    int  nLocalLineNum = pSourceFile->lineNumber;
    
    // Read each source file line until we encounter EOF or an error.
    //
    while(getNextFileLine(pSourceFile, line, MAX_LINE_LENGTH) == 0)
    {       
        // Increment the local line number (used because the source file line number is used recursively).
        //
        ++nLocalLineNum;
        
        // Skip comments or blank lines.
        //
        if (isCommentLine(line) || isBlankLine(line))
            continue;
        
        // If the line contains a symbol definition, read the value or compute the address it
        // refers to and push the information into the symbol table for later use.
        //
        if (isSymbolLine(line))
        {
            // Get the first token - this should be the symbol name (may end with a ':' character).
            //
            if (NULL != (pszToken = strtok (line, ": \t\r\n")))
            {
                strncpy(symbolName, pszToken, MAX_SYMBOL_NAME_LENGTH);
                if (NULL != (pszToken = strtok (NULL, " \t\r\n")))
                {                
                    // *** EQU ***
                    if (strcasecmp(pszToken, "EQU") == 0)
                    {
                        if (NULL != (pszToken = strtok (NULL, " \t\r\n")))
                        {
                            if ('\'' == *pszToken)
                            {
                                // Symbol equates to an ASCII string
                                // TODO - also ends with a ' ?
                                pushSymbol(symbolName, SYMBOL_TYPE_STRING, (pszToken + 1));
                            }
                            else if ('*' == *pszToken)
                            {
                                // Special Case: symbol refers to an address (FOO    EQU    *).
                                //
                                pushSymbol(symbolName, SYMBOL_TYPE_NUMBER_16BIT, &nAddr);
                            }
                            else
                            {
                                // Symbol equates to a number
                                if (convertToNumber(pszToken, &nParam))
                                    return -1;
                                pushSymbol(symbolName, (nParam < 256 ? SYMBOL_TYPE_NUMBER_8BIT : SYMBOL_TYPE_NUMBER_16BIT), &nParam);
                            }
                        }
                        continue;
                    }
                    // *** RMB ***
                    else if (strcasecmp(pszToken, "RMB") == 0)
                    {  
                        // Handle special below.
                    }
                    else
                    {
                        // Symbol refers to an address - note that valid instructions may follow on this same line.
                        //
                        pushSymbol(symbolName, SYMBOL_TYPE_NUMBER_16BIT, &nAddr);
                    }
                }
                else
                {
                    // Symbol refers to an address - no other instructions follow so we can move to the next line.
                    //
                    pushSymbol(symbolName, SYMBOL_TYPE_NUMBER_16BIT, &nAddr);
                    continue;
                }
            }
        }
        else
        {
            if (NULL == (pszToken = strtok (line, " \t\r\n")))
            {
                continue;
            }
            
        }
        
        // If there is no more data to process on this line, continue to the next.
        //
        if (isCommentLine(pszToken) || isBlankLine(pszToken))
        {
            continue;
        }
        
        // At this point we should have a valid instruction, start processing known instructions.
        //
        
        // *** ORG ***
        if (strcasecmp(pszToken, "ORG") == 0)
        {
            if (NULL == (pszToken = strtok (NULL, " \t\r\n")) || convertToNumber(pszToken, &nAddr))
            {
                printf("ERROR: Invalid ORG instruction\r\n");
                return -1;
            }
            
            continue;
        }
        
        // *** RMB ***
        if (strcasecmp(pszToken, "RMB") == 0)
        {
            // Symbol refers to an reserved address.
            //
            pushSymbol(symbolName, SYMBOL_TYPE_NUMBER_16BIT, &nAddr);
            if (NULL == (pszToken = strtok (NULL, " \t\r\n")) || convertToNumber(pszToken, &nParam))
            {
                printf("ERROR: Invalid RMB instruction\r\n");
                return -1;
            }
            nAddr += nParam;
            continue;
        }
        
        // *** FCB ***
        if (strcasecmp(pszToken, "FCB") == 0)
        {
            nAddr += 1; // One byte
            continue;
        }

        // *** FDB ***
        if (strcasecmp(pszToken, "FDB") == 0)
        {
            nAddr += 2; // Two bytes
            continue;
        }

        // *** FCC ***
        if (strcasecmp(pszToken, "FCC") == 0)
        {
            if (NULL == (pszToken = strtok (NULL, "\"\r\n")))
            {
                printf("ERROR: Invalid FCC instruction\r\n");
                return -1;
            }
                
            // TODO - how to handle leading spaces?
            
            nAddr += strlen(pszToken);
            continue;
        }

        // For all other commands, look for the instruction mneumonic in the command list.
        //
        if (NULL == (pInst = lookUpMneumonic(pszToken)))
        {
            printf("ERROR: Invalid mneumonic \'%s\' on line %d\r\n", pszToken, nLocalLineNum);
            return -1;
        }
        strncpy(mneumonic, pszToken, MAX_MNEUMONIC_LENGTH);
        
        // Now, try to find an exact instruction match based on addressing mode.  If this command takes no parameters, we can continue to the next.
        //
        // TODO - need a better way to determine that this command only supports inherent addressing.
        if (NULL == (pszToken = strtok (NULL, " \t\r\n")) || isCommentLine(pszToken) || isBlankLine(pszToken))
        {
            nAddr += pInst->numBytes;
            continue;
        }

        // Try to compute the addressing mode from the insruction parameters.  If we can't find the referenced symbol in the symbol table, recursively
        // call this function so we can find it, then continue.
        //
        if (-2 == (nRetVal = computeAddrMode(nAddr, pInst, pszToken, &addrMode, &nParam)))
        {
            // If we get to this point, the addressing mode can only be direct, extended, or relative (immediate and indirect require a predefined
            // constant value).  In our case, direct isn't supported because we have no need to access bytes 0-255 (internal RAM).  This only leaves
            // extended and relative and the latter is only used in a few limited cases (ex: branching instructions).  Based on this premise, the 
            // algorithm to compute the symbol address will go as follows:
            //
            // 1. Look up the instruction in the table - if it only supports a single relative addressing mode, assume the mode is relative and
            //      compute the number of bytes for the next instruction based on this.  If later the address is outside the relative address range
            //      then we need to flag it as an error and stop further processing.
            //
            // 2. If the addressing mode isn't relative, assume it's extended and look up the corresponding number of instruction bytes for the 
            //      extended addressing mode then proceed to compute the symbols address.
            //
            // NOTE: In the future if we want to support direct addressing, likely any symbols in that range (0-255 bytes) will already be defined
            //         before the code that accesses it so the code may be relativley simple.
            //
            UINT16 nNextLineAddr;
            
            if (pInst->addrMode == REL)
                nNextLineAddr = nAddr + pInst->numBytes;
            else
            {
                INSTRUCTION *pTemp = pInst;
                if (NULL == (pTemp = lookUpMatchingAddrMode(pTemp, EXT)))
                {
                    printf("ERROR: Instruction \'%s\' doesn\'t offer addressing mode %d\r\n", mneumonic, (int)EXT);
                    return -1;
                }
                nNextLineAddr = nAddr + pTemp->numBytes;
            }
            
            if (buildSymbolTable(pSourceFile, nNextLineAddr))
                return -1;
            
            nRetVal = computeAddrMode(nAddr, pInst, pszToken, &addrMode, &nParam);
        }
                
        if (nRetVal)
        {
            printf("ERROR: Invalid address mode on line %d\r\n", nLocalLineNum);
            return -1;            
        }
        
        // Now that we know the instruction addressing mode, look up the exact match in the instruction table.
        //
        if (NULL == (pInst = lookUpMatchingAddrMode(pInst, addrMode)))
        {
            printf("ERROR: Instruction \'%s\' doesn\'t offer addressing mode %d\r\n", mneumonic, (int)addrMode);
            return -1;
        }
            
        // Increment the address counter by the number of bytes required for the instruction.
        //
        nAddr += pInst->numBytes;
    }
    
    return 0;
}


int processSourceFile(SOURCEFILE sourceFile, int fpSRecord, int fpSymbols, int fpListing)
{
    int nRetVal = 0;
    char szTempString[MAX_LINE_LENGTH];
    
    // Clear the symbol table and reset count.
    //
    symbolCount = 0;
    memset(symbols, 0, (sizeof(SYMBOL) * MAX_SYMBOL_COUNT));
    
    // Scan source file contents and build up the symbol table.
    //
    if (0 != (nRetVal = buildSymbolTable(&sourceFile, 0)))
        goto Exit;
    
    if (fpSymbols)
    {
        int i;
        
        sprintf(szTempString, "  SYMBOL NAME    VALUE            [Total=%d]\r\n", (symbolCount));
        write(fpSymbols, szTempString, strlen(szTempString));
        sprintf(szTempString, "-----------------------------------------------\r\n");
        write(fpSymbols, szTempString, strlen(szTempString));

        for (i=0 ; i<symbolCount ; i++)
        {
            switch(symbols[i].symbolType)
            {
                case SYMBOL_TYPE_STRING:
                    sprintf(szTempString, "%15s, %30s\r\n", symbols[i].symbolName, symbols[i].u.symbolValueStr);
                    write(fpSymbols, szTempString, strlen(szTempString));
                    break;
                case SYMBOL_TYPE_NUMBER_8BIT:
                    sprintf(szTempString, "%15s, 0x%02x\r\n", symbols[i].symbolName, symbols[i].u.nsymbolValue8);
                    write(fpSymbols, szTempString, strlen(szTempString));
                    break;
                case SYMBOL_TYPE_NUMBER_16BIT:
                    sprintf(szTempString, "%15s, 0x%04x\r\n", symbols[i].symbolName, symbols[i].u.nsymbolValue16);
                    write(fpSymbols, szTempString, strlen(szTempString));
                    break;
                default:
                    break;
            }

        }
    }
    
    // Check for a symbole called "START" and use it as the start address (otherwise we'll use the first ORG block found during assembly
    //
    SYMBOLTYPE   symbolType;
    SYMBOLVALUE *symbolValue;
    
    if (findSymbol(START_SYMBOL_NAME, &symbolType, &symbolValue))
    {
        g_startAddress = symbolValue->nsymbolValue16;
    }
    
    // Assemble the file.
    //
    sourceFile.piterOffset = sourceFile.pFile;
    sourceFile.byteOffset  = 0;
    sourceFile.lineNumber  = 0;
    sourceFile.fEOF        = false;
    
    nRetVal = assembleSource(&sourceFile, fpSRecord, fpListing);

Exit:
    
    return nRetVal;
}


int main (int argc, const char * argv[])
{
    int nRetVal     = 0;
	int nLength     = 0;
	char *pFileName = NULL;
	int fpSource    = 0;
    int fpSRecord   = 0;
    bool fDumpSymbols = false;
    int fpSymbols   = 0;
    bool fDumpListing = false;
    int fpListing   = 0;
    struct stat fileStat;
    char *pSource   = NULL;
    SOURCEFILE sourceFile;
    

    // Print banner.
	//
    printf("\n6811ASM for Mac Version 0.3\n");
	printf("Copyright (c) 2012, Jeff Glaum.  All rights reserved.\n\n");

    // Validate command line parameters.
    //
	if (argc < 2 || argc > 4)
		goto UsageMsg;
    
    // Check command line parameters.
    //
    for(int nCount=0 ; nCount < argc - 2 ; nCount++)
    {
        if (!strcmp(argv[1+nCount], "-l"))
            fDumpListing = true;
        else if (!strcmp(argv[1+nCount], "-s"))
            fDumpSymbols = true;
        else goto UsageMsg;
    }
            
	// Copy filename into buffer.
    //
	nLength   = (int)strlen(argv[argc-1]);
	nLength  += (int)strlen(ASM_FILE_EXTENSION) + 1;
	pFileName = (char *)malloc(nLength);
	if (!pFileName)
    {
        printf("ERROR: Memory allocation failed (%d bytes)\r\n", nLength);
        nRetVal = -1;
        goto Exit;
    }
    strcpy(pFileName, argv[argc-1]);
    
	// If filename doesn't have extension, add one.
    //
	if (!strchr(pFileName, '.'))
		strcat(pFileName, ASM_FILE_EXTENSION);
    
	// Open source file for reading.
    //
	fpSource = open(pFileName, O_RDONLY);
	if (fpSource < 0)
    {
        printf("ERROR: Source file open failed (%s)\r\n", pFileName);
        nRetVal = -1;
        goto Exit;
    }

    // Get source file size.
    //
    if (fstat(fpSource, &fileStat) < 0)
    {
        printf("ERROR: Failed to obtain source file statistics (%s)\r\n", pFileName);
        nRetVal = -1;
        goto Exit;
    }
    
    // Allocate and read the file into memory.
    //
    pSource = (char *)malloc(fileStat.st_size);
    if (!pSource)
    {
        printf("ERROR: Memory allocation failed (%d bytes)\r\n", (int)fileStat.st_size);
        nRetVal = -1;
        goto Exit;
    }

    if (read(fpSource, pSource, fileStat.st_size) != fileStat.st_size)
    {
        printf("ERROR: Source file read failed (%s)\r\n", pFileName);
        nRetVal = -1;
        goto Exit;
    }

    // Update user message.
    //
    printf("Assembling: %s ...\r\n\n", pFileName);

    // Open SRecord, Symbol, and Listing files for writing if needed.
    //
    // NOTE: (filename + 1) is used to skip a leading "./foo.asm" char
    memcpy((strchr(pFileName+1, '.') + 1), S19_FILE_EXTENSION, strlen(S19_FILE_EXTENSION));
	fpSRecord = open(pFileName, O_CREAT | O_TRUNC | O_WRONLY, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
	if (fpSRecord < 0)
    {
        printf("ERROR: S-Record file open failed (%s)\r\n", pFileName);
        nRetVal = -1;
        goto Exit;
    }
    if (fDumpSymbols)
    {
        memcpy((strchr(pFileName+1, '.') + 1), SYM_FILE_EXTENSION, strlen(SYM_FILE_EXTENSION));
        fpSymbols = open(pFileName, O_CREAT | O_TRUNC | O_WRONLY, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
        if (fpSymbols < 0)
        {
            printf("ERROR: Symbol file open failed (%s)\r\n", pFileName);
            nRetVal = -1;
            goto Exit;
        }
    }
    if (fDumpListing)
    {
        memcpy((strchr(pFileName+1, '.') + 1), LST_FILE_EXTENSION, strlen(LST_FILE_EXTENSION));
        fpListing = open(pFileName, O_CREAT | O_TRUNC | O_WRONLY, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
        if (fpListing < 0)
        {
            printf("ERROR: Listing file open failed (%s)\r\n", pFileName);
            nRetVal = -1;
            goto Exit;
        }
    }
            
    // Process file contents.
    //
    memset(&sourceFile, 0, sizeof(SOURCEFILE));
    
    sourceFile.pFile       = pSource;
    sourceFile.fileSize    = (int)fileStat.st_size;
    sourceFile.piterOffset = pSource;
    sourceFile.fEOF        = false;

    if (processSourceFile(sourceFile, fpSRecord, fpSymbols, fpListing) < 0)
    {
        printf("ERROR: Source file processing failed\r\n");
        nRetVal = -1;
        goto Exit;
    }

Exit:
    
    // Clean up.
    //
	if (fpSource)
		close(fpSource);
    if (fpSRecord)
		close(fpSRecord);
    if (fpSymbols)
		close(fpSymbols);
    if (fpListing)
		close(fpListing);
	if (pSource)
		free (pSource);
	if (pFileName)
		free (pFileName);
    
	return nRetVal;
    
UsageMsg:
    
    // Display usage message.
    //
	printf("USAGE: %s [-l | -s] [<ASM file]\r\n\n", argv[0]);
    printf("    -l  Generate assembly listing file\r\n");
    printf("    -s  Generate symbol file\r\n\n");
    
    return 0;
}

