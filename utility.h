//
//  utility.h
//  MC68HC11 Assembler
//
//  Created by Jeff Glaum on 9/22/11.
//  Copyright 2011 __MyCompanyName__. All rights reserved.
//

int getNextFileLine(SOURCEFILE *pSourceFile, char *pLine, int nMaxLineLength);
int getFirstFileLine(SOURCEFILE *pSourceFile, char *pLine, int nMaxLineLength);
void trimTrailingWhitespace(char *pszString);

bool isCommentLine(char *pszLine);
bool isBlankLine(char *pszLine);
bool isSymbolLine(char *pszLine);

bool isValidSymbolName(char *pszToken);
bool isValidNumber(char *pszToken);
bool isIndirectParams(char *pszParamString, char *pszValue, ADDRMODE *paddrMode);

int convertToNumber(char *pszToken, UINT16 *pnNumber);