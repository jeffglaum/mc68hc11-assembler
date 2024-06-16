//
//  utility.c
//  MC68HC11 Assembler
//
//  Created by Jeff Glaum on 9/22/11.
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


int getNextFileLine(SOURCEFILE *pSourceFile, char *pLine, int nMaxLineLength)
{
    
    // If we've already processed the file to the end, return EOF.
    //
    if (pSourceFile->fEOF)
        return EOF;
    
    // Loop through the file as long as there are bytes to read.
    //
    while ((pSourceFile->byteOffset < pSourceFile->fileSize) && (nMaxLineLength > 0))
    {
        *pLine = *pSourceFile->piterOffset;
        
        // If we encounter an EOL character, NULL-terminate the output string, move the file index to
        // the start of the next line, then exit the loop.
        //
        if (*pLine == '\r' || *pLine == '\n')
        {
            *pLine = '\0';
            
            if (*pSourceFile->piterOffset == '\n')
            {
                pSourceFile->piterOffset++;
                pSourceFile->byteOffset++;
                pSourceFile->lineNumber++;
                break;
            }
        }
        ++pLine;
        nMaxLineLength--;
        pSourceFile->piterOffset++;
        pSourceFile->byteOffset++;
    }
    
    // If we exceeded the space provided by the caller, it's an error.
    //
    if (0 == nMaxLineLength)
    {
        return -2;
    }
    
    // Terminate the caller's buffer (in case we exited the loop due to EOF).
    //
    *pLine = '\0';
    
    // If there are no more bytes to read, signal the EOF flag to be returned on the next call.
    //
    if (pSourceFile->byteOffset >= pSourceFile->fileSize)
        pSourceFile->fEOF = true;
    
    return 0;
}


int getFirstFileLine(SOURCEFILE *pSourceFile, char *pLine, int nMaxLineLength)
{
    pSourceFile->piterOffset = pSourceFile->pFile;
    pSourceFile->byteOffset  = 0;    
    pSourceFile->lineNumber  = 0;
    pSourceFile->fEOF        = false;
    
    return getNextFileLine(pSourceFile, pLine, nMaxLineLength);
}


void trimTrailingWhitespace(char *pszString)
{
    for (int i=0 ; pszString[i] != '\0' ; i++)
    {
        if (pszString[i] == ' ' || pszString[i] == '\t')
        {
            pszString[i] = '\0';
            break;
        }
    }
}

bool isCommentLine(char *pszLine)
{   
    if (pszLine[0] == ';' ||  pszLine[0] == '*' || (pszLine[0] == '/' && pszLine[1] == '/'))
        return true;
    
    return false;
}


bool isBlankLine(char *pszLine)
{  
    if (pszLine[0] == '\0' || pszLine[0] == '\r' || pszLine[0] == '\n')
        return true;
    
    return false;
}


bool isSymbolLine(char *pszLine)
{
    // Symbol lines don't start with whitespace
    if (pszLine[0] == ' ' || pszLine[0] == '\t')
        return false;
    
    return true;
}


bool isValidSymbolName(char *pszToken)
{
    // TODO
    return true;
}


bool isValidNumber(char *pszToken)
{
    bool fisHexNumber = false;
    char *pTemp = pszToken;
    
    if (*pTemp == '$')
        fisHexNumber = true;
    
    // TODO  - cmp #'a'
    if (*pTemp == '\'')
        return true;
    
    if (*pTemp == '$' || *pTemp == '%' || *pTemp == '@' || *pTemp == '\'')
        ++pTemp;
    
    
    
    while(*pTemp != '\0')
    {
        if (fisHexNumber == false && !(*pTemp >= '0' && *pTemp <= '9'))
            return false;
        
        if (fisHexNumber == true && !(*pTemp >= 'A' && *pTemp <= 'F') && !(*pTemp >= 'a' && *pTemp <= 'f') && !(*pTemp >= '0' && *pTemp <= '9'))
            return false;
        
        ++pTemp;
    }
    
    return true;
}



bool isIndirectParams(char *pszParamString, char *pszValue, ADDRMODE *paddrMode)
{
    bool fFoundComma = false;
    char *pTemp = pszParamString;
    
    while (*pTemp != '\0')
    {
        if (true == fFoundComma && *pTemp != ' ' && *pTemp != '\t')
        {
            if (*pTemp == 'X' || *pTemp == 'x')
            {
                *paddrMode = INDX;
                return true;
            }
            else if (*pTemp == 'Y' || *pTemp == 'y')
            {
                *paddrMode = INDY;
                return true;                
            }
            else
                return false;
        }
        if (*pTemp == ',')
        {
            fFoundComma = true;
            *pTemp = '\0';
            strncpy(pszValue, pszParamString, MAX_SYMBOL_NAME_LENGTH);
        }
        ++pTemp;
    }
    
    return false;
}


int convertToNumber(char *pszToken, UINT16 *pnNumber)
{
    
    // Numeric expressions must start with $%@' or [0-9]
    if (*pszToken != '$' && *pszToken != '%' && *pszToken != '@' && *pszToken != '\'' && !(*pszToken >= '0' && *pszToken <= '9'))
    {
        return -1;
    }
    
    switch (*pszToken)
    {
        case '$':
        {
            // Hex
            for (char *pTemp = (pszToken + 1) ; *pTemp != '\0' ; pTemp++)
            {
                if (!(*pTemp >= '0' && *pTemp <= '9') && !(*pTemp >= 'A' && *pTemp <= 'F') && !(*pTemp >= 'a' && *pTemp <= 'f'))
                {
                    return -1;
                }            }
            *pnNumber = strtol((pszToken + 1), NULL, 16);            
        }
            break;
        case '%':
            // Binary - TODO
            for (char *pTemp = (pszToken + 1) ; *pTemp != '\0' ; pTemp++)
            {
                if (*pTemp != '0' && *pTemp != '1')
                {
                    return -1;
                }            
            }
            break;
        case '@':
            // Octal
            for (char *pTemp = (pszToken + 1) ; *pTemp != '\0' ; pTemp++)
            {
                if (!(*pTemp >= '0' && *pTemp <= '7'))
                {
                    return -1;
                }
            }
            *pnNumber = strtol((pszToken + 1), NULL, 8);
            break;
        case '\'':
            // Single ASCII character
            *pnNumber = *(pszToken + 1);
        default:
            // Decimal
            for (char *pTemp = (pszToken + 1) ; *pTemp != '\0' ; pTemp++)
            {
                if (!(*pTemp >= '0' && *pTemp <= '9'))
                {
                    return -1;
                }
            }
            *pnNumber = strtol(pszToken, NULL, 10);
            break;
    }
    
    return 0;
}

