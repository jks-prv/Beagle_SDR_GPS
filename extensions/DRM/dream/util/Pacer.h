#ifndef PACER_H_INCLUDED
#define PACER_H_INCLUDED

#include "../GlobalDefinitions.h"

#ifdef _WIN32
# ifdef _WIN32_WINNT
#  undef _WIN32_WINNT
# endif
# define _WIN32_WINNT _WIN32_WINNT_WINXP
# include <windows.h>
#endif

class CPacer
{
public:
	CPacer(uint64_t ns);
	~CPacer();
	uint64_t nstogo();
	void wait();
protected:
	uint64_t timekeeper;
	uint64_t interval;
#ifdef _WIN32
	HANDLE hTimer;
#endif
};
#endif
