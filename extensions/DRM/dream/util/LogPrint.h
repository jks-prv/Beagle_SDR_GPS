/******************************************************************************\
 *
 * Copyright (c) 2006
 *
 * Author(s):
 *	Oliver Haffenden
 *
 * Description:
//This is a temporary implementation of the new log-printing
//subsystem - essentially just a wrapper for printf.
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

#ifndef LOG_PRINT_H_INCLUDED
#define LOG_PRINT_H_INCLUDED

#include <cstdlib>
#include <cstdio>

void logStatus(char *format, ...);
void logWarning(char *format, ...);
void logError(char *format, ...);

class CChainingLogPrinter;

class CLogPrinter
{
public:
	CLogPrinter() {}
	virtual ~CLogPrinter() {}

	virtual void LogStatus(char *s) = 0;
	virtual void LogWarning(char *s) = 0;
	virtual void LogError(char *s) = 0;


	static CLogPrinter *GetInstance(void);

protected:
	static void AddInstance(CChainingLogPrinter *pInstance);
	static CLogPrinter * GetNullInstance(void);

private:
	static CLogPrinter *mpFirstInstance;
	static CLogPrinter *mpNullPrinter;
};


class CChainingLogPrinter : public CLogPrinter
{
	friend class CLogPrinter;
public:
	virtual void LogStatus(char *s);
	virtual void LogWarning(char *s);
	virtual void LogError(char *s);

	virtual void LogStatusSpecific(char *s) = 0;
	virtual void LogWarningSpecific(char *s) = 0;
	virtual void LogErrorSpecific(char *s) = 0;

protected:
	void SetNextInstance(CLogPrinter *pInstance);
	CChainingLogPrinter();

private:
	CLogPrinter *mpNextInstance;
};

class CFileLogPrinter : public CChainingLogPrinter
{
public:
	virtual void LogStatusSpecific(char *s);
	virtual void LogWarningSpecific(char *s);
	virtual void LogErrorSpecific(char *s);

	static void Instantiate();
protected:
	CFileLogPrinter();
private:
	FILE *mpLogFile;
	static CFileLogPrinter *mpInstance;
};

class CPrintfLogPrinter : public CChainingLogPrinter
{
public:
    virtual void LogStatusSpecific(char *string);
    virtual void LogWarningSpecific(char *string);
    virtual void LogErrorSpecific(char *string);

	static void Instantiate();
protected:
	CPrintfLogPrinter();

private:
	static CPrintfLogPrinter *mpInstance;
};

class CNullLogPrinter : public CLogPrinter
{
public:
    virtual void LogStatus(char *string);
    virtual void LogWarning(char *string);
    virtual void LogError(char *string);
};


#endif
