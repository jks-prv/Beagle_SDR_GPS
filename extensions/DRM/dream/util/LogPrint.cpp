/******************************************************************************\
 *
 * Copyright (c) 2006
 *
 * Author(s):
 *	Oliver Haffenden
 *
 * Description:
 * log-printing
 * subsystem - essentially just a wrapper for printf.
 *
 ******************************************************************************
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation; either version 2 of the License, or (at your option) any later
 * version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
\******************************************************************************/

#include "LogPrint.h"

#include <cstdarg>
#include <cstdio>
#include <cstring>

void logStatus(char *format, ...) {
	char s[256];
    va_list args;
    va_start(args, format);

	vsprintf(s, format, args);
	CLogPrinter::GetInstance()->LogStatus(s);
} 

void logWarning(char *format, ...) {
	char s[256];
    va_list args;
    va_start(args, format);

	vsprintf(s, format, args);
	CLogPrinter::GetInstance()->LogWarning(s);
} 

void logError(char *format, ...) {
	char s[256];
    va_list args;
    va_start(args, format);

	vsprintf(s, format, args);
	CLogPrinter::GetInstance()->LogWarning(s);
} 

CLogPrinter * CLogPrinter::mpFirstInstance = 0;
CLogPrinter * CLogPrinter::mpNullPrinter = 0;

CLogPrinter * CLogPrinter::GetInstance(void)
{
	if (mpFirstInstance == 0)
		mpFirstInstance = GetNullInstance();

	return mpFirstInstance;
}

void CLogPrinter::AddInstance(CChainingLogPrinter *pInstance)
{
	pInstance->SetNextInstance(GetInstance());
	mpFirstInstance = pInstance;
}

CLogPrinter * CLogPrinter::GetNullInstance(void)
{
	if (!mpNullPrinter)
		mpNullPrinter = new CNullLogPrinter;
	return mpNullPrinter;
}

CChainingLogPrinter::CChainingLogPrinter()
{
	mpNextInstance = GetNullInstance(); 
	AddInstance(this);
}

void CChainingLogPrinter::LogStatus(char *s)
{
	LogStatusSpecific(s);
	mpNextInstance->LogStatus(s);
}

void CChainingLogPrinter::LogWarning(char *s)
{
	LogWarningSpecific(s);
	mpNextInstance->LogWarning(s);
}

void CChainingLogPrinter::LogError(char *s)
{
	LogErrorSpecific(s);
	mpNextInstance->LogWarning(s);
}

void CChainingLogPrinter::SetNextInstance(CLogPrinter *pInstance)
{
	mpNextInstance = pInstance;
}


CFileLogPrinter::CFileLogPrinter()
{
	mpLogFile = fopen("DebugLog.txt", "a");
	fprintf(mpLogFile, "Start of logfile\n");
	fflush(mpLogFile);
}


void CFileLogPrinter::LogStatusSpecific(char *s)
{
	fprintf(mpLogFile, "STATUS: %s\n",s);
	fflush(mpLogFile);
}

void CFileLogPrinter::LogWarningSpecific(char *s)
{
	fprintf(mpLogFile, "WARNING: %s\n",s);
	fflush(mpLogFile);
}

void CFileLogPrinter::LogErrorSpecific(char *s)
{
	fprintf(mpLogFile, "ERROR: %s\n",s);
	fflush(mpLogFile);
}

CFileLogPrinter *CFileLogPrinter::mpInstance = 0;
void CFileLogPrinter::Instantiate()
{
	if (!mpInstance)
		mpInstance = new CFileLogPrinter;
}

void CPrintfLogPrinter::LogStatusSpecific(char *s)
{
	fprintf(stderr, "STATUS: %s\n",s);
	fflush(stderr);
}

void CPrintfLogPrinter::LogWarningSpecific(char *s)
{
	fprintf(stderr, "WARNING: %s\n",s);
	fflush(stderr);
}

void CPrintfLogPrinter::LogErrorSpecific(char *s)
{
	fprintf(stderr, "ERROR: %s\n",s);
	fflush(stderr);
}

void CNullLogPrinter::LogStatus(char *)
{
}

void CNullLogPrinter::LogWarning(char *)
{
}

void CNullLogPrinter::LogError(char *)
{
}

CPrintfLogPrinter *CPrintfLogPrinter::mpInstance = 0;
void CPrintfLogPrinter::Instantiate()
{
	if (!mpInstance)
		mpInstance = new CPrintfLogPrinter;
}

CPrintfLogPrinter::CPrintfLogPrinter() {}
